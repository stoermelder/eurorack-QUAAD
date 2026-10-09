#pragma once
#include "pins.h"

// Shift UI: hold the reset button (or a gate on the reset jack) for about 1 s. While
// shift is held, turning the division knob of channel N by more than a threshold selects
// mode N instead of changing that channel's division. On release the mode is saved to
// EEPROM and turned knobs keep their old division until they pass back through their
// position. If no knob is turned, nothing changes.

/** Call every loop() iteration. */
void shiftUpdate();

bool shiftActive();

/** Reports a division knob reading while shift is held. */
void shiftKnob(uint8_t ch, int reading);
