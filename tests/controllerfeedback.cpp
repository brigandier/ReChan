#include "pc/controllerfeedback.h"
#include <cassert>
#include <iostream>
#include <limits>

void TestControllerFeedback() {
    const ControllerLEDColor blue{0, 64, 255}, green{0, 200, 40}, lime{130, 220, 0};
    const ControllerLEDColor yellow{255, 210, 0}, orange{255, 96, 0}, red{255, 0, 0}, dim{32, 0, 0};
    assert(ControllerHealthLED(false, 1, 200, 0.3) == blue);
    assert(ControllerHealthLED(true, 1, 0, 0.3) == blue);
    assert(ControllerHealthLED(true, 200, 200, 0.3) == green);
    assert(ControllerHealthLED(true, 161, 200, 0.3) == green);
    assert(ControllerHealthLED(true, 160, 200, 0.3) == lime);
    assert(ControllerHealthLED(true, 120, 200, 0.3) == yellow);
    assert(ControllerHealthLED(true, 80, 200, 0.3) == orange);
    assert(ControllerHealthLED(true, 40, 200, 0.0) == red);
    assert(ControllerHealthLED(true, 40, 200, 0.25) == red);
    assert(ControllerHealthLED(true, 25, 200, 0.25) == red);
    assert(ControllerHealthLED(true, 24, 200, 0.25) == dim);
    assert(ControllerHealthLED(true, 40, 200, 0.5) == red);
    assert(ControllerHealthLED(true, 0, 200, 0.3) == red);
    assert(ControllerHealthLED(true, -1, 200, 0.3) == red);
    assert(ControllerHealthLED(true, 999, 200, 0.3) == green);
    assert(ControllerHealthLED(true, 1, 200, std::numeric_limits<double>::quiet_NaN()) == red);
    for (int fps : {15, 30, 60, 144}) {
        int changes = 0;
        auto previous = red;
        for (int frame = 1; frame <= fps*2; ++frame) {
            auto color = ControllerHealthLED(true, 1, 200, (double)frame/fps);
            changes += color != previous;
            previous = color;
        }
        assert(changes == 8);
    }
    std::cout << "PASS controller LED: five health bands, menu blue, critical blink and FPS independence\n";
}
