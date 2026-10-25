// SPDX-License-Identifier: GPL-3.0-or-later
// Step 3: the network connection, optional, and the host name.

#include <cctype>

#include "base/i18n.h"
#include "ui/dialog_window.h"
#include "ui/icon.h"
#include "ui/label.h"
#include "ui/scroll_view.h"
#include "ui/text_field.h"
#include "ui/page.h"
#include "ui/pages.h"
#include "ui/installer.h"
#include "ui/widgets.h"
#include "ui/window.h"

using namespace bayu::ui;
using bayu::i18n::format;
using bayu::i18n::tr;

namespace installer {

using style::Role;

namespace {

// RFC 1123 host names: dot-separated labels of letters, digits and inner
// hyphens, each at most 63 long, 253 in all.
bool validHostname(const std::string& name)
{
    if (name.empty() || name.size() > 253)
        return false;
    size_t start = 0;
    while (start <= name.size()) {
        size_t end = name.find('.', start);
        if (end == std::string::npos)
            end = name.size();
        const size_t length = end - start;
        if (length == 0 || length > 63 || name[start] == '-' || name[end - 1] == '-')
            return false;
        for (size_t i = start; i < end; ++i) {
            const unsigned char c = static_cast<unsigned char>(name[i]);
            if (!std::isalnum(c) && c != '-')
                return false;
        }
        start = end + 1;
    }
    return true;
}

std::string signalGlyph(int strength)
{
    if (strength >= 75)
        return "network-wireless-connected-100";
    if (strength >= 50)
        return "network-wireless-connected-66";
    if (strength >= 25)
        return "network-wireless-connected-33";
    return "network-wireless-connected-00";
}

class StatusLine : public Row
{
public:
    StatusLine()
    {
        setSpacing(8);
        m_icon = addChild<Icon>("shield-check", 14);
        m_text = addText(this, {}, 12, 400, Role::SecondaryText);
    }
    void set(std::string text, bool good)
    {
        m_text->setText(std::move(text));
        m_icon->setIconName(good ? "shield-check" : "shield");
        m_icon->setColor(good ? style::color(Role::Success) : style::color(Role::TertiaryText));
    }

private:
    Icon* m_icon;
    Label* m_text;
};

class NetworkPage : public Page
{
public:
    explicit NetworkPage(Installer& installer)
        : Page(installer)
    {
        Row* body = addChild<Row>();
        body->setSpacing(style::columnSpacing);
        body->setLayoutHints({.fillWidth = true, .fillHeight = true});

        Column* left = body->addChild<Column>();
        left->setSpacing(10);
        left->setLayoutHints({.fillWidth = true, .fillHeight = true, .stretch = 1.05, .preferredWidth = 0});
        addSectionLabel(left, tr("AVAILABLE CONNECTIONS"));
        m_list = left->addChild<ListCard>();

        // Scrolls on short screens (1366×768).
        ScrollView* rightScroll = body->addChild<ScrollView>();
        rightScroll->setVerticalBarPolicy(ScrollView::BarPolicy::AlwaysOn);
        rightScroll->setLayoutHints({.fillWidth = true, .fillHeight = true, .preferredWidth = 0});
        Column* right = rightScroll->setContent<Column>();
        right->setSpacing(10);
        addSectionLabel(right, tr("HOST IDENTITY"));
        View* host = addLabeledField(right, tr("Host name"), &m_hostname);
        host->setLayoutHints({.fillWidth = true, .margins = {0, 2, 0, 6}});
        m_hostname->onChanged = [this] {
            m_hostname->setInvalid(!validHostname(m_hostname->text()));
            changed();
        };
        m_details = right->addChild<DetailsCard>();
        m_auto = right->addChild<ToggleCard>(tr("Connect automatically"), tr("Bring this interface up at every boot"));
        m_auto->onToggled = [this](bool on) { this->installer().backend().setConnectAutomatically(on); };
        right->addStretch();
        m_status = right->addChild<StatusLine>();

        installer.backend().onNetworkChanged = [this] {
            if (isVisible())
                fill();
        };
    }

    std::string stepName() const override { return tr("Network"); }
    std::string title() const override { return tr("Get connected."); }
    std::string description() const override
    {
        return tr("A connection is optional, but it lets the installer fetch updates, verify packages and sync the "
                  "clock while you continue.");
    }
    std::string summary() const override { return m_hostname->text(); }
    bool canContinue() const override { return validHostname(m_hostname->text()); }

    void enter() override
    {
        if (m_hostname->text().empty())
            m_hostname->setText(installer().backend().hostname());
        m_auto->setChecked(installer().backend().connectAutomatically());
        const SystemInfo info = installer().backend().systemInfo();
        if (info.secureBoot)
            m_status->set(tr("Secure Boot enabled · packages verified with GPG"), true);
        else
            m_status->set(tr("Packages verified with GPG"), true);
        fill();
    }

    void commit(std::function<void(bool)> done) override
    {
        installer().choices().hostname = m_hostname->text();
        installer().backend().setHostname(m_hostname->text());
        done(true);
    }

private:
    void fill()
    {
        m_list->clearRows();
        m_connections = installer().backend().connections();
        ListRow* selected = nullptr;
        std::string selectedId;
        for (const NetworkConnection& connection : m_connections) {
            ListRow* row = m_list->addRow();
            switch (connection.kind) {
            case NetworkConnection::Kind::Wired:
                row->setGlyph("ethernet");
                break;
            case NetworkConnection::Kind::Wireless:
                row->setGlyph("wifi");
                row->setTrailingGlyph(signalGlyph(connection.strength));
                break;
            case NetworkConnection::Kind::Hotspot:
                row->setGlyph("smartphone");
                row->setTrailingGlyph(signalGlyph(connection.strength));
                break;
            }
            row->setTitle(connection.name);
            row->setDetail(connection.connecting ? tr("Connecting…") : connection.detail);
            if (connection.connected) {
                selected = row;
                selectedId = connection.id;
                // The check takes the signal's place.
                row->setTrailingGlyph({});
            }
            const NetworkConnection copy = connection;
            row->onClicked = [this, copy] { choose(copy); };
        }
        m_list->select(selected);
        m_details->clearLines();
        if (!selectedId.empty()) {
            const NetworkDetails details = installer().backend().details(selectedId);
            m_details->addLine(tr("IPv4 address"), details.ipv4);
            m_details->addLine(tr("Gateway"), details.gateway);
            m_details->addLine(tr("DNS"), details.dns);
            if (!details.linkSpeed.empty())
                m_details->addLine(tr("Link speed"), details.linkSpeed);
            m_details->addLine(tr("Hardware"), details.hardwareAddress);
            installer().choices().connection = selectedId;
        } else {
            m_details->addLine(tr("Status"), tr("Not connected"));
        }
        m_details->setVisible(true);
    }

    void choose(const NetworkConnection& connection)
    {
        if (connection.connected)
            return;
        if (connection.kind != NetworkConnection::Kind::Wired && connection.secured) {
            askPassword(connection);
            return;
        }
        join(connection.id, {});
    }

    void askPassword(const NetworkConnection& connection)
    {
        m_dialog = std::make_unique<DialogWindow>(installer().window(),
                                                  format(tr("Join “{name}”"), {{"name", connection.name}}), 400);
        auto content = std::make_unique<Column>();
        content->setSpacing(14);
        Label* note = addText(content.get(), tr("Enter the network's password."), 14, 400, Role::SecondaryText);
        note->setWrap(true);
        Field* password = nullptr;
        addLabeledField(content.get(), tr("Password"), &password);
        password->setPassword(true);
        Row* actions = content->addChild<Row>();
        actions->setSpacing(10);
        actions->setLayoutHints({.fillWidth = true, .margins = {0, 6, 0, 0}});
        actions->addStretch();
        ActionButton* cancel = actions->addChild<ActionButton>(tr("Cancel"));
        ActionButton* join = actions->addChild<ActionButton>(tr("Join"), ActionButton::Kind::Primary);
        const std::string id = connection.id;
        cancel->onClicked = [this] { m_dialog->close(); };
        join->onClicked = [this, password, id] {
            const std::string secret = password->text();
            m_dialog->close();
            this->join(id, secret);
        };
        password->edit()->onAccepted = join->onClicked;
        m_dialog->setContentView(std::move(content));
        m_dialog->show();
        password->edit()->setFocus();
    }

    void join(const std::string& id, const std::string& password)
    {
        installer().backend().connect(id, password, [this](const std::string& error) {
            fill();
            if (error.empty())
                return;
            m_dialog = std::make_unique<DialogWindow>(installer().window(), tr("Could not connect"), 400);
            auto content = std::make_unique<Column>();
            Label* text = addText(content.get(), error, 14);
            text->setWrap(true);
            m_dialog->setContentView(std::move(content));
            m_dialog->show();
        });
        fill();
    }

    ListCard* m_list;
    Field* m_hostname;
    DetailsCard* m_details;
    ToggleCard* m_auto;
    StatusLine* m_status;
    std::vector<NetworkConnection> m_connections;
    std::unique_ptr<DialogWindow> m_dialog;
};

} // namespace

std::unique_ptr<Page> createNetworkPage(Installer& installer)
{
    return std::make_unique<NetworkPage>(installer);
}

} // namespace installer
