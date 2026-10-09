#pragma once
#include "pins.h"

// Operating modes. A mode decides what happens on each clock edge by combining the
// building blocks in channels.h. The active mode is stored in EEPROM and selected with
// the shift UI (shift.cpp): hold shift and turn the division knob of channel N to
// select mode N, so there can be at most NUM_CHANNELS (4) modes.
//
// To add a mode:
//   1. add an entry to the enum below (before NUM_MODES),
//   2. write its handlers in modes.cpp,
//   3. add a row to the `modes` table in the same order.

enum Mode : uint8_t {
	MODE_NORMAL,      // default: four independent channels (knob A)
	MODE_CHAIN_PAIRS, // pattern chaining: B follows A's sequences, D follows C's (knob B)
	MODE_CHAIN_ALL,   // all chained: A > B > C > D (knob C)
	MODE_OSCILLATOR,  // audio-rate step switching, see osc.h (knob D)
	NUM_MODES
};
static_assert(NUM_MODES <= NUM_CHANNELS, "one division knob per mode");

struct ModeDef {
	/** Clock input went high, after clockAdvance() (ISR context). */
	void (*onClock)();
	/** Reset input fired, after clockReset() and sequencerReset(); may be nullptr (ISR context). */
	void (*onReset)();
	/** Pattern input of channel `ch` (read in loop()). */
	void (*patternInput)(uint8_t ch);
	/** Division knob of channel `ch` with its ADC reading (loop()). */
	void (*divisionInput)(uint8_t ch, int reading);
	/** The mode is activated (interrupts disabled); may be nullptr. */
	void (*onEnter)();
	/** The mode is left (interrupts disabled); may be nullptr. */
	void (*onExit)();
};

extern const ModeDef modes[NUM_MODES];

/** Active mode. Written in loop(), read in the ISR. */
extern volatile uint8_t currentMode;

/** Reads the stored mode from EEPROM (falls back to MODE_NORMAL if invalid) and enters it. */
void modeLoad();
/** Activates `mode` and stores it in EEPROM (only writes if it changed). */
void modeSave(uint8_t mode);
