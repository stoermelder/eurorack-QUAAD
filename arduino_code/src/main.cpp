#include <Arduino.h>
#include <util/atomic.h>
#include "digital.h"

//// VARIABLES ////
// Pin addressing

const uint8_t pin_CLK_DIV_IN[4] = {A0, A1, A6, A3}; // Analog pins
const uint8_t pin_CLK_DIV_OUT[4] = {3, 2, 1, 0};    // Digital pins
const uint8_t pin_PTRN[4] = {A4, A5, A2, A7};
const uint8_t pin_A[4] = {4, 6, 10, 12};
const uint8_t pin_B[4] = {5, 7, 11, 13};

// Clock input
const uint8_t pin_CLOCK = 9;
SchmittTrigger clockTriggerHigh;
SchmittTrigger clockTriggerLow;
uint16_t clockCount = 0;

// Clock divider
const uint16_t divisions[] = {32, 16, 12, 8, 7, 5, 4, 3, 2, 1};
int clk_div_index[4] = {-1, -1, -1, -1}; // current slot per channel, -1 = not read yet
ClockDivider clockDivider[4]; // division is written in loop(), read in ISR

// Reset input
const uint8_t pin_RESET = 8;
SchmittTrigger resetTrigger;

// Pattern reader
// The summed pattern input is ~1 V (30 counts) per pattern, so the full ADC range
// covers several repeats of the pattern list
const int ptrn_size = 30;
const int ptrn_offset = 309;
int ptrn_mid[4] = {2000, 2000, 2000, 2000};
volatile uint8_t pattern[4] = {0, 0, 0, 0}; // pattern index, written in loop(), read in ISR

// Patterns: a step list (values 0..3 = mux code = step - 1) or a random mode
const uint8_t PTRN_RANDOM = 0; // uniformly random step
const uint8_t PTRN_WALK = 255; // random walk, +/-1 step (wraps 4 -> 1)
const uint8_t PTRN_MAX_LEN = 8;
struct Pattern {
	uint8_t length; // number of steps, or PTRN_RANDOM / PTRN_WALK
	uint8_t steps[PTRN_MAX_LEN];
};
const Pattern patterns[] = {
	{4, {0, 1, 2, 3}},                   // 1-2-3-4
	{4, {0, 1, 3, 2}},                   // 1-2-4-3
	{4, {1, 3, 0, 2}},                   // 2-4-1-3
	{4, {2, 3, 1, 0}},                   // 3-4-2-1
	{4, {3, 1, 2, 0}},                   // 4-2-3-1
	{4, {3, 2, 1, 0}},                   // 4-3-2-1
	{6, {0, 1, 2, 3, 2, 1}},             // ping-pong
	{6, {3, 2, 1, 0, 1, 2}},             // reverse ping-pong
	{8, {0, 0, 1, 1, 2, 2, 3, 3}},       // repeats
	{6, {0, 1, 0, 2, 0, 3}},             // home base
	{PTRN_RANDOM, {}},                   // random step
	{PTRN_WALK, {}}                      // random walk
};
const uint8_t NUM_PATTERNS = sizeof(patterns) / sizeof(patterns[0]);

// Sequencer state (ISR only), one per channel; reset by the reset input
uint8_t seq_pos[4] = {0, 0, 0, 0};  // position within the step list
uint8_t walk_pos[4] = {0, 0, 0, 0}; // current step of the random walk
uint8_t rng_state = 1;


void setup() {
	analogReference(DEFAULT);
	pinMode(pin_CLOCK, INPUT);
	for (int i = 0; i <= 3; i++) {
		pinMode(pin_A[i], OUTPUT);
		pinMode(pin_B[i], OUTPUT);
		pinMode(pin_CLK_DIV_OUT[i], OUTPUT);
		// pinMode(pin_PTRN[i],INPUT); pinMode(pin_CLK_DIV_IN[i],INPUT);
	}

	// Setup timer
	cli(); // disable interrupts
	// Turn on CTC mode
	TCCR1A = 0; // set entire TCCR1A register to 0
	TCCR1B = 0; // same for TCCR1B
	TCCR1B |= (1 << WGM12);
	// Set CS11 bit for prescaler 8
	TCCR1B |= (1 << CS11);
	// Initialize counter value to 0;
	TCNT1  = 0;
	// Set timer compare for 8kHz
	OCR1A = 249; // = (16*10^6) / (8000*8) - 1
	// Enable timer compare interrupt
	TIMSK1 |= (1 << OCIE1A);
	sei(); // enable interrupts
}

void loop() {
	// All ADC work lives here so the timer ISR stays short.
	// Multi-byte values shared with the ISR are written with interrupts disabled.
	for (int i = 0; i < 4; i++) {
		// Pattern reader
		int pattern_read = analogRead(pin_PTRN[i]) + ptrn_size / 2;
		if (abs(pattern_read - ptrn_mid[i]) > 20) {
			// Floor division, so readings below the offset continue the pattern cycle
			int d = pattern_read - ptrn_offset;
			int slot = d >= 0 ? d / ptrn_size : -((-d + ptrn_size - 1) / ptrn_size);
			ptrn_mid[i] = ptrn_offset + slot * ptrn_size + ptrn_size / 2;
			uint8_t newPattern = ((slot % NUM_PATTERNS) + NUM_PATTERNS) % NUM_PATTERNS;
			ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
				pattern[i] = newPattern;
			}
		}

		// Clock divider reader
		int clockDivRead = analogRead(pin_CLK_DIV_IN[i]);
		// Hysteresis: only change slot when the reading is > 54 counts from the
		// centre of the current slot (slot width is 1024 / 10 = 102 counts)
		int clk_div_mid = 51 + (1024 / 10) * clk_div_index[i];
		if (clk_div_index[i] < 0 || abs(clockDivRead - clk_div_mid) > 54) {
			int index = constrain(map(clockDivRead, 0, 922, 0, 9), 0, 9);
			clk_div_index[i] = index;
			ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
				clockDivider[i].setDivision(divisions[index]);
			}
		}
	}
}

uint8_t nextRandom() {
	// 8-bit xorshift
	rng_state ^= rng_state << 7;
	rng_state ^= rng_state >> 5;
	rng_state ^= rng_state << 3;
	return rng_state;
}

/** Advances channel `ch` by one step and returns the mux code 0..3. */
uint8_t nextStep(uint8_t ch) {
	const Pattern &p = patterns[pattern[ch]];
	if (p.length == PTRN_RANDOM) {
		return nextRandom() & 3;
	}
	if (p.length == PTRN_WALK) {
		walk_pos[ch] = (walk_pos[ch] + ((nextRandom() & 1) ? 1 : 3)) & 3;
		return walk_pos[ch];
	}
	// The pattern can change between clocks, so the position may be out of range
	if (seq_pos[ch] >= p.length)
		seq_pos[ch] = 0;
	return p.steps[seq_pos[ch]++];
}

void patternDriver(uint8_t code, int pinA, int pinB) {
	// Mux channel 0..3: bit 0 -> A, bit 1 -> B
	digitalWrite(pinA, code & 1);
	digitalWrite(pinB, (code >> 1) & 1);
}

ISR(TIMER1_COMPA_vect) {
	// Reset input
	if (resetTrigger.process(digitalRead(pin_RESET))) {
		clockCount = 0;
		for (int i = 0; i < 4; i++) {
			seq_pos[i] = 0;
			walk_pos[i] = 0;
		}
	}

	// Clock input
	int clock = digitalRead(pin_CLOCK);

	// Clock input went high
	if (clockTriggerHigh.process(clock)) {
		// 3360 can be divided by the numbers in divisions[]
		if (clockCount < 3360)
			clockCount++;
		else
			clockCount = 1;

		for (int i = 0; i < 4; i++) {
			if (clockDivider[i].process(clockCount)) {
				digitalWrite(pin_CLK_DIV_OUT[i], HIGH);
				patternDriver(nextStep(i), pin_A[i], pin_B[i]);
			}
		}
	}

	// Clock input went low
	if (clockTriggerLow.process(!clock)) {
		for (int i = 0; i < 4; i++) {
			digitalWrite(pin_CLK_DIV_OUT[i], LOW);
		}
	}
}

