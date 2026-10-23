// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "reminderservice.h"
#include "calendarstore.h"
#include "localedata.h"

#include <base/i18n.h>

#include <algorithm>

#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace dbus = bayu::base::dbus;
using bayu::i18n::format;
using bayu::i18n::tr;
using bayu::i18n::trn;

// A snoozed reminder comes back after this long.
static const int SnoozeMinutes = 10;
// Alarms just before the service started, as at login, still show.
static const int CatchUpMinutes = 10;
// Checked at least this often, so a changed clock is noticed.
static const int64_t LongestWaitSeconds = 60 * 60;

static std::string escaped(const std::string& text)
{
    std::string result;
    for (char c : text) {
        switch (c) {
        case '&':
            result += "&amp;";
            break;
        case '<':
            result += "&lt;";
            break;
        case '>':
            result += "&gt;";
            break;
        case '"':
            result += "&quot;";
            break;
        default:
            result.push_back(c);
        }
    }
    return result;
}

static std::string timeText(const Occurrence& occurrence)
{
    const Date today = Date::currentDate();
    if (occurrence.allDay) {
        if (occurrence.firstDay == today)
            return tr("Today, all day");
        if (occurrence.firstDay == today.addDays(1))
            return tr("Tomorrow, all day");
        return LocaleData::longDate(occurrence.firstDay);
    }

    std::string range = LocaleData::timeText(occurrence.start.time());
    if (occurrence.end > occurrence.start)
        // TRANSLATORS: a time range, such as "9:00 – 10:00".
        range = format(tr("{start} – {end}"),
                       {{"start", range}, {"end", LocaleData::timeText(occurrence.end.time())}});
    if (occurrence.start.date() == today)
        return range;
    if (occurrence.start.date() == today.addDays(1))
        return format(tr("Tomorrow, {time}"), {{"time", range}});
    // TRANSLATORS: a reminder's date and time, such as "Wednesday, 7 October, 9:00 – 10:00".
    return format(tr("{date}, {time}"), {{"date", LocaleData::longDate(occurrence.start.date())}, {"time", range}});
}

// Through the session's application runtime, or as a process of its own
// when the runtime is not there.
static void startCalendar(dbus::Connection* bus, const std::vector<std::string>& command)
{
    dbus::Proxy runtime(bus, "org.bayu.AppRuntime", "/AppRuntime", "org.bayu.AppRuntime");
    runtime.setTimeout(std::chrono::seconds(5));
    const dbus::Reply reply = runtime.call("launchCommand", std::string("bayu-calendar"), command, std::string(),
                                           std::string());
    if (reply.value<uint32_t>(0).value_or(0) != 0)
        return;

    std::vector<char*> arguments;
    for (const std::string& argument : command)
        arguments.push_back(const_cast<char*>(argument.c_str()));
    arguments.push_back(nullptr);
    // Detached: the child starts the calendar and leaves at once.
    const pid_t child = fork();
    if (child == 0) {
        setsid();
        pid_t pid;
        _exit(posix_spawnp(&pid, arguments[0], nullptr, nullptr, arguments.data(), environ) == 0 ? 0 : 1);
    }
    if (child > 0)
        waitpid(child, nullptr, 0);
}

ReminderService::ReminderService(CalendarStore* store, dbus::Connection* bus)
    : m_store(store)
    , m_bus(bus)
    , m_notifications(bus, "org.freedesktop.Notifications", "/org/freedesktop/Notifications",
                      "org.freedesktop.Notifications")
{
    m_timer.setSingleShot(true);
    m_timer.onTimeout = [this] { check(); };
    m_changed = m_store->changed.connect([this] { check(); });
    m_checked = DateTime::currentDateTime().addSecs(-CatchUpMinutes * 60);

    m_actionInvoked = m_notifications.onSignal("ActionInvoked", [this](const dbus::Message& message) {
        uint32_t id = 0;
        std::string key;
        if (message.read(id, key))
            actionInvoked(id, key);
    });
    m_closed = m_notifications.onSignal("NotificationClosed", [this](const dbus::Message& message) {
        uint32_t id = 0;
        uint32_t reason = 0;
        if (message.read(id, reason))
            m_shown.erase(id);
    });
}

void ReminderService::check()
{
    if (!m_store->isReady())
        return;

    const DateTime now = DateTime::currentDateTime();
    const DateTime until = now.addSecs(1);
    std::vector<Reminder> due = m_checked < until ? Reminders::due(*m_store, m_checked, until) : std::vector<Reminder>();
    for (auto it = m_snoozed.begin(); it != m_snoozed.end();) {
        if (it->time <= now) {
            due.push_back(*it);
            it = m_snoozed.erase(it);
        } else {
            ++it;
        }
    }

    for (const Reminder& reminder : due) {
        const std::string id = reminder.id();
        if (m_fired.contains(id))
            continue;
        m_fired.emplace(id, reminder.time);
        // Missed while asleep and over by now: no longer worth a notification.
        const Occurrence& occurrence = reminder.occurrence;
        const DateTime end = occurrence.allDay ? DateTime(occurrence.lastDay.addDays(1), Time(0, 0)) : occurrence.end;
        if (end <= now && reminder.time < now.addSecs(-CatchUpMinutes * 60))
            continue;
        notify(reminder);
    }
    m_checked = until;
    std::erase_if(m_fired, [&now](const auto& item) {
        return item.second < now.addDays(-1);
    });

    DateTime next = now.addSecs(LongestWaitSeconds);
    const std::vector<Reminder> upcoming = Reminders::due(*m_store, until, next);
    if (!upcoming.empty())
        next = std::min(next, upcoming.front().time);
    for (const Reminder& reminder : m_snoozed)
        next = std::min(next, reminder.time);
    m_timer.start(std::chrono::seconds(std::clamp<int64_t>(now.secsTo(next), 1, LongestWaitSeconds)));
}

void ReminderService::notify(const Reminder& reminder)
{
    const Occurrence& occurrence = reminder.occurrence;
    std::string body = timeText(occurrence);
    if (!occurrence.location.empty())
        body += '\n' + occurrence.location;
    const std::vector<std::string> actions{"default", "", "snooze",
                                           trn("Snooze {n} Minute", "Snooze {n} Minutes", SnoozeMinutes)};
    const dbus::VariantMap hints{
        {"urgency", dbus::Variant(uint8_t(1))},
        {"desktop-entry", dbus::Variant(std::string("bayu-calendar"))},
    };

    // The reply brings the id that its actions arrive with.
    std::erase_if(m_pending, [](const Pending& pending) { return *pending.done; });
    auto done = std::make_shared<bool>(false);
    dbus::PendingCall call = m_notifications.callAsync("Notify", [this, reminder, done](const dbus::Reply& reply) {
        *done = true;
        if (const std::optional<uint32_t> id = reply.value<uint32_t>(0); id && *id != 0)
            m_shown[*id] = reminder;
    }, std::string(), uint32_t(0), std::string("office-calendar"),
       occurrence.summary.empty() ? std::string(tr("New Event")) : occurrence.summary, escaped(body), actions, hints,
       int32_t(0));
    m_pending.push_back({std::move(call), done});
}

void ReminderService::actionInvoked(uint32_t id, const std::string& key)
{
    const auto it = m_shown.find(id);
    if (it == m_shown.end())
        return;
    const Reminder reminder = it->second;
    m_shown.erase(it);

    if (key == "snooze") {
        Reminder later = reminder;
        later.time = DateTime::currentDateTime().addSecs(SnoozeMinutes * 60);
        m_snoozed.push_back(later);
        check();
        return;
    }
    const Occurrence& occurrence = reminder.occurrence;
    const Date date = occurrence.allDay ? occurrence.firstDay : occurrence.start.date();
    startCalendar(m_bus, {"bayu-calendar", "--date", date.toIsoString()});
}
