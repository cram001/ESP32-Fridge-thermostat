#pragma once

#include <cstdint>

extern uint32_t g_fake_millis;

inline uint32_t millis() { return g_fake_millis; }

template <typename T>
constexpr T constrain(T value, T low, T high) {
  return value < low ? low : (value > high ? high : value);
}
