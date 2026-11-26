#pragma once

// Timeline and easing for the startup sequence. Kept independent of Urho3D so
// timing, the readable hold, and skipping can be tested without a GPU.
#include <algorithm>
#include <cmath>

namespace SplashIntro {

struct Stage {
    const char* text;
    float duration;
    float fadeOutAt;
    float glyphDelay;
    float glyphStagger;
    float glyphDuration;
    float sweepAt;
    float sweepDuration;
};

inline constexpr int kStageCount = 3;
inline constexpr Stage kStages[kStageCount] = {
    {"k o c h . s t u d i o s", 3.8f, 2.85f, 0.16f, 0.040f, 1.25f, 1.65f, 1.15f},
    {"powered by fall.os",       4.8f, 3.80f, 0.18f, 0.055f, 1.40f, 2.20f, 1.45f},
    {"CHICKENCUM 2.1",          6.2f, 5.05f, 0.20f, 0.075f, 1.65f, 2.45f, 1.95f}
};
inline constexpr float kExitDuration = 0.75f;
inline constexpr float kSkipDuration = 0.30f;

inline float Clamp01(float value) {
    return std::max(0.0f, std::min(1.0f, value));
}

inline float Smooth(float value) {
    const float t = Clamp01(value);
    return t * t * (3.0f - 2.0f * t);
}

// The motion starts with momentum, then decelerates gently into its final pose.
inline float EaseOut(float value) {
    const float remaining = 1.0f - Clamp01(value);
    return 1.0f - remaining * remaining * remaining;
}

inline float StageVisibility(int stage, float time) {
    const Stage& s = kStages[stage];
    return 1.0f - Smooth((time - s.fadeOutAt) / (s.duration - s.fadeOutAt));
}

inline float GlyphProgress(int stage, int characterIndex, float time) {
    const Stage& s = kStages[stage];
    return Clamp01((time - s.glyphDelay - characterIndex * s.glyphStagger) / s.glyphDuration);
}

inline float SweepProgress(int stage, float time) {
    const Stage& s = kStages[stage];
    return Clamp01((time - s.sweepAt) / s.sweepDuration);
}

class Timeline {
public:
    struct Result {
        bool stageChanged = false;
        bool finished = false;
    };

    int CurrentStage() const { return stage_; }
    float StageTime() const { return stageTime_; }
    float Elapsed() const { return elapsed_; }
    bool IsExiting() const { return exiting_; }

    float OverlayOpacity() const {
        return exiting_ ? exitStartOpacity_ * (1.0f - Smooth(exitTime_ / exitDuration_)) : 1.0f;
    }

    Result Advance(float delta, bool skipRequested = false) {
        Result result;
        if (finished_) { result.finished = true; return result; }
        if (!std::isfinite(delta) || delta < 0.0f) delta = 0.0f;
        elapsed_ += delta;

        // A skip is always responsive, even during the final natural dissolve.
        // Capture the current opacity so the shorter transition never jumps.
        if (skipRequested && (!exiting_ || exitDuration_ > kSkipDuration)) {
            exitStartOpacity_ = OverlayOpacity();
            exitDuration_ = kSkipDuration;
            exitTime_ = 0.0f;
            exiting_ = true;
        }
        if (exiting_) {
            // A skipped scene keeps moving under the dissolve instead of
            // freezing abruptly; never advance into a new scene while exiting.
            if (exitDuration_ == kSkipDuration)
                stageTime_ = std::min(stageTime_ + delta, kStages[stage_].duration);
            exitTime_ += delta;
        } else {
            stageTime_ += delta;
            while (stageTime_ >= kStages[stage_].duration) {
                stageTime_ -= kStages[stage_].duration;
                if (stage_ + 1 < kStageCount) {
                    ++stage_;
                    result.stageChanged = true;
                } else {
                    // Carry overshoot into the dissolve, rather than losing a frame.
                    exitTime_ = stageTime_;
                    stageTime_ = kStages[stage_].duration;
                    exitDuration_ = kExitDuration;
                    exitStartOpacity_ = 1.0f;
                    exiting_ = true;
                    break;
                }
            }
        }
        if (exiting_ && exitTime_ >= exitDuration_) {
            finished_ = true;
            result.finished = true;
        }
        return result;
    }

private:
    int stage_ = 0;
    float stageTime_ = 0.0f;
    float elapsed_ = 0.0f;
    bool exiting_ = false;
    bool finished_ = false;
    float exitTime_ = 0.0f;
    float exitDuration_ = kExitDuration;
    float exitStartOpacity_ = 1.0f;
};

} // namespace SplashIntro
