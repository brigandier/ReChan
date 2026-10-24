#include "pc/autotest.h"
#include "pc/inputaction.h"
#include "pc/settings.h"
#include "pc/audio.h"
#include "pc/textmgr.h"
#include "extra/customtext.h"
#include "extra/prompticons.h"
#include "radmovie/subtitles.h"
#include "snd/sound.h"
#include "xclib/xclib.h"
#include "gen/game.h"
#include "extra/fecustommenumgr.h"
#include "p3d/context.h"
#include "pddi/gl/glrender.h"
#include "vendor/glfw/include/GLFW/glfw3.h"
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>

namespace {
int failures = 0;
void Check(bool condition, const char* name) {
    std::printf("%s runtime: %s\n", condition ? "PASS" : "FAIL", name);
    failures += !condition;
}
void Write(const std::filesystem::path& path, const char* text) {
    std::ofstream file(path, std::ios::binary);
    file << text;
    if (!file) throw std::runtime_error("Cannot write runtime fixture");
}
}

int AutoTest_VerifyRuntime(const char* outputDirectory) {
    failures = 0;
    const auto originalDirectory = std::filesystem::current_path();
    const auto output = std::filesystem::absolute(outputDirectory);
    std::filesystem::create_directories(output / "pc/text");
    std::filesystem::remove(output / "pc/text/french.txt");
    ActionInput actions;
    Check(xcHash("FE_SUB") == 0xa85f3b14, "validator hash vector matches xcHash");
    for (double delta : {0.0, 1.0, -1.0, 0.25, -0.25}) {
        actions.AddVerticalScroll(delta);
        Check(actions.ConsumeScrollDelta() == (delta > 0 ? 1 : delta < 0 ? -1 : 0),
            "vertical scroll sign and zero are preserved");
    }
    for (s32 code : {std::numeric_limits<s32>::min(), -999, -4, 1, 270, 1000000, std::numeric_limits<s32>::max()}) {
        actions.SetDesktopBindingCode(ACTION_JUMP, 0, KEY_A);
        actions.SetDesktopBindingCode(ACTION_JUMP, 0, code);
        Check(actions.GetDesktopBindingCode(ACTION_JUMP, 0) == KEY_A, "invalid desktop code preserves valid binding");
    }
    actions.SetKeyBindingSlot(ACTION_JUMP, 0, KEY_B);
    actions.SetKeyBindingSlot(ACTION_JUMP, 0, std::numeric_limits<s32>::max());
    Check(actions.GetKeyBindingSlot(ACTION_JUMP, 0) == KEY_B, "invalid key setter preserves binding");
    actions.SetMouseButtonBindingSlot(ACTION_JUMP, 0, MouseBtn::Right);
    actions.SetMouseButtonBindingSlot(ACTION_JUMP, 0, 99);
    Check(actions.GetMouseButtonBindingSlot(ACTION_JUMP, 0) == MouseBtn::Right, "invalid mouse setter preserves binding");
    for (s32 code : {0, -1, -2, -3, (s32)KEY_A, (s32)KEY_KP_9, (s32)KEY_RIGHT_ALT}) {
        actions.SetDesktopBindingCode(ACTION_JUMP, 0, code);
        Check(actions.GetDesktopBindingCode(ACTION_JUMP, 0) == code, "valid and unbound desktop codes round trip");
    }
    bool bounded = true;
    for (s32 code : {(s32)KEY_A, (s32)KEY_0, (s32)KEY_F12, (s32)KEY_ENTER, -1, 0}) {
        actions.SetDesktopBindingCode(ACTION_JUMP, 0, code);
        for (s32 capacity : {0, 1, 2, 16}) {
            char label[20];
            std::memset(label, '@', sizeof(label));
            actions.GetDesktopBindingLabel(ACTION_JUMP, 0, label, capacity);
            bounded &= label[capacity] == '@';
            if (capacity) bounded &= std::memchr(label, '\0', capacity) != nullptr;
        }
    }
    Check(bounded, "key labels respect capacities 0, 1, 2 and 16");
    for (s32 key = 0; key <= 348; ++key) {
        if (!ActionInput::IsValidKeyBinding(key)) continue;
        actions.SetKeyBindingSlot(ACTION_JUMP, 0, key);
        for (s32 capacity : {1, 2, 16}) {
            auto label = std::make_unique<char[]>(capacity);
            actions.GetDesktopBindingLabel(ACTION_JUMP, 0, label.get(), capacity);
            bounded &= std::memchr(label.get(), '\0', capacity) != nullptr;
        }
    }
    actions.GetDesktopBindingLabel(ACTION_JUMP, 0, nullptr, 1);
    Check(bounded, "all supported key labels fit exact heap buffer capacities");
    s32 consumed = 77, code = 88;
    for (const char* token : {"<BND:99999999999999999999>", "<BND:-2147483648>", "<BND:2147483648>",
             "<BND:270>", "<BND:-4>", "<BND: >", "<BND:65x>"}) {
        Check(!PromptIcons::ParseInlineDesktopBindingToken(token, 0, &consumed, &code), "invalid inline binding rejected");
    }
    Check(PromptIcons::ParseInlineDesktopBindingToken("<BND:-3>", 0, &consumed, &code) && code == -3,
        "valid mouse inline binding preserved");
    PromptIcons::ResolvedPrompt prompt;
    Check(!PromptIcons::ResolveDesktopBindingCodePrompt(std::numeric_limits<s32>::min(), prompt),
        "invalid prompt rejected before mouse-code negation");

    Sound sound;
    g_sound = &sound;
    auto* savedActions = g_actionInput;
    g_actionInput = &actions;
    const auto ini = output / "runtime-bindings.ini";
    Write(ini, "[keybinds]\njump_1=65\npunch_1=66\njump_1=-2147483648\npunch_1=999999999999999999999\npunch_1=false\n"
        "kick_-2147483648=67\nkick_2147483648=68\ngrab_1=270\nstrafe_1=65x\n"
        "[general]\nsubtitles=off\n");
    Check(g_settings.Load(ini.string().c_str()) && actions.GetDesktopBindingCode(ACTION_JUMP, 0) == KEY_A
        && actions.GetDesktopBindingCode(ACTION_PUNCH, 0) == KEY_B && !g_settings.subtitlesEnabled,
        "INI rejects invalid duplicate bindings without losing valid entries");
    g_actionInput = savedActions;

    Write(output / "pc/text/english.txt", "[FE_SUB]\nSubtitles\n[FE_LPOR]\nPortuguese\n");
    std::filesystem::current_path(output);
    CustomText text;
    text.Init();
    const char* retained = text.GetString("FE_SUB");
    const auto revision = text.GetLocalizationID();
    text.SetLanguage(LangFrench);
    Check(text.GetLanguage() == LangEnglish && text.GetLocalizationID() == revision
        && text.GetString("FE_SUB") == retained, "missing language preserves previous catalog and views");
    Write(output / "pc/text/french.txt", "");
    text.SetLanguage(LangFrench);
    Check(text.GetLanguage() == LangEnglish, "empty language rejected");
    Write(output / "pc/text/french.txt", "[FE_SUB]\nSous-titres\n");
    text.SetLanguage(LangFrench);
    Check(text.GetLanguage() == LangFrench && text.GetString("FE_LPOR")
        && std::strcmp(text.GetString("FE_LPOR"), "Portuguese") == 0, "incomplete catalog uses English for missing tokens");
    std::filesystem::create_directory(output / "pc/text/german.txt");
    const char* french = text.GetString("FE_SUB");
    text.SetLanguage(LangGerman);
    Check(text.GetLanguage() == LangFrench && text.GetString("FE_SUB") == french,
        "unreadable catalog path preserves selected language and view");
    std::filesystem::remove(output / "pc/text/german.txt");
    std::filesystem::rename(output / "pc/text/english.txt", output / "pc/text/english-backup.txt");
    text.Init();
    Check(text.GetLanguage() == LangFrench && text.GetString("FE_SUB") == french,
        "failed fallback reload preserves complete previous catalog");
    std::filesystem::rename(output / "pc/text/english-backup.txt", output / "pc/text/english.txt");
    text.Shutdown();
    text.Init();
    Check(text.GetLanguage() == LangFrench && text.GetString("FE_SUB"), "shutdown and reload preserve language consistency");
    std::filesystem::current_path(originalDirectory);
    MovieSubtitles subtitles;
    Check(subtitles.Parse("1\n00:00:01,000 --> 00:00:02,000\nValid\n\n2\n00:00:02,000 --> 00:00:03,000\nA\nB\nC\n")
        && subtitles.Entries().size() == 1, "malformed multi-line cue skipped without losing valid cue");
    Check(!subtitles.Parse(" \n00:00:01,000 --> 00:00:02,000\nMissing index\n"), "missing SRT index rejected");
    return failures ? 20 : 0;
}

void AutoTest_PrepareExitScenario(Game& game, const char* scenario) {
#if !defined(RC_PLATFORM_NULL)
    if (std::strcmp(scenario, "movie") == 0) {
        game.PlayMovie("prolog.str", 1, 0);
    }
    else {
        game.SetState(GameState::Title);
        game.Step();
        if (std::strcmp(scenario, "menu") == 0 && g_feCustomMenuMgr) {
            g_feCustomMenuMgr->Activate(MenuPage_Sound);
            auto* window = static_cast<glDisplay*>(p3d::display)->GetWindow();
            auto scroll = glfwSetScrollCallback(window, nullptr);
            glfwSetScrollCallback(window, scroll);
            const double offsets[][2] = {{1,0}, {-1,0}, {0,0}, {0,1}, {0,-1}, {1,0.5}, {-1,-0.5}};
            for (const auto& offset : offsets) {
                g_actionInput->ConsumeScrollDelta();
                if (scroll) scroll(window, offset[0], offset[1]);
                const auto expected = (offset[1] > 0) - (offset[1] < 0);
                if (!scroll || g_actionInput->ConsumeScrollDelta() != expected)
                    AutoTest_Fail(21, "window scroll callback changed vertical direction");
            }
            PromptIcons::ResolvedPrompt prompt;
            if (!PromptIcons::ResolveDesktopBindingCodePrompt(KEY_A, prompt) || !prompt.tex)
                AutoTest_Fail(21, "keyboard prompt texture did not load");
            else {
                auto* retained = prompt.tex;
                retained->AddRef();
                const int references = retained->GetRefCount();
                PromptIcons::Shutdown();
                PromptIcons::Shutdown();
                if (retained->GetRefCount() != references - 1)
                    AutoTest_Fail(21, "prompt cache did not release its texture exactly once");
                retained->Release();
            }
            LOG_ALWAYS("[RuntimeExitTest] window scroll and prompt cache release checked");
            g_feCustomMenuMgr->Activate(MenuPage_Quitting);
            g_feCustomMenuMgr->Invoke();
            if (game.GetState() != GameState::End) AutoTest_Fail(21, "menu quit did not reach End");
        }
    }
    LOG_ALWAYS("[RuntimeExitTest] scenario=%s, liveContext=%d", scenario, p3d::context != nullptr);
    if (auto* display = dynamic_cast<glDisplay*>(p3d::display))
        glfwSetWindowShouldClose(display->GetWindow(), GLFW_TRUE);
#else
    AutoTest_Fail(21, "window exit scenarios require a graphics backend");
#endif
}

void AutoTest_CheckShutdown(bool platformDestroyed) {
    bool ok = !g_game && !g_textManager && !AudioEngine::IsInitialized() && !g_actionInput;
    if (platformDestroyed) ok &= !p3d::context && !p3d::display && !p3d::input;
    else {
        ok &= p3d::context && p3d::display;
#if !defined(RC_PLATFORM_NULL)
        ok &= glfwGetCurrentContext() != nullptr;
#endif
    }
    if (!ok) AutoTest_Fail(21, "resource destruction order violated");
    LOG_ALWAYS("[RuntimeExitTest] %s platform destruction: %s", platformDestroyed ? "after" : "before", ok ? "PASS" : "FAIL");
}
