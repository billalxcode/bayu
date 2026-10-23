// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "occurrence.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

typedef struct icalcomponent_impl icalcomponent;

// How an event repeats, as far as the editor shows it. Anything the editor
// cannot show is Custom and left as it is when the event is saved.
struct Repeat
{
    enum Frequency { None, Daily, Weekdays, Weekly, Monthly, Yearly, Custom };

    Frequency frequency = None;
    // Ends after count occurrences, or on the day until; never when neither is set.
    int count = 0;
    Date until;

    bool operator==(const Repeat&) const = default;
};

// The fields the editor changes, for one event or one occurrence of it.
struct EventData
{
    enum Alarm { NoAlarm, OffsetAlarm, CustomAlarm };

    std::string summary;
    std::string location;
    std::string description;
    bool allDay = false;
    DateTime start;
    DateTime end;
    // All-day events; the last day is included.
    Date startDate;
    Date endDate;
    Repeat repeat;
    Alarm alarm = NoAlarm;
    // Minutes from the start; negative is before it.
    int alarmOffset = 0;
};

// What a published calendar says about itself.
struct FeedInfo
{
    std::string name;
    // How often it asks to be fetched again; 0 when it does not say.
    int64_t refreshSeconds = 0;
};

// One event and the instances of it that were changed on their own. Fields the
// editor does not show, from imported or synced files, are kept.
class EventFile
{
public:
    enum Scope { ThisEvent, FutureEvents, AllEvents };

    ~EventFile();

    static std::unique_ptr<EventFile> fromData(std::string_view data);
    // One file per event in a calendar of many, as an import needs them.
    static std::vector<std::unique_ptr<EventFile>> split(std::string_view data);
    static FeedInfo feedInfo(std::string_view data);
    static std::unique_ptr<EventFile> create(const EventData& data);

    std::unique_ptr<EventFile> clone() const;
    std::string toData() const;
    static std::string join(const std::vector<const EventFile*>& files);

    std::string uid() const;
    std::string summary() const;
    std::string location() const;
    std::string description() const;
    bool isRecurring() const;

    // `to` is excluded.
    void expand(Date from, Date to, std::vector<Occurrence>* out) const;
    // The first occurrence starting at or after `after`; for a repeating
    // event the search stops `within` days on.
    bool nextOccurrence(const DateTime& after, int within, Occurrence* out) const;
    std::vector<std::pair<DateTime, Occurrence>> alarms(const DateTime& from, const DateTime& to) const;

    EventData data(const std::string& recurrenceKey = {}) const;
    // Sets the fields of the whole event; a changed start moves every occurrence.
    void setData(const EventData& data);
    void setOccurrenceData(const std::string& recurrenceKey, const EventData& data);
    void resetOccurrence(const std::string& recurrenceKey);
    void excludeOccurrence(const std::string& recurrenceKey);
    // Ends the series before this occurrence. False when nothing is left.
    bool endBefore(const std::string& recurrenceKey);
    // The returned event, with a new UID, takes this occurrence and the later ones;
    // null for the first occurrence, which needs setData instead.
    std::unique_ptr<EventFile> splitAt(const std::string& recurrenceKey, const EventData& data);
    bool isFirstOccurrence(const std::string& recurrenceKey) const;

private:
    explicit EventFile(icalcomponent* calendar);

    icalcomponent* master() const;
    icalcomponent* override(const std::string& recurrenceKey) const;
    void touch(icalcomponent* event) const;

    icalcomponent* m_calendar = nullptr;
};
