// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "base/signal.h"
#include "bayu/accounts/user_account.h"
#include "widgets/listrow.h"

namespace bayu::ui {
class Label;
}

namespace settings {

class IconButton;
class UserAvatar;

// One account on the Users page: its picture, names and whether it is the
// one logged in, with an info button at the end for its details.
class UserRow : public ListRow
{
public:
    UserRow(std::shared_ptr<bayu::accounts::UserAccount> account, int64_t loggedInUid);
    ~UserRow() override;

    const std::shared_ptr<bayu::accounts::UserAccount>& account() const { return m_account; }
    // Shows a picture before the account has it.
    void showPicture(const std::string& path);

    std::function<void()> onPictureClicked;
    std::function<void()> onInfo;

protected:
    void layout() override;

private:
    void update();

    std::shared_ptr<bayu::accounts::UserAccount> m_account;
    int64_t m_loggedInUid;
    bayu::base::Subscription m_changed;
    UserAvatar* m_avatar;
    bayu::ui::Label* m_userName;
    bayu::ui::Label* m_realName;
    bayu::ui::Label* m_loggedIn;
    IconButton* m_info;
};

// The accounts in the service's order, the one logged in first.
std::vector<std::shared_ptr<bayu::accounts::UserAccount>>
loggedInFirst(const std::vector<std::shared_ptr<bayu::accounts::UserAccount>>& accounts, int64_t loggedInUid);

} // namespace settings
