#include "extra/controllerpromptstyle.h"
#include <cassert>
#include <cstring>
#include <iostream>

void TestPromptStyles() {
    static_assert(ControllerPromptStyle_Default == 0 && ControllerPromptStyle_Switch == 1);
    assert(ControllerPromptManager::GetPreference() == ControllerPromptStyle_Auto);
    for (auto family : {GamepadFamily::Xbox, GamepadFamily::PlayStation, GamepadFamily::Switch, GamepadFamily::Unknown}) {
        ControllerPromptManager::SetDetectedFamily(family);
        ControllerPromptManager::SetStyle(ControllerPromptStyle_Auto);
        const int expected = family == GamepadFamily::PlayStation ? ControllerPromptStyle_PlayStation
            : family == GamepadFamily::Switch ? ControllerPromptStyle_Switch : ControllerPromptStyle_Default;
        assert(ControllerPromptManager::GetStyle() == expected);
        assert(ControllerPromptManager::GetPreference() == ControllerPromptStyle_Auto);
        for (int manual : {0, 1, 2}) {
            ControllerPromptManager::SetStyle(manual);
            ControllerPromptManager::SetDetectedFamily(GamepadFamily::Unknown);
            assert(ControllerPromptManager::GetStyle() == manual);
            assert(ControllerPromptManager::GetPreference() == manual);
            assert(kControllerPromptMenuStyles[ControllerPromptManager::GetMenuIndex()] == manual);
        }
    }
    ControllerPromptManager::SetStyle(-100);
    assert(ControllerPromptManager::GetPreference() == ControllerPromptStyle_Auto);
    ControllerPromptManager::SetDetectedFamily(GamepadFamily::PlayStation);
    char path[128];
    ControllerPromptManager::BuildSheetPath(path, sizeof(path));
    assert(std::strstr(path, "controller_sheet_playstation.png"));
    ControllerPromptManager::BuildOverlayPath(path, sizeof(path));
    assert(std::strstr(path, "controller_overlay_playstation.png"));
    assert(GamepadFamilyFromMappingName("Sony DualSense") == GamepadFamily::PlayStation);
    assert(GamepadFamilyFromMappingName("PS3 Controller") == GamepadFamily::PlayStation);
    assert(GamepadFamilyFromMappingName("PS4 Controller") == GamepadFamily::PlayStation);
    assert(GamepadFamilyFromMappingName("Xbox 360 Controller") == GamepadFamily::Xbox);
    assert(GamepadFamilyFromMappingName("Nintendo Switch Pro Controller") == GamepadFamily::Switch);
    assert(GamepadFamilyFromMappingName("Wireless Controller") == GamepadFamily::Unknown);
    assert(GamepadFamilyFromMappingName(nullptr) == GamepadFamily::Unknown);
    std::cout << "PASS prompt auto/manual precedence, legacy IDs, menu order, paths and GLFW mapping names\n";
}
