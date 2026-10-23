// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "privacyservice.h"

#include "base/dbus.h"
#include "base/event_loop.h"
#include "base/logging.h"

// The udev rules test the files in here; systemd's StateDirectory creates it.
static constexpr char kStateDirectory[] = "/var/lib/bayu-privacy";

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    bayu::base::EventLoop loop;
    static const bayu::base::LogCategory log("bayu.privacy");

    PrivacyService service(kStateDirectory);

    bayu::base::dbus::Connection *bus = bayu::base::dbus::Connection::system(&loop);
    if (!bus || !service.exportTo(bus) || !bus->requestName("org.bayu.Privacy")) {
        CF_WARNING(log) << "bayu-privacy: cannot register on the system bus";
        return 1;
    }

    service.onIdle = [&loop] {
        loop.quit();
    };

    return loop.run();
}
