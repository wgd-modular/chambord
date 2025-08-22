 #define NUM_VOICES 32
struct voice_t {
  int16_t sample;   // index of the sample structure in sampledefs.h
  int16_t level;   // 0-1000 for legacy reasons
  uint32_t sampleindex; // 20:12 fixed point index into the sample array
  uint16_t sampleincrement; // 1:12 fixed point sample step for pitch changes
  bool isPlaying;  // true when sample is playing
} voice[NUM_VOICES] = {
  0,300, 0, 4096, false, //snr 10
  1,300, 0, 4096, false, //snr 10
  2,300, 0, 4096, false, //hfht
  3,300, 0, 4096, false, //ohat 3
  4,300, 0, 4096, false, //rim
  5,300, 0, 4096, false, //sdst 07
  6,300, 0, 4096, false, //tome 01
  7,300, 0, 4096, false,  //clH
  8,300, 0, 4096, false,
  9,300, 0, 4096, false,
  10,300, 0, 4096, false,
  11,300, 0, 4096, false,
  12,300, 0, 4096, false,
  13,300, 0, 4096, false,
  14,300, 0, 4096, false,
  15,300, 0, 4096, false,
  16,300, 0, 4096, false,
  17,300, 0, 4096, false,
  18,300, 0, 4096, false,
  19,300, 0, 4096, false, 
  20,300, 0, 4096, false, 
  21,300, 0, 4096, false,
  22,300, 0, 4096, false,
  23,300, 0, 4096, false,
  24,300, 0, 4096, false,
  25,300, 0, 4096, false,
  26,300, 0, 4096, false,
  27,300, 0, 4096, false,
  28,300, 0, 4096, false,
  29,300, 0, 4096, false,
  30,300, 0, 4096, false,
  31,300, 0, 4096, false,
};
#include "tekke/samples.h" // 4 kits in one

/*             
  int16_t sample;   // index of the sample structure in sampledefs.h
  int16_t level;   // 0-1000 for legacy reasons
  uint32_t sampleindex; // 20:12 fixed point index into the sample array
  uint16_t sampleincrement; // 1:12 fixed point sample step for pitch changes
  bool isPlaying;  // true when sample is playing
                                      
*/
