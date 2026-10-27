// SPDX-License-Identifier: BSD-3-Clause

#include "dynamicwallpaper.h"

#include <filesystem>
#include <optional>

namespace DynamicWallpaper {

static constexpr const char *kPictures[] = {"morning.jpg", "day.jpg", "evening.jpg", "night.jpg"};

bool isSet(const std::string &path)
{
    std::error_code error;
    if (!std::filesystem::is_directory(path, error))
        return false;
    for (const char *name : kPictures) {
        if (!std::filesystem::is_regular_file(std::filesystem::path(path) / name, error))
            return false;
    }
    return true;
}

std::string picture(const std::string &set, Phase phase)
{
    return (std::filesystem::path(set) / kPictures[phase]).string();
}

static int minutesOfDay(const ScheduleUtils::DateTime &time)
{
    return int(std::chrono::floor<std::chrono::minutes>(time.timeOfDay()).count());
}

std::array<int, 4> times(const ScheduleUtils::DateTime &now)
{
    int sunrise = 6 * 60;
    int sunset = 18 * 60;
    // zone.tab is read again only when the zone changes.
    static std::string zoneId;
    static std::optional<ScheduleUtils::Coordinates> coordinates;
    if (now.zone->name() != zoneId) {
        zoneId = now.zone->name();
        coordinates = ScheduleUtils::zoneCoordinates(zoneId);
    }
    if (coordinates) {
        if (const auto sun = ScheduleUtils::sunTimes(now.date(), coordinates->latitude, coordinates->longitude,
                                                     now.zone)) {
            sunrise = minutesOfDay(sun->sunrise);
            sunset = minutesOfDay(sun->sunset);
        }
    }
    return {sunrise - 30, sunrise + 3 * 60, sunset - 2 * 60, sunset + 30};
}

Phase phase(const ScheduleUtils::DateTime &now)
{
    const std::array<int, 4> start = times(now);
    const int minute = minutesOfDay(now);
    if (minute < start[Morning] || minute >= start[Night])
        return Night;
    if (minute >= start[Evening])
        return Evening;
    return minute >= start[Day] ? Day : Morning;
}

} // namespace DynamicWallpaper
