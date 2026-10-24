#pragma once
#include "pddi/gamepadfamily.h"
#include "pddi/gamepadaudio.h"

GamepadFamily glGamepadGetFamily();
void glGamepadShutdown();
bool glGamepadPollState(bool outButtons[15], float outAxes[6]);
bool glGamepadRumbleUpdateActive(int glfwGamepadIndex, const char* glfwGuid);
bool glGamepadRumbleSupports();
bool glGamepadRumbleSet(float lowFrequency, float highFrequency);
bool glGamepadSetLED(unsigned char red, unsigned char green, unsigned char blue);
bool glGamepadGetAudioDevice(GamepadAudioDevice& device);
bool glGamepadSetSpeakerEnabled(bool enabled);
