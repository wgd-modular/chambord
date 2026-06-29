/* Copyright 2025 Mark Washeim
  Author: Mark Washeim <blueprint@poetaster.de>

  This program is a derivative of one made by Rich Heslip, 2023,
  The euclidean bits are derivatives of code from bastl instruments.

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.

  See http://creativecommons.org/licenses/MIT/ for more information.

  -----------------------------------------------------------------------------


  // sample player inspired by Jan Ostman's ESP8266 drum machine http://janostman.wordpress.com

  samples for beatbox from:
  giddster ( https://freesound.org/people/giddster/ )
  AlienXXX ( https://freesound.org/people/AlienXXX/

  The euclid code originates at:
  https://github.com/bastl-instruments/one-chip-modules/blob/master/EUCLID/EUCLID.ino

*/

bool debug = false;

#include <Arduino.h>
#include "stdio.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"

#include "io.h"
#include "euclid.h"
#include "filter.h"

#include <math.h>     // tanhf for the output soft-clip
#include <EEPROM.h>   // flash-backed settings persistence

// we have 8 voices that can play any sample when triggered
// this structure holds the settings for each voice
// 80s only to 20, jungle to 29
// we use a header per sample set
//
// Pick the sound kit at compile time. The CI builds one firmware per kit by
// passing e.g. -DCHAMBORD_KIT_80S ; locally, define one of these (or leave the
// default trippy). Each kit header defines voice_t/voice[] and pulls in its
// own samples. ("not ready" kits world/beatbox/bbox have no sample data.)
#if   defined(CHAMBORD_KIT_80S)
  #include "80s.h"
#elif defined(CHAMBORD_KIT_ANGULARJ)
  #include "angularj.h"
#elif defined(CHAMBORD_KIT_MIX)
  #include "mix.h"
#elif defined(CHAMBORD_KIT_TEKKE)
  #include "tekke.h"
#elif defined(CHAMBORD_KIT_ACOUSTIC3)
  #include "acoustic3.h"
#else
  #include "trippy.h"
#endif
// we can have an arbitrary number of samples but you will run out of memory at some point
// sound sample files are 22khz 16 bit signed PCM format - see the sample include files for examples
// you can change the sample rate to whatever you want but most testing was done at 22khz. 44khz probably works but not much testing was done
// use the wave2header22khz.exe utility to automagically batch convert all the .wav files in a directory into the required header files
// put your 22khz or 44khz PCM wav files in a sample subdirectory with a copy of the utility, run the utility and it will generate all the required header files
// wave2header creates a header file containing the signed PCM values for each sample - note that it may change the name of the file if required to make it "c friendly"
// wave2header also creates sampledefs.h which is an array of structures containing information about each sample file
// the samples are arranged in alphabetical order to facilitate grouping samples by name - you can manually edit this file to change the order of the samples as needed
// sampledefs.h contains other information not used by this program e.g. the name of the sample file - I wrote it for another project
// wave2header also creates "samples.h" which #includes all the generated header files


#define NUM_SAMPLES (sizeof(sample)/sizeof(sample_t))

// What turning the encoder does. Press the encoder to step through these
// for the currently selected channel: select -> sample -> volume -> pitch.
enum {
  MODE_SELECT = 0, // turn = choose channel (1-8)
  MODE_SAMPLE,     // turn = change the sample of the selected channel
  MODE_VOLUME,     // turn = change the volume of the selected channel
  MODE_PITCH,      // turn = change the pitch of the selected channel
  MODE_COUNT
};

int display_mode = MODE_SELECT;

// Encoder direction. 1 = normal, -1 = inverted. The CI builds both variants by
// passing -DCHAMBORD_ENC_DIR=... ; to change it locally edit the default below.
#ifndef CHAMBORD_ENC_DIR
#define CHAMBORD_ENC_DIR 1
#endif
const int ENCODER_DIR = CHAMBORD_ENC_DIR;

// on the long ec11 these are swapped A 19, B 18
const int encoderA_pin = 18;
const int encoderB_pin = 19;
const int encoderSW_pin = 28;

#define DOUBLE_CLICK_THRESHOLD 400 
// encoder
#include <RotaryEncoder.h>
RotaryEncoder encoder(encoderB_pin, encoderA_pin, RotaryEncoder::LatchMode::FOUR3);

void __not_in_flash_func(checkEncoderPosition)() {
  encoder.tick();   // call tick() to check the state.
}

// DAC code, approach from mozzi

#include <I2S.h>
// GPIO pin numbers
#define pBCLK 21
#define pWS (pBCLK+1)
#define pDOUT 20

I2S DAC(OUTPUT);

static void startAudio() {
  DAC.setBCLK(pBCLK);
  DAC.setDATA(pDOUT);
  DAC.setBitsPerSample(16);
  DAC.setBuffers(4, 256, 0);
  DAC.begin(44100);
}

static void stopAudio() {
  DAC.end();
}
inline bool canBufferAudioOutput() {
  return (DAC.availableForWrite());
}

// these are irq timers for handling led signals
#include "timers.h"



// encoder button
#include <Bounce2.h>
Bounce2::Button enc_button = Bounce2::Button();

// additions
#include <Wire.h>

volatile int clk_display;
uint32_t clk_sync_last;

// input clk tracking
volatile int clk_state_last; // track the CLOCKIN pin state.

int clk_state = 0;
int clk_hits = 0;
uint32_t clk_sync_ms = 0;
bool sync = false; // used to detect if we have input sync


// begin hardware definitions
const int key_pins[] = { 0, 2, 4, 6, 8, 10, 12, 14 };
const int led_pins[] = { 1, 3, 5, 7, 9, 11, 13, 15 };


// variables for UI state management
int encoder_pos_last = 0;
int encoder_delta = 0;
uint32_t encoder_push_millis;
uint32_t step_push_millis;

// currently not used
int step_push = -1;
bool step_edited = false;
char seq_info[11];  // 10 chars + nul FIXME

bool encoder_held = false;

uint8_t display_repeats = 0;
uint8_t display_vol = 100;
uint8_t display_pitch = 50;
String display_pat;


// END additions

// from pikocore filter
uint8_t filter_fc = LPF_MAX + 10;
uint8_t hpf_fc = 0;
uint8_t filter_q = 0;

int16_t CV_last;

//#define MONITOR_CPU  // define to monitor Core 2 CPU usage on pin CPU_USE

// last time btn_one release
unsigned long btnOneLastTime;
int cv_track = 42; // used to assign a track CV modulation set out of bounds to start


// sample and debounce
// scan input jacks
bool scanbuttons(void)
{
  bool pressed;
  for (int i = 0; i < NUM_BUTTONS; ++i) {
    switch (i) { // sample gate inputs
      case 0:
        pressed = digitalRead(BUTTON0); // active low key inputs
        break;
      case 1:
        pressed = digitalRead(BUTTON1);
        break;
      case 2:
        pressed = digitalRead(BUTTON2);
        break;
      case 3:
        pressed = digitalRead(BUTTON3);
        break;
      case 4:
        pressed = digitalRead(BUTTON4);
        break;
      case 5:
        pressed = digitalRead(BUTTON5);
        break;
      case 6:
        pressed = digitalRead(BUTTON6);
        break;
      case 7:
        pressed = digitalRead(BUTTON7);
        break;
    }

    if (pressed) {
      if (debouncecnt[i] <= 3) ++debouncecnt[i];
      if (debouncecnt[i] == 2) { // trigger on second sample of key active
        button[i] = 1;
      }
    }
    else {
      debouncecnt[i] = 0;
      button[i] = 0;
    }
  }
  if (pressed) return true;
  else return false;
}


// include here to avoid forward references - I'm lazy :)
// this is the main class for generating output samples using the internal sequencer.

#include "seq.h"

// ---------------------------------------------------------------------------
// Audio mixing: precomputed per-voice gain + attack ramp (anti-click)
// ---------------------------------------------------------------------------
#define RAMP_LEN 48                 // ~1.1ms attack at 44.1kHz to kill retrigger clicks
int32_t  voice_gain[NTRACKS];       // Q15 gain (level/1000) precomputed so the mix loop has no divide
uint16_t voice_ramp[NTRACKS];       // attack ramp counter per track, counts up to RAMP_LEN

// set a track level (0-1000) and recompute its Q15 mix gain
inline void setLevel(int track, int level) {
  if (level < 0) level = 0;
  if (level > 1000) level = 1000;
  voice[track].level = level;
  voice_gain[track] = ((int32_t)level * 32768) / 1000;
}

// ---------------------------------------------------------------------------
// Settings persistence to flash (survives power cycle)
// stored: per-track sample + pitch, plus CV/selected track
// ---------------------------------------------------------------------------
#define EE_MAGIC 0xB4
#define EE_SIZE  256
struct persist_t {
  uint8_t  magic;
  uint8_t  cv_track;
  uint8_t  current_track;
  int16_t  sample[NTRACKS];
  uint16_t incr[NTRACKS];
  int16_t  level[NTRACKS];
};

bool     settings_dirty = false;
uint32_t settings_change_ms = 0;
inline void markDirty() { settings_dirty = true; settings_change_ms = millis(); }

void loadSettings() {
  persist_t p;
  EEPROM.get(0, p);
  if (p.magic != EE_MAGIC) return;            // nothing valid stored yet
  cv_track = p.cv_track;
  current_track = p.current_track % NTRACKS;
  for (int t = 0; t < NTRACKS; ++t) {
    if (p.sample[t] >= 0 && p.sample[t] < (int)NUM_SAMPLES) voice[t].sample = p.sample[t];
    voice[t].sampleincrement = constrain(p.incr[t], 2048, 8192);
    setLevel(t, p.level[t]);
  }
}

void saveSettings() {
  persist_t p;
  p.magic = EE_MAGIC;
  p.cv_track = cv_track;
  p.current_track = current_track;
  for (int t = 0; t < NTRACKS; ++t) {
    p.sample[t] = voice[t].sample;
    p.incr[t]   = voice[t].sampleincrement;
    p.level[t]  = voice[t].level;
  }
  EEPROM.put(0, p);
  EEPROM.commit();   // idles core1 internally during the flash erase/program
}

// ---------------------------------------------------------------------------
// Unified LED feedback engine
// Each LED is a single GPIO over a dual-colour part: GPIO low = red (floor),
// high = green, and a duty cycle in between = amber. ledRender() does software
// PWM from led_duty[]; updateUI() sets led_duty[] from the current UI state.
// ---------------------------------------------------------------------------
#define LED_LEVELS 8        // PWM steps -> 1ms tick gives ~125Hz refresh
#define LED_RED    0        // GPIO mostly low  -> red (also the idle floor colour)
#define LED_AMBER  4        // ~50% duty        -> amber
#define LED_GREEN  8        // GPIO high        -> green

volatile uint8_t led_duty[8] = {0};
volatile uint8_t led_phase = 0;

// software PWM, called every 1ms from the ISR timer (kept in RAM so it never
// waits on the flash bus that the audio core is reading samples from)
void __not_in_flash_func(ledRender)() {
  uint8_t ph = led_phase;
  for (int i = 0; i < 8; ++i) {
    digitalWrite(led[i], (led_duty[i] > ph) ? HIGH : LOW);
  }
  led_phase = (ph + 1) % LED_LEVELS;
}

// When the encoder hasn't been touched for this long, the row turns into a
// trigger-activity display ("screensaver"). While you ARE editing, the row
// shows only the selected channel's page so nothing distracts you.
#define SCREENSAVER_MS 3000
uint32_t ui_activity_ms = 0;

// Recompute per-channel LED colours. While editing, feedback lives only on the
// selected channel's own LED, colour-coded by page (green = choosing a thing,
// amber = setting an amount):
//   green steady   = SELECT  (turn to pick the channel)
//   green blinking = SAMPLE  (turn to change the sample)
//   amber steady   = VOLUME  (turn to change the volume)
//   amber blinking = PITCH   (turn to change the pitch)
// After SCREENSAVER_MS idle, every channel flashes green on a trigger instead.
// Throttled by the caller (~5ms) so it doesn't hog the flash bus.
void updateUI() {
  uint32_t now = millis();
  bool blink = (now / 150) & 1; // ~3 Hz
  bool screensaver = (now - ui_activity_ms) > SCREENSAVER_MS;

  for (int i = 0; i < 8; ++i) {
    uint8_t d = LED_RED; // idle floor

    if (screensaver) {
      if (voice[i].isPlaying) d = LED_GREEN;          // trigger-activity display
    } else if (i == current_track) {
      switch (display_mode) {
        case MODE_SELECT: d = LED_GREEN;                  break; // green steady
        case MODE_SAMPLE: d = blink ? LED_GREEN : LED_RED; break; // green blink
        case MODE_VOLUME: d = LED_AMBER;                  break; // amber steady
        case MODE_PITCH:  d = blink ? LED_AMBER : LED_RED; break; // amber blink
      }
    }
    led_duty[i] = d;
  }
}

#define DISPLAY_TIME 2000 // time in ms to display numbers on LEDS
int32_t display_timer;

// show a number in binary on the LEDs
void display_value(int16_t value) {
  for (int i = 7; i >= 0; i--) { // NOPE + 1 can loop this way because port assignments are sequential
    digitalWrite(led[i], value & 1);
    value = value >> 1;
  }
  display_timer = millis();
}

// rotate trigger pattern
uint16_t rightRotate(int shift, uint16_t value, uint8_t pattern_length) {
  uint16_t mask = ((1 << pattern_length) - 1);
  value &= mask;
  return ((value >> shift) | (value << (pattern_length - shift))) & mask;
}



// main core setup
void setup() {
  // set clock speed as in picokore
  //set_sys_clock_khz(264000, true); don't do this :)

  if (debug) Serial.begin(57600);

  // setup timers
  if (ITimer3.attachInterruptInterval(HW_TIMER_INTERVAL_MS * 1000, TimerHandler)) {
    if (debug) Serial.print(F("Starting ITimer1 OK, millis() = "));
    if (debug) Serial.println(millis());
    timersUp = true;
  } else {
    if (debug) Serial.println(F("Can't set ITimer1. Select another freq. or timer"));
    timersUp = false;
  }
  analogReadResolution(10);

  // Audio is rendered free-running on core1 and paced by the I2S DMA buffer
  // (DAC.write blocks when full), so no dedicated audio-rate timer is needed.

  // single LED software-PWM renderer (replaces the old b2/b5/b7/b10 handlers)
  ISR_timer.setInterval(1L, ledRender);


  if (debug) Serial.flush();


  // ENCODER
  pinMode(encoderA_pin, INPUT_PULLUP);
  pinMode(encoderB_pin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(encoderA_pin), checkEncoderPosition, CHANGE);
  attachInterrupt(digitalPinToInterrupt(encoderB_pin), checkEncoderPosition, CHANGE);
  // Encoder button
  
  enc_button.attach( SHIFTBUTTON, INPUT ); // USE EXTERNAL PULL-UP
  enc_button.interval(5); // 5ms debounce
  enc_button.setPressedState(LOW);

#ifdef MONITOR_CPU
  pinMode(CPU_USE, OUTPUT); // for monitoring CPU usage
#endif

  pinMode(BUTTON0, INPUT); // jack inputs
  pinMode(BUTTON1, INPUT);
  pinMode(BUTTON2, INPUT);
  pinMode(BUTTON3, INPUT);
  pinMode(BUTTON4, INPUT);
  pinMode(BUTTON5, INPUT);
  pinMode(BUTTON6, INPUT);
  pinMode(BUTTON7, INPUT);

  pinMode(LED0, OUTPUT);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  pinMode(LED4, OUTPUT);
  pinMode(LED5, OUTPUT);
  pinMode(LED6, OUTPUT);
  pinMode(LED7, OUTPUT);

  //pinMode(CLOCKOUT, OUTPUT);
  //pinMode(CLOCKIN, INPUT_PULLUP);

  pinMode(23, OUTPUT); // thi is to switch to PWM for power to avoid ripple noise
  digitalWrite(23, HIGH);


  //display_value(NUM_SAMPLES); // show number of samples on the display

  // init per-voice mix gains and ramps, then restore saved settings from flash
  for (int t = 0; t < NTRACKS; ++t) {
    setLevel(t, voice[t].level);
    voice_ramp[t] = RAMP_LEN;   // not ramping until first trigger
  }
  EEPROM.begin(EE_SIZE);
  loadSettings();

  startAudio();

}



// main core handles UI
void loop() {

  // timer
  uint32_t now = millis();
  scanbuttons(); // actually jack inputs

  // update the channel & play sample
  for (int i = 0; i < NTRACKS; ++i) { // scan all the trigger inputs (not the encoder button)
    if (button[i]) {
      //digitalWrite(led[i], 1); // we're doing the leds in timers.h
      voice[i].sampleindex = 0; // trigger sample for this track
      voice[i].isPlaying = true;
      voice_ramp[i] = 0; // restart attack ramp -> click-free retrigger
    }
  }

  encoder.tick();

  int encoder_pos = encoder.getPosition();
  if ( (encoder_pos != encoder_pos_last )) {
    encoder_delta = (encoder_pos - encoder_pos_last) * ENCODER_DIR;
  }

  // update encoder button state
  enc_button.update();

  // led updates on timer


  // use encoder and button
  if (encoder_delta) {
    ui_activity_ms = now; // wake the display out of screensaver

    // MODE_SELECT: turn to choose which channel (1-8) you're working on
    if ( display_mode == MODE_SELECT && ! enc_button.pressed() )  {
      // +8 keeps us in positive range for the modulo
      current_track = (current_track + encoder_delta + 8) % 8;
      // NOTE: selecting a channel no longer changes its volume (that caused clicks)
    }

    // MODE_SAMPLE: turn to change the sample of the selected channel
    if ( display_mode == MODE_SAMPLE ) {
      int result = voice[current_track].sample + (encoder_delta > 0 ? 8 : -8);
      if (debug) Serial.println(result);
      if (result >= 0 && result <= NUM_SAMPLES - 1) {
        voice[current_track].sample = result;
        voice_ramp[current_track] = 0; // re-attack to soften the swap if it's ringing
        markDirty();
      }
    }

    // MODE_VOLUME: turn to change the volume of the selected channel
    if ( display_mode == MODE_VOLUME ) {
      setLevel(current_track, voice[current_track].level + encoder_delta * 50);
      markDirty();
    }

    // MODE_PITCH: turn to change the pitch of the selected channel.
    // Unity = 4096 (1:12 fixed point); 2048..8192 spans one octave down..up.
    // 128/detent (~2.6 detents per semitone) - the old step of 10 was so tiny
    // (~0.24%/detent) that it felt like nothing happened.
    if ( display_mode == MODE_PITCH ) {
      int pitch_change = voice[current_track].sampleincrement + (encoder_delta * 128);
      pitch_change = constrain(pitch_change, 2048, 8192); // constrain returns a value, must assign
      pitch_change &= ~1;                                 // keep even -> no click
      voice[current_track].sampleincrement = pitch_change;
      markDirty();
    }
  }

  /// only set new pos last after use of the delta
  encoder_pos_last = encoder_pos;
  encoder_delta = 0;  // we've used it


  // Encoder button:
  //   short press  -> step turn-function: select -> pitch -> sample -> select
  //   long press (>700ms) -> toggle CV control of the selected channel's volume
  if (enc_button.rose()) {
    ui_activity_ms = now;
    btnOneLastTime = enc_button.previousDuration();
    if (btnOneLastTime > 700) {
      cv_track = (cv_track == current_track) ? 99 : current_track; // 99 = CV off
      markDirty();
    }
  } else if (enc_button.pressed() ) {
    ui_activity_ms = now;
    encoder_push_millis = now;
    display_mode = display_mode + 1;
    if ( display_mode >= MODE_COUNT) display_mode = MODE_SELECT;
  } else {
    encoder_push_millis = 0;
    encoder_held = false;
  }

  // CV input modulates the volume of the assigned channel (if any).
  // CV-driven volume is intentionally NOT persisted (would wear flash).
  if (cv_track < NTRACKS) {
    int16_t cvv = CV;
    if (cvv != CV_last) {
      setLevel(cv_track, constrain(cvv, 0, 350));
      CV_last = cvv;
    }
  }

  // Refresh UI + sample CV at ~200Hz (throttled so core0 doesn't starve the
  // audio core's flash reads -> this was a source of pops while turning).
  static uint32_t ui_last = 0;
  if (now - ui_last >= 5) {
    ui_last = now;
    CV = analogRead(A0);
    updateUI();
  }

  // Debounced auto-save, but only while nothing is sounding, so the brief
  // audio pause during the flash write happens in silence (no pop).
  if (settings_dirty && (now - settings_change_ms) > 1500) {
    bool anyPlaying = false;
    for (int i = 0; i < NTRACKS; ++i) if (voice[i].isPlaying) { anyPlaying = true; break; }
    if (!anyPlaying ) {
      saveSettings();
      settings_dirty = false;
    }
  }



  /*
     These are from the original scarp peakobeats
      if ( (encoder_pos != encoder_pos_last ) && display_mode == 1 ) {
        //uint8_t re = seq[i].trigger->getRepeats() + encoder_delta;
        seq[i].trigger->setRepeats(encoder_delta);
        display_repeats = seq[i].trigger->getRepeats();

      }
      // change volume on pot 1
      if (!potlock[1] && display_mode == 0) {
        int16_t level = (int16_t)(map(potvalue[1], POT_MIN, POT_MAX, 0, 1000));
        voice[current_track].level = level;
        display_vol = level / 10;
        // change sample volume level if pot has moved enough
      }
      if (!potlock[0] && display_mode == 1 ) {
        //filter_fc = potvalue[0] * (LPF_MAX + 10) / 4096;
        seq[i].fills = map(potvalue[0], POT_MIN, POT_MAX, 0, 16);
        seq[i].trigger->generateRandomSequence(seq[i].fills, 15);
        display_pat = (String) seq[i].trigger->textSequence;
      }

      // set track euclidean triggers if either pot has moved enough
      if (!potlock[1] && ! button[8] && display_mode == 1) {
        seq[i].fills = map(potvalue[1], POT_MIN, POT_MAX, 0, 16);
        seq[i].trigger->generateSequence(seq[i].fills, 15);
        seq[i].trigger->resetSequence(); // set to 0
        display_pat = (String) seq[i].trigger->textSequence;

      }
      //trig/retrig play
      if ( display_mode == 2 && i < 8 && voice[current_track].isPlaying == false) {
        voice[current_track].sampleindex = 0; // trigger sample for this track
        voice[current_track].isPlaying = true;
      }
  */
}



// second core setup
// second core dedicated to sample processing
void setup1() {
  delay (2000); // wait for main core to start up perhipherals
}

// render a single output frame by mixing all playing voices
// (resampling with linear interpolation, precomputed gain, attack ramp,
//  then tanh soft-clip for headroom instead of a hard clip)
int16_t __not_in_flash_func(renderAudioFrame)() {
  int32_t samplesum = 0;

  /* oct 22 2023 resampling code
     to change pitch we step through the sample by .5 rate for half pitch up to 2 for double pitch
     sample.sampleindex is a fixed point 20:12 fractional number
     we step through the sample array by sampleincrement - sampleincrement is treated as a 1 bit integer and a 12 bit fraction
     for sample lookup sample.sampleindex is converted to a 20 bit integer which limits the max sample size to 2**20 or about 1 million samples, about 45 seconds
  */
  for (int track = 0; track < NTRACKS; ++track) { // look for samples that are playing, scale their volume, and add them up
    if (!voice[track].isPlaying) continue;
    int16_t tracksample = voice[track].sample;
    uint32_t index = voice[track].sampleindex >> 12; // integer part of the fixed-point index
    // stop one sample early so the index+1 interpolation read stays in bounds
    if (index >= sample[tracksample].samplesize - 1) {
      voice[track].isPlaying = false;
      continue;
    }
    int16_t samp0 = sample[tracksample].samplearray[index];     // first sample to interpolate
    int16_t samp1 = sample[tracksample].samplearray[index + 1]; // second sample
    int32_t delta = samp1 - samp0;
    int32_t newsample = (int32_t)samp0 + (delta * (int32_t)(voice[track].sampleindex & 0x0fff)) / 4096; // interpolate

    int32_t s = ((int32_t)newsample * voice_gain[track]) >> 15; // apply precomputed Q15 gain (no per-sample divide)
    if (voice_ramp[track] < RAMP_LEN) {                          // brief attack ramp suppresses retrigger clicks
      s = (s * voice_ramp[track]) / RAMP_LEN;
      voice_ramp[track]++;
    }
    samplesum += s;
    voice[track].sampleindex += voice[track].sampleincrement;    // advance by pitch step
  }

  // headroom + soft clip: a single full-level hit passes ~unchanged while
  // simultaneous voices saturate smoothly through tanh instead of hard-clipping.
  float norm = samplesum * (1.0f / 32767.0f);
  return (int16_t)(tanhf(norm) * 32767.0f);
}

// second core calculates samples and sends them to the DAC.
// DAC.write() blocks on the I2S DMA ring buffer, which paces this loop at the
// sample rate, so no separate audio-rate timer is required.
void __not_in_flash_func(loop1)() {
  int16_t out = renderAudioFrame();
  DAC.write(out); // left
  DAC.write(out); // right
}
