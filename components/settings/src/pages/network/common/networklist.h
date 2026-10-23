// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "base/signal.h"
#include "bayu/networkmanager/network_model.h"

namespace settings::network {

// The networks the pages list: NetworkManager's, or the sample networks of
// --mock-network, which have no NetworkManager connection behind them.
class NetworkList
{
public:
    NetworkList();
    ~NetworkList();

    bool isMock() const { return !m_model; }
    const std::vector<bayu::networkmanager::NetworkItem>& items() const;
    // One type of row, sorted for a list.
    std::vector<bayu::networkmanager::NetworkItem> rows(bayu::networkmanager::ConnectionType type) const;
    // The row whose key, connection path or SSID is the value.
    std::optional<bayu::networkmanager::NetworkItem> find(const std::string& connectionPath,
                                                              const std::string& ssid = {}) const;

    bayu::base::Signal<> changed;

private:
    std::unique_ptr<bayu::networkmanager::NetworkModel> m_model;
    std::vector<bayu::networkmanager::NetworkItem> m_mock;
    bayu::base::Subscription m_subscription;
};

// The sample networks: a connected and a saved Wi-Fi network, two others,
// and two wired connections.
std::vector<bayu::networkmanager::NetworkItem> mockNetworks();

} // namespace settings::network
