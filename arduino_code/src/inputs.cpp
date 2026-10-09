#include "inputs.h"
#include "channels.h"
#include "shift.h"
#include "calibration.h"
#include "modes.h"

namespace {

// Pattern knob: patterns follow the knob immediately.
typedef SlotSelector<0> PatternKnob;
PatternKnob patternKnob[NUM_CHANNELS] = {
	PatternKnob(patternMap), PatternKnob(patternMap),
	PatternKnob(patternMap), PatternKnob(patternMap)
};

// Division knob: what each slot of divisionMap (calibration.h) means. DIV_OFF mutes the gate and
// keeps the division as it was.
const uint16_t DIV_OFF = 0;
const uint16_t divisions[DIV_SLOTS] = {DIV_OFF, 32, 16, 12, 8, 7, 5, 4, 3, 2, 1};
const unsigned long DIV_SETTLE_MS = 500; // how long the knob must rest before a division is applied

typedef SlotSelector<DIV_SETTLE_MS> DivisionKnob;
DivisionKnob divisionKnob[NUM_CHANNELS] = {
	DivisionKnob(divisionMap), DivisionKnob(divisionMap),
	DivisionKnob(divisionMap), DivisionKnob(divisionMap)
};

} // namespace

void patternInputUpdate(uint8_t ch) {
	modes[currentMode].patternInput(ch);
}

void divisionInputUpdate(uint8_t ch) {
	int reading = analogRead(pin_CLK_DIV_IN[ch]);
	if (shiftActive()) {
		shiftKnob(ch, reading); // the knob selects the mode instead of the division
	}
	else {
		modes[currentMode].divisionInput(ch, reading);
	}
}

void normalPatternInput(uint8_t ch) {
	if (!patternKnob[ch].update(analogRead(pin_PTRN[ch]), millis())) {
		return;
	}
	channelSetPattern(ch, patternKnob[ch].value());
}

void normalPatternInputReset() {
	for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
		patternKnob[i].reset();
	}
}

void normalDivisionInput(uint8_t ch, int reading) {
	if (!divisionKnob[ch].update(reading, millis())) {
		return;
	}
	uint16_t division = divisions[divisionKnob[ch].value()];
	bool muted = division == DIV_OFF;
	if (!muted) {
		channelSetDivision(ch, division); // 0 would divide by zero in the ISR
	}
	channelSetMuted(ch, muted);
}

void divisionInputHold(uint8_t ch) {
	divisionKnob[ch].hold();
}
