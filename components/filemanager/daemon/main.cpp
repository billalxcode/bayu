// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "fileoperationsservice.h"

#include "base/dbus.h"
#include "base/event_loop.h"
#include "base/i18n.h"
#include "base/locale.h"
#include "base/session_language.h"

#include <cstdio>

using namespace bayu;

int main()
{
    base::Locale::setProcessLocale();
    i18n::setDomain("bayu-filemanager");
    base::EventLoop loop;
    base::dbus::Connection *bus = base::dbus::Connection::session(&loop);
    if (!bus) {
        std::fprintf(stderr, "Cannot connect to the session D-Bus.\n");
        return 1;
    }

    base::SessionLanguage language(bus);
    FileOperationsService service(bus);
    if (!service.publish() || !bus->requestName("org.bayu.FileOperations")) {
        std::fprintf(stderr, "Cannot own org.bayu.FileOperations.\n");
        return 1;
    }

    return loop.run();
}
