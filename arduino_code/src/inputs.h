#pragma once
#include "pins.h"

// Analog inputs, read in loop() (never in the ISR): pattern CV and division knobs.

/** Reads the pattern input of channel `ch` and hands it to the channel. */
void patternInputUpdate(uint8_t ch);

/** Reads the division knob of channel `ch` and updates its clock division.
 *  While shift is held the knob selects the mode instead (see shift.h). */
void divisionInputUpdate(uint8_t ch);

/** Freezes the division of `ch` until its knob passes back through the position it
 *  was set at (pickup), so it does not jump to where the knob points now. */
void divisionInputHold(uint8_t ch);
