#include "shift.h"
#include "modes.h"
#include "inputs.h"

namespace {

const unsigned long SHIFT_HOLD_MS = 1000;
const int SHIFT_MOVE_THRESHOLD = 60; // counts a knob must move to count as turned

bool resetHeld = false;
unsigned long resetHeldSince = 0;
bool active = false;
uint8_t pendingMode = MODE_NORMAL;
int knobBase[NUM_CHANNELS];     // knob readings when shift started
bool knobMoved[NUM_CHANNELS];   // knob moved past the threshold during shift

void start() {
	active = true;
	pendingMode = currentMode;
	for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
		knobBase[i] = analogRead(pin_CLK_DIV_IN[i]);
		knobMoved[i] = false;
	}
}

void end() {
	active = false;
	modeSave(pendingMode);
	for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
		if (knobMoved[i]) {
			divisionInputHold(i);
		}
	}
}

} // namespace

bool shiftActive() {
	return active;
}

void shiftUpdate() {
	if (digitalRead(pin_RESET) == HIGH) {
		if (!resetHeld) {
			resetHeld = true;
			resetHeldSince = millis();
		}
		else if (!active && millis() - resetHeldSince >= SHIFT_HOLD_MS) {
			start();
		}
	}
	else {
		if (active) {
			end();
		}
		resetHeld = false;
	}
}

void shiftKnob(uint8_t ch, int reading) {
	if (abs(reading - knobBase[ch]) <= SHIFT_MOVE_THRESHOLD) {
		return;
	}
	knobMoved[ch] = true;
	if (ch < NUM_MODES) {
		pendingMode = ch; // knob of channel N selects mode N
	}
}
