// SPDX-License-Identifier: BSD-3-Clause

#ifndef BAYU_SHELL_UPDATES_UPDATE_MONITOR_H_
#define BAYU_SHELL_UPDATES_UPDATE_MONITOR_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/dbus.h"
#include "base/signal.h"
#include "base/thread_pool.h"
#include "base/timer.h"
#include "shell/common/pending_calls.h"

namespace bayu::packagekit {
class Transaction;
}

namespace bayu::shell {

// The updates waiting: the system's, through PackageKit (what
// `dnf upgrade --refresh` would install), and the Flatpak applications'.
// Checked a minute after login, every six hours and on Check(). New updates
// are told once in a notification; the count goes on the Settings icon.
//
// With Updates/AutoDownload (off by default) the system's are downloaded in
// the background, and the user is asked to restart to install them: nothing
// is installed without the user (PackageKit's offline update installs them
// at the restart).
//
// org.bayu.Updates at /Updates on the session bus, for Settings.
class UpdateMonitor
{
public:
    enum class State { Idle, Checking, Downloading, Ready };

    // `settingsPath` is shell.conf.
    UpdateMonitor(base::dbus::Connection *bus, base::dbus::Connection *systemBus, std::string settingsPath);
    ~UpdateMonitor();

    UpdateMonitor(const UpdateMonitor &) = delete;
    UpdateMonitor &operator=(const UpdateMonitor &) = delete;

    bool registerService();
    // Starts the checks.
    void start();
    void check();

    int systemUpdates() const { return int(m_system.size()); }
    int securityUpdates() const { return m_security; }
    int appUpdates() const { return m_apps; }
    int total() const { return systemUpdates() + appUpdates(); }
    State state() const { return m_state; }
    bool autoDownload() const { return m_autoDownload; }
    void setAutoDownload(bool enabled);

    // What the notifications and the bar's icon open.
    std::function<void()> openSystemUpdates;
    std::function<void()> openAppUpdates;
    // The user chose to restart and install.
    std::function<void()> restart;

    base::Signal<> changed;

private:
    void readPrepared();
    void checkSystem();
    void listSystem();
    void checkApps();
    void checked();
    void download();
    void setState(State state);
    void notify(const std::string &title, const std::string &body, const std::vector<std::string> &actions,
                std::function<void(const std::string &action)> invoked);
    void updateBadge();
    void publish();

    base::dbus::Connection *m_bus;
    base::dbus::Connection *m_systemBus;
    std::string m_settingsPath;
    std::unique_ptr<base::dbus::Object> m_object;
    base::dbus::Proxy m_notifications;
    PendingCalls m_calls;
    std::vector<base::dbus::Match> m_matches;
    std::unique_ptr<packagekit::Transaction> m_transaction;
    base::Subscription m_transactionDone;
    base::Job m_appsJob;
    base::Timer m_first;
    base::Timer m_periodic;
    State m_state = State::Idle;
    bool m_autoDownload = false;
    // Package ids.
    std::vector<std::string> m_system;
    int m_security = 0;
    int m_apps = 0;
    bool m_systemDone = false;
    bool m_appsDone = false;
    // What was told last, so the same updates are told once.
    std::set<std::string> m_told;
    int m_toldApps = 0;
    // The notification id and what its actions do.
    uint32_t m_notification = 0;
    std::function<void(const std::string &)> m_invoked;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

} // namespace bayu::shell

#endif // BAYU_SHELL_UPDATES_UPDATE_MONITOR_H_
