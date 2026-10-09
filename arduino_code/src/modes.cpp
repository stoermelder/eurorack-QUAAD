#include <avr/eeprom.h>
#include <util/atomic.h>
#include "modes.h"
#include "channels.h"

namespace {

const uint8_t EEPROM_ADDR_MODE = 0;

void normalOnClock() {
	for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
		if (channelDue(i)) {
			channelTrigger(i);
		}
	}
}

// Chaining: a chained channel is clocked by the sequences of its source channel instead of
// the clock input. It still uses its own division knob: at 1 it advances on every sequence
// its source starts, at 2 on every second one, and so on. A sequence "starts" when the
// source plays its first step, which includes the first step after reset, so the whole
// chain fires on clock 1 just like in the normal mode.
const int8_t FROM_CLOCK = -1;
struct ChainLayout {
	int8_t source[NUM_CHANNELS]; // channel that clocks each channel, or FROM_CLOCK
};
const ChainLayout chainPairs = {{FROM_CLOCK, 0, FROM_CLOCK, 2}}; // A>B, C>D
const ChainLayout chainAll = {{FROM_CLOCK, 0, 1, 2}};            // A>B>C>D

uint8_t chainPhase[NUM_CHANNELS]; // source sequences seen since the channel last fired, 0 = fires next

void chainFire(const ChainLayout &layout, uint8_t ch) {
	if (!channelTrigger(ch))
		return; // not the start of a sequence
	for (uint8_t t = 0; t < NUM_CHANNELS; t++) {
		if (layout.source[t] != ch) {
			continue;
		}
		uint8_t division = clockDivider[t].getDivision();
		if (chainPhase[t] >= division) { // division was turned down
			chainPhase[t] = 0;
		}
		bool fire = chainPhase[t] == 0;
		chainPhase[t] = (chainPhase[t] + 1) % division;
		if (fire) {
			chainFire(layout, t);
		}
	}
}

void chainOnClock(const ChainLayout &layout) {
	for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
		if (layout.source[i] == FROM_CLOCK && channelDue(i)) {
			chainFire(layout, i);
		}
	}
}

void chainPairsOnClock() {
	chainOnClock(chainPairs);
}

void chainAllOnClock() {
	chainOnClock(chainAll);
}

void chainOnReset() {
	for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
		chainPhase[i] = 0;
	}
}

} // namespace

const ModeDef modes[NUM_MODES] = {
	/* MODE_NORMAL */      {normalOnClock, nullptr},
	/* MODE_CHAIN_PAIRS */ {chainPairsOnClock, chainOnReset},
	/* MODE_CHAIN_ALL */   {chainAllOnClock, chainOnReset},
};

volatile uint8_t currentMode = MODE_NORMAL;

void modeLoad() {
	uint8_t stored = eeprom_read_byte((const uint8_t *)EEPROM_ADDR_MODE);
	currentMode = stored < NUM_MODES ? stored : MODE_NORMAL;
}

void modeSave(uint8_t mode) {
	if (mode >= NUM_MODES || mode == currentMode) {
		return;
	}
	currentMode = mode;
	eeprom_update_byte((uint8_t *)EEPROM_ADDR_MODE, mode);
}
