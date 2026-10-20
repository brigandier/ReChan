#pragma once
#include "core.h"
#include "pddi/gamepadfamily.h"
#include <cstdio>

// Which controller-brand glyph set to use for on-screen button prompts
enum ControllerPromptStyle : s32 {
    ControllerPromptStyle_Default = 0, // Xbox-style glyphs
    ControllerPromptStyle_Switch,
    ControllerPromptStyle_PlayStation,
    ControllerPromptStyle_Auto,
    ControllerPromptStyle_Count,
};

struct ControllerPromptStyleInfo {
    const char* fileSuffix;    // controller_overlay_<suffix>.png / controller_sheet_<suffix>.png
    const char* displayToken;  // localization token shown in the menu list
};

static constexpr ControllerPromptStyleInfo kControllerPromptStyles[ControllerPromptStyle_Count] = {
    { "default", "FE_DEFX" },
    { "switch",  "FE_NX" },
    { "playstation", "FE_PS" },
    { "default", "FE_AUT" },
};

static constexpr ControllerPromptStyle kDefaultControllerPromptStyle = ControllerPromptStyle_Auto;

static constexpr ControllerPromptStyle kControllerPromptMenuStyles[] = {
    ControllerPromptStyle_Auto, ControllerPromptStyle_Default,
    ControllerPromptStyle_PlayStation, ControllerPromptStyle_Switch,
};

static inline s32 ClampControllerPromptStyle(s32 style) {
    if (style < 0 || style >= (s32)ControllerPromptStyle_Count) {
        return (s32)kDefaultControllerPromptStyle;
    }
    return style;
}

class ControllerPromptManager {
public:
    static s32 GetPreference() { return s_style; }
    static s32 GetStyle() {
        if (s_style != ControllerPromptStyle_Auto) return s_style;
        switch (s_family) {
            case GamepadFamily::Xbox: return ControllerPromptStyle_Default;
            case GamepadFamily::PlayStation: return ControllerPromptStyle_PlayStation;
            case GamepadFamily::Switch: return ControllerPromptStyle_Switch;
            default:
#if defined(RC_PLATFORM_SWITCH)
                return ControllerPromptStyle_Switch;
#else
                return ControllerPromptStyle_Default;
#endif
        }
    }

    static s32 GetMenuIndex() {
        for (s32 i = 0; i < ControllerPromptStyle_Count; ++i)
            if (kControllerPromptMenuStyles[i] == s_style) return i;
        return 0;
    }

    // Returns true if the style actually changed.
    static bool SetStyle(s32 style) {
        const s32 oldStyle = GetStyle();
        s_style = ClampControllerPromptStyle(style);
        return oldStyle != GetStyle();
    }

    static bool SetDetectedFamily(GamepadFamily family) {
        const s32 oldStyle = GetStyle();
        s_family = family;
        return oldStyle != GetStyle();
    }

    static void BuildOverlayPath(char* outPath, s32 outPathLen) {
        std::snprintf(outPath, (size_t)outPathLen, "pc/textures/frontend/controller_overlay_%s.png",
                      kControllerPromptStyles[GetStyle()].fileSuffix);
    }

    static void BuildSheetPath(char* outPath, s32 outPathLen) {
        std::snprintf(outPath, (size_t)outPathLen, "pc/textures/frontend/controller_sheet_%s.png",
                      kControllerPromptStyles[GetStyle()].fileSuffix);
    }

private:
    static inline s32 s_style = (s32)kDefaultControllerPromptStyle;
    static inline GamepadFamily s_family = GamepadFamily::Unknown;
};
