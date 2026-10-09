#pragma once
#include <Arduino.h>

constexpr uint8_t NUM_CHANNELS = 4;

// Per channel (A..D)
constexpr uint8_t pin_CLK_DIV_IN[NUM_CHANNELS] = {A0, A1, A6, A3};  // division knob (analog)
constexpr uint8_t pin_CLK_DIV_OUT[NUM_CHANNELS] = {3, 2, 1, 0};     // divided clock out (gate)
constexpr uint8_t pin_PTRN[NUM_CHANNELS] = {A4, A5, A2, A7};        // pattern CV (analog)
constexpr uint8_t pin_A[NUM_CHANNELS] = {4, 6, 10, 12};             // step mux select, bit 0
constexpr uint8_t pin_B[NUM_CHANNELS] = {5, 7, 11, 13};             // step mux select, bit 1

constexpr uint8_t pin_CLOCK = 9;
constexpr uint8_t pin_RESET = 8; // jack or push button
