// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <functional>
#include <memory>
#include <string>

#include "pages/accounts/passwordchanger.h"
#include "widgets/formdialog.h"

namespace bayu::accounts {
class AccountsManager;
}

namespace bayu::ui {
class ComboBox;
class DialogButton;
class GridLayout;
class Label;
class TextField;
}

namespace settings {

// A new password for an account, typed twice. With a changer it is the
// user's own: the current password is asked too, and the dialog stays open
// until the change is made, saying why it was not.
class ChangePasswordDialog : public FormDialog
{
public:
    ChangePasswordDialog(SettingsWindow& parent, const std::string& userName,
                         std::unique_ptr<PasswordChanger> changer = nullptr);
    ~ChangePasswordDialog() override;

    // Without a changer: the password as typed; the caller hashes it.
    std::function<void(const std::string& password)> onPasswordSubmitted;

private:
    void update();
    void change();
    void showError(const std::string& error);

    std::unique_ptr<PasswordChanger> m_changer;
    bayu::ui::GridLayout* m_form;
    bayu::ui::TextField* m_current = nullptr;
    bayu::ui::TextField* m_password;
    bayu::ui::TextField* m_verify;
    bayu::ui::Label* m_error = nullptr;
    bayu::ui::DialogButton* m_cancel;
    bayu::ui::DialogButton* m_change;
};

// A new account: its name, password and type. Stays open, and refuses to
// close, until the service has answered.
class AddUserDialog : public FormDialog
{
public:
    AddUserDialog(SettingsWindow& parent, bayu::accounts::AccountsManager& manager);

private:
    void update();
    void add();
    void setWaiting(bool waiting);

    bayu::accounts::AccountsManager& m_manager;
    bayu::ui::GridLayout* m_form;
    bayu::ui::TextField* m_name;
    bayu::ui::TextField* m_password;
    bayu::ui::TextField* m_verify;
    bayu::ui::ComboBox* m_type;
    bayu::ui::Label* m_error;
    bayu::ui::DialogButton* m_cancel;
    bayu::ui::DialogButton* m_add;
    bool m_waiting = false;
};

} // namespace settings
