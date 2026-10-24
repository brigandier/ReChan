#pragma once

struct GamepadAudioDevice {
    char path[512] = {};
    unsigned int instance = 0;
    unsigned short vendor = 0;
    unsigned short product = 0;
};
