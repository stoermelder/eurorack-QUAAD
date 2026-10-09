#pragma once
#include "digital.h"
#include "sequencer.h"

// Calibration of the knobs: where on the (non-linear) knobs each slot starts, as ADC readings
// (0-1023). All knobs are used through a SlotMap / SlotSelector (digital.h), so this is the
// only place to adjust when the pots or the panel marks change. If a slot starts too early,
// raise its entry (and the ones after it); if it starts too late, lower it.

// Division knob: an "off" end stop (gate muted), then ÷32 ... ÷1 (see `divisions` in
// inputs.cpp). The "off" stop is narrow on purpose; the ADC cannot read above 1023, so the
// last entry must stay below what a fully turned knob really reads.
const int DIV_SLOTS = 11;
const int divisionStart[DIV_SLOTS] = {
	0,    // off
	24,   // 32
	138,  // 16
	252,  // 12
	366,  // 8
	480,  // 7
	593,  // 5
	707,  // 4
	821,  // 3
	935,  // 2
	1010  // 1
};
constexpr SlotMap divisionMap(divisionStart, DIV_SLOTS, false);

// Pattern knob: the summed CV (knob + CV jack + master knob), about 29 counts per volt, 309 at
// 0 V; the knob alone sweeps about 0-5 V. Lowest reading of each pattern within one cycle; the
// last entry is where the cycle starts again (master knob, more CV).
const int patternStart[NUM_PATTERNS + 1] = {
	303,  // 1
	314,  // 2
	325,  // 3
	336,  // 4
	347,  // 5
	358,  // 6
	369,  // 7
	382,  // 8
	395,  // 9
	408,  // 10
	421,  // 11
	434,  // 12
	479   // cycle repeats
};
constexpr SlotMap patternMap(patternStart, NUM_PATTERNS, true);
