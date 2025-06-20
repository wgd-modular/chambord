// timer functions used to variously illuminate leds

// These define's must be placed at the beginning before #include "TimerInterrupt_Generic.h"
// _TIMERINTERRUPT_LOGLEVEL_ from 0 to 4
#define _TIMERINTERRUPT_LOGLEVEL_     4
// Can be included as many times as necessary, without `Multiple Definitions` Linker Error
#include "RPi_Pico_TimerInterrupt.h"
// To be included only in main(), .ino with setup() to avoid `Multiple Definitions` Linker Error
#include "RPi_Pico_ISR_Timer.h"
//#include <SimpleTimer.h>

bool timersUp = false;

// 5ms = 200hz, 1ms 1000hz
// 1ms = 1000Us
// Init RPI_PICO_Timer
RPI_PICO_Timer ITimer3(0);

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

#define TINTERVAL_2mS             2L
#define TINTERVAL_5mS             5L
#define TINTERVAL_7mS             7L
#define TINTERVAL_10mS            12L

#define LED_INT_4_MS        4L

volatile  bool toggle_one  = false;
volatile  bool toggle_two  = false;

void b2mS() {
  if (display_mode == 1) {
    digitalWrite(led[7], 1);
  }
}

void b5mS() {
  digitalWrite( led[current_track], 1);
  
  if (display_mode == 2) {
    digitalWrite(led[7], 1);
  }
  
}

void b7mS() {
  if (display_mode == 1) {
    digitalWrite(led[7], 0);
  }

}

void b10mS() {
  digitalWrite( led[current_track], 0);
  
    if (display_mode == 2) {
    digitalWrite(led[7], 0);
  }
}
