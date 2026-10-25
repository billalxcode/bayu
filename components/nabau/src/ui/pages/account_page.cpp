// SPDX-License-Identifier: GPL-3.0-or-later
// Step 6: the user account, its permissions and the root account.

#include <algorithm>
#include <cctype>
#include <set>

#include "base/i18n.h"
#include "ui/file_chooser.h"
#include "ui/icon.h"
#include "ui/label.h"
#include "ui/scroll_view.h"
#include "ui/text_field.h"
#include "ui/page.h"
#include "ui/pages.h"
#include "ui/installer.h"
#include "ui/widgets.h"

using namespace bayu::ui;
using bayu::i18n::format;
using bayu::i18n::tr;

namespace installer {

using style::Role;

namespace {

// Names useradd refuses or the system already has.
bool reservedName(const std::string& name)
{
    static const std::set<std::string> reserved = {
        "root",    "bin",      "daemon",  "adm",   "lp",       "sync",    "shutdown", "halt",
        "mail",    "operator", "games",   "ftp",   "nobody",   "dbus",    "polkitd",  "sshd",
        "chrony",  "tss",      "systemd", "avahi", "pipewire", "geoclue", "rtkit",    "unbound",
        "colord",  "saned",    "setroubleshoot",   "gdm",      "bayu",    "wheel",    "users"};
    return reserved.count(name) || name.starts_with("systemd-");
}

// A POSIX user name: a lower-case letter or underscore, then letters, digits,
// underscores or hyphens, at most 32.
std::string userNameProblem(const std::string& name)
{
    if (name.empty())
        return tr("Choose a user name.");
    if (name.size() > 32)
        return tr("At most 32 characters.");
    if (!(std::islower(static_cast<unsigned char>(name[0])) || name[0] == '_'))
        return tr("Start with a lower-case letter.");
    for (char c : name) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (!(std::islower(u) || std::isdigit(u) || c == '_' || c == '-'))
            return tr("Use lower-case letters, digits, “-” and “_” only.");
    }
    if (reservedName(name))
        return tr("That name belongs to the system.");
    return {};
}

// The user name a full name suggests: its first word, in lower-case ASCII.
std::string suggestUserName(const std::string& fullName)
{
    std::string name;
    for (char c : fullName) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (std::isspace(u)) {
            if (!name.empty())
                break;
            continue;
        }
        if (std::isalnum(u) && u < 128)
            name += char(std::tolower(u));
    }
    while (!name.empty() && std::isdigit(static_cast<unsigned char>(name[0])))
        name.erase(0, 1);
    return name.substr(0, 32);
}

std::string initials(const std::string& fullName)
{
    std::string result;
    bool start = true;
    for (char c : fullName) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (std::isspace(u)) {
            start = true;
        } else if (start) {
            if (u < 128)
                result += char(std::toupper(u));
            start = false;
            if (result.size() == 2)
                break;
        }
    }
    return result;
}

// 0 to 4: length counts most, then the mix of kinds of characters.
int strength(const std::string& password)
{
    if (password.empty())
        return 0;
    int kinds = 0;
    bool lower = false, upperCase = false, digit = false, other = false;
    for (char c : password) {
        const unsigned char u = static_cast<unsigned char>(c);
        lower = lower || std::islower(u);
        upperCase = upperCase || std::isupper(u);
        digit = digit || std::isdigit(u);
        other = other || !std::isalnum(u);
    }
    kinds = int(lower) + int(upperCase) + int(digit) + int(other);
    const size_t length = password.size();
    if (length < 6)
        return 1;
    if (length < 8)
        return kinds >= 3 ? 2 : 1;
    if (length < 12)
        return kinds >= 3 ? 3 : 2;
    return kinds >= 2 || length >= 16 ? 4 : 3;
}

class AccountPage : public Page
{
public:
    explicit AccountPage(Installer& installer)
        : Page(installer)
    {
        Row* body = addChild<Row>();
        body->setSpacing(style::columnSpacing);
        body->setLayoutHints({.fillWidth = true, .fillHeight = true});

        ScrollView* leftScroll = body->addChild<ScrollView>();
        leftScroll->setVerticalBarPolicy(ScrollView::BarPolicy::AlwaysOn);
        leftScroll->setLayoutHints({.fillWidth = true, .fillHeight = true, .stretch = 1.05, .preferredWidth = 0});
        Column* left = leftScroll->setContent<Column>();
        left->setSpacing(14);
        Row* profile = left->addChild<Row>();
        profile->setSpacing(18);
        m_avatar = profile->addChild<Avatar>(56);
        m_avatar->onClicked = [this] { choosePicture(); };
        Column* profileText = profile->addChild<Column>();
        profileText->setSpacing(2);
        addText(profileText, tr("Profile picture"), 14, 600);
        profileText->setLayoutHints({.fillWidth = true});
        m_avatarNote = addText(profileText, tr("Generated from your initials · click to change"), 12, 400,
                               Role::SecondaryText);
        m_avatarNote->setWrap(true);
        m_avatarNote->setLayoutHints({.fillWidth = true});

        addLabeledField(left, tr("Full name"), &m_fullName);
        addLabeledField(left, tr("User name"), &m_userName);
        addLabeledField(left, tr("Password"), &m_password);
        m_password->setPassword(true);
        addLabeledField(left, tr("Confirm password"), &m_confirm);
        m_confirm->setPassword(true);
        m_meter = left->addChild<StrengthMeter>();
        Row* hint = left->addChild<Row>();
        hint->setSpacing(8);
        hint->setLayoutHints({.fillWidth = true, .margins = {0, 4, 0, 0}});
        Icon* bulb = hint->addChild<Icon>("bulb", 14);
        bulb->setColor(style::color(Role::SecondaryText));
        m_hint = addText(hint, tr("Four random words beat one short complex password"), 12, 400, Role::SecondaryText);
        m_hint->setWrap(true);
        m_hint->setLayoutHints({.fillWidth = true});

        ScrollView* rightScroll = body->addChild<ScrollView>();
        rightScroll->setVerticalBarPolicy(ScrollView::BarPolicy::AlwaysOn);
        rightScroll->setLayoutHints({.fillWidth = true, .fillHeight = true, .preferredWidth = 0});
        Column* right = rightScroll->setContent<Column>();
        right->setSpacing(10);
        addSectionLabel(right, tr("PERMISSIONS"));
        m_admin = right->addChild<ToggleCard>(tr("Make this user an administrator"), std::string());
        m_admin->setChecked(true);
        m_requirePassword = right->addChild<ToggleCard>(tr("Require a password to sign in"),
                                                        tr("Disable only on a physically secure machine"));
        m_requirePassword->setChecked(true);
        Label* rootLabel = addSectionLabel(right, tr("ROOT ACCOUNT"));
        rootLabel->setLayoutHints({.margins = {0, 8, 0, 0}});
        m_lockRoot = right->addChild<OptionCard>("padlock");
        m_lockRoot->setTitle(tr("Lock the root account"));
        m_lockRoot->setDetail(tr("Administer with sudo instead — safer by default"));
        m_lockRoot->setNote(tr("recommended"), true);
        m_setRoot = right->addChild<OptionCard>("key");
        m_setRoot->setTitle(tr("Set a root password"));
        m_setRoot->setDetail(tr("Enables direct root sign-in on console and SSH"));
        m_rootFields = right->addChild<Column>();
        m_rootFields->setSpacing(10);
        m_rootFields->setLayoutHints({.fillWidth = true});
        addLabeledField(m_rootFields, tr("Root password"), &m_rootPassword);
        m_rootPassword->setPassword(true);
        addLabeledField(m_rootFields, tr("Confirm root password"), &m_rootConfirm);
        m_rootConfirm->setPassword(true);
        m_info = right->addChild<Banner>(Banner::Kind::Info, std::string(), std::string());
        m_info->setLayoutHints({.fillWidth = true, .margins = {0, 8, 0, 0}});
        m_problem = right->addChild<Banner>(Banner::Kind::Warning, std::string(), std::string());

        m_lockRoot->onClicked = [this] { setLockRoot(true); };
        m_setRoot->onClicked = [this] { setLockRoot(false); };
        m_fullName->onChanged = [this] {
            if (!m_userNameEdited) {
                m_settingName = true;
                m_userName->setText(suggestUserName(m_fullName->text()));
                m_settingName = false;
            }
            update();
        };
        m_userName->onChanged = [this] {
            if (!m_settingName)
                m_userNameEdited = !m_userName->text().empty();
            update();
        };
        for (Field* field : {m_password, m_confirm, m_rootPassword, m_rootConfirm})
            field->onChanged = [this] { update(); };
        m_admin->onToggled = [this](bool) { update(); };
        setLockRoot(true);
    }

    std::string stepName() const override { return tr("Account"); }
    std::string title() const override { return tr("Create your account."); }
    std::string description() const override
    {
        if (installer().choices().storage.encrypt)
            return tr("This is the account you will sign in with on first boot. Pick a strong passphrase — it also "
                      "unlocks your encrypted disk.");
        return tr("This is the account you will sign in with on first boot. Pick a strong passphrase you will "
                  "remember.");
    }
    std::string summary() const override { return m_userName->text(); }

    bool canContinue() const override { return problem().empty(); }

    void enter() override { update(); }

    void commit(std::function<void(bool)> done) override
    {
        Account account;
        account.fullName = m_fullName->text();
        account.userName = m_userName->text();
        account.password = m_password->text();
        account.administrator = m_admin->isChecked();
        account.requirePassword = m_requirePassword->isChecked();
        account.lockRoot = m_lockRootChosen;
        account.rootPassword = m_lockRootChosen ? std::string() : m_rootPassword->text();
        account.picture = m_picture;
        installer().choices().account = account;
        installer().backend().setAccount(account);
        done(true);
    }

private:
    void setLockRoot(bool lock)
    {
        m_lockRootChosen = lock;
        m_lockRoot->setSelected(lock);
        m_setRoot->setSelected(!lock);
        m_rootFields->setVisible(!lock);
        update();
    }

    std::string problem() const
    {
        if (m_fullName->text().empty())
            return tr("Enter your full name.");
        const std::string userProblem = userNameProblem(m_userName->text());
        if (!userProblem.empty())
            return userProblem;
        if (m_password->text().empty())
            return tr("Choose a password.");
        if (m_password->text() != m_confirm->text())
            return tr("The passwords do not match.");
        if (installer().choices().storage.encrypt && m_password->text().size() < 8)
            return tr("A disk passphrase needs at least 8 characters.");
        if (!m_lockRootChosen) {
            if (m_rootPassword->text().empty())
                return tr("Choose a root password.");
            if (m_rootPassword->text() != m_rootConfirm->text())
                return tr("The root passwords do not match.");
        }
        if (m_lockRootChosen && !m_admin->isChecked())
            return tr("With root locked, this user must be an administrator.");
        return {};
    }

    void update()
    {
        const std::string user = m_userName->text().empty() ? tr("this user") : m_userName->text();
        m_admin->setDetail(format(tr("Adds {user} to the wheel group and allows sudo"), {{"user", user}}));
        const std::string name = initials(m_fullName->text());
        m_avatar->setInitials(name.empty() ? "?" : name);
        m_userName->setInvalid(!m_userName->text().empty() && !userNameProblem(m_userName->text()).empty());
        m_confirm->setInvalid(!m_confirm->text().empty() && m_confirm->text() != m_password->text());
        const std::string password = m_password->text();
        const int level = strength(password);
        const std::string captions[] = {"", tr("Weak passphrase"), tr("Fair passphrase"), tr("Good passphrase"),
                                        tr("Strong passphrase")};
        m_meter->setStrength(level, captions[level],
                             password.empty() ? std::string()
                                              : bayu::i18n::trn("{n} character", "{n} characters", password.size()));
        if (installer().choices().storage.encrypt)
            m_info->setText({}, tr("Your passphrase unlocks LUKS2 at every boot. It is never written to the "
                                   "kickstart file or to installation logs."));
        else
            m_info->setText({}, tr("The password is stored only as a hash on the new system and never written to "
                                   "the installation logs."));
        const std::string issue = problem();
        // Only once the user has typed something does a problem show.
        const bool started = !m_fullName->text().empty() || !password.empty();
        m_problem->setVisible(started && !issue.empty());
        m_problem->setText({}, issue);
        changed();
    }

    void choosePicture()
    {
        m_chooser = std::make_unique<FileChooser>(installer().window());
        m_chooser->setTitle(tr("Choose a profile picture"));
        m_chooser->setFilters({FileChooser::Filter::fromNameFilter(tr("Images (*.png *.jpg *.jpeg *.webp *.svg)"))});
        m_chooser->onAccepted = [this](const std::vector<std::string>& paths, const std::string&) {
            if (paths.empty())
                return;
            m_picture = paths.front();
            m_avatar->setPicture(m_picture);
            m_avatarNote->setText(tr("Click to choose another"));
        };
        m_chooser->open();
    }

    Avatar* m_avatar;
    Label* m_avatarNote;
    Field* m_fullName;
    Field* m_userName;
    Field* m_password;
    Field* m_confirm;
    StrengthMeter* m_meter;
    Label* m_hint;
    ToggleCard* m_admin;
    ToggleCard* m_requirePassword;
    OptionCard* m_lockRoot;
    OptionCard* m_setRoot;
    Column* m_rootFields;
    Field* m_rootPassword;
    Field* m_rootConfirm;
    Banner* m_info;
    Banner* m_problem;
    bool m_lockRootChosen = true;
    bool m_userNameEdited = false;
    bool m_settingName = false;
    std::string m_picture;
    std::unique_ptr<FileChooser> m_chooser;
};

} // namespace

std::unique_ptr<Page> createAccountPage(Installer& installer)
{
    return std::make_unique<AccountPage>(installer);
}

} // namespace installer
