// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef LOGINBACKGROUND_H
#define LOGINBACKGROUND_H

#include <vector>

#include "base/dbus.h"
#include "base/signal.h"
#include "base/timer.h"

class WallpaperManager;

/**
 * Hands the desktop background to bayu-dm, which keeps a copy the login
 * screen can read: once at login, then after every change. The picture goes
 * as an open file, so the display manager reads only what this user can.
 */
class LoginBackground
{
public:
    LoginBackground(WallpaperManager &wallpapers, bayu::base::dbus::Connection *system);

    LoginBackground(const LoginBackground &) = delete;
    LoginBackground &operator=(const LoginBackground &) = delete;

private:
    void send();

    WallpaperManager &m_wallpapers;
    bayu::base::dbus::Connection *m_system;
    // A wallpaper, its type and its colour often change together: one send.
    bayu::base::Timer m_pending;
    std::vector<bayu::base::Subscription> m_subscriptions;
};

#endif // LOGINBACKGROUND_H
