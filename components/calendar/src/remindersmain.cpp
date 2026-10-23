// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "calendarstore.h"
#include "reminderservice.h"

#include <base/dbus.h>
#include <base/event_loop.h>
#include <base/i18n.h>
#include <base/locale.h>
#include <base/session_language.h>


int main()
{
    bayu::base::Locale::setProcessLocale();
    bayu::i18n::setDomain("bayu-calendar");
    bayu::base::EventLoop loop;
    bayu::base::dbus::Connection* bus = bayu::base::dbus::Connection::session(&loop);
    if (!bus || !bus->requestName("org.bayu.CalendarReminders"))
        return 0;

    bayu::base::SessionLanguage language(bus);
    CalendarStore store;
    ReminderService service(&store, bus);
    store.loadAsync();
    return loop.run();
}
