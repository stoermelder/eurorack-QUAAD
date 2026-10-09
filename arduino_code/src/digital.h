#pragma once
#include <Arduino.h>

struct ClockDivider {
	uint32_t division = 1;

	void setDivision(uint32_t division) {
		this->division = division;
	}

	uint32_t getDivision() {
		return division;
	}

	/** Returns true on clocks 1, 1+division, 1+2*division, ... of the shared
	 *  `clockCount` (1-based), so the output stays phase-locked to reset. */
	bool process(uint16_t clockCount) {
		return (clockCount - 1) % division == 0;
	}
};


struct SchmittTrigger {
	bool state = true;

	void reset() {
		state = true;
	}

	bool process(int in, int offThreshold = 0, int onThreshold = 1) {
		if (state) {
			// HIGH to LOW
			if (in <= offThreshold) {
				state = false;
			}
		}
		else {
			// LOW to HIGH
			if (in >= onThreshold) {
				state = true;
				return true;
			}
		}
		return false;
	}

	bool isHigh() {
		return state;
	}
};