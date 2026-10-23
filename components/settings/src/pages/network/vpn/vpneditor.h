// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "bayu/networkmanager/handler.h"
#include "ui/cell_layout.h"

namespace bayu::ui {
class Label;
class Switch;
class TextField;
}

namespace settings::network {

class PluginForm;
class WireGuardEditor;

// What a VPN profile is made of, for the Add VPN and details dialogs: its
// name, the fields of its kind, and who may use it. load() takes
// Handler::vpnForm(), collect() gives Handler::saveVpn() its form.
class VpnEditor : public bayu::ui::ColumnLayout
{
public:
    VpnEditor();
    ~VpnEditor() override;

    void load(const bayu::networkmanager::VpnForm& form);
    void setSecrets(const bayu::networkmanager::VpnSecrets& secrets);
    bayu::networkmanager::VpnValues collect() const;
    void focusFirst();
    // Also measured: a label beside the editor sharing its column.
    void measure(double width);

    const bayu::networkmanager::VpnForm& form() const { return m_form; }
    bool installed() const { return m_form.installed; }
    bool complete() const;
    // The label column's width, which a label beside the editor takes too.
    double labelWidth() const;

    // Return in a field.
    std::function<void()> onAccepted;
    // Anything that changes complete() or the size.
    std::function<void()> onChanged;

protected:
    void layout() override;

private:
    void changed();

    bayu::networkmanager::VpnForm m_form;
    double m_extraLabelWidth = 0;
    bayu::ui::Label* m_notInstalled;
    bayu::ui::RowLayout* m_nameRow;
    bayu::ui::Label* m_nameLabel;
    bayu::ui::TextField* m_name;
    PluginForm* m_plugin = nullptr;
    WireGuardEditor* m_wireGuard = nullptr;
    bayu::ui::ColumnLayout* m_switches;
    bayu::ui::Switch* m_shared;
    bayu::ui::Switch* m_ownNetworkOnly;
};

} // namespace settings::network
