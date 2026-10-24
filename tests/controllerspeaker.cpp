#include "pc/controllerspeakermixer.h"
#include "pddi/gamepadaudioreport.h"
#include <cassert>
#include <limits>
#include <iostream>

void TestControllerSpeaker() {
    for (unsigned int id = 0; id < 260; ++id)
        assert(ControllerSpeakerTransient(id) == (id >= 251 && id <= 257));
    const auto off = DualSenseSpeakerReport(false, 0, 0, 0x0224);
    const auto on = DualSenseSpeakerReport(true, 0, 0, 0x0224);
    assert(on[0] == 0xa0 && on[7] == 0x30 && on[5] == 100 && on[37] == 2 && off[5] == 0 && off[7] == 0);
    assert(on[1] == 0x80 && on[4] == 0 && on[6] == 0 && on[42] == 0);
    const auto rumble = DualSenseSpeakerReport(true, 200, 80, 0x0224);
    assert(rumble[2] == 80 && rumble[3] == 200 && rumble[38] == 4 && (rumble[0] & 2));
    const auto legacy = DualSenseSpeakerReport(true, 200, 80, 0x0223);
    assert(legacy[2] == 40 && legacy[3] == 100 && (legacy[0] & 3) == 3);

    ControllerSpeakerEndpoint valid{true,true,true,false,4};
    assert(SelectControllerSpeakerEndpoint({}) == -1);
    assert(SelectControllerSpeakerEndpoint({valid}) == 0);
    auto wrong = valid; wrong.sameContainer = false;
    assert(SelectControllerSpeakerEndpoint({wrong,valid}) == 1);
    for (int condition = 0; condition < 4; ++condition) {
        auto invalid = valid;
        if (condition == 0) invalid.active = false;
        if (condition == 1) invalid.speakers = false;
        if (condition == 2) invalid.primary = true;
        if (condition == 3) invalid.channels = 2;
        assert(SelectControllerSpeakerEndpoint({invalid}) == -1);
    }
    assert(SelectControllerSpeakerEndpoint({valid,valid}) == -1);
    ControllerSpeakerMixer mixer;
    std::array<float, 32> output;
    mixer.Render(output.data(), 8, 48000, 1);
    for (auto value : output) assert(value == 0);
    const s16 pcm[]{16384,16384,16384,16384};
    assert(mixer.Play(pcm,4,24000,1,1,1));
    mixer.Render(output.data(),8,48000,0.5f);
    for (int i = 0; i < 8; ++i) {
        assert(output[i*4] == 0 && output[i*4+2] == 0 && output[i*4+3] == 0);
        assert(std::abs(output[i*4+1] - 0.25f) < 0.001f);
    }
    mixer.Clear();
    for (int i = 0; i < 4; ++i) assert(mixer.Play(pcm,4,48000,1,1,1));
    assert(!mixer.Play(pcm,4,48000,1,1,1));
    mixer.Render(output.data(), 8, 48000, 1);
    assert(output[1] == 1 && output[17] == 0);
    mixer.Clear();
    mixer.Render(output.data(), 8, 48000, 1);
    for (auto value : output) assert(value == 0);
    assert(!mixer.Play(nullptr,4,48000,1,1,1));
    assert(!mixer.Play(pcm,4,48000,4,1,1));
    assert(!mixer.Play(pcm,4,48000,1,0,1));
    assert(!mixer.Play(pcm,4,48000,1,1,std::numeric_limits<float>::quiet_NaN()));
    assert(mixer.Play(pcm,4,48000,1,1,1));
    mixer.Render(output.data(),1,48000,1);
    assert(output[1] == 0.5f);
    mixer.Clear();
    mixer.Render(output.data(),8,48000,1);
    for (auto value : output) assert(value == 0);
    s16 replaceable[]{16384,16384};
    assert(mixer.Play(replaceable,2,48000,1,1,1));
    replaceable[0] = replaceable[1] = 0;
    mixer.Render(output.data(),2,48000,1);
    assert(output[1] == 0.5f && output[5] == 0.5f);
    mixer.Clear();
    const s16 stereo[]{16384,-16384,16384,-16384};
    assert(mixer.Play(stereo,2,48000,2,1,1));
    mixer.Render(output.data(),2,48000,1);
    assert(output[1] == 0 && output[5] == 0);
    std::cout << "PASS controller speaker: event allowlist, unique endpoint/container, primary/headphone rejection, bounded overlap, volume/resampling, silent haptic lanes and rumble-preserving reports\n";
}
