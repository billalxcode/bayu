// Copyright 2026 Reion Wong <reionwong@gmail.com>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAYU_AUTH_STATUS_H
#define BAYU_AUTH_STATUS_H

#include <memory>
#include <vector>

#include "base/signal.h"
#include "shell/statusbar/bar_item.h"

namespace bayu::shell {
class Battery;
class StatusIcon;
class WirelessStatus;
} // namespace bayu::shell

namespace bayu::auth {

// The status bar's Wi-Fi and battery icons for the screen's top-right corner
// at the login and lock screen, with the percentage always shown. They only
// show: nothing passes input to them.
class AuthStatus final : public ui::View, public shell::BarContext
{
public:
    // The status bar's height.
    static constexpr double kHeight = 32;

    // At the login screen there is no session bus: the battery comes from
    // UPower directly.
    explicit AuthStatus(bool login);
    ~AuthStatus() override;

    void setLightContents(bool light);
    bool lightContents() const override { return m_lightContents; }

    // The icons' width, the last one reaching as near the edge as the
    // status bar's last item does; 0 when neither shows.
    double preferredWidth() const;

    shell::StatusIcon *networkIcon() const { return m_network; }
    shell::StatusIcon *batteryIcon() const { return m_battery; }

protected:
    void layout() override;

private:
    void updateBattery();
    void updateNetwork();
    // The icons that show, right to left.
    std::vector<shell::StatusIcon *> shownIcons() const;

    std::unique_ptr<shell::Battery> m_batteryStatus;
    std::unique_ptr<shell::WirelessStatus> m_wirelessStatus;
    shell::StatusIcon *m_network = nullptr;
    shell::StatusIcon *m_battery = nullptr;
    bool m_lightContents = false;
    std::vector<base::Subscription> m_subscriptions;
};

} // namespace bayu::auth

#endif // BAYU_AUTH_STATUS_H
