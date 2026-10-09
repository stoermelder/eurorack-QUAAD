#include "sequencer.h"

namespace {

const uint8_t PTRN_RANDOM = 0; // uniformly random step
const uint8_t PTRN_WALK = 255; // random walk, +/-1 step (wraps 4 -> 1)
const uint8_t PTRN_MAX_LEN = 8;

struct Pattern {
	uint8_t length; // number of steps, or PTRN_RANDOM / PTRN_WALK
	uint8_t steps[PTRN_MAX_LEN]; // mux codes 0..3 (= step - 1)
};

const Pattern patterns[] = {
	{4, {0, 1, 2, 3}},             // 1-2-3-4
	{4, {0, 1, 3, 2}},             // 1-2-4-3
	{4, {1, 3, 0, 2}},             // 2-4-1-3
	{4, {2, 3, 1, 0}},             // 3-4-2-1
	{4, {3, 1, 2, 0}},             // 4-2-3-1
	{4, {3, 2, 1, 0}},             // 4-3-2-1
	{6, {0, 1, 2, 3, 2, 1}},       // ping-pong
	{6, {3, 2, 1, 0, 1, 2}},       // reverse ping-pong
	{8, {0, 0, 1, 1, 2, 2, 3, 3}}, // repeats
	{6, {0, 1, 0, 2, 0, 3}},       // home base
	{PTRN_RANDOM, {}},             // random step
	{PTRN_WALK, {}}                // random walk
};
static_assert(sizeof(patterns) / sizeof(patterns[0]) == NUM_PATTERNS,
              "NUM_PATTERNS must match the pattern table");

// Per-channel state, ISR only
uint8_t seq_pos[NUM_CHANNELS] = {0, 0, 0, 0};  // position within the step list
uint8_t walk_pos[NUM_CHANNELS] = {0, 0, 0, 0}; // current step of the random walk
uint8_t rnd_pos[NUM_CHANNELS] = {0, 0, 0, 0};  // position in the 4-step "sequence" of random patterns
uint8_t rng_state = 1;

uint8_t nextRandom() {
	// 8-bit xorshift
	rng_state ^= rng_state << 7;
	rng_state ^= rng_state >> 5;
	rng_state ^= rng_state << 3;
	return rng_state;
}

} // namespace

volatile uint8_t pattern[NUM_CHANNELS] = {0, 0, 0, 0};

void sequencerReset() {
	for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
		seq_pos[i] = 0;
		walk_pos[i] = 0;
		rnd_pos[i] = 0;
	}
}

uint8_t sequencerNext(uint8_t ch, bool &atStart) {
	const Pattern &p = patterns[pattern[ch]];
	if (p.length == PTRN_RANDOM || p.length == PTRN_WALK) {
		atStart = rnd_pos[ch] == 0;
		rnd_pos[ch] = (rnd_pos[ch] + 1) & 3;
		if (p.length == PTRN_RANDOM) {
			return nextRandom() & 3;
		}
		walk_pos[ch] = (walk_pos[ch] + ((nextRandom() & 1) ? 1 : 3)) & 3;
		return walk_pos[ch];
	}
	// The pattern can change between clocks, so the position may be out of range
	if (seq_pos[ch] >= p.length) {
		seq_pos[ch] = 0;
	}
	atStart = seq_pos[ch] == 0;
	return p.steps[seq_pos[ch]++];
}
