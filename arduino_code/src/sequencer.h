#pragma once
#include "pins.h"

// Step sequencing: which of the 4 steps (mux code 0..3) a channel plays next.
// Knows nothing about clocks or modes; callers decide *when* a channel advances.

constexpr uint8_t NUM_PATTERNS = 12; // checked against the table in sequencer.cpp

/** Selects the pattern (index into the pattern table) of channel `ch` (ISR context). */
void sequencerSetPattern(uint8_t ch, uint8_t pattern);

/** Rewinds all channels to their first step. */
void sequencerReset();

/** Advances channel `ch` by one step and returns the mux code 0..3 (ISR context).
 *  `atStart` is set to true if the step is the first of a sequence (the random patterns
 *  count as a sequence of 4 steps). */
uint8_t sequencerNext(uint8_t ch, bool &atStart);
