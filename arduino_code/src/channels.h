#pragma once
#include "digital.h"
#include "sequencer.h"

// The four channels: shared clock counter, clock dividers and the gate/step outputs.
// These are the building blocks the modes (modes.cpp) combine.

/** Division per channel (ISR context). */
extern ClockDivider clockDivider[NUM_CHANNELS];

// Settings from the knobs. loop() reads the knobs and hands the results over with these
// setters; they only take effect at the start of the next clock tick (channelsApplySettings),
// so nothing changes between two clock edges. Each value is a single byte, so loop() and the
// ISR can share them without disabling interrupts.

/** Clock division of channel `ch` (1..255). */
void channelSetDivision(uint8_t ch, uint8_t division);
/** Mute the gate output of channel `ch`; its sequencer keeps running. */
void channelSetMuted(uint8_t ch, bool muted);
/** Pattern (index into the pattern table) of channel `ch`. */
void channelSetPattern(uint8_t ch, uint8_t pattern);
/** Applies the settings handed over so far (ISR context, at the start of a clock tick). */
void channelsApplySettings();

void channelsInit();

/** Clock input went high: advance the shared clock counter (ISR context). */
void clockAdvance();
/** Reset input: the next clock is clock 1 again (ISR context). */
void clockReset();

/** True if channel `ch` is due to fire on the current clock (phase-locked to clockReset). */
bool channelDue(uint8_t ch);
/** Raises the gate of channel `ch` and advances its sequencer to the next step.
 *  Returns true if that step is the first of a new sequence. */
bool channelTrigger(uint8_t ch);
/** Clock input went low: lowers all gates. */
void channelsGatesLow();
