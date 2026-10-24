#include "gen/common.h"
#include "pc/audio.h"
#include "pc/controllerspeaker.h"
#include "pc/settings.h"
#include "snd/rsdformat.h"
#include "snd/trnssnd.h"
#include "snd/sndmath.h"
#include "p3d/context.h"
#include "p3d/input.h"
#include "pddi/pddidev.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <thread>

static void SaveWav(const std::filesystem::path& path, const std::vector<s16>& pcm, u32 rate) {
    const u32 size = (u32)pcm.size() * sizeof(s16);
    const u32 header[]{0x46464952, 36+size, 0x45564157, 0x20746d66,16,0x00010001,
        rate,rate*2,0x00100002,0x61746164,size};
    std::ofstream out(path,std::ios::binary);
    out.write((const char*)header,sizeof(header));
    out.write((const char*)pcm.data(),size);
}

int AutoTest_ProbeControllerSpeaker(const char* outputDirectory, bool play, int volume) {
    const auto output = std::filesystem::absolute(outputDirectory);
    std::filesystem::create_directories(output);
    std::ofstream report(output/"probe.txt");
    std::ifstream file("SOUND/FX/RS0000.WAX",std::ios::binary);
    if (!file) { report << "Missing original WAX bank\n"; return 30; }
    std::vector<u8> data((std::istreambuf_iterator<char>(file)),{});
    const auto bank = RsdFormat::LoadWax(data.data(),(u32)data.size());
    std::unique_ptr<pddiDevice> device;
    std::unique_ptr<pddiGamepad> pad;
    std::unique_ptr<pddiDisplay> display;
    PlatformInput input;
    auto* previous = p3d::input;
    if (play) {
        device.reset(pddiCreate());
        display.reset(device->NewDisplay());
        input.SetDisplay(display.get());
        pad.reset(device->NewGamepad());
        input.SetGamepad(pad.get());
        p3d::input = &input;
        if (!AudioEngine::Init()) { p3d::input = previous; return 31; }
        g_settings.controllerSpeakerEnabled = true;
        g_settings.controllerSpeakerVolume = std::clamp(volume, 0, 100);
        for (int i=0;i<500 && !ControllerSpeaker::IsReady();++i) {
            input.ServiceInput(); ControllerSpeaker::Update();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        if (!ControllerSpeaker::IsReady()) {
            report << "No matched, routable USB speaker endpoint\n";
            AudioEngine::Shutdown(); p3d::input = previous; return 32;
        }
        report << "Matched USB speaker ready. Listening confirmation required.\n";
    }
    for (u16 id : {251,252,253,254,255,256,257}) {
        if (play && id != 251 && id != 256 && id != 257) continue;
        const auto* entry = reinterpret_cast<const u8*>(&g_transData[2*id]);
        const u32 sample = entry[0];
        if (sample >= bank.pcmSamples.size() || bank.pcmSamples[sample].empty()) continue;
        u32 rate = 22050;
        if (sample < bank.sampleDescs.size()) {
            const u16 pitch = bank.sampleDescs[sample].params & 0xffff;
            if (pitch) rate = 44100u*pitch/4096u;
        }
        const auto& pcm = bank.pcmSamples[sample];
        SaveWav(output/(std::to_string(id)+".wav"),pcm,rate);
        report << "transient=" << id << " bank=rs0000 sample=" << sample << " frames=" << pcm.size() << " rate=" << rate
            << " volume=" << (int)entry[4] << " volume_variation=" << (int)entry[5]
            << " pitch=" << (int)entry[6] << " pitch_variation=" << (int)entry[7] << '\n';
        report.flush();
        if (play) {
            const auto handle = AudioEngine::LoadSample(pcm.data(),(u32)pcm.size(),rate,1);
            const bool accepted = AudioEngine::PlayControllerSample(handle,PsxVolToFloat(PsxVol100(entry[4])),
                PsxPitchToFloat(PsxPitch200(entry[6])));
            report << "speaker accepted=" << accepted << '\n'; report.flush();
            const int ticks = (int)((double)pcm.size()/rate*50)+50;
            for (int i=0;i<ticks;++i) {
                input.ServiceInput(); ControllerSpeaker::Update();
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            AudioEngine::UnloadSample(handle);
        }
    }
    if (play) { AudioEngine::Shutdown(); p3d::input = previous; }
    return 0;
}
