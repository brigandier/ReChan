#include "pc/autotest.h"
#include "pc/settings.h"
#include "pc/audio.h"
#include "pc/controllerspeaker.h"
#include "pc/textmgr.h"
#include "pddi/gamepadaudio.h"
#include "extra/customtext.h"
#include "extra/fecustommenumgr.h"
#include "extra/controllerpromptstyle.h"
#include "snd/sound.h"
#include "snd/snddrct.h"
#include "snd/fesnd.h"
#include "ai/generator.h"
#include "ai/player.h"
#include "gen/ai.h"
#include "gen/blockmgr.h"
#include "gen/control.h"
#include "gen/world.h"
#include "gen/game.h"
#include "gen/time.h"
#include "p3d/context.h"
#include "p3d/input.h"
#include "pddi/pddidev.h"
#include "pc/inputaction.h"
#include "radmovie/subtitles.h"

#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <cstring>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

#include "vendor/stb/stb_image_write.h"

static bool VerifyControllerVibration() {
    struct TestPad : pddiGamepad {
        bool connected = true;
        int calls = 0;
        float low = 0, high = 0;
        void Poll() override {}
        bool IsConnected() const override { return connected; }
        bool IsButtonDown(int) const override { return false; }
        float GetAxis(int axis) const override { return axis >= 4 ? -1.0f : 0.0f; }
        bool SupportsVibration() const override { return true; }
        bool SetVibration(float l, float h) override { ++calls; low = l; high = h; return true; }
    } pad;
    std::unique_ptr<pddiDevice> device(pddiCreate());
    std::unique_ptr<pddiDisplay> display(device->NewDisplay());
    PlatformInput input;
    input.SetDisplay(display.get());
    input.SetGamepad(&pad);
    input.ServiceInput();
    ActionInput desktop;
    auto* savedInput = p3d::input;
    auto* savedActions = g_actionInput;
    const s32 savedShock = GetShock();
    p3d::input = &input;
    g_actionInput = &desktop;
    SetShock(1);
    SetActuator(0, 128, 15);
    bool ok = pad.calls == 0;
    TestControllerVibration();
    ok &= pad.calls == 1 && pad.low == 1.0f && pad.high == 0.0f;
    for (int i = 0; i < 61; ++i) UpdateActuator(0);
    ok &= pad.low == 0.0f && pad.high == 0.0f;
    SetShock(0);
    int calls = pad.calls;
    TestControllerVibration();
    ok &= pad.calls == calls;
    SetShock(1);
    pad.connected = false;
    input.ServiceInput();
    TestControllerVibration();
    ok &= pad.calls == calls;
    ClearActuator();
    SetShock(savedShock);
    p3d::input = savedInput;
    g_actionInput = savedActions;
    return ok;
}

#if defined(RECHAN_VISUAL_TESTS)
struct ControllerSpeakerMenuTest {
    static bool Blocked() {
        if (ControllerSpeaker::IsSupportedController()) return false;
        const auto saved = g_settings;
        feCustomMenuMgr menu;
        Entry item{};
        item.binding = EntryBinding_ControllerSpeaker;
        item.lo = 0; item.hi = 1;
        menu.ApplyValue(item, saved.controllerSpeakerEnabled ? 0 : 1);
        item.binding = EntryBinding_ControllerSpeakerVolume;
        item.hi = 100;
        menu.ApplyValue(item, saved.controllerSpeakerVolume == 100 ? 0 : 100);
        return g_settings.controllerSpeakerEnabled == saved.controllerSpeakerEnabled
            && g_settings.controllerSpeakerVolume == saved.controllerSpeakerVolume;
    }
};
#endif

static bool VerifyControllerSpeakerFallback() {
    struct TestPad : pddiGamepad {
        bool connected = true;
        mutable int identityQueries = 0;
        unsigned int instance = 1;
        void Poll() override {}
        bool IsConnected() const override { return connected; }
        bool IsButtonDown(int) const override { return false; }
        float GetAxis(int axis) const override { return axis >= 4 ? -1.0f : 0.0f; }
        bool SupportsVibration() const override { return false; }
        bool SetVibration(float, float) override { return false; }
        bool GetAudioDevice(GamepadAudioDevice& value) const override {
            ++identityQueries;
            value = {};
            value.instance = instance; value.vendor = 0x054c; value.product = 0x0ce6;
            std::strcpy(value.path, "unavailable-speaker-test-interface");
            return connected;
        }
    } pad;
    std::unique_ptr<pddiDevice> device(pddiCreate());
    std::unique_ptr<pddiDisplay> display(device->NewDisplay());
    PlatformInput input;
    input.SetDisplay(display.get()); input.SetGamepad(&pad); input.ServiceInput();
    auto* previousInput = p3d::input;
    const GameSettings previousSettings = g_settings;
    const bool primaryInitialized = AudioEngine::IsInitialized();
    p3d::input = &input;
    g_settings.controllerSpeakerEnabled = false;
    ControllerSpeaker::Update();
    bool ok = !ControllerSpeaker::IsReady() && !ControllerSpeaker::IsSupportedController();
#if defined(RECHAN_VISUAL_TESTS)
    ok &= ControllerSpeakerMenuTest::Blocked();
#endif
    g_settings.controllerSpeakerEnabled = true;
    for (int i=0; i<5; ++i) {
        ControllerSpeaker::Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    s16 pcm[]{1,2};
    ok &= !ControllerSpeaker::IsReady() && !ControllerSpeaker::Play(pcm,2,48000,1,1,1);
    pad.connected = false; input.ServiceInput(); ControllerSpeaker::Update();
    ok &= !ControllerSpeaker::IsReady();
    pad.connected = true; ++pad.instance; input.ServiceInput(); ControllerSpeaker::Update();
    ok &= !ControllerSpeaker::IsReady();
    g_settings.controllerSpeakerEnabled = false; ControllerSpeaker::Update(); ControllerSpeaker::Shutdown();
    ok &= AudioEngine::IsInitialized() == primaryInitialized;
    p3d::input = previousInput; g_settings = previousSettings;
    return ok;
}

int AutoTest_VerifyLocalization(const char* outputDirectory) {
    if (!VerifyControllerSpeakerFallback()) return 16;
    std::puts("PASS controller speaker: disabled opens no audio output; unavailable USB interface, disconnect/reconnect and shutdown preserve primary audio state");
#if defined(RECHAN_VISUAL_TESTS)
    std::puts("PASS unsupported speaker controls cannot change saved enable/volume preferences");
#endif
    if (!VerifyControllerVibration()) return 15;
    std::puts("PASS native vibration menu test: keyboard/mouse activation, expiry, disabled and disconnected");
    std::error_code ec;
    std::filesystem::create_directories(outputDirectory, ec);
    if (ec) return 1;
    const auto path = std::filesystem::path(outputDirectory) / "localization-settings.ini";
    Sound sound;
    g_sound = &sound;
    g_customText.Init();
    static const char* expectedCodes[] = { "en", "de", "fr", "it", "es", "pt-BR" };
    for (s32 language = 0; language < NumLanguages; ++language) {
        g_customText.SetLanguage((GameLanguage)language);
        if (std::strcmp(g_customText.GetLanguageCode(), expectedCodes[language]) != 0) return 12;
        for (const char* movie : { "PROLOG", "FACTORY", "MAKING", "VICTORY", "CREDITS" }) {
            const std::string subtitlePath = std::string("pc/subtitles/")
                + g_customText.GetLanguageCode() + "/" + movie + ".srt";
            std::ifstream input(subtitlePath, std::ios::binary);
            if (!input) return 13;
            const std::string contents((std::istreambuf_iterator<char>(input)), {});
            MovieSubtitles subtitles;
            if (!subtitles.Parse(contents)) return 14;
        }
        for (s32 style = 0; style < ControllerPromptStyle_Count; ++style) {
            for (int enabled = 0; enabled <= 1; ++enabled) {
                g_customText.SetLanguage((GameLanguage)language);
                ControllerPromptManager::SetStyle(style);
                g_settings.subtitlesEnabled = enabled != 0;
                g_settings.controllerSpeakerEnabled = enabled != 0;
                g_settings.controllerSpeakerVolume = style * 33;
                if (!g_customText.GetString("FE_LPOR") || !g_customText.GetString("FE_SUB")
                    || !g_settings.Save(path.string().c_str())) return 2;
                g_customText.SetLanguage(LangEnglish);
                ControllerPromptManager::SetStyle(ControllerPromptStyle_Auto);
                g_settings.subtitlesEnabled = !enabled;
                g_settings.controllerSpeakerEnabled = !enabled;
                g_settings.controllerSpeakerVolume = -1;
                if (!g_settings.Load(path.string().c_str())
                    || g_customText.GetLanguage() != language
                    || ControllerPromptManager::GetPreference() != style
                    || g_settings.subtitlesEnabled != (enabled != 0)
                    || g_settings.controllerSpeakerEnabled != (enabled != 0)
                    || g_settings.controllerSpeakerVolume != style * 33) return 3;
            }
        }
    }
    g_customText.SetLanguage(LangPortugueseBrazil);
    const char* value = g_customText.GetString("FE_LPOR");
    if (!value || std::strcmp(value, "Portugu\xc3\xaas (Brasil)") != 0) return 4;
    g_customText.SetLanguage((GameLanguage)999);
    if (g_customText.GetLanguage() != LangEnglish) return 5;
    // An older settings file has no subtitle key or automatic-style value.
    FILE* legacy = std::fopen(path.string().c_str(), "wb");
    if (!legacy) return 6;
    std::fputs("[general]\nlanguage = 5\n[controls]\nprompt_style = 1\n", legacy);
    std::fclose(legacy);
    g_settings.subtitlesEnabled = false;
    if (!g_settings.Load(path.string().c_str()) || !g_settings.subtitlesEnabled
        || ControllerPromptManager::GetPreference() != ControllerPromptStyle_Switch) return 7;
    legacy = std::fopen(path.string().c_str(), "wb");
    if (!legacy) return 8;
    std::fputs("[general]\nlanguage = 5\n[controls]\nprompt_style = 0\n", legacy);
    std::fclose(legacy);
    if (!g_settings.Load(path.string().c_str())
        || ControllerPromptManager::GetPreference() != ControllerPromptStyle_Default) return 9;
    legacy = std::fopen(path.string().c_str(), "wb");
    if (!legacy) return 10;
    std::fclose(legacy);
    g_settings.Load(path.string().c_str());
    if (ControllerPromptManager::GetPreference() != ControllerPromptStyle_Auto
        || !g_settings.subtitlesEnabled || g_customText.GetLanguage() != LangEnglish
        || g_settings.controllerSpeakerEnabled || g_settings.controllerSpeakerVolume != 35) return 11;
    std::puts("PASS controller speaker settings: disabled/35 defaults, enable and independent-volume round trips");
    std::puts("PASS native catalog loading and settings round trips: 6 languages x 4 styles x 2 subtitle preferences");
    std::puts("PASS native subtitle language selection: 6 languages x 5 movies");
    g_customText.Shutdown();
    return 0;
}

static bool s_enabled = false;
static bool s_speakerEvents = false;
static bool s_speakerHotplug = false;
static bool s_speakerDisconnected = false;
static bool s_soundMenu = false;
static u32 s_primaryChannels = 0;
static bool s_primaryMusic = false;
static int s_speakerEvent = 0;
static s32 s_speakerNextFrame = 60;
static u64 s_speakerInitialCount = 0;
static s32 s_levelIndex = 0;
static s32 s_levelId = -1;
static s32 s_petalIndex = 0;
static s32 s_warpGenerator = -1;
static bool s_traceSpawns = false;
static s32 s_killInterval = 0;
static s32 s_frames = 300;
static s32 s_fps = 60;
static u32 s_heldButtons = 0;
static std::string s_screenshotPath;

static s32 s_playFrames = 0;
static bool s_captureRequested = false;
static bool s_captureDone = false;
static bool s_finished = false;
static int s_exitCode = 0;
static f64 s_startTime = 0.0;

static constexpr f64 kReachGameplayTimeoutSec = 180.0;

struct AutoTestButtonName {
    const char* name;
    u32 mask;
};

static const AutoTestButtonName kButtonNames[] = {
    { "up", PsxPad::Up },
    { "down", PsxPad::Down },
    { "left", PsxPad::Left },
    { "right", PsxPad::Right },
    { "cross", PsxPad::Cross },
    { "circle", PsxPad::Circle },
    { "square", PsxPad::Square },
    { "triangle", PsxPad::Triangle },
    { "l1", PsxPad::L1 },
    { "r1", PsxPad::R1 },
    { "l2", PsxPad::L2 },
    { "r2", PsxPad::R2 },
};

static u32 ParseButtons(const char* list) {
    u32 mask = 0;
    std::string token;
    for (const char* c = list;; ++c) {
        if (*c == ',' || *c == '\0') {
            for (const AutoTestButtonName& b : kButtonNames) {
                if (token == b.name) {
                    mask |= b.mask;
                }
            }
            token.clear();
            if (*c == '\0') {
                break;
            }
        }
        else {
            token += *c;
        }
    }
    return mask;
}

static void WarpToGenerator(s32 wanted) {
    Player* player = Player::s_player;
    if (!g_ai || !player) {
        return;
    }

    s32 index = 0;
    for (ccMinNode* n = g_ai->moveList.head; n; n = n->next) {
        Thing* thing = static_cast<Thing*>(n);
        if (thing->thingType != AITypes::TT_ENEMYGENERATOR) {
            continue;
        }
        if (index++ != wanted) {
            continue;
        }

        player->pos = thing->pos;
        player->homePos = thing->pos;
        player->UpdatePosition();
        if (g_blockManager) {
            const u16 block = g_blockManager->GetBlockNumber(thing->pos);
            player->blockNum = (block != 4096) ? block : thing->blockNum;
        }
        LOG_ALWAYS("[AutoTest] Warped to generator %d at (%d,%d,%d) block=%u", wanted,
                   thing->pos.x, thing->pos.y, thing->pos.z, (u32)player->blockNum);
        return;
    }
    LOG_ALWAYS("[AutoTest] Generator %d not found (%d in level)", wanted, index);
}

static void CaptureOverlay() {
    if (!s_captureRequested || s_captureDone) {
        return;
    }
    s_captureDone = true;

    std::vector<unsigned char> rgb;
    int width = 0;
    int height = 0;
    if (!p3d::display->ReadBackBuffer(rgb, width, height)
        || !stbi_write_png(s_screenshotPath.c_str(), width, height, 3, rgb.data(), width * 3)) {
        AutoTest_Fail(12, "screenshot failed");
        return;
    }
    LOG_ALWAYS("[AutoTest] Wrote %s (%dx%d)", s_screenshotPath.c_str(), width, height);
}

void AutoTest_ParseArgs(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const bool hasValue = (i + 1 < argc);
        if (std::strcmp(argv[i], "--autotest") == 0 && hasValue) {
            s_enabled = true;
            s_levelIndex = std::atoi(argv[++i]);
        }
        else if (std::strcmp(argv[i], "--level-id") == 0 && hasValue) {
            s_levelId = std::atoi(argv[++i]);
        }
        else if (std::strcmp(argv[i], "--warp-generator") == 0 && hasValue) {
            s_warpGenerator = std::atoi(argv[++i]);
        }
        else if (std::strcmp(argv[i], "--kill-interval") == 0 && hasValue) {
            s_killInterval = std::atoi(argv[++i]);
        }
        else if (std::strcmp(argv[i], "--trace-spawns") == 0) {
            s_traceSpawns = true;
        }
#if defined(RECHAN_VISUAL_TESTS)
        else if (std::strcmp(argv[i], "--speaker-events") == 0) {
            s_speakerEvents = true;
        }
        else if (std::strcmp(argv[i], "--speaker-hotplug") == 0) {
            s_speakerEvents = s_speakerHotplug = true;
        }
        else if (std::strcmp(argv[i], "--sound-menu") == 0) {
            s_soundMenu = true;
        }
#endif
        else if (std::strcmp(argv[i], "--petal") == 0 && hasValue) {
            s_petalIndex = std::atoi(argv[++i]);
        }
        else if (std::strcmp(argv[i], "--frames") == 0 && hasValue) {
            s_frames = std::atoi(argv[++i]);
        }
        else if (std::strcmp(argv[i], "--fps") == 0 && hasValue) {
            s_fps = std::atoi(argv[++i]);
        }
        else if (std::strcmp(argv[i], "--hold") == 0 && hasValue) {
            s_heldButtons = ParseButtons(argv[++i]);
        }
        else if (std::strcmp(argv[i], "--screenshot") == 0 && hasValue) {
            s_screenshotPath = argv[++i];
        }
    }

    if (s_fps < 1) {
        s_fps = 60;
    }

    if (s_enabled) {
        LOG_ALWAYS("[AutoTest] level=%d petal=%d frames=%d fps=%d hold=0x%04X screenshot=%s",
                   s_levelIndex, s_petalIndex, s_frames, s_fps, s_heldButtons,
                   s_screenshotPath.empty() ? "none" : s_screenshotPath.c_str());
    }
}

bool AutoTest_IsEnabled() {
    return s_enabled;
}

s32 AutoTest_GetLevelIndex() {
    if (s_levelId >= 0 && g_game && g_game->GetWorld()) {
        const s32 index = g_game->GetWorld()->LevelIDToIndex(s_levelId);
        if (index >= 0) {
            return index;
        }
    }
    return s_levelIndex;
}

bool AutoTest_TraceSpawns() {
    return s_enabled && s_traceSpawns;
}

s32 AutoTest_GetPetalIndex() {
    return s_petalIndex;
}

s32 AutoTest_GetFps() {
    return s_fps;
}

f32 AutoTest_GetFixedDeltaTime() {
    return 1.0f / (f32)s_fps;
}

u32 AutoTest_GetHeldButtons() {
    return (s_enabled && s_playFrames > 0) ? s_heldButtons : 0;
}

void AutoTest_Fail(int exitCode, const char* reason) {
    LOG_ALWAYS("[AutoTest] FAILED: %s", reason);
    s_exitCode = exitCode;
    s_finished = true;
}

bool AutoTest_Step(GameState state) {
    if (!s_enabled) {
        return true;
    }
    if (s_finished) {
        return false;
    }

    if (s_startTime == 0.0) {
        s_startTime = Time::GetTimeInSeconds();
        p3d::display->AddOverlayCallback(CaptureOverlay);
    }

    if (state != GameState::Play && !(s_soundMenu && state == GameState::Menu)) {
        if (s_playFrames == 0 && Time::GetTimeInSeconds() - s_startTime > kReachGameplayTimeoutSec) {
            AutoTest_Fail(11, "never reached gameplay");
            return false;
        }
        return true;
    }

    ++s_playFrames;
#if defined(RECHAN_VISUAL_TESTS)
    if (s_soundMenu && state == GameState::Play && s_playFrames >= 30 && g_feCustomMenuMgr) {
        LOG_ALWAYS("[SpeakerMenuTest] controlsEnabled=%d", (int)ControllerSpeaker::IsSupportedController());
        const auto previousLanguage = g_customText.GetLanguage();
        g_textManager->PushState();
        g_textManager->SetFontByName("Menu");
        g_textManager->SetScale(SCREEN_SCALE_Y(0.4f));
        g_textManager->SetWrapWidth(0);
        bool fits = true;
        const float left = SCALE_AND_CENTER_X(DEF_WINDOW_CENTER_X - DEF_WINDOW_W / 2 + DEF_LABEL_X_PAD);
        const float right = SCALE_AND_CENTER_X(DEF_WINDOW_CENTER_X + DEF_WINDOW_W / 2 - DEF_VALUE_X_PAD);
        const float sliderLeft = SCALE_AND_CENTER_X(DEF_WINDOW_CENTER_X + DEF_WINDOW_W / 2 - DEF_VALUE_X_PAD
            - DEF_SLIDER_CIRCLE_SEGMENTS * DEF_SLIDER_CIRCLE_STEP + (DEF_SLIDER_CIRCLE_STEP - 12) / 2);
        for (int language = 0; language < NumLanguages; ++language) {
            g_customText.SetLanguage((GameLanguage)language);
            const float toggleWidth = std::max(g_textManager->MeasureString(g_customText.GetText("FE_ON")).width,
                g_textManager->MeasureString(g_customText.GetText("FE_OFF")).width);
            const float labelWidth = g_textManager->MeasureString(g_customText.GetText("FE_CSPK")).width;
            const float volumeWidth = g_textManager->MeasureString(g_customText.GetText("FE_CSPKV")).width;
            LOG_ALWAYS("[SpeakerMenuTest] language=%d toggle=%.1f/%.1f volume=%.1f/%.1f", language,
                labelWidth, right-left-toggleWidth, volumeWidth, sliderLeft-left);
            fits &= labelWidth + 8 < right-left-toggleWidth && volumeWidth + 8 < sliderLeft-left;
        }
        g_customText.SetLanguage(previousLanguage);
        g_textManager->PopState();
        if (!fits) { AutoTest_Fail(20, "controller speaker label overlaps its value"); return false; }
        g_feCustomMenuMgr->Activate(MenuPage_Sound);
        g_game->SetState(GameState::Menu);
    }
    if (s_speakerEvents) {
        g_settings.controllerSpeakerEnabled = true;
        g_settings.controllerSpeakerVolume = 100;
        if (Time::GetTimeInSeconds() - s_startTime > 90) {
            AutoTest_Fail(17, "speaker event test did not reach a compatible endpoint");
            return false;
        }
        if (!ControllerSpeaker::IsReady()) {
            if (s_speakerHotplug && s_speakerEvent == 2 && !s_speakerDisconnected) {
                s_speakerDisconnected = true;
                LOG_ALWAYS("[SpeakerTest] Disconnection observed; waiting for USB reconnection");
            }
            return true;
        }
        if (s_speakerHotplug && s_speakerEvent == 2 && !s_speakerDisconnected) return true;
        if (s_playFrames < s_speakerNextFrame) return true;
        if (s_speakerEvent == 0) {
            s_speakerInitialCount = ControllerSpeaker::AcceptedEffects();
            s_primaryChannels = AudioEngine::GetOutputChannels();
            s_primaryMusic = AudioEngine::IsMusicPlaying();
        }
        if (s_speakerEvent < 8) {
            const u16 id = 251 + s_speakerEvent++;
            const u64 before = ControllerSpeaker::AcceptedEffects();
            s32 result = 0;
            if (id <= 253 && g_frontEndSound) {
                g_frontEndSound->ProcessSoundEvent(id - 228);
            }
            else {
                LVector position = Player::s_player ? Player::s_player->pos : LVector{};
                if (id == 254) position.x += 1000000;
                result = CSoundDirect::PlayTransient(id, id <= 257 ? &position : nullptr, 0, 0);
            }
            const u64 copied = ControllerSpeaker::AcceptedEffects() - before;
            LOG_ALWAYS("[SpeakerTest] transient=%u result=%d copied=%llu", (unsigned int)id, result, (unsigned long long)copied);
            if (copied != (id <= 257 ? 1 : 0)) {
                AutoTest_Fail(18, "speaker event was missing, duplicated or outside the allowlist");
                return false;
            }
            s_speakerNextFrame = s_playFrames + 120;
            return true;
        }
        if (ControllerSpeaker::AcceptedEffects() != s_speakerInitialCount + 7) {
            AutoTest_Fail(18, "unexpected extra speaker events");
            return false;
        }
        if (!AudioEngine::IsInitialized() || AudioEngine::GetOutputChannels() != s_primaryChannels
            || (s_primaryMusic && !AudioEngine::IsMusicPlaying())) {
            AutoTest_Fail(19, "primary audio state changed during controller playback");
            return false;
        }
        LOG_ALWAYS("[SpeakerTest] PASS seven selected events copied once, including extra life; unselected event stayed on primary output");
        LOG_ALWAYS("[SpeakerTest] PASS primary device channel count and music state preserved");
        if (s_speakerHotplug) LOG_ALWAYS("[SpeakerTest] PASS disconnect/reconnect recovered the matched speaker endpoint");
        s_finished = true;
        return false;
    }
#endif
    if (s_playFrames == 30 && s_warpGenerator >= 0) {
        WarpToGenerator(s_warpGenerator);
    }
    if (s_killInterval > 0 && s_playFrames > 30 && (s_playFrames % s_killInterval) == 0 && g_ai) {
        for (ccMinNode* n = g_ai->humanoidList.head; n; n = n->next) {
            Thing* thing = static_cast<Thing*>(n);
            if (thing != Player::s_player && (thing->flags & TF_ACTIVATED) && !(thing->flags & TF_DEAD)) {
                LOG_ALWAYS("[AutoTest] Killing enemy %p", (void*)thing);
                thing->flags |= TF_DEAD;
                break;
            }
        }
    }
    if (s_playFrames < s_frames) {
        return true;
    }

    if (!s_screenshotPath.empty() && !s_captureDone) {
        s_captureRequested = true;
        return true;
    }

    LOG_ALWAYS("[AutoTest] Finished after %d gameplay frames", s_playFrames);
    s_finished = true;
    return false;
}

int AutoTest_GetExitCode() {
    return s_exitCode;
}
