// SPDX-License-Identifier: GPL-3.0-or-later
// Step 5: the base environment and the add-ons installed with it.

#include <algorithm>

#include "base/i18n.h"
#include "ui/label.h"
#include "ui/scroll_view.h"
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

std::string upper(std::string text)
{
    for (char& c : text)
        c = char(std::toupper(static_cast<unsigned char>(c)));
    return text;
}

class SoftwarePage : public Page
{
public:
    explicit SoftwarePage(Installer& installer)
        : Page(installer)
    {
        Row* body = addChild<Row>();
        body->setSpacing(style::columnSpacing);
        body->setLayoutHints({.fillWidth = true, .fillHeight = true});

        ScrollView* leftScroll = body->addChild<ScrollView>();
        leftScroll->setVerticalBarPolicy(ScrollView::BarPolicy::AlwaysOn);
        leftScroll->setLayoutHints({.fillWidth = true, .fillHeight = true, .stretch = 1.05, .preferredWidth = 0});
        Column* left = leftScroll->setContent<Column>();
        left->setSpacing(10);
        addSectionLabel(left, tr("BASE ENVIRONMENT"));
        m_environments = left->addChild<Column>();
        m_environments->setSpacing(10);
        m_environments->setLayoutHints({.fillWidth = true});
        m_note = left->addChild<Banner>(Banner::Kind::Info, std::string(), std::string());
        m_note->setLayoutHints({.fillWidth = true, .margins = {0, 4, 0, 0}});

        Column* right = body->addChild<Column>();
        right->setSpacing(10);
        right->setLayoutHints({.fillWidth = true, .fillHeight = true, .preferredWidth = 0});
        m_addonsLabel = addSectionLabel(right, {});
        Card* addons = right->addChild<Card>(6);
        addons->setLayoutHints({.fillWidth = true, .fillHeight = true});
        ScrollView* scroll = addons->addChild<ScrollView>();
        scroll->setLayoutHints({.fillWidth = true, .fillHeight = true});
        m_addons = scroll->setContent<Column>();
        m_summary = right->addChild<DetailsCard>();
    }

    std::string stepName() const override { return tr("Software"); }
    std::string title() const override { return tr("Choose what to install."); }
    std::string description() const override
    {
        return tr("Start from a base environment, then add only what you need. You can install or remove anything "
                  "later with dnf.");
    }
    std::string summary() const override
    {
        for (const SoftwareEnvironment& environment : m_list) {
            if (environment.id == m_environment)
                return environment.name;
        }
        return {};
    }
    bool canContinue() const override { return !m_environment.empty(); }

    void enter() override
    {
        if (m_list.empty()) {
            m_list = installer().backend().environments();
            for (const SoftwareEnvironment& environment : m_list) {
                if (environment.available && m_environment.empty())
                    m_environment = environment.id;
            }
        }
        fillEnvironments();
        fillAddons();
    }

    void commit(std::function<void(bool)> done) override
    {
        Choices& choices = installer().choices();
        choices.environment = m_environment;
        choices.addons = m_selected;
        installer().backend().setSoftware(m_environment, m_selected);
        done(true);
    }

private:
    void fillEnvironments()
    {
        while (!m_environments->children().empty())
            m_environments->takeChild(m_environments->children().back().get());
        bool unavailable = false;
        for (const SoftwareEnvironment& environment : m_list) {
            OptionCard* card = m_environments->addChild<OptionCard>(environment.icon);
            card->setTitle(environment.name);
            card->setDetail(environment.description);
            card->setNote(style::formatSize(environment.installedSize));
            card->setSelected(environment.id == m_environment);
            card->setEnabled(environment.available);
            unavailable = unavailable || !environment.available;
            const std::string id = environment.id;
            card->onClicked = [this, id] {
                m_environment = id;
                fillEnvironments();
                fillAddons();
                changed();
            };
        }
        m_note->setVisible(unavailable);
        m_note->setText(tr("Installing from this live system"),
                        tr("The live system is copied to the disk as it is. The other environments come from the "
                           "network installation image."));
    }

    void fillAddons()
    {
        while (!m_addons->children().empty())
            m_addons->takeChild(m_addons->children().back().get());
        std::string name;
        for (const SoftwareEnvironment& environment : m_list) {
            if (environment.id == m_environment)
                name = environment.name;
        }
        m_addonsLabel->setText(format(tr("ADD-ONS FOR {name}"), {{"name", upper(name)}}));
        const bool online = installer().backend().isOnline();
        const std::vector<SoftwareAddon> addons = installer().backend().addons(m_environment);
        if (!m_initialized) {
            for (const SoftwareAddon& addon : addons) {
                if (addon.selected && (online || !addon.needsNetwork))
                    m_selected.push_back(addon.id);
            }
            m_initialized = true;
        }
        for (const SoftwareAddon& addon : addons) {
            CheckRow* row = m_addons->addChild<CheckRow>(addon.name, addon.description);
            row->setChecked(std::find(m_selected.begin(), m_selected.end(), addon.id) != m_selected.end());
            row->setEnabled(online || !addon.needsNetwork);
            const std::string id = addon.id;
            row->onToggled = [this, id](bool checked) {
                std::erase(m_selected, id);
                if (checked)
                    m_selected.push_back(id);
                installer().backend().setSoftware(m_environment, m_selected);
                updateSummary();
            };
        }
        if (!online && !addons.empty())
            addText(m_addons, tr("Add-ons are downloaded: connect to a network to choose them."), 12, 400,
                    Role::SecondaryText)
                ->setLayoutHints({.margins = {16, 6, 16, 6}});
        installer().backend().setSoftware(m_environment, m_selected);
        updateSummary();
    }

    void updateSummary()
    {
        const SoftwareSummary summary = installer().backend().softwareSummary();
        m_summary->clearLines();
        m_summary->addLine(tr("Packages"), summary.packages ? style::formatCount(summary.packages) : "—");
        m_summary->addLine(tr("Download size"), summary.downloadSize ? style::formatSize(summary.downloadSize)
                                                                     : tr("nothing"));
        m_summary->addLine(tr("Installed size"), style::formatSize(summary.installedSize));
        m_summary->addLine(tr("Est. install time"),
                           summary.estimatedMinutes ? format(tr("~{n} min"), {{"n", std::to_string(summary.estimatedMinutes)}})
                                                    : "—",
                           std::nullopt);
    }

    Column* m_environments;
    Banner* m_note;
    Label* m_addonsLabel;
    Column* m_addons;
    DetailsCard* m_summary;
    std::vector<SoftwareEnvironment> m_list;
    std::string m_environment;
    std::vector<std::string> m_selected;
    bool m_initialized = false;
};

} // namespace

std::unique_ptr<Page> createSoftwarePage(Installer& installer)
{
    return std::make_unique<SoftwarePage>(installer);
}

} // namespace installer
