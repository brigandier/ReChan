#pragma once

namespace CrashReporter {
    // Installs process-wide fatal exception and signal handlers. Crash reports are
    // written beside the executable's working data and never uploaded automatically.
    void Install();
    // Main-thread breadcrumbs use static operation names; fatal handlers never acquire the log mutex.
    void SetGameContext(int state, int level, int region);
    void SetOperation(const char* operation);

    // Startup failure that is not a crash.
    void ShowFatalMessage(const char* title, const char* message);

    // Intentional fatal signal used to verify packaging and report permissions.
    [[noreturn]] void TriggerTestCrash();
}
