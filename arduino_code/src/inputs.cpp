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

// Division knob
const uint16_t divisions[] = {32, 16, 12, 8, 7, 5, 4, 3, 2, 1};
const int CLK_DIV_HYSTERESIS = 54;
int clk_div_index[NUM_CHANNELS] = {-1, -1, -1, -1}; // current slot, -1 = not read yet
bool divisionHold[NUM_CHANNELS] = {false, false, false, false};

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

	// Hysteresis: only change slot when the reading is > 54 counts from the
	// centre of the current slot (slot width is 1024 / 10 = 102 counts)
	int mid = 51 + (1024 / 10) * clk_div_index[ch];
	if (divisionHold[ch]) {
		if (abs(reading - mid) > CLK_DIV_HYSTERESIS) {
			return;
		}
		divisionHold[ch] = false;
	}
	if (clk_div_index[ch] >= 0 && abs(reading - mid) <= CLK_DIV_HYSTERESIS) {
		return;
	}

	int index = constrain(map(reading, 0, 922, 0, 9), 0, 9);
	clk_div_index[ch] = index;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		clockDivider[ch].setDivision(divisions[index]);
	}
}

void divisionInputHold(uint8_t ch) {
	divisionHold[ch] = true;
}
