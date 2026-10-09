#pragma once
#include "digital.h"
#include "sequencer.h"

// The four channels: shared clock counter, clock dividers and the gate/step outputs.
// These are the building blocks the modes (modes.cpp) combine.

/** Division per channel. Written in loop() with interrupts disabled, read in the ISR. */
extern ClockDivider clockDivider[NUM_CHANNELS];

/** Gate output of a channel is silent while muted; its sequencer keeps running.
 *  Written in loop() with interrupts disabled, read in the ISR. */
extern volatile bool channelMuted[NUM_CHANNELS];

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
