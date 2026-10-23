// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "reminders.h"

#include <base/dbus.h>
#include <base/signal.h>
#include <base/timer.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class CalendarStore;

// Shows the calendar's alarms as notifications while the calendar is closed.
class ReminderService
{
public:
    ReminderService(CalendarStore* store, bayu::base::dbus::Connection* bus);

    ReminderService(const ReminderService&) = delete;
    ReminderService& operator=(const ReminderService&) = delete;

private:
    void check();
    void notify(const Reminder& reminder);
    void actionInvoked(uint32_t id, const std::string& key);

    CalendarStore* m_store;
    bayu::base::dbus::Connection* m_bus;
    bayu::base::dbus::Proxy m_notifications;
    bayu::base::Timer m_timer;
    DateTime m_checked;
    // Fired alarms, kept until a day after they went off.
    std::unordered_map<std::string, DateTime> m_fired;
    std::vector<Reminder> m_snoozed;
    // Notifications on screen by their server id.
    std::map<uint32_t, Reminder> m_shown;
    struct Pending
    {
        bayu::base::dbus::PendingCall call;
        std::shared_ptr<bool> done;
    };
    std::vector<Pending> m_pending;
    bayu::base::dbus::Match m_actionInvoked;
    bayu::base::dbus::Match m_closed;
    bayu::base::Subscription m_changed;
};
