#pragma once
#include <algorithm>
#include <cmath>

namespace CasinoAnimation {
// Preserve existing saved indices: 2 has always meant unlimited.
inline int FpsLimit(int index) {
    constexpr int limits[] = {60, 120, 0, 240};
    return limits[std::clamp(index, 0, 3)];
}
struct Timeline {
    double elapsed = 0.0;
    double duration = 1.0;
    void Reset(double seconds) { elapsed = 0; duration = std::max(0.001, seconds); }
    void Step(double dt) {
        if (std::isfinite(dt) && dt > 0) elapsed = std::min(duration, elapsed + dt);
    }
    float Progress() const { return static_cast<float>(std::clamp(elapsed / duration, 0.0, 1.0)); }
    bool Done() const { return elapsed >= duration; }
};
inline float Ease(float t) { t = std::clamp(t, 0.0f, 1.0f); return t*t*(3.0f-2.0f*t); }
inline int Outcome(int stake, int payout) { return (payout > stake) - (payout < stake); }
}
