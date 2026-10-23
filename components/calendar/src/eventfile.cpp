// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "eventfile.h"
#include "text.h"
#include "timezones.h"
#include "uuid.h"

#include <libical/ical.h>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <strings.h>
#include <unordered_map>
#include <unordered_set>

namespace {

struct Zone
{
    enum Kind { Floating, Utc, Named, Custom };

    Kind kind = Floating;
    TimeZone zone;
    // As the file writes it, for Named and Custom.
    std::string tzid;
    // A VTIMEZONE of the file that no known zone matches.
    icaltimezone* custom = nullptr;
};

struct IcalTime
{
    icaltimetype wall = icaltime_null_time();
    Zone zone;

    bool isNull() const { return icaltime_is_null_time(wall); }
    bool isDate() const { return wall.is_date; }
};

// When an event happens: its first start and how long each occurrence lasts.
struct Series
{
    IcalTime start;
    bool allDay = false;
    int64_t seconds = 0;
    int days = 1;
};

}

// Repeating events are expanded at most this far.
static const int MaximumInstances = 100000;

// libical walks children with one iterator per parent, which any lookup on
// the parent moves: loops take a copy of the children first.
static std::vector<icalcomponent*> components(icalcomponent* parent, icalcomponent_kind kind)
{
    std::vector<icalcomponent*> list;
    for (icalcomponent* child = icalcomponent_get_first_component(parent, kind); child;
         child = icalcomponent_get_next_component(parent, kind))
        list.push_back(child);
    return list;
}

static std::vector<icalproperty*> properties(icalcomponent* parent, icalproperty_kind kind)
{
    std::vector<icalproperty*> list;
    for (icalproperty* property = icalcomponent_get_first_property(parent, kind); property;
         property = icalcomponent_get_next_property(parent, kind))
        list.push_back(property);
    return list;
}

static Zone utcZone()
{
    Zone zone;
    zone.kind = Zone::Utc;
    return zone;
}

static Zone systemZone()
{
    Zone zone;
    const TimeZone system = TimeZones::system();
    if (!system.isLocal()) {
        zone.kind = Zone::Named;
        zone.zone = system;
        zone.tzid = system.id();
    }
    return zone;
}

static Zone zoneOf(icalproperty* property, icalcomponent* event)
{
    Zone zone;
    icalparameter* parameter = icalproperty_get_first_parameter(property, ICAL_TZID_PARAMETER);
    if (!parameter)
        return zone;

    zone.tzid = icalparameter_get_tzid(parameter);
    zone.zone = TimeZones::fromTzid(zone.tzid);
    if (zone.zone.isValid()) {
        zone.kind = Zone::Named;
        return zone;
    }

    icalcomponent* calendar = event ? icalcomponent_get_parent(event) : nullptr;
    zone.custom = calendar ? icalcomponent_get_timezone(calendar, zone.tzid.c_str()) : nullptr;
    // A TZID nothing defines leaves the time floating.
    zone.kind = zone.custom ? Zone::Custom : Zone::Floating;
    return zone;
}

static IcalTime makeTime(icaltimetype value, icalproperty* property, icalcomponent* event)
{
    IcalTime time;
    time.wall = value;
    time.wall.zone = nullptr;
    if (value.is_date)
        return time;
    if (icaltime_is_utc(value))
        time.zone.kind = Zone::Utc;
    else
        time.zone = zoneOf(property, event);
    return time;
}

static IcalTime propertyTime(icalcomponent* event, icalproperty_kind kind)
{
    icalproperty* property = icalcomponent_get_first_property(event, kind);
    if (!property)
        return {};

    icalvalue* value = icalproperty_get_value(property);
    if (!value)
        return {};
    switch (icalvalue_isa(value)) {
    case ICAL_DATE_VALUE:
        return makeTime(icalvalue_get_date(value), property, event);
    case ICAL_DATETIME_VALUE:
        return makeTime(icalvalue_get_datetime(value), property, event);
    default:
        return {};
    }
}

static Date dateOf(const icaltimetype& wall)
{
    return Date(wall.year, wall.month, wall.day);
}

static Time clockOf(const icaltimetype& wall)
{
    return Time(wall.hour, wall.minute, wall.second);
}

static DateTime toDateTime(const IcalTime& time)
{
    const Date date = dateOf(time.wall);
    const Time clock = clockOf(time.wall);
    switch (time.zone.kind) {
    case Zone::Utc:
        return DateTime(date, clock, TimeZone::utc());
    case Zone::Named:
        return DateTime(date, clock, time.zone.zone);
    case Zone::Custom: {
        icaltimetype wall = time.wall;
        return DateTime::fromSecsSinceEpoch(icaltime_as_timet_with_zone(wall, time.zone.custom));
    }
    case Zone::Floating:
        break;
    }
    return DateTime(date, clock);
}

static icaltimetype dateWall(Date date)
{
    icaltimetype wall = icaltime_null_time();
    wall.year = date.year();
    wall.month = date.month();
    wall.day = date.day();
    wall.is_date = 1;
    return wall;
}

static icaltimetype toWall(const DateTime& instant, const Zone& zone)
{
    DateTime local;
    switch (zone.kind) {
    case Zone::Utc:
        local = instant.toUtc();
        break;
    case Zone::Named:
        local = instant.toTimeZone(zone.zone);
        break;
    case Zone::Custom: {
        icaltimetype wall = icaltime_from_timet_with_zone(instant.toSecsSinceEpoch(), 0, zone.custom);
        wall.zone = nullptr;
        return wall;
    }
    case Zone::Floating:
        local = instant.toLocalTime();
        break;
    }

    icaltimetype wall = icaltime_null_time();
    const Date date = local.date();
    const Time clock = local.time();
    wall.year = date.year();
    wall.month = date.month();
    wall.day = date.day();
    wall.hour = clock.hour();
    wall.minute = clock.minute();
    wall.second = clock.second();
    return wall;
}

static void setTimeValue(icalproperty* property, const IcalTime& time)
{
    icalproperty_remove_parameter_by_kind(property, ICAL_TZID_PARAMETER);
    icaltimetype value = time.wall;
    value.zone = nullptr;
    if (value.is_date) {
        icalproperty_set_value(property, icalvalue_new_date(value));
        return;
    }

    if (time.zone.kind == Zone::Utc)
        value.zone = icaltimezone_get_utc_timezone();
    icalproperty_set_value(property, icalvalue_new_datetime(value));
    if (time.zone.kind == Zone::Named || time.zone.kind == Zone::Custom)
        icalproperty_add_parameter(property, icalparameter_new_tzid(time.zone.tzid.c_str()));
}

static icalproperty* newTimeProperty(icalproperty_kind kind, const IcalTime& time)
{
    icalproperty* property = icalproperty_new(kind);
    setTimeValue(property, time);
    return property;
}

static void removeProperties(icalcomponent* component, icalproperty_kind kind)
{
    while (icalproperty* property = icalcomponent_get_first_property(component, kind)) {
        icalcomponent_remove_property(component, property);
        icalproperty_free(property);
    }
}

static std::string text(icalcomponent* event, icalproperty_kind kind)
{
    icalproperty* property = icalcomponent_get_first_property(event, kind);
    if (!property)
        return {};
    icalvalue* value = icalproperty_get_value(property);
    const char* string = value ? icalvalue_get_text(value) : nullptr;
    return string ? std::string(string) : std::string();
}

static void setText(icalcomponent* event, icalproperty_kind kind, const std::string& value)
{
    removeProperties(event, kind);
    if (value.empty() && kind != ICAL_SUMMARY_PROPERTY)
        return;
    icalproperty* property = icalproperty_new(kind);
    icalproperty_set_value(property, icalvalue_new_text(value.c_str()));
    icalcomponent_add_property(event, property);
}

static std::string uidOf(icalcomponent* event)
{
    const char* uid = icalcomponent_get_uid(event);
    return uid ? std::string(uid) : std::string();
}

static bool isOverride(icalcomponent* event)
{
    return icalcomponent_get_first_property(event, ICAL_RECURRENCEID_PROPERTY) != nullptr;
}

static bool isCancelled(icalcomponent* event)
{
    return icalcomponent_get_first_property(event, ICAL_STATUS_PROPERTY)
            && icalcomponent_get_status(event) == ICAL_STATUS_CANCELLED;
}

static Series seriesOf(icalcomponent* event)
{
    Series series;
    series.start = propertyTime(event, ICAL_DTSTART_PROPERTY);
    series.allDay = series.start.isDate();

    const IcalTime end = propertyTime(event, ICAL_DTEND_PROPERTY);
    icalproperty* duration = icalcomponent_get_first_property(event, ICAL_DURATION_PROPERTY);
    if (series.allDay) {
        if (!end.isNull())
            series.days = int(dateOf(series.start.wall).daysTo(dateOf(end.wall)));
        else if (duration)
            series.days = icaldurationtype_as_int(icalproperty_get_duration(duration)) / 86400;
        series.days = std::max(1, series.days);
    } else {
        if (!end.isNull())
            series.seconds = toDateTime(series.start).secsTo(toDateTime(end));
        else if (duration)
            series.seconds = icaldurationtype_as_int(icalproperty_get_duration(duration));
        series.seconds = std::max<int64_t>(0, series.seconds);
    }
    return series;
}

static std::string dateKey(Date date)
{
    return 'D' + date.toCompactString();
}

static std::string instantKey(const DateTime& instant)
{
    return 'T' + std::to_string(instant.toSecsSinceEpoch());
}

static std::string keyOf(const IcalTime& time)
{
    return time.isDate() ? dateKey(dateOf(time.wall)) : instantKey(toDateTime(time));
}

static bool keyIsDate(const std::string& key)
{
    return !key.empty() && key.front() == 'D';
}

static Date keyDate(const std::string& key)
{
    return key.empty() ? Date() : Date::fromCompactString(std::string_view(key).substr(1));
}

static DateTime keyInstant(const std::string& key)
{
    return DateTime::fromSecsSinceEpoch(key.size() > 1 ? std::strtoll(key.c_str() + 1, nullptr, 10) : 0);
}

static bool keyBefore(const std::string& a, const std::string& b)
{
    if (keyIsDate(a) && keyIsDate(b))
        return keyDate(a) < keyDate(b);
    if (!keyIsDate(a) && !keyIsDate(b))
        return keyInstant(a) < keyInstant(b);
    const Date first = keyIsDate(a) ? keyDate(a) : keyInstant(a).toLocalTime().date();
    const Date second = keyIsDate(b) ? keyDate(b) : keyInstant(b).toLocalTime().date();
    return first < second;
}

// The instance a key names, as the series' start property writes it.
static IcalTime instanceTime(const Series& series, const std::string& key)
{
    IcalTime time;
    time.zone = series.start.zone;
    if (series.allDay)
        time.wall = dateWall(keyIsDate(key) ? keyDate(key) : keyInstant(key).toLocalTime().date());
    else
        time.wall = toWall(keyIsDate(key) ? DateTime(keyDate(key), toDateTime(series.start).time())
                                          : keyInstant(key), series.start.zone);
    return time;
}

static void fillDays(Occurrence* occurrence)
{
    if (occurrence->allDay)
        return;
    occurrence->firstDay = occurrence->start.date();
    occurrence->lastDay = occurrence->end > occurrence->start ? occurrence->end.addSecs(-1).date()
                                                              : occurrence->firstDay;
}

// The occurrence the event's own fields describe, starting at `start`.
static Occurrence makeOccurrence(icalcomponent* event, const Series& series, const IcalTime& start)
{
    Occurrence occurrence;
    occurrence.summary = text(event, ICAL_SUMMARY_PROPERTY);
    occurrence.location = text(event, ICAL_LOCATION_PROPERTY);
    occurrence.allDay = series.allDay;
    if (series.allDay) {
        occurrence.firstDay = dateOf(start.wall);
        occurrence.lastDay = occurrence.firstDay.addDays(series.days - 1);
    } else {
        occurrence.start = toDateTime(start).toLocalTime();
        occurrence.end = occurrence.start.addSecs(series.seconds);
        fillDays(&occurrence);
    }
    return occurrence;
}

static bool touches(const Occurrence& occurrence, Date from, Date to)
{
    return occurrence.firstDay < to && occurrence.lastDay >= from;
}

static Repeat repeatOf(icalcomponent* event, const Series& series)
{
    Repeat repeat;
    icalproperty* property = icalcomponent_get_first_property(event, ICAL_RRULE_PROPERTY);
    if (!property)
        return repeat;

    const icalrecurrencetype rule = icalproperty_get_rrule(property);
    const auto length = [](const short* array, int size) {
        int count = 0;
        while (count < size && array[count] != ICAL_RECURRENCE_ARRAY_MAX)
            ++count;
        return count;
    };
    const int days = length(rule.by_day, ICAL_BY_DAY_SIZE);
    const int monthDays = length(rule.by_month_day, ICAL_BY_MONTHDAY_SIZE);
    const int months = length(rule.by_month, ICAL_BY_MONTH_SIZE);
    const bool otherParts = length(rule.by_second, ICAL_BY_SECOND_SIZE) || length(rule.by_minute, ICAL_BY_MINUTE_SIZE)
            || length(rule.by_hour, ICAL_BY_HOUR_SIZE) || length(rule.by_year_day, ICAL_BY_YEARDAY_SIZE)
            || length(rule.by_week_no, ICAL_BY_WEEKNO_SIZE) || length(rule.by_set_pos, ICAL_BY_SETPOS_SIZE);

    const Date start = dateOf(series.start.wall);
    // iCalendar counts weekdays from Sunday = 1.
    const short startDay = short(start.dayOfWeek() % 7 + 1);
    const bool weekdays = days == 5 && [&rule] {
        for (int i = 0; i < 5; ++i) {
            if (rule.by_day[i] != ICAL_MONDAY_WEEKDAY + i)
                return false;
        }
        return true;
    }();

    repeat.frequency = Repeat::Custom;
    if (rule.interval <= 1 && !otherParts && !monthDays && !months) {
        if (rule.freq == ICAL_DAILY_RECURRENCE && !days)
            repeat.frequency = Repeat::Daily;
        else if ((rule.freq == ICAL_DAILY_RECURRENCE || rule.freq == ICAL_WEEKLY_RECURRENCE) && weekdays)
            repeat.frequency = Repeat::Weekdays;
        else if (rule.freq == ICAL_WEEKLY_RECURRENCE && (!days || (days == 1 && rule.by_day[0] == startDay)))
            repeat.frequency = Repeat::Weekly;
    }
    if (rule.interval <= 1 && !otherParts && !days) {
        if (rule.freq == ICAL_MONTHLY_RECURRENCE && !months
                && (!monthDays || (monthDays == 1 && rule.by_month_day[0] == start.day())))
            repeat.frequency = Repeat::Monthly;
        else if (rule.freq == ICAL_YEARLY_RECURRENCE
                 && (!months || (months == 1 && rule.by_month[0] == start.month()))
                 && (!monthDays || (monthDays == 1 && rule.by_month_day[0] == start.day())))
            repeat.frequency = Repeat::Yearly;
    }
    if (icalcomponent_get_first_property(event, ICAL_RDATE_PROPERTY)
            || properties(event, ICAL_RRULE_PROPERTY).size() > 1)
        repeat.frequency = Repeat::Custom;

    repeat.count = rule.count;
    if (!icaltime_is_null_time(rule.until)) {
        IcalTime until;
        until.wall = rule.until;
        until.wall.zone = nullptr;
        if (rule.until.is_date)
            repeat.until = dateOf(rule.until);
        else {
            until.zone = icaltime_is_utc(rule.until) ? utcZone() : series.start.zone;
            repeat.until = toDateTime(until).toLocalTime().date();
        }
    }
    return repeat;
}

static icalrecurrencetype ruleFor(const Repeat& repeat, const Series& series)
{
    icalrecurrencetype rule;
    icalrecurrencetype_clear(&rule);
    switch (repeat.frequency) {
    case Repeat::Daily:
        rule.freq = ICAL_DAILY_RECURRENCE;
        break;
    case Repeat::Weekdays:
        rule.freq = ICAL_WEEKLY_RECURRENCE;
        for (int i = 0; i < 5; ++i)
            rule.by_day[i] = short(ICAL_MONDAY_WEEKDAY + i);
        rule.by_day[5] = ICAL_RECURRENCE_ARRAY_MAX;
        break;
    case Repeat::Weekly:
        rule.freq = ICAL_WEEKLY_RECURRENCE;
        break;
    case Repeat::Monthly:
        rule.freq = ICAL_MONTHLY_RECURRENCE;
        break;
    case Repeat::Yearly:
        rule.freq = ICAL_YEARLY_RECURRENCE;
        break;
    case Repeat::None:
    case Repeat::Custom:
        break;
    }
    rule.interval = 1;

    if (repeat.count > 0) {
        rule.count = repeat.count;
    } else if (repeat.until.isValid()) {
        if (series.allDay) {
            rule.until = dateWall(repeat.until);
        } else {
            // The last day is included; a time with a zone ends in UTC.
            const DateTime end(repeat.until, Time(23, 59, 59));
            IcalTime until;
            until.zone = series.start.zone.kind == Zone::Floating ? Zone() : utcZone();
            until.wall = toWall(end, until.zone);
            rule.until = until.wall;
            if (until.zone.kind == Zone::Utc)
                rule.until.zone = icaltimezone_get_utc_timezone();
        }
    }
    return rule;
}

// The start of every instance a rule or RDATE makes, in the series' zone;
// stops at the first one that starts at or after `end`.
static void instances(icalcomponent* event, const Series& series, const DateTime& skipTo, const DateTime& end,
                      const std::function<bool(const IcalTime&)>& visit)
{
    for (icalproperty* property : properties(event, ICAL_RRULE_PROPERTY)) {
        icalrecurrencetype rule = icalproperty_get_rrule(property);
        // Compared with floating instances below, so it has to be one too.
        if (!icaltime_is_null_time(rule.until)) {
            if (series.allDay) {
                if (!rule.until.is_date) {
                    IcalTime until;
                    until.wall = rule.until;
                    until.wall.zone = nullptr;
                    until.zone = icaltime_is_utc(rule.until) ? utcZone() : series.start.zone;
                    rule.until = dateWall(toDateTime(until).toLocalTime().date());
                }
            } else if (rule.until.is_date) {
                rule.until.is_date = 0;
                rule.until.hour = 23;
                rule.until.minute = 59;
                rule.until.second = 59;
            } else if (icaltime_is_utc(rule.until)) {
                IcalTime until;
                until.wall = rule.until;
                until.wall.zone = nullptr;
                until.zone.kind = Zone::Utc;
                rule.until = toWall(toDateTime(until), series.start.zone);
            }
            rule.until.zone = nullptr;
        }

        icalrecur_iterator* iterator = icalrecur_iterator_new(rule, series.start.wall);
        if (!iterator)
            continue;
        if (rule.count == 0 && skipTo.isValid()) {
            const icaltimetype skip = series.allDay ? dateWall(skipTo.toLocalTime().date())
                                                    : toWall(skipTo, series.start.zone);
            if (icaltime_compare(skip, series.start.wall) > 0 && !icalrecur_iterator_set_start(iterator, skip)) {
                icalrecur_iterator_free(iterator);
                iterator = icalrecur_iterator_new(rule, series.start.wall);
            }
        }

        for (int i = 0; i < MaximumInstances; ++i) {
            const icaltimetype next = icalrecur_iterator_next(iterator);
            if (icaltime_is_null_time(next))
                break;
            IcalTime time;
            time.wall = next;
            time.wall.zone = nullptr;
            time.zone = series.start.zone;
            const DateTime start = series.allDay ? DateTime(dateOf(next), Time(0, 0)) : toDateTime(time);
            if (start >= end || !visit(time))
                break;
        }
        icalrecur_iterator_free(iterator);
    }

    for (icalproperty* property : properties(event, ICAL_RDATE_PROPERTY)) {
        const icaldatetimeperiodtype value = icalproperty_get_rdate(property);
        IcalTime time = makeTime(icaltime_is_null_time(value.time) ? value.period.start : value.time, property, event);
        if (series.allDay != time.isDate())
            continue;
        if (!visit(time))
            break;
    }
}

static std::unordered_set<std::string> exclusions(icalcomponent* event)
{
    std::unordered_set<std::string> keys;
    for (icalproperty* property : properties(event, ICAL_EXDATE_PROPERTY))
        keys.insert(keyOf(makeTime(icalproperty_get_exdate(property), property, event)));
    return keys;
}

static bool excluded(const std::unordered_set<std::string>& exclusions, const IcalTime& time)
{
    return exclusions.contains(keyOf(time)) || (!time.isDate() && exclusions.contains(dateKey(dateOf(time.wall))));
}

static void addTimezone(icalcomponent* calendar, const Zone& zone)
{
    if (zone.kind != Zone::Named || icalcomponent_get_timezone(calendar, zone.tzid.c_str()))
        return;
    icaltimezone* builtin = icaltimezone_get_builtin_timezone(zone.zone.id().c_str());
    icalcomponent* definition = builtin ? icaltimezone_get_component(builtin) : nullptr;
    if (!definition)
        return;
    // The builtin zones carry a vendor prefix; the events name the zone without it.
    icalcomponent* copy = icalcomponent_new_clone(definition);
    removeProperties(copy, ICAL_TZID_PROPERTY);
    icalcomponent_add_property(copy, icalproperty_new_tzid(zone.tzid.c_str()));
    icalcomponent_add_component(calendar, copy);
}

static void writeAlarm(icalcomponent* event, const EventData& data)
{
    if (data.alarm == EventData::CustomAlarm)
        return;
    while (icalcomponent* alarm = icalcomponent_get_first_component(event, ICAL_VALARM_COMPONENT)) {
        icalcomponent_remove_component(event, alarm);
        icalcomponent_free(alarm);
    }
    if (data.alarm != EventData::OffsetAlarm)
        return;

    icalcomponent* alarm = icalcomponent_new(ICAL_VALARM_COMPONENT);
    icalcomponent_add_property(alarm, icalproperty_new_action(ICAL_ACTION_DISPLAY));
    icalcomponent_add_property(alarm, icalproperty_new_description(data.summary.c_str()));
    icaltriggertype trigger;
    trigger.time = icaltime_null_time();
    trigger.duration = icaldurationtype_from_int(data.alarmOffset * 60);
    icalcomponent_add_property(alarm, icalproperty_new_trigger(trigger));
    icalcomponent_add_component(event, alarm);
}

static void readAlarm(icalcomponent* event, EventData* data)
{
    icalcomponent* alarm = icalcomponent_get_first_component(event, ICAL_VALARM_COMPONENT);
    if (!alarm)
        return;
    data->alarm = EventData::CustomAlarm;
    if (components(event, ICAL_VALARM_COMPONENT).size() > 1)
        return;

    icalproperty* property = icalcomponent_get_first_property(alarm, ICAL_TRIGGER_PROPERTY);
    if (!property)
        return;
    icalparameter* related = icalproperty_get_first_parameter(property, ICAL_RELATED_PARAMETER);
    const icaltriggertype trigger = icalproperty_get_trigger(property);
    if (!icaltime_is_null_time(trigger.time) || (related && icalparameter_get_related(related) == ICAL_RELATED_END))
        return;
    const int seconds = icaldurationtype_as_int(trigger.duration);
    if (seconds % 60)
        return;
    data->alarm = EventData::OffsetAlarm;
    data->alarmOffset = seconds / 60;
}

static std::vector<DateTime> alarmTimes(icalcomponent* event, const Occurrence& occurrence)
{
    std::vector<DateTime> times;
    const DateTime start = occurrence.allDay ? DateTime(occurrence.firstDay, Time(0, 0)) : occurrence.start;
    const DateTime end = occurrence.allDay ? DateTime(occurrence.lastDay.addDays(1), Time(0, 0)) : occurrence.end;
    for (icalcomponent* alarm : components(event, ICAL_VALARM_COMPONENT)) {
        icalproperty* property = icalcomponent_get_first_property(alarm, ICAL_TRIGGER_PROPERTY);
        if (!property)
            continue;
        const icaltriggertype trigger = icalproperty_get_trigger(property);
        if (!icaltime_is_null_time(trigger.time))
            continue;
        icalparameter* related = icalproperty_get_first_parameter(property, ICAL_RELATED_PARAMETER);
        const bool fromEnd = related && icalparameter_get_related(related) == ICAL_RELATED_END;
        times.push_back((fromEnd ? end : start).addSecs(icaldurationtype_as_int(trigger.duration)));
    }
    return times;
}

// The alarms of the event that go off at a time of their own.
static std::vector<DateTime> fixedAlarmTimes(icalcomponent* event)
{
    std::vector<DateTime> times;
    for (icalcomponent* alarm : components(event, ICAL_VALARM_COMPONENT)) {
        icalproperty* property = icalcomponent_get_first_property(alarm, ICAL_TRIGGER_PROPERTY);
        if (!property)
            continue;
        const icaltriggertype trigger = icalproperty_get_trigger(property);
        if (!icaltime_is_null_time(trigger.time))
            times.push_back(toDateTime(makeTime(trigger.time, property, event)).toLocalTime());
    }
    return times;
}

// How far before or after its occurrence any alarm of the file goes off, in days.
static int alarmReach(icalcomponent* calendar)
{
    int reach = 0;
    for (icalcomponent* event : components(calendar, ICAL_VEVENT_COMPONENT)) {
        for (icalcomponent* alarm : components(event, ICAL_VALARM_COMPONENT)) {
            icalproperty* property = icalcomponent_get_first_property(alarm, ICAL_TRIGGER_PROPERTY);
            if (property) {
                const int seconds = icaldurationtype_as_int(icalproperty_get_trigger(property).duration);
                reach = std::max(reach, std::abs(seconds) / 86400 + 1);
            }
        }
    }
    return reach;
}

static void shiftTime(icalproperty* property, icalcomponent* event, int64_t seconds, int64_t days)
{
    IcalTime time;
    icalvalue* value = icalproperty_get_value(property);
    if (!value)
        return;
    time = makeTime(icalvalue_isa(value) == ICAL_DATE_VALUE ? icalvalue_get_date(value) : icalvalue_get_datetime(value),
                    property, event);
    if (time.isDate())
        time.wall = dateWall(dateOf(time.wall).addDays(days));
    else
        time.wall = toWall(toDateTime(time).addSecs(seconds), time.zone);
    setTimeValue(property, time);
}

EventFile::EventFile(icalcomponent* calendar)
    : m_calendar(calendar)
{
}

EventFile::~EventFile()
{
    if (m_calendar)
        icalcomponent_free(m_calendar);
}

static icalcomponent* newCalendar()
{
    icalcomponent* calendar = icalcomponent_new(ICAL_VCALENDAR_COMPONENT);
    icalcomponent_add_property(calendar, icalproperty_new_version("2.0"));
    icalcomponent_add_property(calendar, icalproperty_new_prodid("-//Bayu//Calendar//EN"));
    return calendar;
}

static icalcomponent* parse(std::string_view data)
{
    // The parser reads up to the first NUL.
    const std::string text(data);
    return icalparser_parse_string(text.c_str());
}

static std::string asText(icalcomponent* component)
{
    char* string = icalcomponent_as_ical_string_r(component);
    std::string data(string ? string : "");
    icalmemory_free_buffer(string);
    return data;
}

std::unique_ptr<EventFile> EventFile::fromData(std::string_view data)
{
    icalcomponent* root = parse(data);
    if (!root)
        return nullptr;

    if (icalcomponent_isa(root) == ICAL_VEVENT_COMPONENT) {
        icalcomponent* calendar = newCalendar();
        icalcomponent_add_component(calendar, root);
        root = calendar;
    }
    if (icalcomponent_isa(root) != ICAL_VCALENDAR_COMPONENT
            || !icalcomponent_get_first_component(root, ICAL_VEVENT_COMPONENT)) {
        icalcomponent_free(root);
        return nullptr;
    }
    return std::unique_ptr<EventFile>(new EventFile(root));
}

std::vector<std::unique_ptr<EventFile>> EventFile::split(std::string_view data)
{
    std::vector<std::unique_ptr<EventFile>> files;
    icalcomponent* root = parse(data);
    if (!root)
        return files;

    std::vector<icalcomponent*> calendars;
    if (icalcomponent_isa(root) == ICAL_XROOT_COMPONENT)
        calendars = components(root, ICAL_VCALENDAR_COMPONENT);
    else if (icalcomponent_isa(root) == ICAL_VCALENDAR_COMPONENT)
        calendars.push_back(root);

    std::unordered_map<std::string, size_t> byUid;
    for (icalcomponent* calendar : calendars) {
        for (icalcomponent* event : components(calendar, ICAL_VEVENT_COMPONENT)) {
            if (!icalcomponent_get_first_property(event, ICAL_DTSTART_PROPERTY))
                continue;
            std::string uid = uidOf(event);
            icalcomponent* copy = icalcomponent_new_clone(event);
            if (uid.empty()) {
                uid = createUuid();
                icalcomponent_set_uid(copy, uid.c_str());
            }

            auto it = byUid.find(uid);
            if (it == byUid.end()) {
                icalcomponent* target = newCalendar();
                // The zones it may name; unused ones cost nothing but bytes.
                for (icalcomponent* zone : components(calendar, ICAL_VTIMEZONE_COMPONENT))
                    icalcomponent_add_component(target, icalcomponent_new_clone(zone));
                files.push_back(std::unique_ptr<EventFile>(new EventFile(target)));
                it = byUid.emplace(uid, files.size() - 1).first;
            }
            icalcomponent_add_component(files[it->second]->m_calendar, copy);
        }
    }
    icalcomponent_free(root);
    return files;
}

FeedInfo EventFile::feedInfo(std::string_view data)
{
    FeedInfo info;
    icalcomponent* root = parse(data);
    if (!root)
        return info;
    icalcomponent* calendar = icalcomponent_isa(root) == ICAL_XROOT_COMPONENT
            ? icalcomponent_get_first_component(root, ICAL_VCALENDAR_COMPONENT)
            : root;
    if (!calendar || icalcomponent_isa(calendar) != ICAL_VCALENDAR_COMPONENT) {
        icalcomponent_free(root);
        return info;
    }

    std::string xName;
    std::string xTtl;
    for (icalproperty* property : properties(calendar, ICAL_X_PROPERTY)) {
        const char* name = icalproperty_get_x_name(property);
        const char* value = icalproperty_get_x(property);
        if (!name || !value)
            continue;
        if (strcasecmp(name, "X-WR-CALNAME") == 0)
            xName = value;
        else if (strcasecmp(name, "X-PUBLISHED-TTL") == 0)
            xTtl = value;
    }
    // RFC 7986 first, then the older extensions most publishers write.
    if (icalproperty* name = icalcomponent_get_first_property(calendar, ICAL_NAME_PROPERTY)) {
        if (const char* value = icalvalue_get_text(icalproperty_get_value(name)))
            info.name = value;
    }
    if (info.name.empty())
        info.name = xName;
    info.name = trimmed(info.name);

    icaldurationtype interval = icaldurationtype_null_duration();
    if (icalproperty* refresh = icalcomponent_get_first_property(calendar, ICAL_REFRESHINTERVAL_PROPERTY))
        interval = icalproperty_get_refreshinterval(refresh);
    else if (!xTtl.empty())
        interval = icaldurationtype_from_string(trimmed(xTtl).c_str());
    if (!icaldurationtype_is_bad_duration(interval) && !icaldurationtype_is_null_duration(interval))
        info.refreshSeconds = std::max(0, icaldurationtype_as_int(interval));

    icalcomponent_free(root);
    return info;
}

std::unique_ptr<EventFile> EventFile::create(const EventData& data)
{
    icalcomponent* calendar = newCalendar();
    icalcomponent* event = icalcomponent_new(ICAL_VEVENT_COMPONENT);
    icalcomponent_set_uid(event, createUuid().c_str());
    const icaltimetype now = icaltime_current_time_with_zone(icaltimezone_get_utc_timezone());
    icalcomponent_add_property(event, icalproperty_new_dtstamp(now));
    icalcomponent_add_property(event, icalproperty_new_created(now));
    icalcomponent_add_property(event, icalproperty_new_lastmodified(now));
    icalcomponent_add_property(event, icalproperty_new_sequence(0));
    icalcomponent_add_component(calendar, event);

    std::unique_ptr<EventFile> file(new EventFile(calendar));
    file->setData(data);
    // A new event starts at sequence 0; setData counted it as a change.
    icalcomponent_set_sequence(event, 0);
    return file;
}

std::unique_ptr<EventFile> EventFile::clone() const
{
    return std::unique_ptr<EventFile>(new EventFile(icalcomponent_new_clone(m_calendar)));
}

std::string EventFile::toData() const
{
    return asText(m_calendar);
}

std::string EventFile::join(const std::vector<const EventFile*>& files)
{
    icalcomponent* calendar = newCalendar();
    std::unordered_set<std::string> zones;
    for (const EventFile* file : files) {
        for (icalcomponent* component : components(file->m_calendar, ICAL_ANY_COMPONENT)) {
            if (icalcomponent_isa(component) == ICAL_VTIMEZONE_COMPONENT) {
                icalproperty* tzid = icalcomponent_get_first_property(component, ICAL_TZID_PROPERTY);
                const std::string id = tzid ? std::string(icalproperty_get_tzid(tzid)) : std::string();
                if (!zones.insert(id).second)
                    continue;
            } else if (icalcomponent_isa(component) != ICAL_VEVENT_COMPONENT) {
                continue;
            }
            icalcomponent_add_component(calendar, icalcomponent_new_clone(component));
        }
    }
    const std::string data = asText(calendar);
    icalcomponent_free(calendar);
    return data;
}

icalcomponent* EventFile::master() const
{
    icalcomponent* first = nullptr;
    for (icalcomponent* event : components(m_calendar, ICAL_VEVENT_COMPONENT)) {
        if (!isOverride(event))
            return event;
        if (!first)
            first = event;
    }
    return first;
}

icalcomponent* EventFile::override(const std::string& recurrenceKey) const
{
    if (recurrenceKey.empty())
        return nullptr;
    for (icalcomponent* event : components(m_calendar, ICAL_VEVENT_COMPONENT)) {
        if (isOverride(event) && keyOf(propertyTime(event, ICAL_RECURRENCEID_PROPERTY)) == recurrenceKey)
            return event;
    }
    return nullptr;
}

void EventFile::touch(icalcomponent* event) const
{
    const icaltimetype now = icaltime_current_time_with_zone(icaltimezone_get_utc_timezone());
    removeProperties(event, ICAL_DTSTAMP_PROPERTY);
    removeProperties(event, ICAL_LASTMODIFIED_PROPERTY);
    icalcomponent_add_property(event, icalproperty_new_dtstamp(now));
    icalcomponent_add_property(event, icalproperty_new_lastmodified(now));
    const bool sequenced = icalcomponent_get_first_property(event, ICAL_SEQUENCE_PROPERTY);
    icalcomponent_set_sequence(event, sequenced ? icalcomponent_get_sequence(event) + 1 : 1);
}

std::string EventFile::uid() const
{
    icalcomponent* event = master();
    return event ? uidOf(event) : std::string();
}

std::string EventFile::summary() const
{
    icalcomponent* event = master();
    return event ? text(event, ICAL_SUMMARY_PROPERTY) : std::string();
}

std::string EventFile::location() const
{
    icalcomponent* event = master();
    return event ? text(event, ICAL_LOCATION_PROPERTY) : std::string();
}

std::string EventFile::description() const
{
    icalcomponent* event = master();
    return event ? text(event, ICAL_DESCRIPTION_PROPERTY) : std::string();
}

bool EventFile::isRecurring() const
{
    icalcomponent* event = master();
    return event && !isOverride(event)
            && (icalcomponent_get_first_property(event, ICAL_RRULE_PROPERTY)
                || icalcomponent_get_first_property(event, ICAL_RDATE_PROPERTY));
}

void EventFile::expand(Date from, Date to, std::vector<Occurrence>* out) const
{
    icalcomponent* event = master();
    if (!event)
        return;
    const Series series = seriesOf(event);
    if (series.start.isNull())
        return;

    if (!isRecurring()) {
        // An instance on its own, as a calendar without its series sends it.
        if (isOverride(event) && isCancelled(event))
            return;
        Occurrence occurrence = makeOccurrence(event, series, series.start);
        if (touches(occurrence, from, to))
            out->push_back(occurrence);
        return;
    }

    std::unordered_map<std::string, icalcomponent*> overrides;
    for (icalcomponent* other : components(m_calendar, ICAL_VEVENT_COMPONENT)) {
        if (isOverride(other))
            overrides.emplace(keyOf(propertyTime(other, ICAL_RECURRENCEID_PROPERTY)), other);
    }
    const std::unordered_set<std::string> excludedKeys = exclusions(event);

    const DateTime rangeStart(from, Time(0, 0));
    const DateTime rangeEnd(to, Time(0, 0));
    // Instances that start before the range may still reach into it.
    const DateTime skipTo = series.allDay ? rangeStart.addDays(-series.days - 1)
                                          : rangeStart.addSecs(-series.seconds).addDays(-1);

    instances(event, series, skipTo, rangeEnd, [&](const IcalTime& time) {
        const std::string key = keyOf(time);
        // A date names the timed instance on that day, as it does in EXDATE.
        if (overrides.contains(key) || (!time.isDate() && overrides.contains(dateKey(dateOf(time.wall))))
                || excluded(excludedKeys, time))
            return true;
        Occurrence occurrence = makeOccurrence(event, series, time);
        occurrence.recurrenceKey = key;
        occurrence.recurring = true;
        if (touches(occurrence, from, to))
            out->push_back(occurrence);
        return true;
    });

    // Changed instances appear where they were moved to, whatever range they came from.
    for (const auto& [key, changed] : overrides) {
        if (isCancelled(changed) || excludedKeys.contains(key))
            continue;
        const Series moved = seriesOf(changed);
        if (moved.start.isNull())
            continue;
        Occurrence occurrence = makeOccurrence(changed, moved, moved.start);
        occurrence.recurrenceKey = key;
        occurrence.recurring = true;
        if (touches(occurrence, from, to))
            out->push_back(occurrence);
    }
}

bool EventFile::nextOccurrence(const DateTime& after, int within, Occurrence* out) const
{
    std::vector<Occurrence> occurrences;
    const Date first = after.toLocalTime().date();
    expand(first, isRecurring() ? first.addDays(within) : Date(9999, 12, 31), &occurrences);

    const Occurrence* best = nullptr;
    DateTime bestStart;
    for (const Occurrence& occurrence : occurrences) {
        const DateTime start = occurrence.allDay ? DateTime(occurrence.firstDay, Time(0, 0)) : occurrence.start;
        if (start < after && !(occurrence.allDay && occurrence.firstDay == first))
            continue;
        if (!best || start < bestStart) {
            best = &occurrence;
            bestStart = start;
        }
    }
    if (best)
        *out = *best;
    return best;
}

std::vector<std::pair<DateTime, Occurrence>> EventFile::alarms(const DateTime& from, const DateTime& to) const
{
    std::vector<std::pair<DateTime, Occurrence>> result;
    const std::vector<icalcomponent*> events = components(m_calendar, ICAL_VEVENT_COMPONENT);
    if (std::none_of(events.cbegin(), events.cend(), [](icalcomponent* event) {
            return icalcomponent_get_first_component(event, ICAL_VALARM_COMPONENT) != nullptr;
        }))
        return result;

    const int reach = alarmReach(m_calendar);
    std::vector<Occurrence> occurrences;
    expand(from.toLocalTime().date().addDays(-reach - 1), to.toLocalTime().date().addDays(reach + 1), &occurrences);
    for (const Occurrence& occurrence : occurrences) {
        icalcomponent* event = occurrence.recurrenceKey.empty() ? master() : override(occurrence.recurrenceKey);
        if (!event)
            event = master();
        for (const DateTime& time : alarmTimes(event, occurrence)) {
            if (time >= from && time < to)
                result.emplace_back(time, occurrence);
        }
    }

    // A trigger at a set time goes off once, however far from the event and
    // however many occurrences surround it: for a repeating event, with the
    // first occurrence from then on.
    std::unordered_set<int64_t> fixed;
    for (icalcomponent* event : events) {
        for (const DateTime& time : fixedAlarmTimes(event)) {
            if (time < from || time >= to || !fixed.insert(time.toSecsSinceEpoch()).second)
                continue;
            Occurrence occurrence;
            if (!isRecurring()) {
                std::vector<Occurrence> own;
                expand(Date(1, 1, 1), Date(9999, 12, 31), &own);
                if (own.empty())
                    continue;
                occurrence = own.front();
            } else if (isOverride(event)) {
                const Series moved = seriesOf(event);
                if (moved.start.isNull())
                    continue;
                const std::string key = keyOf(propertyTime(event, ICAL_RECURRENCEID_PROPERTY));
                const Occurrence own = makeOccurrence(event, moved, moved.start);
                std::vector<Occurrence> around;
                expand(own.firstDay, own.lastDay.addDays(1), &around);
                const auto found = std::find_if(around.begin(), around.end(), [&key](const Occurrence& item) {
                    return item.recurrenceKey == key;
                });
                if (found == around.end())
                    continue;
                occurrence = *found;
            } else if (!nextOccurrence(time, 366, &occurrence)) {
                continue;
            }
            result.emplace_back(time, occurrence);
        }
    }
    return result;
}

EventData EventFile::data(const std::string& recurrenceKey) const
{
    EventData data;
    icalcomponent* event = master();
    if (!event)
        return data;

    icalcomponent* changed = override(recurrenceKey);
    icalcomponent* source = changed ? changed : event;
    data.summary = text(source, ICAL_SUMMARY_PROPERTY);
    data.location = text(source, ICAL_LOCATION_PROPERTY);
    data.description = text(source, ICAL_DESCRIPTION_PROPERTY);
    readAlarm(source, &data);

    const Series series = seriesOf(event);
    data.repeat = isRecurring() ? repeatOf(event, series) : Repeat();

    const Series timing = changed ? seriesOf(changed) : series;
    const IcalTime start = changed || recurrenceKey.empty() ? timing.start : instanceTime(series, recurrenceKey);
    data.allDay = timing.allDay;
    if (timing.allDay) {
        data.startDate = dateOf(start.wall);
        data.endDate = data.startDate.addDays(timing.days - 1);
    } else {
        data.start = toDateTime(start).toLocalTime();
        data.end = data.start.addSecs(timing.seconds);
    }
    return data;
}

// Writes the fields every event has; the zone of a timed start stays as it was.
static void writeFields(icalcomponent* calendar, icalcomponent* event, const EventData& data)
{
    setText(event, ICAL_SUMMARY_PROPERTY, data.summary);
    setText(event, ICAL_LOCATION_PROPERTY, data.location);
    setText(event, ICAL_DESCRIPTION_PROPERTY, data.description);

    const IcalTime old = propertyTime(event, ICAL_DTSTART_PROPERTY);
    const Zone zone = old.isNull() || old.isDate() ? systemZone() : old.zone;
    removeProperties(event, ICAL_DTSTART_PROPERTY);
    removeProperties(event, ICAL_DTEND_PROPERTY);
    removeProperties(event, ICAL_DURATION_PROPERTY);

    IcalTime start;
    IcalTime end;
    if (data.allDay) {
        start.wall = dateWall(data.startDate);
        end.wall = dateWall(std::max(data.endDate, data.startDate).addDays(1));
    } else {
        start.zone = zone;
        end.zone = zone;
        start.wall = toWall(data.start, zone);
        end.wall = toWall(std::max(data.end, data.start), zone);
        addTimezone(calendar, zone);
    }
    icalcomponent_add_property(event, newTimeProperty(ICAL_DTSTART_PROPERTY, start));
    icalcomponent_add_property(event, newTimeProperty(ICAL_DTEND_PROPERTY, end));
    writeAlarm(event, data);
}

void EventFile::setData(const EventData& data)
{
    icalcomponent* event = master();
    if (!event)
        return;

    const Series before = seriesOf(event);
    const Repeat oldRepeat = isRecurring() ? repeatOf(event, before) : Repeat();
    writeFields(m_calendar, event, data);
    const Series after = seriesOf(event);

    const bool kindChanged = before.allDay != after.allDay;
    const bool ruleChanged = data.repeat.frequency != Repeat::Custom && !(data.repeat == oldRepeat);
    // A new end alone keeps the same occurrences, and so their changes and removals.
    const bool frequencyChanged = ruleChanged && data.repeat.frequency != oldRepeat.frequency;
    if (ruleChanged) {
        removeProperties(event, ICAL_RRULE_PROPERTY);
        if (data.repeat.frequency == Repeat::None)
            removeProperties(event, ICAL_RDATE_PROPERTY);
        else
            icalcomponent_add_property(event, icalproperty_new_rrule(ruleFor(data.repeat, after)));
    }
    if (!frequencyChanged && !kindChanged && !before.start.isNull()) {
        // The instances moved with the start: so do the ones that were changed or removed.
        const int64_t seconds = before.allDay ? 0 : toDateTime(before.start).secsTo(toDateTime(after.start));
        const int64_t days = before.allDay ? dateOf(before.start.wall).daysTo(dateOf(after.start.wall)) : 0;
        if (seconds || days) {
            for (icalproperty* property : properties(event, ICAL_EXDATE_PROPERTY))
                shiftTime(property, event, seconds, days);
            for (icalcomponent* other : components(m_calendar, ICAL_VEVENT_COMPONENT)) {
                if (!isOverride(other))
                    continue;
                for (icalproperty_kind kind : { ICAL_RECURRENCEID_PROPERTY, ICAL_DTSTART_PROPERTY, ICAL_DTEND_PROPERTY }) {
                    if (icalproperty* property = icalcomponent_get_first_property(other, kind))
                        shiftTime(property, other, seconds, days);
                }
            }
        }
    }

    // Instances of another rule, or of another kind of start, are not these instances.
    if (frequencyChanged || kindChanged) {
        removeProperties(event, ICAL_EXDATE_PROPERTY);
        std::vector<icalcomponent*> stale;
        for (icalcomponent* other : components(m_calendar, ICAL_VEVENT_COMPONENT)) {
            if (isOverride(other))
                stale.push_back(other);
        }
        for (icalcomponent* other : stale) {
            icalcomponent_remove_component(m_calendar, other);
            icalcomponent_free(other);
        }
    }
    if (kindChanged && !ruleChanged && icalcomponent_get_first_property(event, ICAL_RRULE_PROPERTY)) {
        // UNTIL has to be a date for dates and a time for times.
        const Repeat repeat = repeatOf(event, before);
        icalproperty* property = icalcomponent_get_first_property(event, ICAL_RRULE_PROPERTY);
        icalrecurrencetype rule = icalproperty_get_rrule(property);
        if (repeat.until.isValid()) {
            Repeat ending;
            ending.until = repeat.until;
            ending.frequency = Repeat::Daily;
            rule.until = ruleFor(ending, after).until;
            icalproperty_set_rrule(property, rule);
        }
    }
    touch(event);
}

void EventFile::setOccurrenceData(const std::string& recurrenceKey, const EventData& data)
{
    icalcomponent* event = master();
    if (!event)
        return;

    icalcomponent* changed = override(recurrenceKey);
    if (!changed) {
        changed = icalcomponent_new_clone(event);
        removeProperties(changed, ICAL_RRULE_PROPERTY);
        removeProperties(changed, ICAL_RDATE_PROPERTY);
        removeProperties(changed, ICAL_EXDATE_PROPERTY);
        removeProperties(changed, ICAL_RECURRENCEID_PROPERTY);
        removeProperties(changed, ICAL_SEQUENCE_PROPERTY);
        icalcomponent_add_property(changed, newTimeProperty(ICAL_RECURRENCEID_PROPERTY,
                                                            instanceTime(seriesOf(event), recurrenceKey)));
        icalcomponent_add_component(m_calendar, changed);
    }
    EventData fields = data;
    fields.repeat = Repeat();
    writeFields(m_calendar, changed, fields);
    touch(changed);
}

void EventFile::resetOccurrence(const std::string& recurrenceKey)
{
    if (icalcomponent* changed = override(recurrenceKey)) {
        icalcomponent_remove_component(m_calendar, changed);
        icalcomponent_free(changed);
    }
}

void EventFile::excludeOccurrence(const std::string& recurrenceKey)
{
    icalcomponent* event = master();
    if (!event)
        return;
    if (icalcomponent* changed = override(recurrenceKey)) {
        icalcomponent_remove_component(m_calendar, changed);
        icalcomponent_free(changed);
    }
    icalcomponent_add_property(event, newTimeProperty(ICAL_EXDATE_PROPERTY,
                                                      instanceTime(seriesOf(event), recurrenceKey)));
    touch(event);
}

bool EventFile::isFirstOccurrence(const std::string& recurrenceKey) const
{
    icalcomponent* event = master();
    if (!event)
        return true;
    return !keyBefore(keyOf(seriesOf(event).start), recurrenceKey);
}

static DateTime endOfTime()
{
    return DateTime(Date(9999, 12, 31), Time(0, 0));
}

// As COUNT counts them: instances of the rule, not the ones RDATE adds.
static int instancesBefore(icalcomponent* event, const Series& series, const std::string& recurrenceKey)
{
    icalcomponent* rule = icalcomponent_new_clone(event);
    removeProperties(rule, ICAL_RDATE_PROPERTY);
    int count = 0;
    instances(rule, series, DateTime(), endOfTime(), [&](const IcalTime& time) {
        if (!keyBefore(keyOf(time), recurrenceKey))
            return false;
        ++count;
        return true;
    });
    icalcomponent_free(rule);
    return count;
}

bool EventFile::endBefore(const std::string& recurrenceKey)
{
    icalcomponent* event = master();
    if (!event || isFirstOccurrence(recurrenceKey))
        return false;

    const Series series = seriesOf(event);
    IcalTime last = series.start;
    instances(event, series, DateTime(), endOfTime(), [&](const IcalTime& time) {
        if (!keyBefore(keyOf(time), recurrenceKey))
            return false;
        // RDATE instances come after the rule's, whenever they are.
        if (keyBefore(keyOf(last), keyOf(time)))
            last = time;
        return true;
    });
    for (icalproperty* property : properties(event, ICAL_RRULE_PROPERTY)) {
        icalrecurrencetype rule = icalproperty_get_rrule(property);
        rule.count = 0;
        if (series.allDay) {
            rule.until = last.wall;
        } else {
            const Zone zone = series.start.zone.kind == Zone::Floating ? Zone() : utcZone();
            rule.until = toWall(toDateTime(last), zone);
            if (zone.kind == Zone::Utc)
                rule.until.zone = icaltimezone_get_utc_timezone();
        }
        icalproperty_set_rrule(property, rule);
    }

    // What came after it belongs to the part that was cut off.
    const auto later = [&](icalproperty* property, icalcomponent* owner) {
        icalvalue* value = icalproperty_get_value(property);
        if (!value)
            return false;
        icaltimetype time;
        if (icalvalue_isa(value) == ICAL_PERIOD_VALUE)
            time = icalvalue_get_period(value).start;
        else
            time = icalvalue_isa(value) == ICAL_DATE_VALUE ? icalvalue_get_date(value) : icalvalue_get_datetime(value);
        return !keyBefore(keyOf(makeTime(time, property, owner)), recurrenceKey);
    };
    for (icalproperty_kind kind : { ICAL_EXDATE_PROPERTY, ICAL_RDATE_PROPERTY }) {
        std::vector<icalproperty*> remove;
        for (icalproperty* property : properties(event, kind)) {
            if (later(property, event))
                remove.push_back(property);
        }
        for (icalproperty* property : remove) {
            icalcomponent_remove_property(event, property);
            icalproperty_free(property);
        }
    }
    std::vector<icalcomponent*> remove;
    for (icalcomponent* other : components(m_calendar, ICAL_VEVENT_COMPONENT)) {
        if (isOverride(other) && later(icalcomponent_get_first_property(other, ICAL_RECURRENCEID_PROPERTY), other))
            remove.push_back(other);
    }
    for (icalcomponent* other : remove) {
        icalcomponent_remove_component(m_calendar, other);
        icalcomponent_free(other);
    }
    touch(event);
    return true;
}

std::unique_ptr<EventFile> EventFile::splitAt(const std::string& recurrenceKey, const EventData& data)
{
    icalcomponent* event = master();
    if (!event || isFirstOccurrence(recurrenceKey))
        return nullptr;

    const Series series = seriesOf(event);
    const int before = instancesBefore(event, series, recurrenceKey);

    std::unique_ptr<EventFile> rest = clone();
    icalcomponent* restEvent = rest->master();
    const std::string uid = createUuid();
    for (icalcomponent* other : components(rest->m_calendar, ICAL_VEVENT_COMPONENT))
        icalcomponent_set_uid(other, uid.c_str());

    // The rest starts where it was cut, still lasting as long; earlier
    // changes and removals stay with the first part.
    std::vector<icalcomponent*> earlier;
    for (icalcomponent* other : components(rest->m_calendar, ICAL_VEVENT_COMPONENT)) {
        if (isOverride(other) && keyBefore(keyOf(propertyTime(other, ICAL_RECURRENCEID_PROPERTY)), recurrenceKey))
            earlier.push_back(other);
    }
    for (icalcomponent* other : earlier) {
        icalcomponent_remove_component(rest->m_calendar, other);
        icalcomponent_free(other);
    }
    std::vector<icalproperty*> removed;
    for (icalproperty* property : properties(restEvent, ICAL_EXDATE_PROPERTY)) {
        if (keyBefore(keyOf(makeTime(icalproperty_get_exdate(property), property, restEvent)), recurrenceKey))
            removed.push_back(property);
    }
    for (icalproperty* property : properties(restEvent, ICAL_RDATE_PROPERTY)) {
        const icaldatetimeperiodtype value = icalproperty_get_rdate(property);
        const IcalTime time = makeTime(icaltime_is_null_time(value.time) ? value.period.start : value.time, property,
                                       restEvent);
        if (keyBefore(keyOf(time), recurrenceKey))
            removed.push_back(property);
    }
    for (icalproperty* property : removed) {
        icalcomponent_remove_property(restEvent, property);
        icalproperty_free(property);
    }

    const IcalTime start = instanceTime(series, recurrenceKey);
    removeProperties(restEvent, ICAL_DTSTART_PROPERTY);
    removeProperties(restEvent, ICAL_DTEND_PROPERTY);
    removeProperties(restEvent, ICAL_DURATION_PROPERTY);
    IcalTime end = start;
    if (series.allDay)
        end.wall = dateWall(dateOf(start.wall).addDays(series.days));
    else
        end.wall = toWall(toDateTime(start).addSecs(series.seconds), start.zone);
    icalcomponent_add_property(restEvent, newTimeProperty(ICAL_DTSTART_PROPERTY, start));
    icalcomponent_add_property(restEvent, newTimeProperty(ICAL_DTEND_PROPERTY, end));
    for (icalproperty* property : properties(restEvent, ICAL_RRULE_PROPERTY)) {
        icalrecurrencetype rule = icalproperty_get_rrule(property);
        if (rule.count > 0) {
            rule.count = std::max(1, rule.count - before);
            icalproperty_set_rrule(property, rule);
        }
    }
    removeProperties(restEvent, ICAL_CREATED_PROPERTY);
    removeProperties(restEvent, ICAL_SEQUENCE_PROPERTY);

    // An unchanged rule keeps what is left of a count.
    EventData fields = data;
    if (fields.repeat == repeatOf(event, series))
        fields.repeat.frequency = Repeat::Custom;
    rest->setData(fields);

    endBefore(recurrenceKey);
    return rest;
}
