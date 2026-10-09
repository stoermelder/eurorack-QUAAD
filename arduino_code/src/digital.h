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


/**
 * Turns a 10-bit ADC reading (a knob) into one of SLOTS slots, with
 *  - a table of slot starts: `starts[s]` is the lowest reading of slot s (ascending,
 *    starts[0] is ignored). The slots can have any width, so the table can follow the taper
 *    of the pot and the marks on the panel. Readings below/above the table stay in the
 *    first/last slot,
 *  - hysteresis: the slot only changes when the reading is MARGIN counts outside the current one,
 *  - settling: a new slot is only applied once the knob has rested in it for SETTLE_MS,
 *    so turning through several slots does not apply all the ones in between,
 *  - pickup: after hold(), the knob is ignored until it passes back through the applied slot.
 * The first reading is applied immediately.
 */
template <int SLOTS, unsigned long SETTLE_MS>
struct SlotSelector {
	static constexpr int MARGIN = 3;

	/** `starts` must stay valid (use a global const array). */
	SlotSelector(const int *starts) : starts(starts) {}

	/** The applied slot, or -1 before the first update(). */
	int value() const {
		return applied;
	}

	/** Freezes the selector until the knob is back at the applied slot (pickup). */
	void hold() {
		holding = true;
	}

	/** Feed a reading (call regularly); returns true if a new slot was applied. */
	bool update(int reading, unsigned long now) {
		if (holding) {
			if (!inside(applied, reading))
				return false;
			holding = false;
			candidate = applied;
		}

		if (candidate < 0 || !inside(candidate, reading)) {
			candidate = slotOf(reading);
			since = now;
		}

		if (candidate == applied)
			return false;
		if (applied >= 0 && now - since < SETTLE_MS)
			return false;
		applied = candidate;
		return true;
	}

private:
	const int *starts;
	int applied = -1;   // slot in use
	int candidate = -1; // slot the knob is in
	unsigned long since = 0; // when the knob entered the candidate slot
	bool holding = false;

	int slotOf(int reading) const {
		for (int slot = SLOTS - 1; slot > 0; slot--) {
			if (reading >= starts[slot])
				return slot;
		}
		return 0;
	}

	/** Reading is within the slot, extended by MARGIN on both sides (hysteresis). The first
	 *  and last slot extend to the ends of the ADC range. */
	bool inside(int slot, int reading) const {
		bool aboveStart = slot == 0 || reading >= starts[slot] - MARGIN;
		bool belowEnd = slot == SLOTS - 1 || reading <= starts[slot + 1] + MARGIN;
		return aboveStart && belowEnd;
	}
};
