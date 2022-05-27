#include <Arduino.h>

struct ClockDivider {
	uint32_t clock = 0;
	uint32_t division = 1;

	void reset() {
		clock = 0;
	}

	void setDivision(uint32_t division) {
		this->division = division;
	}

	uint32_t getDivision() {
		return division;
	}

	uint32_t getClock() {
		return clock;
	}

	/** Returns true when the clock reaches `division` and resets. */
	bool process() {
		clock++;
		if (clock >= division) {
			clock = 0;
			return true;
		}
		return false;
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