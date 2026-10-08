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
int ptrn_size = 30;
int ptrn_offset = 309;
int ptrn_mid[4] = {2000, 2000, 2000, 2000};
volatile int pattern[4] = {1, 1, 1, 1}; // written in loop(), read in ISR


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
			int newPattern = 0;
			// -5:0 V
			if (pattern_read >= 129 && pattern_read < 309) {
				newPattern = map(pattern_read, 129, 279, 1, 6);
				ptrn_mid[i] = ptrn_offset - 6 * ptrn_size + ptrn_size / 2 + (newPattern - 1) * ptrn_size;
			}
			// 0:5 V
			else if (pattern_read >= 309 && pattern_read < 489) {
				newPattern = map(pattern_read, 309, 459, 1, 6);
				ptrn_mid[i] = ptrn_offset + 0 * ptrn_size + ptrn_size / 2 + (newPattern - 1) * ptrn_size;
			}
			// 5:10 V
			else if (pattern_read >= 489 && pattern_read < 669) {
				newPattern = map(pattern_read, 489, 639, 1, 6);
				ptrn_mid[i] = ptrn_offset + 6 * ptrn_size + ptrn_size / 2 + (newPattern - 1) * ptrn_size;
			}
			// 10:15V
			else if (pattern_read >= 669 && pattern_read < 849) {
				newPattern = map(pattern_read, 669, 819, 1, 6);
				ptrn_mid[i] = ptrn_offset + 12 * ptrn_size + ptrn_size / 2 + (newPattern - 1) * ptrn_size;
			}
			if (newPattern != 0) {
				ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
					pattern[i] = newPattern;
				}
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

void patternDriver(int ptrn, int step, int pinA, int pinB) {
	// 6 patterns x 4 steps, values 1..4 = mux channel + 1
	static const uint8_t patterns[] = {
		1, 2, 3, 4,
		1, 2, 4, 3,
		2, 4, 1, 3,
		3, 4, 2, 1,
		4, 2, 3, 1,
		4, 3, 2, 1
	};
	// Mux channel 0..3: bit 0 -> A, bit 1 -> B
	uint8_t state = patterns[4 * (ptrn - 1) + step - 1] - 1;
	digitalWrite(pinA, state & 1);
	digitalWrite(pinB, (state >> 1) & 1);
}

ISR(TIMER1_COMPA_vect) {
	// Reset input
	if (resetTrigger.process(digitalRead(pin_RESET))) {
		clockCount = 0;
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
				uint8_t seq_step = (((clockCount - 1) / clockDivider[i].getDivision()) % 4) + 1;
				patternDriver(pattern[i], seq_step, pin_A[i], pin_B[i]);
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

