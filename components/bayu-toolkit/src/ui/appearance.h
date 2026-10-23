// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <systemd/sd-bus.h>

namespace bayu::base::dbus {
class Connection;
}

namespace bayu::ui {

// Keeps the theme in step with the user's appearance settings, which
// bayu-services keeps: dark mode, accent colour, font and icon theme.
class AppearanceWatcher
{
public:
    // Asks for the settings, then follows their change signals.
    explicit AppearanceWatcher(base::dbus::Connection& connection);
    ~AppearanceWatcher();

    AppearanceWatcher(const AppearanceWatcher&) = delete;
    AppearanceWatcher& operator=(const AppearanceWatcher&) = delete;

    // Moves the connection on without waiting, so the call goes out while
    // the caller does other work; nothing is applied yet.
    void advance();
    // Blocks until the settings asked for at construction have been applied,
    // or the call failed, so the first frame has the right colours.
    void waitForSettings();

private:
    static int changed(sd_bus_message* message, void* data, sd_bus_error* error);
    static int replied(sd_bus_message* reply, void* data, sd_bus_error* error);
    static void apply(sd_bus_message* reply);

    base::dbus::Connection& m_connection;
    sd_bus_slot* m_match = nullptr;
    sd_bus_slot* m_call = nullptr;
    // The first reply, held until waitForSettings() applies it.
    bool m_waited = false;
    sd_bus_message* m_firstReply = nullptr;
};

} // namespace bayu::ui
