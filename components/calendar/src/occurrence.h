// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "datetime.h"

#include <string>

// One appearance of an event on the calendar: the event itself, or one
// instance of a repeating one.
struct Occurrence
{
    // "<calendar id>/<file name>", the event's file in the store.
    std::string key;
    // Which instance of a repeating event this is; empty for a single event.
    std::string recurrenceKey;
    std::string summary;
    std::string location;
    bool allDay = false;
    bool recurring = false;
    DateTime start;
    DateTime end;
    // The local days it touches, last one included.
    Date firstDay;
    Date lastDay;
    std::string calendarId;
};
