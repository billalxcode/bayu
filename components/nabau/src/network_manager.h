// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "base/dbus.h"
#include "model.h"

namespace bayu::base {
class Timer;
}

namespace installer {

// The live system's network, through NetworkManager on the system bus: the
// devices and Wi-Fi networks to pick from, and joining one.
class NetworkManager
{
public:
    NetworkManager();
    ~NetworkManager();

    bool isAvailable() const { return m_bus != nullptr; }
    std::vector<NetworkConnection> connections() const;
    NetworkDetails details(const std::string& id) const;
    // A wired device's path, or "<device>|<access point>" for Wi-Fi.
    void connect(const std::string& id, const std::string& password, std::function<void(const std::string&)> done);
    bool isOnline() const;
    // The UUID of the connection the device is up with, for the installed
    // system's copy of it.
    std::string activeConnectionUuid() const;

    // The list or a state changed.
    std::function<void()> onChanged;

private:
    void poll();

    bayu::base::dbus::Connection* m_bus = nullptr;
    std::unique_ptr<bayu::base::Timer> m_timer;
    std::string m_signature;
    std::string m_connecting;
    bayu::base::dbus::PendingCall m_pending;
};

} // namespace installer
