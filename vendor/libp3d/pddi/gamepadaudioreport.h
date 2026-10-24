#pragma once
#include <array>

inline std::array<unsigned char, 47> DualSenseSpeakerReport(bool enabled,
    unsigned char low, unsigned char high, unsigned short firmware) {
    std::array<unsigned char, 47> report{};
    // Only audio routing/volume are writable; headphone, microphone, LED and trigger state are preserved.
    report[0] = 0xa0;
    report[1] = 0x80;
    report[5] = enabled ? 0x64 : 0;
    report[7] = enabled ? 0x30 : 0;
    report[37] = enabled ? 0x02 : 0; // Speaker preamp: +6 dB, independent of microphone/headphone gain.
    if (low || high) {
        report[0] |= 0x02;
        if (firmware >= 0x0224) report[38] = 0x04;
        else {
            report[0] |= 0x01;
            low >>= 1;
            high >>= 1;
        }
        report[2] = high;
        report[3] = low;
    }
    return report;
}
