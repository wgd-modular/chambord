// timer functions used to variously illuminate leds

// These define's must be placed at the beginning before #include "TimerInterrupt_Generic.h"
// _TIMERINTERRUPT_LOGLEVEL_ from 0 to 4
#define _TIMERINTERRUPT_LOGLEVEL_     4

// Can be included as many times as necessary, without `Multiple Definitions` Linker Error
#include "RPi_Pico_TimerInterrupt.h"

// Audio is now rendered free-running on core1 (loop1) and paced by the I2S DMA
// buffer, so the old audio-rate timer (ITimer0 / TimerHandler0) has been removed.

// To be included only in main(), .ino with setup() to avoid `Multiple Definitions` Linker Error
#include "RPi_Pico_ISR_Timer.h"

//#include <SimpleTimer.h>

bool timersUp = false;

// 5ms = 200hz, 1ms 1000hz
// 1ms = 1000Us
// Init RPI_PICO_Timer
RPI_PICO_Timer ITimer3(3);

// Init ISR_Timer
// Each ISR_Timer can service 16 different ISR-based timers
RPI_PICO_ISR_Timer ISR_timer;

bool TimerHandler(struct repeating_timer *t) {
  (void) t;
  ISR_timer.run();
  return true;
}

/////////////////////////////////////////////////
#define NUMBER_ISR_TIMERS         4
typedef void (*irqCallback)  ();
/////////////////////////////////////////////////


#define HW_TIMER_INTERVAL_MS          1L

volatile  int16_t CV;   // latest CV ADC reading (sampled in updateUI())

// LED feedback is now handled by the unified ledRender()/updateUI() engine in
// Pikobeats.ino. The old b2/b5/b7/b10 mS handlers (and their blue/green
// variant) have been removed.
