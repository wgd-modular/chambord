
#define NUM_VOICES 32

struct voice_t {
  int16_t sample;   // index of the sample structure in sampledefs.h
  int16_t level;   // 0-1000 for legacy reasons
  uint32_t sampleindex; // 20:12 fixed point index into the sample array
  uint16_t sampleincrement; // 1:12 fixed point sample step for pitch changes
  bool isPlaying;  // true when sample is playing
} voice[NUM_VOICES] = {
  0,400, 0, 4096, false,
  1,400, 0, 4096, false,
  2,600, 0, 4096, false,
  3,400, 0, 4096, false,
  4,400, 0, 4096, false,
  5,400, 0, 4096, false,
  6,400, 0, 4096, false,
  7,400, 0, 4096, false,
  8,400, 0, 4096, false,
  9,400, 0, 4096, false,
  10,400, 0, 4096, false,
  11,400, 0, 4096, false,
  12,400, 0, 4096, false,
  13,400, 0, 4096, false,
  14,400, 0, 4096, false,
  15,400, 0, 4096, false,
  16,400, 0, 4096, false,
  17,400, 0, 4096, false,
  18,400, 0, 4096, false,
  19,400, 0, 4096, false, 
  20,400, 0, 4096, false, 
  21,400, 0, 4096, false,
  22,400, 0, 4096, false,
  23,400, 0, 4096, false,
  24,400, 0, 4096, false,
  25,400, 0, 4096, false,
  26,400, 0, 4096, false,
  27,400, 0, 4096, false,
  28,400, 0, 4096, false,
  29,400, 0, 4096, false,
  30,400, 0, 4096, false,
  31,400, 0, 4096, false,
};
#include "trippy/samples.h" 
