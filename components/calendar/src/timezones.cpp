// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "timezones.h"

#include <algorithm>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <unicode/ucal.h>
#include <unicode/ustring.h>

static TimeZone fromWindowsId(std::string_view id)
{
    UErrorCode status = U_ZERO_ERROR;
    UChar windows[128];
    int32_t windowsLength = 0;
    u_strFromUTF8(windows, 128, &windowsLength, id.data(), int32_t(id.size()), &status);
    if (U_FAILURE(status))
        return TimeZone::fromId({});

    UChar iana[128];
    const int32_t length = ucal_getTimeZoneIDForWindowsID(windows, windowsLength, nullptr, iana, 128, &status);
    if (U_FAILURE(status) || length <= 0)
        return TimeZone::fromId({});
    char utf8[256];
    int32_t utf8Length = 0;
    u_strToUTF8(utf8, sizeof(utf8), &utf8Length, iana, length, &status);
    if (U_FAILURE(status))
        return TimeZone::fromId({});
    return TimeZone::fromId(std::string_view(utf8, size_t(utf8Length)));
}

static TimeZone lookup(std::string_view tzid)
{
    const TimeZone exact = TimeZone::fromId(tzid);
    if (exact.isValid())
        return exact;

    // Vendor prefixes keep the IANA id at the end, as one to three components.
    std::vector<size_t> slashes;
    for (size_t i = 0; i < tzid.size(); ++i) {
        if (tzid[i] == '/')
            slashes.push_back(i);
    }
    for (size_t count = std::min<size_t>(3, slashes.size()); count >= 1; --count) {
        const TimeZone zone = TimeZone::fromId(tzid.substr(slashes[slashes.size() - count] + 1));
        if (zone.isValid())
            return zone;
    }
    return fromWindowsId(tzid);
}

TimeZone TimeZones::fromTzid(std::string_view tzid)
{
    // Files are parsed on worker threads as well.
    static std::mutex mutex;
    static std::unordered_map<std::string, TimeZone> cache;

    std::lock_guard locker(mutex);
    auto it = cache.find(std::string(tzid));
    if (it == cache.end())
        it = cache.emplace(std::string(tzid), lookup(tzid)).first;
    return it->second;
}

TimeZone TimeZones::system()
{
    const TimeZone zone = TimeZone::fromId(TimeZone().id());
    return zone.isValid() ? zone : TimeZone();
}
