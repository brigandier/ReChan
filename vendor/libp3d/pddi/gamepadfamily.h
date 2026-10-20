#pragma once
#include <string>

enum class GamepadFamily {
    Unknown,
    Xbox,
    PlayStation,
    Switch,
};

// GLFW exposes a mapping name rather than a controller-type enum.
inline GamepadFamily GamepadFamilyFromMappingName(const char* name) {
    if (!name) return GamepadFamily::Unknown;
    std::string value(name);
    for (char& c : value) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    if (value.find("xbox") != std::string::npos || value.find("xinput") != std::string::npos) return GamepadFamily::Xbox;
    if (value.find("playstation") != std::string::npos || value.find("dualshock") != std::string::npos
        || value.find("dualsense") != std::string::npos || value.find("ps3") != std::string::npos
        || value.find("ps4") != std::string::npos || value.find("ps5") != std::string::npos) return GamepadFamily::PlayStation;
    if (value.find("switch") != std::string::npos || value.find("joy-con") != std::string::npos) return GamepadFamily::Switch;
    return GamepadFamily::Unknown;
}
