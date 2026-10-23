// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "datetime.h"

// Days as the views count them; weeks start on firstDay, 1 for Monday to 7 for Sunday.
namespace Dates
{
// 0 for Sunday to 6 for Saturday.
inline int weekday(Date date)
{
    return date.dayOfWeek() % 7;
}

inline Date weekStart(Date date, int firstDay)
{
    const int offset = (weekday(date) - firstDay % 7 + 7) % 7;
    return date.addDays(-offset);
}

inline Date monthStart(Date date)
{
    return Date(date.year(), date.month(), 1);
}

// The first day of the six-week grid that shows the month.
inline Date monthGridStart(Date date, int firstDay)
{
    return weekStart(monthStart(date), firstDay);
}

// Weekday 0-6 of the column, counted from the first day of the week.
inline int weekdayOfColumn(int column, int firstDay)
{
    return (firstDay % 7 + column) % 7;
}

// Minute 24 * 60 is the next midnight.
inline DateTime atMinute(Date date, int minute)
{
    return DateTime(date.addDays(minute / (24 * 60)), Time(minute % (24 * 60) / 60, minute % 60));
}

inline int minuteOf(const DateTime& dateTime)
{
    const Time time = dateTime.time();
    return time.hour() * 60 + time.minute();
}
}
