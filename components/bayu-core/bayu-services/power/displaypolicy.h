// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef DISPLAYPOLICY_H
#define DISPLAYPOLICY_H

#include <cstdint>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "base/dbus.h"
#include "base/elapsed_timer.h"
#include "base/signal.h"
#include "base/timer.h"
#include "bayu/display/display_client.h"

class BrightnessManager;
class WaylandIdle;

// Turns the display off when the user is away, then sleeps; handles the lid.
class DisplayPolicy
{
public:
    // The screen saver on `session`, logind on `system`; `displayService`
    // names the compositor's display service, which tests run themselves.
    // Without `idle`, as in tests, the caller reports idleness itself.
    DisplayPolicy(WaylandIdle *idle, BrightnessManager *brightness,
                  bayu::base::dbus::Connection *session, bayu::base::dbus::Connection *system,
                  const std::string &displayService = "org.bayu.DisplayConfig");
    ~DisplayPolicy();

    DisplayPolicy(const DisplayPolicy &) = delete;
    DisplayPolicy &operator=(const DisplayPolicy &) = delete;

    // The user has been away for `seconds`, one of the watched durations.
    void idleFor(int seconds);
    // The user came back after at least one idleFor().
    void userReturned();

    void setPolicy(int timeout, int suspendDelay);
    void setLidPresent(bool present);
    void handleLidClosed();
    void handleLidOpened();

private:
    int watchIdle(int seconds);
    void unwatchIdle(int identifier);

    void onPrepareForSleep(bool sleeping);
    void onScreenSaverActiveChanged(bool active);
    void onLogindPropertiesChanged(const bayu::base::dbus::VariantMap &changedProperties,
                                   const std::vector<std::string> &invalidatedProperties);
    void onSuspendTimerTimeout();
    void onDisplayStateChanged();

    bool lockSession();
    void trySuspend();
    void fade(int divisor);
    void turnDisplayOff();
    void restoreDisplay();
    void startSuspendCountdown();
    void stopSuspendCountdown();
    void turnDisplayOffWhenIdle();
    void sleepWithLidClosed(bool waitForLock);
    bool idleSleepApplies() const;
    void updateLidSwitchInhibitor();
    bool hasNativeDisplayPower() const;
    bool setDisplayPower(bool on);
    void requestDockedLid(bool closed);
    void finishDockedLid(bool closed, bool succeeded, bayu::display::ErrorCode error);
    int screenBrightness() const;
    void setScreenBrightness(int value);

    WaylandIdle *m_idle;
    BrightnessManager *m_brightness;
    bayu::base::dbus::Proxy m_screenSaver;
    bayu::base::dbus::Proxy m_logind;
    bayu::display::DisplayClient m_display;
    std::vector<bayu::base::dbus::Match> m_matches;
    std::vector<bayu::base::Subscription> m_idleSubscriptions;
    bayu::base::Subscription m_displayStateChanged;
    bayu::base::dbus::PendingCall m_suspendCall;
    bayu::base::dbus::PendingCall m_inhibitCall;
    std::set<int> m_idleIds;
    // One of the watched durations has passed since the user last came back.
    bool m_away = false;
    int m_timeout = 0;
    int m_suspendDelay = 900;
    // The brightness to return to; -1 while nothing has lowered it.
    int m_savedBrightness = -1;
    bool m_dimmed = false;
    bool m_displayPoweredOff = false;
    bool m_displayOff = false;
    bool m_suspendRequested = false;
    bool m_suspendDue = false;
    uint64_t m_suspendSerial = 0;
    bool m_lidClosed = false;
    bool m_lidPresent = false;
    bool m_lidDisplayOff = false;
    // Closed with no display in use: logind handles the lid until it opens.
    bool m_lidSleeping = false;
    bool m_locked = false;
    // The user has been away for the locked timeout since their last input.
    bool m_lockedIdle = false;
    int m_lockedIdleTimeout = -1;
    std::vector<int> m_policyIdleTimeouts;
    bool m_dockedLidPending = false;
    bool m_dockedLidActive = false;
    bool m_dockedLidRecheck = false;
    int m_dockedLidRetries = 0;
    uint64_t m_dockedLidSerial = 0;
    bayu::base::Timer m_dockedLidRetryTimer;
    std::tuple<std::string, uint64_t, uint64_t> m_displayTopology;

    bayu::base::dbus::UnixFd m_lidSwitchInhibitor;
    bool m_lidSwitchInhibiting = false;

    bayu::base::Timer m_suspendTimer;
    bayu::base::Timer m_wakeTimer;
    bayu::base::ElapsedTimer m_displayOffTimer;
};

#endif // DISPLAYPOLICY_H
