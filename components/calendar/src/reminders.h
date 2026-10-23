// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "occurrence.h"

#include <string>
#include <vector>

class CalendarStore;

struct Reminder
{
    DateTime time;
    Occurrence occurrence;

    // Names this alarm of this occurrence, to fire it once.
    std::string id() const;
};

namespace Reminders
{
std::vector<Reminder> due(const CalendarStore& store, const DateTime& from, const DateTime& to);
}
