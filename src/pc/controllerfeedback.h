#pragma once
#include <cmath>
#include <cstdint>

struct ControllerLEDColor {
    std::uint8_t red, green, blue;
    bool operator==(const ControllerLEDColor&) const = default;
};

inline ControllerLEDColor ControllerHealthLED(bool playing, int health, int maxHealth, double seconds) {
    if (!playing || maxHealth <= 0) return {0, 64, 255};
    const double ratio = (double)health / maxHealth;
    if (ratio > 0.8) return {0, 200, 40};
    if (ratio > 0.6) return {130, 220, 0};
    if (ratio > 0.4) return {255, 210, 0};
    if (ratio > 0.2) return {255, 96, 0};
    const bool dim = health > 0 && ratio <= 0.12 && std::isfinite(seconds) && std::fmod(seconds, 0.5) >= 0.25;
    return {dim ? (std::uint8_t)32 : (std::uint8_t)255, 0, 0};
}
