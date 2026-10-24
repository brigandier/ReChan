#include "pddi/gl/glgamepadrumble_sdl.h"
#include "pddi/gl/glrender.h"
#include "SDL.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

struct RumbleState {
    int calls = 0, ledCalls = 0;
    Uint16 low = 0, high = 0;
    Uint8 red = 0, green = 0, blue = 0;
    bool ledFails = false;
    int playerIndex = -1;
};
static void SDLCALL SetPlayerIndex(void* data, int index) {
    ((RumbleState*)data)->playerIndex = index;
}
static int SDLCALL Rumble(void* data, Uint16 low, Uint16 high) {
    auto& state = *(RumbleState*)data;
    ++state.calls;
    state.low = low;
    state.high = high;
    return 0;
}

static int SDLCALL SetLED(void* data, Uint8 red, Uint8 green, Uint8 blue) {
    auto& state = *(RumbleState*)data;
    ++state.ledCalls;
    state.red = red;
    state.green = green;
    state.blue = blue;
    return state.ledFails ? -1 : 0;
}

static int Attach(const char* name, const char* type, RumbleState& rumble, bool supportsRumble = true, bool supportsLED = true) {
    SDL_VirtualJoystickDesc desc = {};
    desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    desc.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
    desc.naxes = 6;
    desc.nbuttons = 15;
    desc.button_mask = (1 << 15) - 1;
    desc.axis_mask = (1 << 6) - 1;
    desc.name = name;
    desc.userdata = &rumble;
    desc.Rumble = supportsRumble ? Rumble : nullptr;
    desc.SetLED = supportsLED ? SetLED : nullptr;
    desc.SetPlayerIndex = SetPlayerIndex;
    const int index = SDL_JoystickAttachVirtualEx(&desc);
    assert(index >= 0);
    char guid[33];
    SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(index), guid, sizeof(guid));
    const std::string mapping = std::string(guid) + "," + name
        + ",a:b0,b:b1,x:b2,y:b3,leftshoulder:b4,rightshoulder:b5,back:b6,start:b7,guide:b8,"
          "leftstick:b9,rightstick:b10,dpup:b11,dpright:b12,dpdown:b13,dpleft:b14,"
          "leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,type:" + type + ",";
    assert(SDL_GameControllerAddMapping(mapping.c_str()) >= 0);
    return index;
}

int main(int argc, char** argv) {
    SDL_SetMainReady();
    const bool diagnostic = argc == 2 && (std::strcmp(argv[1], "--inspect") == 0
        || std::strcmp(argv[1], "--pulse") == 0 || std::strcmp(argv[1], "--led") == 0);
#ifdef _WIN32
    if (!diagnostic) {
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "0");
        SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, "0");
        SDL_SetHint(SDL_HINT_JOYSTICK_WGI, "0");
        SDL_SetHint(SDL_HINT_XINPUT_ENABLED, "0");
        SDL_SetHint(SDL_HINT_DIRECTINPUT_ENABLED, "0");
    }
#endif
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    assert(SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) == 0);
    if (diagnostic) {
        std::cout << "SDL " << SDL_MAJOR_VERSION << '.' << SDL_MINOR_VERSION << '.' << SDL_PATCHLEVEL << '\n';
        std::cout << "Controllers: " << SDL_NumJoysticks() << '\n';
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            auto* controller = SDL_GameControllerOpen(i);
            if (!controller) continue;
            std::cout << SDL_GameControllerName(controller)
                << " type=" << SDL_GameControllerGetType(controller)
                << " vendor=" << SDL_GameControllerGetVendor(controller)
                << " product=" << SDL_GameControllerGetProduct(controller)
                << " rumble=" << SDL_GameControllerHasRumble(controller)
                << " LED=" << SDL_GameControllerHasLED(controller) << '\n';
            SDL_GameControllerClose(controller);
        }
        glGamepad host;
        host.Poll();
        std::cout << "Host connected=" << host.IsConnected()
            << " family=" << (int)host.GetFamily()
            << " rumble=" << host.SupportsVibration() << '\n';
        if (std::strcmp(argv[1], "--pulse") == 0) {
            const bool started = host.SetVibration(0.6f, 0.6f);
            SDL_Delay(300);
            const bool stopped = host.SetVibration(0.0f, 0.0f);
            std::cout << "Rumble pulse start=" << started << " stop=" << stopped << '\n';
            return started && stopped ? 0 : 1;
        }
        if (std::strcmp(argv[1], "--led") == 0) {
            const bool green = host.SetLED(0, 200, 40);
            SDL_Delay(1500);
            const bool orange = host.SetLED(255, 96, 0);
            SDL_Delay(1500);
            const bool red = host.SetLED(255, 0, 0);
            SDL_Delay(1500);
            bool blink = true;
            for (int i = 0; i < 8; ++i) {
                blink &= host.SetLED(i % 2 ? 255 : 32, 0, 0);
                SDL_Delay(250);
            }
            const bool blue = host.SetLED(0, 64, 255);
            SDL_Delay(3000);
            std::cout << "LED green=" << green << " orange=" << orange << " red=" << red << " blink=" << blink << " blue=" << blue << '\n';
            return green && orange && red && blink && blue ? 0 : 1;
        }
        return 0;
    }
    bool buttons[15]; float axes[6];
    if (SDL_NumJoysticks() != 0) {
        std::cerr << "Disconnect physical controllers before isolated virtual-device tests.\n";
        return 2;
    }
    assert(!glGamepadPollState(buttons, axes));
    {
        RumbleState first, second;
        SDL_VirtualJoystickDesc desc{};
        desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
        desc.type = SDL_JOYSTICK_TYPE_UNKNOWN;
        desc.naxes = 6;
        desc.nbuttons = 15;
        desc.name = "Identical unmapped joystick";
        desc.Rumble = Rumble;
        desc.userdata = &first;
        const int a = SDL_JoystickAttachVirtualEx(&desc);
        desc.userdata = &second;
        const int b = SDL_JoystickAttachVirtualEx(&desc);
        assert(a >= 0 && b >= 0 && !SDL_IsGameController(a) && !SDL_IsGameController(b));
        char guid[33];
        SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(a), guid, sizeof(guid));
        assert(!glGamepadPollState(buttons, axes));
        assert(!glGamepadRumbleUpdateActive(a, guid));
        assert(!glGamepadRumbleSet(1, 1) && !first.calls && !second.calls);
        SDL_JoystickDetachVirtual(b);
        assert(glGamepadRumbleUpdateActive(a, guid));
        assert(glGamepadRumbleSet(0.5f, 1) && first.high == 65535 && !second.calls);
        glGamepadShutdown();
        SDL_JoystickDetachVirtual(a);
        std::cout << "PASS GLFW/SDL fallback: ambiguous identical GUIDs rejected; unique device retains rumble\n";
    }
    for (const char* type : {"PS4", "PS5"}) {
        glGamepad host;
        RumbleState first, second, reconnected;
        const int a = Attach("Identical PlayStation", type, first);
        const int b = Attach("Identical PlayStation", type, second);
        const auto guidA = SDL_JoystickGetDeviceGUID(a), guidB = SDL_JoystickGetDeviceGUID(b);
        assert(std::memcmp(&guidA, &guidB, sizeof(guidA)) == 0);
        auto* padA = SDL_JoystickOpen(a);
        auto* padB = SDL_JoystickOpen(b);
        const auto oldInstance = SDL_JoystickInstanceID(padB);
        host.Poll();
        assert(host.SetVibration(0.5f, 1) && first.high == 65535 && !second.calls);
        assert(host.SetLED(255, 0, 0));
        SDL_JoystickSetVirtualButton(padB, 0, 1);
        host.Poll();
        assert(first.high == 0 && host.SetVibration(1, 0.5f) && second.low == 65535);
        assert(host.SetLED(0, 255, 0) && second.green == 255 && first.blue == 255);
        SDL_JoystickDetachVirtual(b);
        host.Poll();
        assert(host.SetVibration(0.5f, 1) && first.high == 65535);
        SDL_JoystickClose(padB);
        const int c = Attach("Identical PlayStation", type, reconnected);
        auto* padC = SDL_JoystickOpen(c);
        assert(SDL_JoystickInstanceID(padC) != oldInstance);
        host.Poll();
        SDL_JoystickSetVirtualButton(padC, 1, 1);
        host.Poll();
        assert(host.SetVibration(1, 0) && reconnected.low == 65535 && first.high == 0);
        assert(host.SetLED(255, 128, 0) && reconnected.red == 255);
        if (std::strcmp(type, "PS5") == 0)
            assert(reconnected.playerIndex == 0 && first.playerIndex == -1);
        glGamepadShutdown();
        SDL_JoystickDetachVirtual(c);
        SDL_JoystickDetachVirtual(a);
        SDL_JoystickClose(padC);
        SDL_JoystickClose(padA);
        std::cout << "PASS identical " << type << " GUIDs: instance selection, rumble, LED, detach and new-instance reconnection\n";
    }
    for (auto type : {"PS3", "PS4", "PS5", "Xbox360", "XboxOne", "SwitchPro", "JoyConLeft", "JoyConRight", "JoyConPair", "Virtual", "Unknown"}) {
        RumbleState rumble;
        const int index = Attach(type, type, rumble);
        SDL_Joystick* joystick = SDL_JoystickOpen(index);
        assert(joystick);
        SDL_JoystickSetVirtualAxis(joystick, 4, -32768);
        SDL_JoystickSetVirtualAxis(joystick, 5, -32768);
        assert(glGamepadPollState(buttons, axes));
        auto family = std::string(type).starts_with("PS") ? GamepadFamily::PlayStation
            : std::string(type).starts_with("Xbox") ? GamepadFamily::Xbox
            : std::string(type).starts_with("Switch") || std::string(type).starts_with("JoyCon") ? GamepadFamily::Switch : GamepadFamily::Unknown;
        assert(glGamepadGetFamily() == family);
        assert(SDL_JoystickSetVirtualButton(joystick, 0, 1) == 0);
        assert(glGamepadPollState(buttons, axes) && buttons[0]);
        assert(glGamepadRumbleSupports());
        assert(glGamepadRumbleSet(0.5f, 1.0f));
        assert(rumble.calls > 0 && rumble.high == 65535);
        assert(glGamepadSetLED(0, 64, 255));
        const int ledCalls = rumble.ledCalls;
        assert(glGamepadSetLED(0, 64, 255) && rumble.ledCalls == ledCalls);
        assert(glGamepadSetLED(255, 0, 0));
        assert(rumble.red == 255 && rumble.blue == 0 && rumble.high == 65535);
        assert(SDL_JoystickDetachVirtual(index) == 0);
        assert(!glGamepadSetLED(0, 64, 255));
        assert(!glGamepadPollState(buttons, axes));
        assert(glGamepadGetFamily() == GamepadFamily::Unknown);
        SDL_JoystickClose(joystick);
        std::cout << "PASS SDL " << type << ": hotplug, family, button, rumble, detach\n";
    }
    RumbleState xboxRumble, psRumble;
    const int xboxIndex = Attach("Virtual Xbox", "Xbox360", xboxRumble);
    SDL_Joystick* xbox = SDL_JoystickOpen(xboxIndex);
    assert(glGamepadPollState(buttons, axes));
    const int psIndex = Attach("Virtual PS5", "PS5", psRumble);
    SDL_Joystick* ps = SDL_JoystickOpen(psIndex);
    assert(glGamepadPollState(buttons, axes));
    assert(glGamepadSetLED(255, 0, 0));
    SDL_JoystickSetVirtualButton(ps, 1, 1);
    assert(glGamepadPollState(buttons, axes) && buttons[1]);
    assert(glGamepadGetFamily() == GamepadFamily::PlayStation);
    assert(xboxRumble.red == 0 && xboxRumble.green == 64 && xboxRumble.blue == 255);
    assert(glGamepadSetLED(0, 64, 255) && psRumble.blue == 255);
    SDL_JoystickSetVirtualButton(xbox, 0, 1);
    assert(glGamepadPollState(buttons, axes) && buttons[0]);
    assert(glGamepadGetFamily() == GamepadFamily::Xbox);
    // A held button on the other pad must not steal selection every poll.
    assert(glGamepadPollState(buttons, axes));
    assert(glGamepadGetFamily() == GamepadFamily::Xbox);
    SDL_JoystickDetachVirtual(psIndex);
    SDL_JoystickDetachVirtual(xboxIndex);
    glGamepadPollState(buttons, axes);
    SDL_JoystickClose(ps);
    SDL_JoystickClose(xbox);
    glGamepadShutdown();
    {
        glGamepad host;
        const int withRumble = Attach("Rumble Xbox", "Xbox360", xboxRumble);
        const int withoutRumble = Attach("Silent PS5", "PS5", psRumble, false);
        SDL_Joystick* silent = SDL_JoystickOpen(withoutRumble);
        host.Poll();
        SDL_JoystickSetVirtualButton(silent, 0, 1);
        host.Poll();
        assert(host.GetFamily() == GamepadFamily::PlayStation);
        assert(!host.SupportsVibration());
        assert(!host.SetVibration(1.0f, 1.0f));
        host.Poll();
        assert(host.GetFamily() == GamepadFamily::PlayStation && host.IsButtonDown(0));
        SDL_JoystickDetachVirtual(withoutRumble);
        SDL_JoystickDetachVirtual(withRumble);
        SDL_JoystickClose(silent);
    }
    {
        glGamepad host;
        RumbleState first, second;
        const int firstIndex = Attach("First PS5", "PS5", first);
        SDL_Joystick* firstPad = SDL_JoystickOpen(firstIndex);
        host.Poll();
        const int secondIndex = Attach("Second PS5", "PS5", second);
        SDL_Joystick* secondPad = SDL_JoystickOpen(secondIndex);
        host.Poll();
        assert(first.playerIndex == 0 && second.playerIndex == -1);
        assert(host.SetLED(255, 0, 0));
        SDL_JoystickSetVirtualButton(secondPad, 0, 1);
        host.Poll();
        assert(first.playerIndex == -1 && second.playerIndex == 0);
        assert(host.SetLED(0, 64, 255));
        assert(second.playerIndex == 0 && second.blue == 255);
        SDL_JoystickSetVirtualButton(firstPad, 1, 1);
        host.Poll();
        assert(first.playerIndex == 0 && second.playerIndex == -1);
        glGamepadShutdown();
        assert(first.playerIndex == -1 && second.playerIndex == -1);
        SDL_JoystickDetachVirtual(secondIndex);
        SDL_JoystickDetachVirtual(firstIndex);
        SDL_JoystickClose(secondPad);
        SDL_JoystickClose(firstPad);
    }
    {
        glGamepad host;
        RumbleState state;
        const int index = Attach("No LED PS4", "PS4", state, true, false);
        host.Poll();
        assert(!host.SetLED(255, 0, 0) && state.ledCalls == 0);
        assert(host.SupportsVibration() && host.SetVibration(0.5f, 1.0f));
        assert(state.high == 65535);
        SDL_JoystickDetachVirtual(index);
        host.Poll();
    }
    {
        glGamepad host;
        RumbleState state;
        state.ledFails = true;
        const int index = Attach("Transient LED PS5", "PS5", state);
        host.Poll();
        assert(!host.SetLED(255, 0, 0));
        int calls = state.ledCalls;
        assert(!host.SetLED(255, 0, 0) && state.ledCalls == calls);
        state.ledFails = false;
        SDL_Delay(5010);
        assert(host.SetLED(255, 0, 0) && state.ledCalls == calls+1);
        SDL_JoystickDetachVirtual(index);
        host.Poll();
    }
    assert(SDL_WasInit(SDL_INIT_GAMECONTROLLER) != 0);
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    assert(SDL_WasInit(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) == 0);
    glGamepadPollState(buttons, axes);
    assert(SDL_WasInit(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) != 0);
    glGamepadShutdown();
    assert(SDL_WasInit(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) == 0);
    glGamepadShutdown();
    std::cout << "PASS SDL shutdown: owned subsystems released, external ownership preserved, repeat shutdown safe\n";
    std::cout << "PASS multiple controllers: active-device switching and held-button stability\n";
    std::cout << "PASS querying unsupported rumble does not change active input or family\n";
    std::cout << "PASS LED: capability fallback, caching, transient failure, hotplug, device switching and rumble coexistence\n";
    std::cout << "PASS PS5 player lights: player one on active pad, inactive pads and shutdown off, RGB independent\n";
}
