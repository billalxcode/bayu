// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <chrono>
#include <climits>
#include <compare>
#include <cstdint>
#include <string>
#include <string_view>

namespace std::chrono {
class time_zone;
}

// A day of the proleptic Gregorian calendar; invalid when default-made.
class Date
{
public:
    Date() = default;
    Date(int year, int month, int day);

    static Date fromDays(int64_t daysSinceEpoch);
    // "2026-10-05"; invalid for anything else.
    static Date fromIsoString(std::string_view text);
    // "20261005"; invalid for anything else.
    static Date fromCompactString(std::string_view text);
    static Date currentDate();

    bool isValid() const { return m_days != Invalid; }
    int64_t daysSinceEpoch() const { return m_days; }

    int year() const;
    int month() const;
    int day() const;
    // 1 for Monday to 7 for Sunday.
    int dayOfWeek() const;
    int daysInMonth() const;
    // The ISO 8601 week.
    int weekNumber() const;

    Date addDays(int64_t days) const;
    // Lands on the month's last day when it is shorter.
    Date addMonths(int months) const;
    Date addYears(int years) const;
    int64_t daysTo(Date other) const { return other.m_days - m_days; }

    std::string toIsoString() const;
    std::string toCompactString() const;

    bool operator==(const Date&) const = default;
    auto operator<=>(const Date&) const = default;

private:
    static constexpr int64_t Invalid = INT64_MIN;

    int64_t m_days = Invalid;
};

// A time of day to the second; invalid when default-made.
class Time
{
public:
    Time() = default;
    Time(int hour, int minute, int second = 0);

    static Time fromSecondsSinceMidnight(int seconds);

    bool isValid() const { return m_seconds >= 0; }
    int hour() const { return m_seconds / 3600; }
    int minute() const { return m_seconds / 60 % 60; }
    int second() const { return m_seconds % 60; }
    int secondsSinceMidnight() const { return m_seconds; }

    bool operator==(const Time&) const = default;
    auto operator<=>(const Time&) const = default;

private:
    int m_seconds = -1;
};

// Where wall-clock times are read: the system's zone, UTC, a named zone, or
// a fixed offset from UTC.
class TimeZone
{
public:
    // The system's zone.
    TimeZone() = default;

    static TimeZone utc();
    static TimeZone fixed(int offsetSeconds);
    // An IANA id; invalid when unknown.
    static TimeZone fromId(std::string_view id);

    bool isValid() const { return m_kind != Kind::Invalid; }
    bool isLocal() const { return m_kind == Kind::Local; }
    bool isUtc() const { return m_kind == Kind::Utc; }
    // The IANA id of a named zone or of the system's.
    std::string id() const;

    // Seconds east of UTC at the instant.
    int offsetAt(int64_t secondsSinceEpoch) const;
    // The instant a wall-clock time names. In a gap or a fold the offset in
    // effect before the transition applies.
    int64_t toEpoch(int64_t localSeconds) const;

    bool operator==(const TimeZone&) const = default;

private:
    enum class Kind {
        Invalid,
        Local,
        Utc,
        Named,
        Fixed,
    };

    const std::chrono::time_zone* zone() const;

    Kind m_kind = Kind::Local;
    const std::chrono::time_zone* m_zone = nullptr;
    int m_offset = 0;
};

// An instant, read as a date and time in a zone. Comparisons look at the
// instant alone.
class DateTime
{
public:
    DateTime() = default;
    // The wall-clock time in the zone.
    DateTime(Date date, Time time, TimeZone zone = {});

    static DateTime fromSecsSinceEpoch(int64_t seconds, TimeZone zone = {});
    static DateTime currentDateTime();

    bool isValid() const { return m_valid; }
    int64_t toSecsSinceEpoch() const { return m_seconds; }
    const TimeZone& timeZone() const { return m_zone; }

    Date date() const;
    Time time() const;

    DateTime toTimeZone(TimeZone zone) const;
    DateTime toLocalTime() const { return toTimeZone({}); }
    DateTime toUtc() const { return toTimeZone(TimeZone::utc()); }

    DateTime addSecs(int64_t seconds) const;
    // Keeps the wall-clock time in its zone.
    DateTime addDays(int64_t days) const;
    int64_t secsTo(const DateTime& other) const { return other.m_seconds - m_seconds; }

    bool operator==(const DateTime& other) const;
    std::strong_ordering operator<=>(const DateTime& other) const;

private:
    int64_t localSeconds() const;

    int64_t m_seconds = 0;
    TimeZone m_zone;
    bool m_valid = false;
};
