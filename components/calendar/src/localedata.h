// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "datetime.h"

#include <string>
#include <vector>

// Dates and times as the calendar writes them, in the session's locale.
namespace LocaleData
{
enum class Width {
    Long,
    Short,
    Narrow,
};

// The language's code, such as "en".
std::string language();
// The session's ISO 3166 region, such as "SG"; empty when it names none.
std::string region();
// Chinese, Japanese and Korean write dates with their own counters.
bool isCjk();

// 1 for Monday to 7 for Sunday.
int firstDayOfWeek();
// 0 for Sunday to 6 for Saturday.
std::vector<int> weekendDays();
bool use24Hour();

// 1 to 12, as a name standing alone.
std::string monthName(int month, Width width = Width::Long);
// 0 for Sunday to 6 for Saturday, as a name standing alone.
std::string dayName(int weekday, Width width = Width::Long);

// The short time, such as "9:30 am".
std::string timeText(Time time);
// A whole hour; without the minutes when the clock has am and pm.
std::string hourText(int hour);
// The long date, such as "Wednesday, 7 October 2026".
std::string longDate(Date date);
// The long date without the year, such as "Wednesday, 7 October".
std::string longDateWithoutYear(Date date);
}
