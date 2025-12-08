
#define NUM_VOICES 32

struct voice_t {
  int16_t sample;   // index of the sample structure in sampledefs.h
  int16_t level;   // 0-1000 for legacy reasons
  uint32_t sampleindex; // 20:12 fixed point index into the sample array
  uint16_t sampleincrement; // 1:12 fixed point sample step for pitch changes
  bool isPlaying;  // true when sample is playing
} voice[NUM_VOICES] = {
  0,      // default voice 0 assignment - typically a kick but you can map them any way you want
  800,  // initial level
  1,    // sampleindex
  4096, // initial pitch step - normal pitch
  false, // sample not playing
  1,800, 0, 4096, false, //snr 10
  2,800, 0, 4096, false, //hfht
  3,800, 0, 4096, false, //ohat 3
  4,800, 0, 4096, false, //rim
  5,800, 0, 4096, false, //sdst 07
  6,800, 0, 4096, false, //tome 01
  7,800, 0, 4096, false,  //clH
  8,800, 0, 4096, false,
  9,800, 0, 4096, false,
  10,800, 0, 4096, false,
  11,800, 0, 4096, false,
  12,800, 0, 4096, false,
  13,800, 0, 4096, false,
  14,800, 0, 4096, false,
  15,800, 0, 4096, false,
  16,800, 0, 4096, false,
  17,800, 0, 4096, false,
  18,800, 0, 4096, false,
  19,800, 0, 4096, false, 
  20,800, 0, 4096, false, 
  21,800, 0, 4096, false,
  22,800, 0, 4096, false,
  23,800, 0, 4096, false,
  24,800, 0, 4096, false,
  25,800, 0, 4096, false,
  26,800, 0, 4096, false,
  27,800, 0, 4096, false,
  28,800, 0, 4096, false,
  29,800, 0, 4096, false,
  30,800, 0, 4096, false,
  31,800, 0, 4096, false,
};
#include "Angular_Jungle_Set/samples.h" // 808, mt40sr88sy1, sounds
