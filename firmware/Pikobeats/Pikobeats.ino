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

// we have 8 voices that can play any sample when triggered
// this structure holds the settings for each voice
// 80s only to 20, jungle to 29
//we use a header per sample set
#include "80s.h"
//#include "beatbox.h"
//#include "bbox.h"
//#include "angularj.h"

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

//#include "Jungle/samples.h"
//#include "808samples/samples.h" // 808 sounds
//#include "Angular_Jungle_Set/samples.h"   // Jungle soundfont set - great!
//#include "Angular_Techno_Set/samples.h"   // Techno
//#include "Acoustic3/samples.h"   // acoustic drums
//#include "Pico_kit/samples.h"   // assorted samples
//#include "testkit/samples.h"   // small kit for testing
//#include "Trashrez/samples.h"
//#include "world/samples.h"
//#include "mt40sr88sy1/samples.h"
//#include "kurzweill/samples.h"
//#include "beatbox/samples.h"
//#include "bbox/samples.h"


#define NUM_SAMPLES (sizeof(sample)/sizeof(sample_t))

enum {
  MODE_PLAY = 0,
  MODE_CONFIG,
  MODE_COUNT   // how many modes we got
};

int display_mode = MODE_PLAY;

// on the long ec11 these are swapped A 19, B 18
const int encoderA_pin = 19;
const int encoderB_pin = 18;
const int encoderSW_pin = 28;


// encoder
#include <RotaryEncoder.h>
RotaryEncoder encoder(encoderB_pin, encoderA_pin, RotaryEncoder::LatchMode::FOUR3);

void checkEncoderPosition() {
  encoder.tick();   // call tick() to check the state.
}

// these are irq timers for handling led signals
#include "timers.h"

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
  DAC.setBuffers(4, 128, 0);
  DAC.begin(48000);
}

static void stopAudio() {
  DAC.end();
}
inline bool canBufferAudioOutput() {
  return (DAC.availableForWrite());
}

// encoder button
#include <Bounce2.h>
Bounce2::Button enc_button = Bounce2::Button();

// additions
#include <Wire.h>


// from pikocore for bpm calcs on clk input
// this is unused, deprecate?
#include "runningavg.h"
RunningAverage ra;
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

  // Just to demonstrate, don't use too many ISR Timers if not absolutely necessary
  // You can use up to 16 timer for each ISR_Timer
  ISR_timer.setInterval(TINTERVAL_2mS, b2mS);
  ISR_timer.setInterval(TINTERVAL_5mS, b5mS);
  ISR_timer.setInterval(TINTERVAL_7mS, b7mS);
  ISR_timer.setInterval(TINTERVAL_10mS, b10mS);


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


  // set up runningavg
  //ra.Init(5);
  /* not yet
    seq[0].trigger->generateRandomSequence(8, 16);
    seq[2].trigger->generateRandomSequence(3, 16);
    seq[5].trigger->generateRandomSequence(5, 16);
    seq[7].trigger->generateRandomSequence(6, 16);
  */
  //display_value(NUM_SAMPLES); // show number of samples on the display

  startAudio();

}



// main core handles UI
void loop() {

  // timer
  uint32_t now = millis();
  scanbuttons(); // actually jack inputs
  
  // update the channel led & play sample
  for (int i = 0; i <= 8; ++i) { // scan all the buttons
    if (button[i]) {
      //digitalWrite(led[i], 1);
      //voice[i].isPlaying = false;
      voice[i].sampleindex = 0; // trigger sample for this track
      voice[i].isPlaying = true;

    } else {
      // not a hit, turn it off, except for pin 7 in mode 1&2
/*
      if (i != current_track ) {
        if ( ( display_mode != 0 && i != 7 ) || ( display_mode == 0 ) ) {
          digitalWrite(led[i], 0);
        }
      }
      */
    }
    
  }

  encoder.tick();

  int encoder_pos = encoder.getPosition();
  if ( (encoder_pos != encoder_pos_last )) {
    encoder_delta = encoder_pos - encoder_pos_last;
  }

  // set play mode 0 play 1 edit pitch, 2 edit channel sample,
  if (encoder_push_millis > 0 ) {
    if ((now - encoder_push_millis) > 25 && ! encoder_delta ) {
      if ( !encoder_held ) {
        encoder_held = true;
      }
    }

    if (step_push_millis > 0) { // we're pushing a step key too
      if (encoder_push_millis < step_push_millis) {  // and encoder was pushed first
        //strcpy(seq_info, "saveseq");
      }
    }
  }

  // update encoder button state
  enc_button.update();

  // led updates on timer


  // use encoder and button
  if (encoder_delta) {
    // mode 0, channel select
    if ( display_mode == 0 && ! enc_button.pressed() )  {
      // select a channel in mode one
      // first reset level if CV is in use
      voice[current_track].level = 800;
      current_track = current_track + encoder_delta;
      constrain(current_track, 0, 7);

    }

    // mode 1, adjust pitch.
    if ( display_mode == 1 ) {
      int pitch_change = voice[current_track].sampleincrement - (encoder_delta * 10);
      constrain(pitch_change, 2048, 8192);

      // divisible by 2 and it won't click
      if (pitch_change % 2 == 0) {
        voice[current_track].sampleincrement = pitch_change;
        // display_pitch = map(pitch, 2048, 8192, 0, 100);
      }
    }

    // permits us to switch sample on channel in mode 2
    if ( display_mode == 2 ) {
      int result = voice[current_track].sample + encoder_delta;
      if (result >= 0 && result <= NUM_SAMPLES - 1) {
        voice[current_track].sample = result;
      }
    }
  }

  /// only set new pos last after use of the delta
  encoder_pos_last = encoder_pos;
  encoder_delta = 0;  // we've used it

  // start tracking time encoder button held
  if (enc_button.pressed()) {
    encoder_push_millis = now;
    // switch mode
    display_mode = display_mode + 1;
    if ( display_mode > 2) { // switched back to play mode
      display_mode = 0;
    }
  } else {
    encoder_push_millis = 0;
    encoder_held = false;
  }

  // change sample volume level on current_track with cv in.
  // ADC is on a timer
  if (display_mode == 0) {
    if (CV != CV_last) {
      voice[current_track].level = CV;
      CV_last = CV;

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

void update_leds() {
  // update the channel led & play sample
  for (int i = 0; i <= 8; ++i) { // scan all the buttons
    if (button[i]) {
      digitalWrite(led[i], 1);
      //voice[i].isPlaying = false;
      voice[i].sampleindex = 0; // trigger sample for this track
      voice[i].isPlaying = true;

    } else {
      // not a hit, turn it off, except for pin 7 in mode 1&2
      /*
        if (i != current_track && display_mode == 0) {
        digitalWrite(led[i], 0);
        }*/
      if (i != current_track ) {
        if ( ( display_mode != 0 && i != 7 ) || ( display_mode == 0 ) ) {
          digitalWrite(led[i], 0);
        }
      }
      //voice[i].isPlaying = false;
    }
  }
}



// second core setup
// second core dedicated to sample processing
void setup1() {
  delay (2000); // wait for main core to start up perhipherals
}

// second core calculates samples and sends to DAC
void loop1() {

  // check if we have a new bpm value from interrupt
  // since debouncing is flaky, force more than 1 bpm diff
  //if (ra.Value() != bpm && ra.Value() > 49) {
  /* if (RPM > bpm + 1 || RPM < bpm -1 && RPM > 49) {
         //reset = true; //reset seq
         bpm = RPM;
    }
    do_clocks();  // process sequencer clocks
  */


  int32_t newsample, samplesum = 0, filtersum;
  uint32_t index;
  int16_t samp0, samp1, delta, tracksample;

  /* oct 22 2023 resampling code
     to change pitch we step through the sample by .5 rate for half pitch up to 2 for double pitch
     sample.sampleindex is a fixed point 20:12 fractional number
     we step through the sample array by sampleincrement - sampleincrement is treated as a 1 bit integer and a 12 bit fraction
     for sample lookup sample.sampleindex is converted to a 20 bit integer which limits the max sample size to 2**20 or about 1 million samples, about 45 seconds
  */
  for (int track = 0; track < NTRACKS; ++track) { // look for samples that are playing, scale their volume, and add them up
    tracksample = voice[track].sample; // precompute for a little more speed below
    index = voice[track].sampleindex >> 12; // get the integer part of the sample increment
    if (index >= sample[tracksample].samplesize) voice[track].isPlaying = false; // have we played the whole sample?
    if (voice[track].isPlaying) { // if sample is still playing, do interpolation
      samp0 = sample[tracksample].samplearray[index]; // get the first sample to interpolate
      samp1 = sample[tracksample].samplearray[index + 1]; // get the second sample
      delta = samp1 - samp0;
      newsample = (int32_t)samp0 + ((int32_t)delta * ((int32_t)voice[track].sampleindex & 0x0fff)) / 4096; // interpolate between the two samples
      //samplesum+=((int32_t)samp0+(int32_t)delta*(sample[i].sampleindex & 0x0fff)/4096)*sample[i].play_volume;
      samplesum += (newsample * (127 * voice[track].level)) / 1000;
      voice[track].sampleindex += voice[track].sampleincrement; // add step increment
    }
  }

  samplesum = samplesum >> 7; // adjust for play_volume multiply above
  if  (samplesum > 32767) samplesum = 32767; // clip if sample sum is too large
  if  (samplesum < -32767) samplesum = -32767;

  /*
      // filter
    if (filter_fc <=  LPF_MAX) {
        filtersum = (uint8_t)filter_lpf( (int64_t)samplesum, filter_fc ,filter_q);
     }
  */

#ifdef MONITOR_CPU
  digitalWrite(CPU_USE, 0); // low - CPU not busy
#endif

  // write samples to DMA buffer - this is a blocking call so it stalls when buffer is full
  DAC.write(int16_t(samplesum)); // left
  DAC.write(int16_t(samplesum)); // left


#ifdef MONITOR_CPU
  digitalWrite(CPU_USE, 1); // hi = CPU busy
#endif


}
