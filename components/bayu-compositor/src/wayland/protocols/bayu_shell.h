// Copyright 2026 Reion Wong <reionwong@gmail.com>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAYU_WAYLAND_PROTOCOLS_BAYU_SHELL_H_
#define BAYU_WAYLAND_PROTOCOLS_BAYU_SHELL_H_

#include <wayland-server-core.h>

#include <cstdint>

namespace bayu::wm {
class WindowManager;
}

namespace bayu::wayland {

class PrivilegedClients;

constexpr uint32_t kBayuShellVersion = 1;

// The per-display state the bayu_shell_v1 global hands to its resources.
//
// bayu_shell_v1 is the private extension the trusted client (the file
// manager's desktop) uses to say that an ordinary xdg surface is an output's
// desktop. Everything it carries is adapted into wm:: concepts; no wl_resource
// crosses into the window manager.
class WaylandBayuShell
{
public:
    WaylandBayuShell(wm::WindowManager* windowManager, PrivilegedClients* privileged);
    WaylandBayuShell(const WaylandBayuShell&) = delete;
    WaylandBayuShell& operator=(const WaylandBayuShell&) = delete;

    wm::WindowManager* windowManager() const { return m_windowManager; }
    PrivilegedClients* privileged() const { return m_privileged; }

private:
    wm::WindowManager* m_windowManager = nullptr;
    PrivilegedClients* m_privileged = nullptr;
};

// `shell` must outlive the global. Registering the returned global with the
// privileged client registry is what hides the interface from ordinary clients.
wl_global* createBayuShellGlobal(wl_display* display, WaylandBayuShell* shell);

} // namespace bayu::wayland

#endif // BAYU_WAYLAND_PROTOCOLS_BAYU_SHELL_H_
