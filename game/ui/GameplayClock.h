#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace OpenYAMM::Game
{
inline std::string formatGameplayClock(float gameMinutes)
{
    const int totalMinutes = std::max(0, static_cast<int>(std::floor(gameMinutes + 0.5f)));
    const int minuteOfDay = totalMinutes % 1440;
    const int hour24 = minuteOfDay / 60;
    const int minute = minuteOfDay % 60;
    const int hour12 = hour24 == 0 ? 12 : hour24 > 12 ? hour24 - 12 : hour24;
    char timeText[16] = {};
    std::snprintf(timeText, sizeof(timeText), "%d:%02d %s", hour12, minute, hour24 >= 12 ? "pm" : "am");
    return timeText;
}
}
