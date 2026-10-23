// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "datetime.h"

#include <cstdio>
#include <stdexcept>

namespace chrono = std::chrono;

static constexpr int64_t SecondsPerDay = 86400;

static int64_t floorDiv(int64_t value, int64_t divisor)
{
    return value / divisor - (value % divisor < 0 ? 1 : 0);
}

static chrono::year_month_day ymdOf(int64_t days)
{
    return chrono::year_month_day(chrono::sys_days(chrono::sys_days::duration(days)));
}

static bool parseDigits(std::string_view text, size_t from, size_t count, int* value)
{
    int result = 0;
    for (size_t i = from; i < from + count; ++i) {
        if (text[i] < '0' || text[i] > '9')
            return false;
        result = result * 10 + (text[i] - '0');
    }
    *value = result;
    return true;
}

Date::Date(int y, int m, int d)
{
    const chrono::year_month_day ymd{chrono::year(y), chrono::month(unsigned(m)), chrono::day(unsigned(d))};
    if (y != 0 && m >= 1 && d >= 1 && ymd.ok())
        m_days = chrono::sys_days(ymd).time_since_epoch().count();
}

Date Date::fromDays(int64_t daysSinceEpoch)
{
    Date date;
    date.m_days = daysSinceEpoch;
    return date;
}

Date Date::fromIsoString(std::string_view text)
{
    int y = 0;
    int m = 0;
    int d = 0;
    if (text.size() != 10 || text[4] != '-' || text[7] != '-' || !parseDigits(text, 0, 4, &y)
            || !parseDigits(text, 5, 2, &m) || !parseDigits(text, 8, 2, &d))
        return {};
    return Date(y, m, d);
}

Date Date::fromCompactString(std::string_view text)
{
    int y = 0;
    int m = 0;
    int d = 0;
    if (text.size() != 8 || !parseDigits(text, 0, 4, &y) || !parseDigits(text, 4, 2, &m)
            || !parseDigits(text, 6, 2, &d))
        return {};
    return Date(y, m, d);
}

Date Date::currentDate()
{
    return DateTime::currentDateTime().date();
}

int Date::year() const
{
    return isValid() ? int(ymdOf(m_days).year()) : 0;
}

int Date::month() const
{
    return isValid() ? int(unsigned(ymdOf(m_days).month())) : 0;
}

int Date::day() const
{
    return isValid() ? int(unsigned(ymdOf(m_days).day())) : 0;
}

int Date::dayOfWeek() const
{
    if (!isValid())
        return 0;
    return int(chrono::weekday(chrono::sys_days(chrono::sys_days::duration(m_days))).iso_encoding());
}

int Date::daysInMonth() const
{
    if (!isValid())
        return 0;
    const chrono::year_month_day ymd = ymdOf(m_days);
    return int(unsigned(chrono::year_month_day_last(ymd.year(), chrono::month_day_last(ymd.month())).day()));
}

int Date::weekNumber() const
{
    if (!isValid())
        return 0;
    // The week belongs to the year of its Thursday.
    const Date thursday = addDays(4 - dayOfWeek());
    return int(Date(thursday.year(), 1, 1).daysTo(thursday) / 7 + 1);
}

Date Date::addDays(int64_t days) const
{
    return isValid() ? fromDays(m_days + days) : Date();
}

Date Date::addMonths(int months) const
{
    if (!isValid())
        return {};
    const chrono::year_month_day ymd = ymdOf(m_days);
    const int index = int(ymd.year()) * 12 + int(unsigned(ymd.month())) - 1 + months;
    const int y = int(floorDiv(index, 12));
    const int m = index - y * 12 + 1;
    const int last = Date(y, m, 1).daysInMonth();
    return Date(y, m, std::min(int(unsigned(ymd.day())), last));
}

Date Date::addYears(int years) const
{
    return addMonths(years * 12);
}

std::string Date::toIsoString() const
{
    if (!isValid())
        return {};
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", year(), month(), day());
    return buffer;
}

std::string Date::toCompactString() const
{
    if (!isValid())
        return {};
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%04d%02d%02d", year(), month(), day());
    return buffer;
}

Time::Time(int hour, int minute, int second)
{
    if (hour >= 0 && hour < 24 && minute >= 0 && minute < 60 && second >= 0 && second < 60)
        m_seconds = hour * 3600 + minute * 60 + second;
}

Time Time::fromSecondsSinceMidnight(int seconds)
{
    Time time;
    if (seconds >= 0 && seconds < SecondsPerDay)
        time.m_seconds = seconds;
    return time;
}

TimeZone TimeZone::utc()
{
    TimeZone zone;
    zone.m_kind = Kind::Utc;
    return zone;
}

TimeZone TimeZone::fixed(int offsetSeconds)
{
    TimeZone zone;
    zone.m_kind = Kind::Fixed;
    zone.m_offset = offsetSeconds;
    return zone;
}

TimeZone TimeZone::fromId(std::string_view id)
{
    TimeZone zone;
    zone.m_kind = Kind::Invalid;
    if (id.empty())
        return zone;
    try {
        zone.m_zone = chrono::locate_zone(id);
        zone.m_kind = Kind::Named;
    } catch (const std::runtime_error&) {
    }
    return zone;
}

const chrono::time_zone* TimeZone::zone() const
{
    if (m_kind == Kind::Named)
        return m_zone;
    if (m_kind != Kind::Local)
        return nullptr;
    try {
        return chrono::current_zone();
    } catch (const std::runtime_error&) {
        return nullptr;
    }
}

std::string TimeZone::id() const
{
    const chrono::time_zone* named = zone();
    if (named)
        return std::string(named->name());
    return m_kind == Kind::Utc ? "UTC" : std::string();
}

int TimeZone::offsetAt(int64_t secondsSinceEpoch) const
{
    if (m_kind == Kind::Fixed)
        return m_offset;
    const chrono::time_zone* named = zone();
    if (!named)
        return 0;
    return int(named->get_info(chrono::sys_seconds(chrono::seconds(secondsSinceEpoch))).offset.count());
}

int64_t TimeZone::toEpoch(int64_t localSeconds) const
{
    if (m_kind == Kind::Fixed)
        return localSeconds - m_offset;
    const chrono::time_zone* named = zone();
    if (!named)
        return localSeconds;
    const chrono::local_info info = named->get_info(chrono::local_seconds(chrono::seconds(localSeconds)));
    return localSeconds - info.first.offset.count();
}

DateTime::DateTime(Date date, Time time, TimeZone zone)
    : m_zone(zone)
{
    if (!date.isValid() || !zone.isValid())
        return;
    const int64_t local = date.daysSinceEpoch() * SecondsPerDay + (time.isValid() ? time.secondsSinceMidnight() : 0);
    m_seconds = zone.toEpoch(local);
    m_valid = true;
}

DateTime DateTime::fromSecsSinceEpoch(int64_t secs, TimeZone zone)
{
    DateTime dateTime;
    dateTime.m_seconds = secs;
    dateTime.m_zone = zone;
    dateTime.m_valid = zone.isValid();
    return dateTime;
}

DateTime DateTime::currentDateTime()
{
    return fromSecsSinceEpoch(chrono::floor<chrono::seconds>(chrono::system_clock::now()).time_since_epoch().count());
}

int64_t DateTime::localSeconds() const
{
    return m_seconds + m_zone.offsetAt(m_seconds);
}

Date DateTime::date() const
{
    return m_valid ? Date::fromDays(floorDiv(localSeconds(), SecondsPerDay)) : Date();
}

Time DateTime::time() const
{
    if (!m_valid)
        return {};
    const int64_t local = localSeconds();
    return Time::fromSecondsSinceMidnight(int(local - floorDiv(local, SecondsPerDay) * SecondsPerDay));
}

DateTime DateTime::toTimeZone(TimeZone zone) const
{
    if (!m_valid)
        return {};
    return fromSecsSinceEpoch(m_seconds, zone);
}

DateTime DateTime::addSecs(int64_t secs) const
{
    if (!m_valid)
        return {};
    return fromSecsSinceEpoch(m_seconds + secs, m_zone);
}

DateTime DateTime::addDays(int64_t days) const
{
    if (!m_valid)
        return {};
    return DateTime(date().addDays(days), time(), m_zone);
}

bool DateTime::operator==(const DateTime& other) const
{
    if (!m_valid || !other.m_valid)
        return m_valid == other.m_valid;
    return m_seconds == other.m_seconds;
}

std::strong_ordering DateTime::operator<=>(const DateTime& other) const
{
    if (!m_valid || !other.m_valid)
        return m_valid <=> other.m_valid;
    return m_seconds <=> other.m_seconds;
}
