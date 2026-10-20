#pragma once
#include "core.h"
#include "pc/apppaths.h"

struct GameSettings {
    bool subtitlesEnabled = true;
    bool Load(const char* path);
    bool Save(const char* path);
};

extern GameSettings g_settings;

static constexpr const char* SETTINGS_PATH = apppaths::kSettingsPath;
