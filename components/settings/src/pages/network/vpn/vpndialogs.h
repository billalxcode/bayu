// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "base/signal.h"
#include "bayu/networkmanager/handler.h"

namespace bayu::ui {
class ColumnLayout;
class ComboBox;
class DialogButton;
class DialogWindow;
class FileChooser;
class Label;
}

namespace settings {
class FormDialog;
class InfoRow;
class ListRow;
class SettingsWindow;
}

namespace settings::network {

class FormScroll;
class NetworkList;
class VpnEditor;

// Sets up a VPN from what the provider or the network's administrator hands
// out: the server and how to sign in. A password left empty is asked for when
// connecting.
class AddVpnDialog
{
public:
    AddVpnDialog(SettingsWindow& window, bayu::networkmanager::Handler& handler);
    ~AddVpnDialog();

    void show();
    FormDialog* dialog() const { return m_dialog.get(); }
    VpnEditor* editor() const { return m_editor; }

private:
    void loadKind();
    void add();
    void update();

    SettingsWindow& m_window;
    bayu::networkmanager::Handler& m_handler;
    std::unique_ptr<FormDialog> m_dialog;
    std::vector<bayu::base::Subscription> m_subscriptions;
    // Read again each time the dialog opens, so a plugin installed meanwhile shows up.
    std::vector<bayu::networkmanager::vpn::Kind> m_kinds;
    bool m_adding = false;
    std::string m_error;
    bayu::ui::Label* m_kindLabel = nullptr;
    bayu::ui::ComboBox* m_kindBox = nullptr;
    FormScroll* m_scroll = nullptr;
    VpnEditor* m_editor = nullptr;
    bayu::ui::Label* m_errorLabel = nullptr;
    bayu::ui::DialogButton* m_addButton = nullptr;
};

// One VPN's settings, state, and the ways to export or remove it. Changes are
// kept with Save; a password shown empty is one that is asked for when
// connecting.
class VpnDetailsDialog
{
public:
    VpnDetailsDialog(SettingsWindow& window, NetworkList& list, bayu::networkmanager::Handler& handler,
                     std::string connectionPath);
    ~VpnDetailsDialog();

    void show();
    bayu::ui::DialogWindow* dialog() const { return m_dialog.get(); }

private:
    void update();
    void save();
    void close();

    SettingsWindow& m_window;
    NetworkList& m_list;
    bayu::networkmanager::Handler& m_handler;
    std::string m_connectionPath;
    std::unique_ptr<bayu::ui::DialogWindow> m_dialog;
    std::unique_ptr<bayu::ui::FileChooser> m_exportChooser;
    std::vector<bayu::base::Subscription> m_subscriptions;
    bool m_found = false;
    bool m_saving = false;
    std::string m_error;
    std::string m_statusMessage;
    bayu::ui::ColumnLayout* m_content = nullptr;
    FormScroll* m_scroll = nullptr;
    VpnEditor* m_editor = nullptr;
    bayu::ui::ColumnLayout* m_statusCard = nullptr;
    InfoRow* m_type = nullptr;
    InfoRow* m_status = nullptr;
    InfoRow* m_address = nullptr;
    ListRow* m_export = nullptr;
    bayu::ui::Label* m_message = nullptr;
    bayu::ui::DialogButton* m_saveButton = nullptr;
};

} // namespace settings::network
