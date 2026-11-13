#pragma once

#include <algorithm>
#include <cmath>
#include <cstdlib>

// Shared by the native settings loader and its headless tests.
namespace SettingsParsing {
inline float Float(const char* text, float fallback, float minimum, float maximum) {
    if (!text || !*text) return fallback;
    char* end = nullptr;
    const float value = std::strtof(text, &end);
    if (end == text || !std::isfinite(value)) return fallback;
    return std::clamp(value, minimum, maximum);
}
} // namespace SettingsParsing
