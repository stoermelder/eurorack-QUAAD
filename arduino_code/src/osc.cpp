#include <avr/pgmspace.h>
#include <util/atomic.h>
#include "osc.h"
#include "channels.h"
#include "sequencer.h"
#include "inputs.h"
#include "calibration.h"

namespace {

// Phase accumulator increment per sample (40 kHz) of one step, for C1 .. B1 (each octave
// above that is a shift): f * 2^32 / 40000. A voice with a pattern of L steps steps at L * f.
const uint32_t semitoneInc[12] PROGMEM = {
	3511479, 3720283, 3941502, 4175876, 4424187, 4687263,
	4965982, 5261274, 5574126, 5905581, 6256745, 6628790
};
// Step rate limit of fs / 4 (= 2^32 / 4), above that the step timing aliases heavily
const uint32_t INC_MAX = 1UL << 30;

// Settings from loop(); the increment is 32 bits wide, so it is handed over atomically
volatile uint32_t increment[NUM_CHANNELS] = {INC_MAX / 8, INC_MAX / 8, INC_MAX / 8, INC_MAX / 8};
uint16_t pitch[NUM_CHANNELS] = {24 * 256, 24 * 256, 24 * 256, 24 * 256}; // loop() only, 1/256 semitone
uint8_t waveform[NUM_CHANNELS] = {0, 0, 0, 0};      // loop() only
volatile uint8_t half[NUM_CHANNELS] = {2, 2, 2, 2}; // gate is high for this many steps of a cycle

// Voice state, ISR only
uint32_t phase[NUM_CHANNELS];
uint8_t code[NUM_CHANNELS]; // mux code being played
uint8_t pos[NUM_CHANNELS];  // step within the cycle
uint8_t gates;              // bit ch: gate of channel ch

void update(uint8_t ch) {
	uint8_t length = sequencerLength(waveform[ch]);
	// Interpolate linearly between the two neighbouring semitones (error < 1 cent)
	uint8_t semitone = pitch[ch] >> 8;
	uint8_t frac = pitch[ch] & 0xFF;
	uint32_t low = pgm_read_dword(&semitoneInc[semitone % 12]);
	uint32_t high = semitone % 12 == 11 ? semitoneInc[0] * 2 : pgm_read_dword(&semitoneInc[semitone % 12 + 1]);
	uint32_t base = (low + (((high - low) * frac) >> 8)) << (semitone / 12);
	uint32_t maxBase = INC_MAX / length;
	uint32_t inc = (base > maxBase ? maxBase : base) * length;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		increment[ch] = inc;
		half[ch] = length / 2;
	}
}

void advance(uint8_t ch) {
	bool atStart;
	code[ch] = sequencerNext(ch, atStart);
	pos[ch] = atStart ? 0 : pos[ch] + 1;
	if (pos[ch] < half[ch]) {
		gates |= _BV(ch);
	}
	else {
		gates &= ~_BV(ch);
	}
}

// Inputs. The division knob selects the waveform (pattern) immediately.
typedef SlotSelector<0> WaveformKnob;
WaveformKnob waveformKnob[NUM_CHANNELS] = {
	WaveformKnob(waveformMap), WaveformKnob(waveformMap),
	WaveformKnob(waveformMap), WaveformKnob(waveformMap)
};

// Pitch: readings are averaged, positions are in hundredths of a count
const int PITCH_AVERAGE = 8;
const long PITCH_HYSTERESIS_X100 = 25; // ignore changes below a quarter count (noise)
long pitchLast[NUM_CHANNELS];          // position of the applied pitch
bool pitchRestart[NUM_CHANNELS] = {true, true, true, true}; // apply the next reading whatever it is

void setPitch(uint8_t ch, uint16_t newPitch) {
	pitch[ch] = newPitch > OSC_MAX_PITCH ? OSC_MAX_PITCH : newPitch;
	update(ch);
}

void setWaveform(uint8_t ch, uint8_t pattern) {
	waveform[ch] = pattern;
	sequencerSetPattern(ch, pattern);
	update(ch);
}

} // namespace

ISR(TIMER1_COMPA_vect) {
	bool changed = false;
	for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
		uint32_t previous = phase[ch];
		phase[ch] = previous + increment[ch];
		if (phase[ch] < previous) { // wrapped: next step
			advance(ch);
			changed = true;
		}
	}
	if (changed) {
		channelsWriteAll(code, gates);
	}
}

void oscRestart() {
	sequencerReset();
	for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
		phase[ch] = 0;
		advance(ch);
	}
	channelsWriteAll(code, gates);
}

void oscEnter() {
	for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
		pitchRestart[ch] = true;
		waveformKnob[ch].reset();
	}
	for (uint8_t ch = 0; ch < NUM_CHANNELS; ch++) {
		update(ch);
	}
	oscRestart();
	pinChangeMask(RESET_BIT); // the clock input is ignored
	TCCR1A = 0;
	TCCR1B = _BV(WGM12) | _BV(CS10); // CTC, no prescaler: 16 MHz / 400 = 40 kHz
	OCR1A = 399;
	TCNT1 = 0;
	TIMSK1 = _BV(OCIE1A);
}

void oscExit() {
	TIMSK1 = 0;
	TCCR1B = 0;
	pinChangeMask(RESET_BIT | CLOCK_BIT);
	channelsGatesLow();
	sequencerReset();
	clockReset();
	normalPatternInputReset(); // the sequencer patterns were the waveforms
}

void oscPatternInput(uint8_t ch) {
	int sum = 0;
	for (uint8_t i = 0; i < PITCH_AVERAGE; i++) {
		sum += analogRead(pin_PTRN[ch]);
	}
	// 0 V = PITCH_OFFSET; x100 counts relative to that
	long x = sum * 100L / PITCH_AVERAGE - PITCH_OFFSET * 100L;
	if (!pitchRestart[ch] && labs(x - pitchLast[ch]) <= PITCH_HYSTERESIS_X100) {
		return;
	}
	pitchLast[ch] = x;
	pitchRestart[ch] = false;
	// 1 V = PITCH_COUNTS_PER_VOLT_X10 / 10 counts = 12 semitones; unit 1/256 semitone (x fits in 32 bits)
	long newPitch = x * (12 * 256) / (10L * PITCH_COUNTS_PER_VOLT_X10);
	setPitch(ch, constrain(newPitch, 0, OSC_MAX_PITCH));
}

void oscDivisionInput(uint8_t ch, int reading) {
	if (waveformKnob[ch].update(reading, millis())) {
		setWaveform(ch, waveformKnob[ch].value());
	}
}
