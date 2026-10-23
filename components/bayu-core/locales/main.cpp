// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "localesservice.h"

#include "base/dbus.h"
#include "base/event_loop.h"
#include "base/logging.h"

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    bayu::base::EventLoop loop;
    static const bayu::base::LogCategory log("bayu.locales");

    LocalesService service;

    bayu::base::dbus::Connection *bus = bayu::base::dbus::Connection::system(&loop);
    if (!bus || !service.exportTo(bus) || !bus->requestName("org.bayu.Locales")) {
        CF_WARNING(log) << "bayu-locales: cannot register on the system bus";
        return 1;
    }

    service.onIdle = [&loop] {
        loop.quit();
    };

    return loop.run();
}
