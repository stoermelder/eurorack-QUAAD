#pragma once
#include "pins.h"

// Step sequencing: which of the 4 steps (mux code 0..3) a channel plays next.
// Knows nothing about clocks or modes; callers decide *when* a channel advances.

constexpr uint8_t NUM_PATTERNS = 12; // checked against the table in sequencer.cpp

// Selected pattern per channel (index into the pattern table).
// Written in loop() with interrupts disabled, read in the ISR.
extern volatile uint8_t pattern[NUM_CHANNELS];

/** Rewinds all channels to their first step. */
void sequencerReset();

/** Advances channel `ch` by one step and returns the mux code 0..3 (ISR context).
 *  `atStart` is set to true if the step is the first of a sequence (the random patterns
 *  count as a sequence of 4 steps). */
uint8_t sequencerNext(uint8_t ch, bool &atStart);
