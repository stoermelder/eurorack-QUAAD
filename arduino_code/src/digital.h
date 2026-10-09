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
 * Maps an ADC reading (a knob) to a slot using a table of slot starts, so the slots can have
 * any width and follow the taper of the pot and the marks on the panel. This is the only
 * place that knows about the non-linearity of the knobs.
 *
 * `starts[s]` is the lowest reading of slot s (ascending).
 *  - Not cyclic: `slots` entries (starts[0] is ignored); the first and last slot extend to
 *    the ends of the ADC range.
 *  - Cyclic: `slots + 1` entries, the last one is where the cycle starts again. Beyond the
 *    table the slots repeat, below it they continue backwards. Slot numbers then count over
 *    all cycles and can be negative; index() gives the position within the table.
 */
struct SlotMap {
	const int *starts;
	int slots;
	bool cyclic;

	constexpr SlotMap(const int *starts, int slots, bool cyclic)
		: starts(starts), slots(slots), cyclic(cyclic) {}

	int slotOf(int reading) const {
		if (!cyclic) {
			int s = slots - 1;
			while (s > 0 && reading < starts[s]) {
				s--;
			}
			return s;
		}
		int cycle = floorDiv(reading - starts[0], period());
		int within = reading - cycle * period();
		int s = slots - 1;
		while (s > 0 && within < starts[s]) {
			s--;
		}
		return cycle * slots + s;
	}

	/** Reading is within the slot, extended by `margin` on both sides. */
	bool contains(int slot, int reading, int margin) const {
		if (cyclic) {
			return reading >= slotStart(slot) - margin && reading < slotStart(slot + 1) + margin;
		}
		return (slot == 0 || reading >= starts[slot] - margin)
		    && (slot == slots - 1 || reading < starts[slot + 1] + margin);
	}

	/** Position of the slot within the table (0 .. slots - 1). */
	int index(int slot) const {
		return cyclic ? slot - floorDiv(slot, slots) * slots : slot;
	}

private:
	int period() const {
		return starts[slots] - starts[0];
	}

	/** Lowest reading of a slot of a cyclic map. */
	int slotStart(int slot) const {
		int cycle = floorDiv(slot, slots);
		return cycle * period() + starts[slot - cycle * slots];
	}

	static int floorDiv(int a, int b) {
		return a >= 0 ? a / b : -((-a + b - 1) / b);
	}
};


/**
 * Turns the readings of a knob into a stable slot of a SlotMap, with
 *  - hysteresis: the slot only changes when the reading is MARGIN counts outside the current one,
 *  - settling: a new slot is only applied once the knob has rested in it for SETTLE_MS,
 *    so turning through several slots does not apply all the ones in between,
 *  - pickup: after hold(), the knob is ignored until it passes back through the applied slot.
 * The first reading is applied immediately.
 */
template <unsigned long SETTLE_MS>
struct SlotSelector {
	static constexpr int MARGIN = 3;

	/** `map` must stay valid (use a global). */
	explicit SlotSelector(const SlotMap &map) : map(&map) {}

	/** Applied slot as a position in the table (0 .. slots - 1); only valid after the first update(). */
	int value() const {
		return map->index(applied);
	}

	/** Freezes the selector until the knob is back at the applied slot (pickup). */
	void hold() {
		holding = true;
	}

	/** Feed a reading (call regularly); returns true if a new slot was applied. */
	bool update(int reading, unsigned long now) {
		if (holding) {
			if (!map->contains(applied, reading, MARGIN))
				return false;
			holding = false;
			candidate = applied;
		}

		if (!started || !map->contains(candidate, reading, MARGIN)) {
			candidate = map->slotOf(reading);
			since = now;
		}

		if (started && candidate == applied) {
			return false;
		}
		if (started && now - since < SETTLE_MS) {
			return false;
		}
		started = true;
		applied = candidate;
		return true;
	}

private:
	const SlotMap *map;
	bool started = false;
	int applied = 0;   // slot in use
	int candidate = 0; // slot the knob is in
	unsigned long since = 0; // when the knob entered the candidate slot
	bool holding = false;
};
