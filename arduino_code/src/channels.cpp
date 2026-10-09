#include "channels.h"

ClockDivider clockDivider[NUM_CHANNELS];

namespace {
uint16_t clockCount = 0;
bool channelMuted[NUM_CHANNELS] = {false, false, false, false}; // ISR only

// Settings handed over by loop(), applied at the next clock tick
volatile uint8_t pendingDivision[NUM_CHANNELS] = {1, 1, 1, 1};
volatile bool pendingMuted[NUM_CHANNELS] = {false, false, false, false};
volatile uint8_t pendingPattern[NUM_CHANNELS] = {0, 0, 0, 0};

// Replace the two bits of `port` starting at `shift` with `code` in a single write
inline void writeBits(volatile uint8_t &port, uint8_t shift, uint8_t code) {
	port = (port & ~(0b11 << shift)) | (code << shift);
}
}

void channelSetDivision(uint8_t ch, uint8_t division) {
	pendingDivision[ch] = division;
}

void channelSetMuted(uint8_t ch, bool muted) {
	pendingMuted[ch] = muted;
}

void channelSetPattern(uint8_t ch, uint8_t pattern) {
	pendingPattern[ch] = pattern;
}

void channelsApplySettings() {
	for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
		clockDivider[i].setDivision(pendingDivision[i]);
		channelMuted[i] = pendingMuted[i];
		sequencerSetPattern(i, pendingPattern[i]);
	}
}

void channelsInit() {
	DDRD |= 0xFF;         // PD0..PD3 clock outs D..A, PD4..PD7 mux selects of A and B
	DDRB |= 0b00111100;   // PB2..PB5 mux selects of C and D
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
		PORTD |= _BV(3 - ch);  // clock outs A..D = PD3..PD0
	}
	// Mux code 0..3: bit 0 -> A, bit 1 -> B. A and B are adjacent port bits,
	// so one write switches both at once (no intermediate mux state).
	bool atStart;
	uint8_t code = sequencerNext(ch, atStart);
	switch (ch) {
		case 0: writeBits(PORTD, 4, code); break;  // PD4/PD5
		case 1: writeBits(PORTD, 6, code); break;  // PD6/PD7
		case 2: writeBits(PORTB, 2, code); break;  // PB2/PB3
		default: writeBits(PORTB, 4, code); break; // PB4/PB5
	}
	return atStart;
}

void channelsGatesLow() {
	PORTD &= ~0x0F;  // all four clock outs (PD0..PD3) in one write
}
