// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "localedata.h"

#include <base/locale.h>

#include <algorithm>
#include <cctype>
#include <regex>

using bayu::base::CalendarDate;
using bayu::base::ClockTime;
using bayu::base::Locale;

static Locale::FormatType formatType(LocaleData::Width width)
{
    switch (width) {
    case LocaleData::Width::Short:
        return Locale::FormatType::Short;
    case LocaleData::Width::Narrow:
        return Locale::FormatType::Narrow;
    case LocaleData::Width::Long:
        break;
    }
    return Locale::FormatType::Long;
}

static CalendarDate calendarDate(Date date)
{
    return CalendarDate{date.year(), date.month(), date.day()};
}

std::string LocaleData::language()
{
    return Locale::system().language();
}

std::string LocaleData::region()
{
    // "en_SG"; a script or other part may come between ("zh_Hans_CN").
    const std::string name = Locale::system().name();
    const size_t separator = name.find_last_of('_');
    if (separator == std::string::npos)
        return {};
    const std::string code = name.substr(separator + 1);
    const bool letters = code.size() == 2 && std::all_of(code.begin(), code.end(), [](unsigned char c) {
        return std::isupper(c);
    });
    return letters ? code : std::string();
}

bool LocaleData::isCjk()
{
    const std::string code = language();
    return code == "zh" || code == "ja" || code == "ko";
}

int LocaleData::firstDayOfWeek()
{
    return Locale::system().firstDayOfWeek();
}

std::vector<int> LocaleData::weekendDays()
{
    const std::vector<int> weekdays = Locale::system().weekdays();
    std::vector<int> days;
    for (int day = 1; day <= 7; ++day) {
        if (std::find(weekdays.begin(), weekdays.end(), day) == weekdays.end())
            days.push_back(day % 7);
    }
    return days;
}

bool LocaleData::use24Hour()
{
    const std::string format = Locale::system().timeFormat(Locale::FormatType::Short);
    return format.find_first_of("aA") == std::string::npos;
}

std::string LocaleData::monthName(int month, Width width)
{
    return Locale::system().standaloneMonthName(month, formatType(width));
}

std::string LocaleData::dayName(int weekday, Width width)
{
    return Locale::system().standaloneDayName(weekday == 0 ? 7 : weekday, formatType(width));
}

std::string LocaleData::timeText(Time time)
{
    if (!time.isValid())
        return {};
    return Locale::system().toString(ClockTime{time.hour(), time.minute(), time.second()}, Locale::FormatType::Short);
}

std::string LocaleData::hourText(int hour)
{
    std::string format = Locale::system().timeFormat(Locale::FormatType::Short);
    if (!use24Hour())
        format = std::regex_replace(format, std::regex("[:.]mm"), "", std::regex_constants::format_first_only);
    return Locale::system().toString(ClockTime{hour, 0}, format);
}

std::string LocaleData::longDate(Date date)
{
    if (!date.isValid())
        return {};
    return Locale::system().toString(calendarDate(date), Locale::FormatType::Long);
}

std::string LocaleData::longDateWithoutYear(Date date)
{
    if (!date.isValid())
        return {};
    std::string format = std::regex_replace(Locale::system().dateFormat(Locale::FormatType::Long),
                                            std::regex("[,\\s]*y+[.]?"), "", std::regex_constants::format_first_only);
    const size_t begin = format.find_first_not_of(" \t");
    const size_t end = format.find_last_not_of(" \t");
    format = begin == std::string::npos ? std::string() : format.substr(begin, end - begin + 1);
    return Locale::system().toString(calendarDate(date), format);
}
