// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "calendarmodel.h"
#include "calendarstore.h"
#include "reminderservice.h"

#include <base/dbus.h>
#include <base/event_loop.h>

#include <gtest/gtest.h>

#include <filesystem>

namespace fs = std::filesystem;
namespace base = bayu::base;
namespace dbus = bayu::base::dbus;

namespace {

// What a notification server was asked to show.
struct Shown
{
    std::string application;
    uint32_t replaces = 0;
    std::string icon;
    std::string summary;
    std::string body;
    std::vector<std::string> actions;
    dbus::VariantMap hints;
    int32_t timeout = -1;
};

}

static bool runUntil(base::EventLoop& loop, const std::function<bool()>& condition,
                     std::chrono::milliseconds timeout = std::chrono::seconds(5))
{
    if (condition())
        return true;
    const int check = loop.startTimer(std::chrono::milliseconds(10), true, [&] {
        if (condition())
            loop.quit();
    });
    const int expire = loop.startTimer(timeout, false, [&] { loop.quit(); });
    loop.run();
    loop.stopTimer(check);
    loop.stopTimer(expire);
    return condition();
}

// Runs in dbus-run-session: a private notification server on the same bus.
TEST(ReminderServiceTest, NotifiesDueAlarms)
{
    base::EventLoop loop;
    dbus::Connection* bus = dbus::Connection::session(&loop);
    ASSERT_TRUE(bus);
    std::unique_ptr<dbus::Connection> server = dbus::Connection::open(dbus::Connection::Bus::Session, &loop);
    ASSERT_TRUE(server);
    ASSERT_TRUE(server->requestName("org.freedesktop.Notifications"));
    std::vector<Shown> shown;
    dbus::Object notifications(server.get(), "/org/freedesktop/Notifications", "org.freedesktop.Notifications");
    notifications.addMethod("Notify", [&shown](const std::string& application, uint32_t replaces, const std::string& icon,
                                               const std::string& summary, const std::string& body,
                                               const std::vector<std::string>& actions, const dbus::VariantMap& hints,
                                               int32_t timeout) {
        shown.push_back({application, replaces, icon, summary, body, actions, hints, timeout});
        return uint32_t(shown.size());
    });
    ASSERT_TRUE(notifications.publish());

    std::string pattern = (fs::temp_directory_path() / "calendar-reminders-XXXXXX").string();
    const std::string dir = mkdtemp(pattern.data());
    {
        CalendarStore store(dir + "/calendars");
        CalendarModel model(&store, dir + "/calendar.conf");
        store.load();
        // Five minutes from now, its alert went off five minutes ago.
        const DateTime start = DateTime::currentDateTime().addSecs(5 * 60);
        EventForm form = model.newEventData(start, start.addSecs(1800), false);
        form.title = "Call <Sam>";
        form.location = "Room 4";
        form.alarm = EventData::OffsetAlarm;
        form.alarmOffset = -10;
        ASSERT_TRUE(model.saveEvent(form, EventFile::AllEvents));
    }

    CalendarStore store(dir + "/calendars");
    ReminderService service(&store, bus);
    store.loadAsync();
    ASSERT_TRUE(runUntil(loop, [&] { return !shown.empty(); }));
    const Shown& notification = shown.front();
    EXPECT_EQ(notification.application, "");
    EXPECT_EQ(notification.replaces, 0u);
    EXPECT_EQ(notification.icon, "office-calendar");
    EXPECT_EQ(notification.summary, "Call <Sam>");
    EXPECT_NE(notification.body.find("\nRoom 4"), std::string::npos) << notification.body;
    EXPECT_EQ(notification.actions, (std::vector<std::string>{"default", "", "snooze", "Snooze 10 Minutes"}));
    EXPECT_EQ(notification.hints.at("desktop-entry").get<std::string>().value_or(""), "bayu-calendar");
    EXPECT_EQ(notification.hints.at("urgency").get<uint8_t>().value_or(0), 1);
    EXPECT_EQ(notification.timeout, 0);

    // Fired once, however often the calendars change.
    store.changed.emit();
    runUntil(loop, [] { return false; }, std::chrono::milliseconds(300));
    EXPECT_EQ(shown.size(), 1u);

    std::error_code error;
    fs::remove_all(dir, error);
}
