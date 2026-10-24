#include "pddi/gl/glgamepadrumble_sdl.h"
#include "pddi/gamepadaudioreport.h"

#include <cstring>
#include <array>
#include <vector>
#include <algorithm>
#include <cmath>

#if defined(P3D_USE_VENDORED_SDL2)
#include "SDL.h"
#endif

static char LowerAscii(char c) {
    if (c >= 'A' && c <= 'Z') {
        return (char)(c + ('a' - 'A'));
    }
    return c;
}

static bool GuidEquals(const char* a, const char* b) {
    if (!a || !b) {
        return false;
    }

    for (int i = 0; i < 32; i++) {
        if (a[i] == '\0' || b[i] == '\0') {
            return false;
        }
        if (LowerAscii(a[i]) != LowerAscii(b[i])) {
            return false;
        }
    }

    return a[32] == '\0' && b[32] == '\0';
}

static void CopyGuid(char* dst, int dstSize, const char* src) {
    if (!dst || dstSize <= 0) {
        return;
    }

    dst[0] = '\0';
    if (!src || src[0] == '\0') {
        return;
    }

    std::strncpy(dst, src, (size_t)(dstSize - 1));
    dst[dstSize - 1] = '\0';
}

#if defined(P3D_USE_VENDORED_SDL2)

static SDL_GameController* g_activeController = nullptr;
static SDL_Joystick* g_activeJoystick = nullptr;
static bool g_activeJoystickOpenedDirect = false;
static bool g_activeControllerFromPoll = false;
struct PolledController {
    SDL_GameController* controller = nullptr;
    std::array<Uint8, SDL_CONTROLLER_BUTTON_MAX> buttons = {};
    std::array<Sint16, SDL_CONTROLLER_AXIS_MAX> axes = {};
};
static std::vector<PolledController> g_polledControllers;
static char g_activeGuid[64] = {};
static bool g_sdlMainReady = false;
static Uint32 g_ownedSubsystems = 0;
static bool g_ledAttempted = false;
static bool g_ledSucceeded = false;
static bool g_ledOwned = false;
static Uint8 g_ledRed = 0, g_ledGreen = 0, g_ledBlue = 0;
static Uint32 g_ledLastAttempt = 0;
static bool g_speakerOwned = false;
static Uint16 g_lastLow = 0, g_lastHigh = 0;
static Uint32 g_lastRumbleTime = 0;

static void SetPlayerIndicator(SDL_GameController* controller, int playerIndex) {
#if SDL_VERSION_ATLEAST(2, 0, 12)
    if (controller && SDL_GameControllerGetAttached(controller)
        && SDL_GameControllerGetType(controller) == SDL_CONTROLLER_TYPE_PS5
        && SDL_GameControllerGetPlayerIndex(controller) != playerIndex) {
        SDL_GameControllerSetPlayerIndex(controller, playerIndex);
    }
#endif
}

static void UpdatePlayerIndicators() {
    for (auto& device : g_polledControllers) {
        if (device.controller != g_activeController) SetPlayerIndicator(device.controller, -1);
    }
    SetPlayerIndicator(g_activeController, 0);
}

static void CloseActiveController() {
    if (g_speakerOwned) glGamepadSetSpeakerEnabled(false);
    g_speakerOwned = false;
    g_lastLow = g_lastHigh = 0;
    SetPlayerIndicator(g_activeController, -1);
#if SDL_VERSION_ATLEAST(2, 0, 14)
    if (g_ledOwned && g_activeJoystick && SDL_JoystickGetAttached(g_activeJoystick)) {
        SDL_JoystickSetLED(g_activeJoystick, 0, 64, 255);
    }
#endif
    g_ledAttempted = g_ledSucceeded = g_ledOwned = false;
    if (g_activeController) {
        SDL_GameControllerRumble(g_activeController, 0, 0, 0);
        if (!g_activeControllerFromPoll) SDL_GameControllerClose(g_activeController);
    }
    else if (g_activeJoystickOpenedDirect && g_activeJoystick) {
        SDL_JoystickRumble(g_activeJoystick, 0, 0, 0);
        SDL_JoystickClose(g_activeJoystick);
    }

    g_activeController = nullptr;
    g_activeControllerFromPoll = false;
    g_activeJoystick = nullptr;
    g_activeJoystickOpenedDirect = false;
    g_activeGuid[0] = '\0';
}

static void GetJoystickGuidString(SDL_Joystick* joystick, char* outGuid, int outGuidSize) {
    if (!outGuid || outGuidSize <= 0) {
        return;
    }

    outGuid[0] = '\0';
    if (!joystick) {
        return;
    }

    const SDL_JoystickGUID guid = SDL_JoystickGetGUID(joystick);
    SDL_JoystickGetGUIDString(guid, outGuid, outGuidSize);
}

void glGamepadShutdown() {
    CloseActiveController();
    for (auto& device : g_polledControllers) SDL_GameControllerClose(device.controller);
    g_polledControllers.clear();
    if (g_ownedSubsystems & SDL_INIT_GAMECONTROLLER) SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    if (g_ownedSubsystems & SDL_INIT_JOYSTICK) SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
    g_ownedSubsystems = 0;
}

static bool BindControllerAtIndex(int sdlIndex, const char* expectedGuid, bool requireGuidMatch) {
    if (sdlIndex < 0 || sdlIndex >= SDL_NumJoysticks()) {
        return false;
    }

    if (!SDL_IsGameController(sdlIndex)) {
        return false;
    }

    SDL_GameController* controller = SDL_GameControllerOpen(sdlIndex);
    if (!controller) {
        return false;
    }

    SDL_Joystick* joystick = SDL_GameControllerGetJoystick(controller);
    if (!joystick) {
        SDL_GameControllerClose(controller);
        return false;
    }

    char sdlGuidString[64] = {};
    GetJoystickGuidString(joystick, sdlGuidString, (int)sizeof(sdlGuidString));

    if (requireGuidMatch && expectedGuid && expectedGuid[0] != '\0' && !GuidEquals(sdlGuidString, expectedGuid)) {
        SDL_GameControllerClose(controller);
        return false;
    }

    g_activeController = controller;
    g_activeJoystick = joystick;
    g_activeJoystickOpenedDirect = false;
    if (expectedGuid && expectedGuid[0] != '\0') {
        CopyGuid(g_activeGuid, (int)sizeof(g_activeGuid), expectedGuid);
    }
    else {
        CopyGuid(g_activeGuid, (int)sizeof(g_activeGuid), sdlGuidString);
    }
    UpdatePlayerIndicators();
    return true;
}

static bool BindJoystickAtIndex(int sdlIndex, const char* expectedGuid, bool requireGuidMatch) {
    if (sdlIndex < 0 || sdlIndex >= SDL_NumJoysticks()) {
        return false;
    }

    SDL_Joystick* joystick = SDL_JoystickOpen(sdlIndex);
    if (!joystick) {
        return false;
    }

    char sdlGuidString[64] = {};
    GetJoystickGuidString(joystick, sdlGuidString, (int)sizeof(sdlGuidString));

    if (requireGuidMatch && expectedGuid && expectedGuid[0] != '\0' && !GuidEquals(sdlGuidString, expectedGuid)) {
        SDL_JoystickClose(joystick);
        return false;
    }

    g_activeController = nullptr;
    g_activeJoystick = joystick;
    g_activeJoystickOpenedDirect = true;
    if (expectedGuid && expectedGuid[0] != '\0') {
        CopyGuid(g_activeGuid, (int)sizeof(g_activeGuid), expectedGuid);
    }
    else {
        CopyGuid(g_activeGuid, (int)sizeof(g_activeGuid), sdlGuidString);
    }
    return true;
}

static bool EnsureSDLGameControllerSubsystem() {
    if (!g_sdlMainReady) {
        SDL_SetMainReady();
#ifdef SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
#endif
#ifdef SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");
#endif
#ifdef SDL_HINT_JOYSTICK_HIDAPI_PS5_PLAYER_LED
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_PLAYER_LED, "1");
#endif
        g_sdlMainReady = true;
    }

    if ((SDL_WasInit(SDL_INIT_JOYSTICK) & SDL_INIT_JOYSTICK) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) != 0) {
            return false;
        }
        g_ownedSubsystems |= SDL_INIT_JOYSTICK;
    }

    if ((SDL_WasInit(SDL_INIT_GAMECONTROLLER) & SDL_INIT_GAMECONTROLLER) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) == 0) g_ownedSubsystems |= SDL_INIT_GAMECONTROLLER;
    }

    return true;
}

static bool ActiveBindingCanRumble() {
    if (!g_activeController && !g_activeJoystick) {
        return false;
    }

#if SDL_VERSION_ATLEAST(2, 0, 18)
    return (g_activeController && SDL_GameControllerHasRumble(g_activeController) == SDL_TRUE)
        || (g_activeJoystick && SDL_JoystickHasRumble(g_activeJoystick) == SDL_TRUE);
#else
    if (g_activeController && SDL_GameControllerRumble(g_activeController, 0, 0, 0) == 0) {
        return true;
    }

    return g_activeJoystick && SDL_JoystickRumble(g_activeJoystick, 0, 0, 0) == 0;
#endif
}

static float NormalizeSignedAxis(Sint16 value) {
    if (value >= 0) {
        return (float)value / 32767.0f;
    }
    return (float)value / 32768.0f;
}

static float NormalizeTriggerAxis(Sint16 value) {
    float normalized = (float)value / 32767.0f;
    if (normalized < 0.0f) {
        normalized = 0.0f;
    }
    else if (normalized > 1.0f) {
        normalized = 1.0f;
    }

    return normalized * 2.0f - 1.0f;
}

bool glGamepadPollState(bool outButtons[15], float outAxes[6]) {
    if (outButtons) {
        std::memset(outButtons, 0, sizeof(bool) * 15);
    }
    if (outAxes) {
        std::memset(outAxes, 0, sizeof(float) * 6);
    }

    if (!EnsureSDLGameControllerSubsystem()) {
        return false;
    }

    SDL_GameControllerUpdate();

    for (auto it = g_polledControllers.begin(); it != g_polledControllers.end();) {
        if (SDL_GameControllerGetAttached(it->controller) != SDL_TRUE) {
            if (g_activeController == it->controller) CloseActiveController();
            SDL_GameControllerClose(it->controller);
            it = g_polledControllers.erase(it);
        }
        else ++it;
    }
    if (g_activeController && SDL_GameControllerGetAttached(g_activeController) != SDL_TRUE) CloseActiveController();

    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (!SDL_IsGameController(i)) continue;
        const SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(i);
        const bool known = std::any_of(g_polledControllers.begin(), g_polledControllers.end(), [id](const auto& c) {
            return SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(c.controller)) == id;
        });
        if (!known) {
            if (auto* controller = SDL_GameControllerOpen(i)) g_polledControllers.push_back({controller});
        }
    }

    SDL_GameController* selected = g_activeController;
    for (auto& device : g_polledControllers) {
        bool activity = false;
        for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; ++b) {
            const Uint8 value = SDL_GameControllerGetButton(device.controller, (SDL_GameControllerButton)b);
            activity |= value && !device.buttons[b];
            device.buttons[b] = value;
        }
        for (int a = 0; a < SDL_CONTROLLER_AXIS_MAX; ++a) {
            const Sint16 value = SDL_GameControllerGetAxis(device.controller, (SDL_GameControllerAxis)a);
            // Ignore resting triggers and stick drift when choosing the active device.
            activity |= (a < SDL_CONTROLLER_AXIS_TRIGGERLEFT ? std::abs((int)value) > 8000 : value > 8000)
                && std::abs((int)value - device.axes[a]) > 4000;
            device.axes[a] = value;
        }
        if (!selected || activity) selected = device.controller;
    }
    if (selected && (selected != g_activeController || !g_activeControllerFromPoll)) {
        CloseActiveController();
        g_activeController = selected;
        g_activeControllerFromPoll = true;
        g_activeJoystick = SDL_GameControllerGetJoystick(selected);
        GetJoystickGuidString(g_activeJoystick, g_activeGuid, (int)sizeof(g_activeGuid));
    }
    UpdatePlayerIndicators();

    if (!g_activeController) {
        return false;
    }

    static const SDL_GameControllerButton kButtonMap[15] = {
        SDL_CONTROLLER_BUTTON_A,
        SDL_CONTROLLER_BUTTON_B,
        SDL_CONTROLLER_BUTTON_X,
        SDL_CONTROLLER_BUTTON_Y,
        SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
        SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
        SDL_CONTROLLER_BUTTON_BACK,
        SDL_CONTROLLER_BUTTON_START,
        SDL_CONTROLLER_BUTTON_GUIDE,
        SDL_CONTROLLER_BUTTON_LEFTSTICK,
        SDL_CONTROLLER_BUTTON_RIGHTSTICK,
        SDL_CONTROLLER_BUTTON_DPAD_UP,
        SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
        SDL_CONTROLLER_BUTTON_DPAD_DOWN,
        SDL_CONTROLLER_BUTTON_DPAD_LEFT,
    };

    if (outButtons) {
        for (int i = 0; i < 15; i++) {
            outButtons[i] = SDL_GameControllerGetButton(g_activeController, kButtonMap[i]) == SDL_PRESSED;
        }
    }

    if (outAxes) {
        outAxes[0] = NormalizeSignedAxis(SDL_GameControllerGetAxis(g_activeController, SDL_CONTROLLER_AXIS_LEFTX));
        outAxes[1] = NormalizeSignedAxis(SDL_GameControllerGetAxis(g_activeController, SDL_CONTROLLER_AXIS_LEFTY));
        outAxes[2] = NormalizeSignedAxis(SDL_GameControllerGetAxis(g_activeController, SDL_CONTROLLER_AXIS_RIGHTX));
        outAxes[3] = NormalizeSignedAxis(SDL_GameControllerGetAxis(g_activeController, SDL_CONTROLLER_AXIS_RIGHTY));
        outAxes[4] = NormalizeTriggerAxis(SDL_GameControllerGetAxis(g_activeController, SDL_CONTROLLER_AXIS_TRIGGERLEFT));
        outAxes[5] = NormalizeTriggerAxis(SDL_GameControllerGetAxis(g_activeController, SDL_CONTROLLER_AXIS_TRIGGERRIGHT));
    }

    return true;
}

GamepadFamily glGamepadGetFamily() {
    if (!g_activeController || SDL_GameControllerGetAttached(g_activeController) != SDL_TRUE) return GamepadFamily::Unknown;
    switch (SDL_GameControllerGetType(g_activeController)) {
        case SDL_CONTROLLER_TYPE_PS3:
        case SDL_CONTROLLER_TYPE_PS4:
        case SDL_CONTROLLER_TYPE_PS5: return GamepadFamily::PlayStation;
        case SDL_CONTROLLER_TYPE_XBOX360:
        case SDL_CONTROLLER_TYPE_XBOXONE: return GamepadFamily::Xbox;
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO:
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR: return GamepadFamily::Switch;
        default: return GamepadFamily::Unknown;
    }
}

bool glGamepadRumbleUpdateActive(int glfwGamepadIndex, const char* glfwGuid) {
    // GLFW and SDL enumerate independently; neither an index nor a model GUID
    // distinguishes identical devices. Only a unique match is safe for feedback.
    (void)glfwGamepadIndex;
    const bool hasGuid = glfwGuid && glfwGuid[0] != '\0';
    if (!EnsureSDLGameControllerSubsystem()) {
        CloseActiveController();
        return false;
    }

    SDL_GameControllerUpdate();

    const int joystickCount = SDL_NumJoysticks();
    int match = -1;
    for (int i = 0; i < joystickCount; i++) {
        char guid[64] = {};
        SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(i), guid, sizeof(guid));
        if ((hasGuid && GuidEquals(guid, glfwGuid)) || (!hasGuid && joystickCount == 1)) {
            if (match >= 0) {
                CloseActiveController();
                return false;
            }
            match = i;
        }
    }
    if (match < 0) {
        CloseActiveController();
        return false;
    }
    if (g_activeJoystick && SDL_JoystickGetAttached(g_activeJoystick)
        && SDL_JoystickInstanceID(g_activeJoystick) == SDL_JoystickGetDeviceInstanceID(match)) {
        return true;
    }
    CloseActiveController();
    return BindControllerAtIndex(match, glfwGuid, hasGuid) || BindJoystickAtIndex(match, glfwGuid, hasGuid);
}

bool glGamepadRumbleSupports() {
    return ActiveBindingCanRumble();
}

static float ClampUnit(float value) {
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

bool glGamepadRumbleSet(float lowFrequency, float highFrequency) {
    if (!g_activeController && !g_activeJoystick) {
        return false;
    }

    const float low = ClampUnit(lowFrequency);
    const float high = ClampUnit(highFrequency);
    const Uint16 lowAmp = (Uint16)(low * 65535.0f);
    const Uint16 highAmp = (Uint16)(high * 65535.0f);

    const Uint32 durationMs = (lowAmp == 0 && highAmp == 0) ? 0u : 500u;

    if (g_activeController && SDL_GameControllerRumble(g_activeController, lowAmp, highAmp, durationMs) == 0) {
        g_lastLow = lowAmp; g_lastHigh = highAmp; g_lastRumbleTime = SDL_GetTicks();
        return true;
    }

    return g_activeJoystick && SDL_JoystickRumble(g_activeJoystick, lowAmp, highAmp, durationMs) == 0;
}

bool glGamepadGetAudioDevice(GamepadAudioDevice& device) {
    device = {};
#if SDL_VERSION_ATLEAST(2, 24, 0)
    if (!g_activeJoystick || !SDL_JoystickGetAttached(g_activeJoystick)) return false;
    const char* path = SDL_JoystickPath(g_activeJoystick);
    if (!path || std::strlen(path) >= sizeof(device.path)) return false;
    std::strcpy(device.path, path);
    device.instance = (unsigned int)SDL_JoystickInstanceID(g_activeJoystick);
    device.vendor = SDL_JoystickGetVendor(g_activeJoystick);
    device.product = SDL_JoystickGetProduct(g_activeJoystick);
    return true;
#else
    return false;
#endif
}

bool glGamepadSetSpeakerEnabled(bool enabled) {
#if SDL_VERSION_ATLEAST(2, 24, 0)
    if (!g_activeController || !SDL_GameControllerGetAttached(g_activeController)
        || SDL_GameControllerGetType(g_activeController) != SDL_CONTROLLER_TYPE_PS5
        || SDL_JoystickGetVendor(g_activeJoystick) != 0x054c) return false;
    const bool vibrating = SDL_GetTicks() - g_lastRumbleTime < 500;
    const auto report = DualSenseSpeakerReport(enabled, vibrating ? (g_lastLow >> 8) : 0,
        vibrating ? (g_lastHigh >> 8) : 0, SDL_JoystickGetFirmwareVersion(g_activeJoystick));
    if (SDL_GameControllerSendEffect(g_activeController, report.data(), (int)report.size()) != 0) return false;
    g_speakerOwned = enabled;
    return true;
#else
    return false;
#endif
}

bool glGamepadSetLED(unsigned char red, unsigned char green, unsigned char blue) {
#if SDL_VERSION_ATLEAST(2, 0, 14)
    if (!g_activeJoystick || !SDL_JoystickGetAttached(g_activeJoystick)) return false;
    const Uint32 now = SDL_GetTicks();
    // SDL throttles identical LED reports for five seconds, including failed writes.
    if (g_ledAttempted && red == g_ledRed && green == g_ledGreen && blue == g_ledBlue
        && (g_ledSucceeded || now - g_ledLastAttempt < 5000)) return g_ledSucceeded;
    g_ledAttempted = true;
    g_ledLastAttempt = now;
    g_ledRed = red;
    g_ledGreen = green;
    g_ledBlue = blue;
    g_ledSucceeded = SDL_JoystickHasLED(g_activeJoystick)
        && (g_activeController ? SDL_GameControllerSetLED(g_activeController, red, green, blue)
                               : SDL_JoystickSetLED(g_activeJoystick, red, green, blue)) == 0;
    g_ledOwned |= g_ledSucceeded;
    return g_ledSucceeded;
#else
    return false;
#endif
}

#else

bool glGamepadGetAudioDevice(GamepadAudioDevice&) { return false; }
bool glGamepadSetSpeakerEnabled(bool) { return false; }
bool glGamepadSetLED(unsigned char, unsigned char, unsigned char) { return false; }

GamepadFamily glGamepadGetFamily() { return GamepadFamily::Unknown; }
void glGamepadShutdown() {}

bool glGamepadRumbleUpdateActive(int /*glfwGamepadIndex*/, const char* /*glfwGuid*/) {
    return false;
}

bool glGamepadPollState(bool* /*outButtons*/, float* /*outAxes*/) {
    return false;
}

bool glGamepadRumbleSupports() {
    return false;
}

bool glGamepadRumbleSet(float /*lowFrequency*/, float /*highFrequency*/) {
    return false;
}

#endif
