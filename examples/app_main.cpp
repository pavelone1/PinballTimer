#include <Arduino.h>
#include "App.h"

// Entry point for the "app" PlatformIO environment. Per the
// architecture doc, main.cpp/this equivalent should stay tiny: start
// the application, call its update procedure repeatedly. No game
// rules, pins, timers, or networking here -- that's all in App.
// Build/upload with: pio run -e app -t upload

App app;

void setup()
{
    app.begin();
}

void loop()
{
    app.update();
}
