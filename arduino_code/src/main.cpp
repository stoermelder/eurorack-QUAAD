#include <Arduino.h>
#include "digital.h"
#include "pins.h"
#include "channels.h"
#include "sequencer.h"
#include "modes.h"
#include "inputs.h"
#include "shift.h"

// Structure:
//   ISR (this file)  clock/reset edge detection, dispatches to the active mode (modes.cpp)
//   loop()           all ADC work: shift UI (shift.cpp), pattern and division inputs (inputs.cpp)
//   channels.cpp     clock counter, dividers, gate and step outputs
//   sequencer.cpp    patterns and step selection

void setup() {
	analogReference(DEFAULT);
	pinMode(pin_CLOCK, INPUT);
	channelsInit();
	modeLoad();

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
	// Multi-byte values shared with the ISR are written with interrupts disabled
	// (see inputs.cpp).
	shiftUpdate();
	for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
		patternInputUpdate(i);
		divisionInputUpdate(i);
	}
}

ISR(TIMER1_COMPA_vect) {
	static SchmittTrigger resetTrigger;
	static SchmittTrigger clockTriggerHigh;
	static SchmittTrigger clockTriggerLow;

	const ModeDef &mode = modes[currentMode];

	// Reset input
	if (resetTrigger.process(digitalRead(pin_RESET))) {
		clockReset();
		sequencerReset();
		if (mode.onReset) {
			mode.onReset();
		}
	}

	// Clock input
	int clock = digitalRead(pin_CLOCK);

	// Clock input went high
	if (clockTriggerHigh.process(clock)) {
		clockAdvance();
		mode.onClock();
	}

	// Clock input went low
	if (clockTriggerLow.process(!clock)) {
		channelsGatesLow();
	}
}
