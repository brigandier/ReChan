#pragma once
#include "core.h"

namespace ControllerSpeaker {
    void Update();
    void Shutdown();
    void StopAll();
    bool IsReady();
    bool IsSupportedController();
    u64 AcceptedEffects();
    bool Play(const s16* pcm, u32 frames, u32 rate, u32 channels, float volume, float pitch);
}
