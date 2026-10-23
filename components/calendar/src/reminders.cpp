// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "reminders.h"
#include "calendarstore.h"

#include <algorithm>

std::string Reminder::id() const
{
    return occurrence.key + '#' + occurrence.recurrenceKey + '@' + std::to_string(time.toSecsSinceEpoch());
}

std::vector<Reminder> Reminders::due(const CalendarStore& store, const DateTime& from, const DateTime& to)
{
    std::vector<Reminder> reminders;
    for (const std::string& key : store.keys()) {
        const EventFile* file = store.event(key);
        // A subscribed calendar's alerts are the publisher's, not the user's.
        if (!file || store.isReadOnly(CalendarStore::calendarOf(key)))
            continue;
        for (const auto& [time, occurrence] : file->alarms(from, to)) {
            Reminder reminder{time, occurrence};
            reminder.occurrence.key = key;
            reminder.occurrence.calendarId = CalendarStore::calendarOf(key);
            reminders.push_back(reminder);
        }
    }
    std::stable_sort(reminders.begin(), reminders.end(), [](const Reminder& a, const Reminder& b) {
        return a.time < b.time;
    });
    return reminders;
}
