#include <Arduino.h>
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
ClockDivider clockDivider[4];

// Reset input
const uint8_t pin_RESET = 8;
SchmittTrigger resetTrigger;

// Pattern reader
int ptrn_size = 30;
int ptrn_offset = 309;
int ptrn_mid[4] = {2000, 2000, 2000, 2000};
int pattern[4] = {1, 1, 1, 1};

// Sequencer
int seq_step = 1;


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

void loop() {}

void patternDriver(int pattern, int seq_step, int pinA, int pinB) {
	int patterns[] = {
		1, 2, 3, 4,
		1, 2, 4, 3,
		2, 4, 1, 3,
		3, 4, 2, 1,
		4, 2, 3, 1,
		4, 3, 2, 1
	};
	int step_states[] = {
		LOW, LOW,	// step 1 - B, A
		LOW, HIGH,	// step 2 - B, A
		HIGH, LOW,	// step 3 - B, A
		HIGH, HIGH	// step 4 - B, A
	};
	digitalWrite(pinA, step_states[2 * (patterns[4 * (pattern - 1) + seq_step - 1] - 1) + 1]);
	digitalWrite(pinB, step_states[2 * (patterns[4 * (pattern - 1) + seq_step - 1] - 1)]);
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
			if (clockDivider[i].process()) {
				digitalWrite(pin_CLK_DIV_OUT[i], HIGH);
				seq_step = (((clockCount - 1) / clockDivider[i].getDivision()) % 4) + 1;
				patternDriver(pattern[i], seq_step, pin_A[i], pin_B[i]);
			}
		}
	}

	// Clock input went low
	if (clockTriggerLow.process(!clock)) {
		for (int i = 0; i < 4; i++) {
			digitalWrite(pin_CLK_DIV_OUT[i], LOW);
			// Pattern reader
			int pattern_read = analogRead(pin_PTRN[i]) + ptrn_size / 2;
			if (abs(pattern_read - ptrn_mid[i]) > 20) {
				// -5:0 V
				if (pattern_read >= 129 && pattern_read < 309) {
					pattern[i] = map(pattern_read, 129, 279, 1, 6);
					ptrn_mid[i] = ptrn_offset - 6 * ptrn_size + ptrn_size / 2 + (pattern[i] - 1) * ptrn_size;
				}
				// 0:5 V
				else if (pattern_read >= 309 && pattern_read < 489) {
					pattern[i] = map(pattern_read, 309, 459, 1, 6);
					ptrn_mid[i] = ptrn_offset + 0 * ptrn_size + ptrn_size / 2 + (pattern[i] - 1) * ptrn_size;
				}
				// 5:10 V
				else if (pattern_read >= 489 && pattern_read < 669) {
					pattern[i] = map(pattern_read, 489, 639, 1, 6);
					ptrn_mid[i] = ptrn_offset + 6 * ptrn_size + ptrn_size / 2 + (pattern[i] - 1) * ptrn_size;
				}
				// 10:15V
				else if (pattern_read >= 669 && pattern_read < 849) {
					pattern[i] = map(pattern_read, 669, 819, 1, 6);
					ptrn_mid[i] = ptrn_offset + 12 * ptrn_size + ptrn_size / 2 + (pattern[i] - 1) * ptrn_size;
				}
			}

			// Clock divider reader
			int clockDivRead = analogRead(pin_CLK_DIV_IN[i]);
			int index = constrain(map(clockDivRead, 0, 922, 0, 9), 0, 9);
			clockDivider[i].setDivision(divisions[index]);
		}
	}
}

