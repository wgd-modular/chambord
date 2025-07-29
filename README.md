# Chambord

Chambord is a eurorack module, using the RP2040 or 2350 to implement a sampler.

# UI

The ui is controlled via the encoder and encoder button. The feedback is the color of the current selected track or track 7.

1. In the default mode (0) turning the encoder selects the current track to work on. It will blink orange from default green.
2. Press the encoder button (short press) once, enter mode 1 to change the pitch of the current track.
3. Press the encoder again (short press) to enter mode 2, to change the sample. Each track has 4 samples associated with it.
5. Hold the ecoder buton for 1/2 a second (say 'one') and you assign the CV input to modulate the volume of the current track.

That's about it.
