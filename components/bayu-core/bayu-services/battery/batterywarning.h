// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef BATTERYWARNING_H
#define BATTERYWARNING_H

#include <cstdint>
#include <string>
#include <vector>

#include "base/dbus.h"
#include "notification.h"

// Warns at the levels UPower.conf sets, before UPower's critical power action.
class BatteryWarning
{
public:
    // UpDeviceLevel; UPower keeps it at None unless the battery is discharging.
    enum Level { Unknown, None, Discharging, Low, Critical, Action };

    // UPower on `system`, the notification on `session`.
    BatteryWarning(bayu::base::dbus::Connection *system, bayu::base::dbus::Connection *session);
    ~BatteryWarning();

    BatteryWarning(const BatteryWarning &) = delete;
    BatteryWarning &operator=(const BatteryWarning &) = delete;

private:
    void onPropertiesChanged(const bayu::base::dbus::VariantMap &changed,
                             const std::vector<std::string> &invalidated);
    void fetchProperties();
    void fetchCriticalAction();
    void update();
    void notify(uint32_t level);
    std::string body(uint32_t level) const;

    bayu::base::dbus::Proxy m_upower;
    bayu::base::dbus::Proxy m_device;
    bayu::base::dbus::Match m_changed;
    bayu::base::dbus::PendingCall m_propertiesCall;
    bayu::base::dbus::PendingCall m_actionCall;
    Notification m_notification;
    bayu::base::Subscription m_failed;
    bayu::base::dbus::VariantMap m_properties;
    std::string m_criticalAction;
    bool m_criticalActionKnown = false;
    // GetCriticalAction is in flight; its reply runs update() again.
    bool m_fetchingAction = false;
    uint32_t m_shownLevel = None;
};

#endif // BATTERYWARNING_H
