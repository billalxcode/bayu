// SPDX-License-Identifier: BSD-3-Clause

#ifndef DYNAMICWALLPAPER_H
#define DYNAMICWALLPAPER_H

#include <array>
#include <string>

#include "scheduleutils.h"

// A dynamic wallpaper is a folder of four pictures, morning.jpg, day.jpg,
// evening.jpg and night.jpg; which one shows follows the sun where the user is,
// as the time zone places them, or the theme.
namespace DynamicWallpaper {

enum Phase {
    Morning,
    Day,
    Evening,
    Night,
};

// What decides the picture.
enum Follows {
    Time,
    // The day picture in light mode, the night one in dark mode.
    Theme,
};

// Whether `path` is a folder with all four pictures.
bool isSet(const std::string &path);

std::string picture(const std::string &set, Phase phase);

// When the morning, the day, the evening and the night start on `now`'s date,
// in minutes after midnight: from half an hour before sunrise, three hours
// after it, two hours before sunset and half an hour after it. 5:30, 9:00,
// 16:00 and 18:30 where the zone's location is unknown or the sun does not set.
std::array<int, 4> times(const ScheduleUtils::DateTime &now);

Phase phase(const ScheduleUtils::DateTime &now);

} // namespace DynamicWallpaper

#endif // DYNAMICWALLPAPER_H
