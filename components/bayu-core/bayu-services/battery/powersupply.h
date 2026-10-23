// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef POWERSUPPLY_H
#define POWERSUPPLY_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/dbus.h"
#include "base/settings.h"
#include "base/signal.h"

class Battery;

// What UPower knows of the power source and the lid, and the computer's batteries.
class PowerSupply
{
public:
    // UPower on `system`; the battery that holds the path is published on `session`.
    PowerSupply(bayu::base::dbus::Connection *system, bayu::base::dbus::Connection *session,
                bayu::base::Settings &settings);
    ~PowerSupply();

    PowerSupply(const PowerSupply &) = delete;
    PowerSupply &operator=(const PowerSupply &) = delete;

    bool onBattery() const { return m_onBattery; }
    bool hasLid() const { return m_hasLid; }
    bool lidClosed() const { return m_lidClosed; }

    bayu::base::Signal<> powerSourceChanged;
    bayu::base::Signal<> lidChanged;

private:
    void readFlags(const bayu::base::dbus::VariantMap &values);
    void addBattery(const std::string &path);
    void removeBattery(const std::string &path);
    void publishBattery();

    bayu::base::dbus::Connection *m_system;
    bayu::base::dbus::Connection *m_session;
    bayu::base::Settings &m_settings;
    bayu::base::dbus::Proxy m_upower;
    std::vector<bayu::base::dbus::Match> m_matches;
    // By UPower device path.
    std::map<std::string, std::unique_ptr<Battery>> m_batteries;
    Battery *m_published = nullptr;
    bool m_onBattery = false;
    bool m_hasLid = false;
    bool m_lidClosed = false;
};

#endif // POWERSUPPLY_H
