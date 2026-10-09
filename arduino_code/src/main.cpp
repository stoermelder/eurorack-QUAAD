#include <Arduino.h>
#include "digital.h"
#include "pins.h"
#include "channels.h"
#include "sequencer.h"
#include "modes.h"
#include "inputs.h"
#include "shift.h"

// Structure:
//   pin-change ISR (this file)  clock/reset edge handling, dispatches to the active mode (modes.cpp)
//   loop()                      all ADC work: shift UI (shift.cpp), pattern and division inputs
//                               (inputs.cpp); the results are applied at the next clock tick
//   channels.cpp                clock counter, dividers, gate and step outputs
//   sequencer.cpp               patterns and step selection
//
// Everything that drives the outputs runs in the pin-change interrupt of the clock and reset
// inputs, so outputs follow the clock edge immediately; nothing happens between clock edges.

uint8_t lastPins;

void pinChangeMask(uint8_t mask) {
	uint8_t sreg = SREG;
	cli();
	lastPins = PINB & (RESET_BIT | CLOCK_BIT);
	PCMSK0 = mask;
	PCIFR = _BV(PCIF0);
	SREG = sreg;
}

void setup() {
	analogReference(DEFAULT);
	pinMode(pin_CLOCK, INPUT);
	pinMode(pin_RESET, INPUT);
	channelsInit();

	// Pin-change interrupt on the clock and reset inputs; the active mode may narrow it
	pinChangeMask(RESET_BIT | CLOCK_BIT);
	PCICR |= _BV(PCIE0);
	modeLoad();
}

void loop() {
	shiftUpdate();
	for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
		patternInputUpdate(i);
		divisionInputUpdate(i);
	}
}

ISR(PCINT0_vect) {
	uint8_t pins = PINB & (RESET_BIT | CLOCK_BIT);
	uint8_t changed = pins ^ lastPins;
	lastPins = pins;

	const ModeDef &mode = modes[currentMode];

	// Reset input went high
	if (changed & pins & RESET_BIT) {
		clockReset();
		sequencerReset();
		if (mode.onReset) {
			mode.onReset();
		}
	}

	if ((changed & CLOCK_BIT) && (PCMSK0 & CLOCK_BIT)) { // the clock is masked in the oscillator mode
		if (pins & CLOCK_BIT) {
			// Clock input went high
			channelsApplySettings();
			clockAdvance();
			mode.onClock();
		}
		else {
			// Clock input went low
			channelsGatesLow();
		}
	}
}
