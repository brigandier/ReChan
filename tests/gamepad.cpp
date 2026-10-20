#include "pddi/gl/glgamepadrumble_sdl.h"
#include "pddi/gl/glrender.h"
#include "SDL.h"
#include <cassert>
#include <iostream>
#include <string>

struct RumbleState { int calls = 0; Uint16 low = 0, high = 0; };
static int SDLCALL Rumble(void* data, Uint16 low, Uint16 high) {
    auto& state = *(RumbleState*)data;
    ++state.calls;
    state.low = low;
    state.high = high;
    return 0;
}

static int Attach(const char* name, const char* type, RumbleState& rumble, bool supportsRumble = true) {
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

int main() {
    SDL_SetMainReady();
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    assert(SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) == 0);
    bool buttons[15]; float axes[6];
    if (SDL_NumJoysticks() != 0) {
        std::cerr << "Disconnect physical controllers before isolated virtual-device tests.\n";
        return 2;
    }
    assert(!glGamepadPollState(buttons, axes));
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
        assert(SDL_JoystickDetachVirtual(index) == 0);
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
    SDL_JoystickSetVirtualButton(ps, 1, 1);
    assert(glGamepadPollState(buttons, axes) && buttons[1]);
    assert(glGamepadGetFamily() == GamepadFamily::PlayStation);
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
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    std::cout << "PASS multiple controllers: active-device switching and held-button stability\n";
    std::cout << "PASS querying unsupported rumble does not change active input or family\n";
}
