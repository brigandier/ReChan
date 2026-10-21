#include "pc/autotest.h"
#include "pc/settings.h"
#include "extra/customtext.h"
#include "extra/controllerpromptstyle.h"
#include "snd/sound.h"
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

int AutoTest_VerifyLocalization(const char* outputDirectory) {
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
                if (!g_customText.GetString("FE_LPOR") || !g_customText.GetString("FE_SUB")
                    || !g_settings.Save(path.string().c_str())) return 2;
                g_customText.SetLanguage(LangEnglish);
                ControllerPromptManager::SetStyle(ControllerPromptStyle_Auto);
                g_settings.subtitlesEnabled = !enabled;
                if (!g_settings.Load(path.string().c_str())
                    || g_customText.GetLanguage() != language
                    || ControllerPromptManager::GetPreference() != style
                    || g_settings.subtitlesEnabled != (enabled != 0)) return 3;
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
        || !g_settings.subtitlesEnabled || g_customText.GetLanguage() != LangEnglish) return 11;
    std::puts("PASS native catalog loading and settings round trips: 6 languages x 4 styles x 2 subtitle preferences");
    std::puts("PASS native subtitle language selection: 6 languages x 5 movies");
    g_customText.Shutdown();
    return 0;
}

static bool s_enabled = false;
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

    if (state != GameState::Play) {
        if (s_playFrames == 0 && Time::GetTimeInSeconds() - s_startTime > kReachGameplayTimeoutSec) {
            AutoTest_Fail(11, "never reached gameplay");
            return false;
        }
        return true;
    }

    ++s_playFrames;
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
