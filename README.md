# Chambord

Chambord is a eurorack module, using the RP2040 or 2350 to implement an
8-channel trigger-in drum sampler.

The 8 channels are laid out **top to bottom = channel 1 to 8**. Each channel
has a trigger input jack and a bicolor (red/green) LED. There is one encoder
(turn + press) and one CV input.

# How to use it

There are only two controls: **turn** the encoder and **press** the encoder.

## Turning the encoder

What turning does depends on the current "page". You step through the pages by
**short-pressing** the encoder:

```
press        press         press        press
SELECT  -->  SAMPLE  -->  VOLUME  -->  PITCH  --> (back to SELECT)
```

- **SELECT** (start here): turn to choose the channel you want to work on.
- **SAMPLE**: turn to change the sample on the selected channel. Each channel
  has 4 samples (banks A–D); turning steps between them.
- **VOLUME**: turn to change the volume of the selected channel.
- **PITCH**: turn to change the pitch of the selected channel.

A short press always advances to the next page and wraps back to SELECT.

## Reading the LEDs

While you're editing, only the **selected channel's** LED lights, colour-coded
by page (green = choosing a thing, amber = setting an amount):

| LED              | Page                                  |
|------------------|---------------------------------------|
| green steady     | SELECT  (turn to pick the channel)    |
| green blinking   | SAMPLE  (turn to change the sample)   |
| amber steady     | VOLUME  (turn to change the volume)   |
| amber blinking   | PITCH   (turn to change the pitch)    |

You'll also hear sample/volume/pitch changes as you turn.

## Screensaver / trigger activity

When you haven't touched the encoder for about **10 seconds**, the row switches
to a trigger-activity display: every channel flashes **green** when it gets a
trigger. Touch the encoder and it goes back to showing what you're editing.
(This keeps trigger flashes from distracting you while you work.)

## CV input

**Long-press** the encoder (hold ~3/4 second) to assign the CV input to the
**volume** of the currently selected channel. Long-press again on the same
channel to turn CV off.

## Settings are saved

Your per-channel sample and pitch choices (and the CV assignment) are saved
automatically to flash a moment after you stop adjusting, and restored on the
next power-up. Saving only happens while nothing is sounding, so it never
interrupts playback.

## If the encoder turns the wrong way

Open `firmware/Pikobeats/Pikobeats.ino` and change `ENCODER_DIR` from `1` to
`-1` (near the top), then rebuild.
