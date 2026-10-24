#pragma once
#include "core.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

inline bool ControllerSpeakerTransient(u16 id) {
    // The three tally dragon events and both pickups share the original dragon sample,
    // with their own volume/pitch parameters; healing and extra life use separate samples.
    return id >= 251 && id <= 257;
}

struct ControllerSpeakerEndpoint {
    bool sameContainer = false;
    bool active = false;
    bool speakers = false;
    bool primary = false;
    u32 channels = 0;
};

inline int SelectControllerSpeakerEndpoint(const std::vector<ControllerSpeakerEndpoint>& endpoints) {
    int found = -1;
    for (size_t i = 0; i < endpoints.size(); ++i) {
        const auto& e = endpoints[i];
        if (!e.sameContainer || !e.active || !e.speakers || e.primary || e.channels != 4) continue;
        if (found >= 0) return -1;
        found = (int)i;
    }
    return found;
}

class ControllerSpeakerMixer {
    struct Voice {
        std::vector<s16> pcm;
        u32 rate = 0, channels = 0;
        double position = 0;
        float volume = 0, pitch = 1;
        bool active = false;
    };
    std::array<Voice, 4> voices;
public:
    void Clear() { for (auto& v : voices) v = {}; }
    bool Play(const s16* pcm, u32 frames, u32 rate, u32 channels, float volume, float pitch) {
        if (!pcm || !frames || rate < 8000 || rate > 192000 || (channels != 1 && channels != 2)
            || frames > (u64)rate * 15 || !std::isfinite(volume) || volume <= 0
            || !std::isfinite(pitch) || pitch <= 0 || pitch > 8) return false;
        for (auto& v : voices) {
            if (!v.active) {
                v.pcm.assign(pcm, pcm + (size_t)frames * channels);
                v.rate = rate; v.channels = channels; v.position = 0;
                v.volume = std::min(volume, 1.0f); v.pitch = pitch;
                v.active = true;
                return true;
            }
        }
        return false;
    }
    void Render(float* out, u32 frames, u32 rate, float volume) {
        std::fill(out, out + (size_t)frames * 4, 0.0f);
        if (!rate) return;
        for (auto& v : voices) {
            if (!v.active) continue;
            const u32 count = (u32)(v.pcm.size() / v.channels);
            const auto mono = [&](u32 frame) {
                const size_t offset = (size_t)frame * v.channels;
                return v.channels == 1 ? (float)v.pcm[offset] : (v.pcm[offset] + (float)v.pcm[offset + 1]) * 0.5f;
            };
            for (u32 i = 0; i < frames; ++i) {
                const u32 frame = (u32)v.position;
                if (frame >= count) { v.active = false; break; }
                const float mix = (float)(v.position - frame);
                const float value = mono(frame) * (1 - mix) + mono(std::min(frame + 1, count - 1)) * mix;
                // The USB front-right lane feeds the speaker; the haptic and headphone lanes remain silent.
                out[i * 4 + 1] += value / 32768.0f * v.volume * volume;
                v.position += (double)v.rate / rate * v.pitch;
            }
        }
        for (u32 i = 0; i < frames; ++i) out[i * 4 + 1] = std::clamp(out[i * 4 + 1], -1.0f, 1.0f);
    }
};
