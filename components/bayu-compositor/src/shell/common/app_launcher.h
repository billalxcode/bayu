// Copyright 2026 Reion Wong <reionwong@gmail.com>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAYU_SHELL_COMMON_APP_LAUNCHER_H_
#define BAYU_SHELL_COMMON_APP_LAUNCHER_H_

#include <functional>
#include <string>
#include <vector>

#include "base/dbus.h"
#include "shell/common/pending_calls.h"

namespace bayu::base {
struct DesktopEntry;
} // namespace bayu::base

namespace bayu::shell {

// Bayu Store's opener of Android apps: it shows that Android starts again
// when it was turned off.
inline constexpr const char *kAndroidLaunch = "/usr/libexec/bayu-store/android-launch";

// The command an entry of lapis' runs instead, `lapis app launch <package>`
// through `opener`; empty for any other command.
std::vector<std::string> androidLaunchCommand(const std::vector<std::string> &command,
                                              const std::string &opener = kAndroidLaunch);

// Starts applications the way every start in the session goes: through the
// session's AppRuntime (org.bayu.AppRuntime), which puts each in a scope
// of its own, or here when the runtime does not answer. Nothing waits: the
// runtime's answer, or its absence, arrives later.
class ApplicationLauncher
{
public:
    using Done = std::function<void(bool started)>;

    explicit ApplicationLauncher(base::dbus::Connection *bus = base::dbus::Connection::session());

    // The entry by its id, as DesktopEntry::launch() did: the runtime starts
    // it and ties it to the entry; its command is the fallback.
    void launch(const base::DesktopEntry &entry, const std::vector<std::string> &arguments = {},
                Done done = {});
    // A command for the application `appId`.
    void launchCommand(const std::vector<std::string> &command, const std::string &workingDirectory,
                       const std::string &appId, Done done = {});
    // Asks the runtime to end what still runs of the application.
    void quitApplication(const std::string &appId);
    // Runs a program of the session's own, such as Settings, on its own;
    // `environment` holds `NAME=value` entries set over the compositor's.
    static bool startDetached(const std::string &program, const std::vector<std::string> &arguments = {},
                              const std::vector<std::string> &environment = {});
    // A token from the compositor that lets the next window an application
    // maps take the focus; empty without the activation protocol.
    static std::string activationToken();
    static void setActivationTokenSource(std::function<std::string()> source);
    // Opens a link or a file with xdg-open, the application it opens in coming
    // to the front with a token (issue #94).
    static bool openUrl(const std::string &url);

private:
    base::dbus::Proxy m_runtime;
    PendingCalls m_calls;
};

} // namespace bayu::shell

#endif // BAYU_SHELL_COMMON_APP_LAUNCHER_H_
