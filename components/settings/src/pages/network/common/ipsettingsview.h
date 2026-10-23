// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "base/signal.h"
#include "bayu/networkmanager/ip_utils.h"
#include "ui/cell_layout.h"

namespace bayu::networkmanager {
class Handler;
}

namespace bayu::ui {
class ComboBox;
class DialogButton;
class Label;
class Switch;
class TextField;
}

namespace settings::network {

// How a saved network gets its addresses and name servers, shown in place of
// its details. Save writes the profile and applies it to the connection at
// once; onFinished follows a save or Cancel.
class IpSettingsView : public bayu::ui::ColumnLayout
{
public:
    explicit IpSettingsView(std::string connectionPath);
    ~IpSettingsView() override;

    std::function<void()> onFinished;

protected:
    void layout() override;

private:
    struct FormRow
    {
        bayu::ui::RowLayout* row;
        bayu::ui::Label* label;
    };

    FormRow addRow(bayu::ui::ColumnLayout* card, const std::string& label);
    bayu::ui::TextField* addField(bayu::ui::RowLayout* row, const std::string& placeholder);
    void load();
    bayu::networkmanager::ip::Form collect() const;
    void fillFromCurrent(bool v4);
    void save();
    void update();

    std::string m_connectionPath;
    std::unique_ptr<bayu::networkmanager::Handler> m_handler;
    std::vector<bayu::base::Subscription> m_subscriptions;
    bayu::networkmanager::ip::Form m_form;
    bool m_saving = false;
    std::string m_error;
    std::vector<bayu::ui::Label*> m_fieldLabels;

    bayu::ui::ComboBox* m_ipv4Method;
    bayu::ui::RowLayout* m_ipv4AddressRow;
    bayu::ui::TextField* m_ipv4Address;
    bayu::ui::RowLayout* m_ipv4NetmaskRow;
    bayu::ui::TextField* m_ipv4Netmask;
    bayu::ui::RowLayout* m_ipv4GatewayRow;
    bayu::ui::TextField* m_ipv4Gateway;
    bayu::ui::ComboBox* m_ipv6Method;
    bayu::ui::RowLayout* m_ipv6AddressRow;
    bayu::ui::TextField* m_ipv6Address;
    bayu::ui::RowLayout* m_ipv6PrefixRow;
    bayu::ui::TextField* m_ipv6Prefix;
    bayu::ui::RowLayout* m_ipv6GatewayRow;
    bayu::ui::TextField* m_ipv6Gateway;
    bayu::ui::RowLayout* m_automaticDnsRow;
    bayu::ui::Switch* m_automaticDns;
    bayu::ui::TextField* m_dns;
    bayu::ui::Label* m_message;
    bool m_problem = false;
    bayu::ui::DialogButton* m_saveButton;
};

} // namespace settings::network
