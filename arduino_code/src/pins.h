#pragma once
#include <Arduino.h>

constexpr uint8_t NUM_CHANNELS = 4;

// Per channel (A..D)
constexpr uint8_t pin_CLK_DIV_IN[NUM_CHANNELS] = {A0, A1, A6, A3};  // division knob (analog)
constexpr uint8_t pin_PTRN[NUM_CHANNELS] = {A4, A5, A2, A7};        // pattern CV (analog)

// Not listed here: the divided clock outs (PD3..PD0 = A..D) and the step mux selects
// (A/B: PD4/PD5, PD6/PD7, PB2/PB3, PB4/PB5). They are driven with direct port writes
// in channels.cpp, so changing pins here would have no effect on them.

constexpr uint8_t pin_CLOCK = 9;
constexpr uint8_t pin_RESET = 8; // jack or push button
