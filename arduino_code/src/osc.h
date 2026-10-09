#pragma once
#include "pins.h"

// Oscillator mode: each channel switches its step mux at audio rate, so the CV output becomes
// a 4-level staircase wave. The four step pots set the levels, the pattern the order in which
// they are played, and the pattern input the pitch (1 V/oct, continuous). The clock
// output carries a square wave at the same frequency. Timer1 runs a 40 kHz phase accumulator
// per channel; the clock input is ignored and the reset input hard-syncs all four voices.

constexpr uint16_t OSC_MAX_PITCH = 96 * 256; // pitch unit: 1/256 semitone; 0 = 32.7 Hz (C1)

/** Mode entry/exit: start/stop the timer, take over/give back the clock input. */
void oscEnter();
void oscExit();
/** Hard sync: all voices restart on their first step (ISR context). */
void oscRestart();

/** Inputs in this mode (loop()): the pattern input sets the pitch, the division knob the waveform. */
void oscPatternInput(uint8_t ch);
void oscDivisionInput(uint8_t ch, int reading);
