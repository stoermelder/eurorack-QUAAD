#pragma once
#include "pins.h"

// Analog inputs, read in loop() (never in the ISR): pattern CV and division knobs.

/** Reads the pattern input of channel `ch` and hands it to the active mode (modes.h). */
void patternInputUpdate(uint8_t ch);

/** Reads the division knob of channel `ch` and hands it to the active mode.
 *  While shift is held the knob selects the mode instead (see shift.h). */
void divisionInputUpdate(uint8_t ch);

// What the pattern input and the division knob do in the sequencer modes. Used by the
// ModeDef table (modes.cpp); other modes bring their own handlers.

/** Pattern input selects the pattern of the channel. */
void normalPatternInput(uint8_t ch);
/** Division knob sets the clock division (or mutes the gate at its "off" end stop). */
void normalDivisionInput(uint8_t ch, int reading);
/** Forgets the pattern knobs, so the next reading is applied immediately (after a mode
 *  that used the pattern input differently). */
void normalPatternInputReset();

/** Freezes the division of `ch` until its knob passes back through the position it
 *  was set at (pickup), so it does not jump to where the knob points now. */
void divisionInputHold(uint8_t ch);
