// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <string>
#include <vector>

// A published calendar of one region's public holidays, in one language.
struct HolidayFeed
{
    // ISO 3166, such as "SG".
    std::string region;
    std::string language;
    // As the calendar names itself.
    std::string name;
    std::string url;
};

namespace HolidayFeeds
{
// Every feed: the given region's first, the one in the given language leading,
// then by name.
std::vector<HolidayFeed> list(const std::string& language, const std::string& region);
// As the session's language and region have it.
std::vector<HolidayFeed> list();
}
