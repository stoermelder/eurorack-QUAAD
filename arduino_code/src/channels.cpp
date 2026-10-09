#include "channels.h"

ClockDivider clockDivider[NUM_CHANNELS];
volatile bool channelMuted[NUM_CHANNELS] = {false, false, false, false};

namespace {
uint16_t clockCount = 0;
}

void channelsInit() {
	for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
		pinMode(pin_A[i], OUTPUT);
		pinMode(pin_B[i], OUTPUT);
		pinMode(pin_CLK_DIV_OUT[i], OUTPUT);
	}
}

void clockAdvance() {
	// 3360 can be divided by the numbers in divisions[] (inputs.cpp)
	if (clockCount < 3360) {
		clockCount++;
	}
	else {
		clockCount = 1;
	}
}

void clockReset() {
	clockCount = 0;
}

bool channelDue(uint8_t ch) {
	return clockDivider[ch].process(clockCount);
}

bool channelTrigger(uint8_t ch) {
	if (!channelMuted[ch]) {
		digitalWrite(pin_CLK_DIV_OUT[ch], HIGH);
	}
	// Mux code 0..3: bit 0 -> A, bit 1 -> B
	bool atStart;
	uint8_t code = sequencerNext(ch, atStart);
	digitalWrite(pin_A[ch], code & 1);
	digitalWrite(pin_B[ch], (code >> 1) & 1);
	return atStart;
}

void channelsGatesLow() {
	for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
		digitalWrite(pin_CLK_DIV_OUT[i], LOW);
	}
}
