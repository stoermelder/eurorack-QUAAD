#include <util/atomic.h>
#include "inputs.h"
#include "channels.h"
#include "shift.h"

namespace {

// Pattern input: the summed CV is ~1 V (30 counts) per pattern, so the full ADC range
// covers several repeats of the pattern list
const int ptrn_size = 30;
const int ptrn_offset = 309;
const int ptrn_hysteresis = 20;
int ptrn_mid[NUM_CHANNELS] = {2000, 2000, 2000, 2000};

// Division knob: an "off" end stop (gate muted, the division stays as it was) at the low end,
// then ÷32 ... ÷1.
const int DIV_SLOTS = 11;
const uint16_t DIV_OFF = 0;
const uint16_t divisions[DIV_SLOTS] = {DIV_OFF, 32, 16, 12, 8, 7, 5, 4, 3, 2, 1};

// Lowest ADC reading of each slot (same order as `divisions`; the first entry is unused,
// off starts at 0). Adjust these to the knob: if a division starts too early (the knob
// points to 3, the division is 2), raise the start of the slot of that division (here
// ÷2) and of the ones after it; if it starts too late, lower it. The end stop "off" is narrow on
// purpose (24 counts).
const int divisionStart[DIV_SLOTS] = {
	0,   // off
	24,  // 32
	138, // 16
	252, // 12
	366, // 8
	480, // 7
	593, // 5
	707, // 4
	821, // 3
	935, // 2
	1010 // 1
};
const unsigned long DIV_SETTLE_MS = 500; // how long the knob must rest before a division is applied

typedef SlotSelector<DIV_SLOTS, DIV_SETTLE_MS> DivisionKnob;
DivisionKnob divisionKnob[NUM_CHANNELS] = {
	DivisionKnob(divisionStart), DivisionKnob(divisionStart),
	DivisionKnob(divisionStart), DivisionKnob(divisionStart)
};

} // namespace

void patternInputUpdate(uint8_t ch) {
	int pattern_read = analogRead(pin_PTRN[ch]) + ptrn_size / 2;
	if (abs(pattern_read - ptrn_mid[ch]) <= ptrn_hysteresis) {
		return;
	}

	// Floor division, so readings below the offset continue the pattern cycle
	int d = pattern_read - ptrn_offset;
	int slot = d >= 0 ? d / ptrn_size : -((-d + ptrn_size - 1) / ptrn_size);
	ptrn_mid[ch] = ptrn_offset + slot * ptrn_size + ptrn_size / 2;
	uint8_t newPattern = ((slot % NUM_PATTERNS) + NUM_PATTERNS) % NUM_PATTERNS;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		pattern[ch] = newPattern;
	}
}

void divisionInputUpdate(uint8_t ch) {
	int reading = analogRead(pin_CLK_DIV_IN[ch]);

	// Shift: the knob selects the mode instead of the division
	if (shiftActive()) {
		shiftKnob(ch, reading);
		return;
	}

	if (!divisionKnob[ch].update(reading, millis()))
		return;
	uint16_t division = divisions[divisionKnob[ch].value()];
	bool muted = division == DIV_OFF;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		if (!muted)
			clockDivider[ch].setDivision(division); // 0 would divide by zero in the ISR
		channelMuted[ch] = muted;
	}
}

void divisionInputHold(uint8_t ch) {
	divisionKnob[ch].hold();
}
