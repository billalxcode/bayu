// SPDX-License-Identifier: BSD-3-Clause
// Step 2: the keyboard layout, and the time zone and clock.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

#include "base/i18n.h"
#include "base/locale.h"
#include "base/timer.h"
#include "ui/color_provider.h"
#include "ui/icon.h"
#include "ui/label.h"
#include "ui/scroll_view.h"
#include "ui/painter.h"
#include "ui/theme.h"
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

std::string displayName(std::string name)
{
    std::replace(name.begin(), name.end(), '_', ' ');
    return name;
}

// The time now in a zone: the wall clock, its abbreviation and offset.
struct ZoneTime
{
    bayu::base::DateTime dateTime;
    std::string abbreviation;
    int offset = 0;
};

ZoneTime zoneTime(const std::string& zone)
{
    ZoneTime result;
    const auto now = std::chrono::system_clock::now();
    try {
        const std::chrono::time_zone* tz = std::chrono::locate_zone(zone);
        const std::chrono::sys_info info = tz->get_info(now);
        result.abbreviation = info.abbrev;
        result.offset = int(info.offset.count());
    } catch (...) {
        result.abbreviation = "UTC";
    }
    const auto local = std::chrono::floor<std::chrono::seconds>(now) + std::chrono::seconds(result.offset);
    const auto days = std::chrono::floor<std::chrono::days>(local);
    const std::chrono::year_month_day ymd{days};
    const std::chrono::hh_mm_ss hms{local - days};
    result.dateTime.date = {int(ymd.year()), int(unsigned(ymd.month())), int(unsigned(ymd.day()))};
    result.dateTime.time = {int(hms.hours().count()), int(hms.minutes().count()), int(hms.seconds().count()), 0};
    result.dateTime.utcOffset = result.offset;
    result.dateTime.zoneAbbreviation = result.abbreviation;
    result.dateTime.zoneId = zone;
    return result;
}

std::string offsetText(int offset)
{
    char text[16];
    const int minutes = std::abs(offset) / 60;
    snprintf(text, sizeof(text), "UTC%c%02d:%02d", offset < 0 ? '-' : '+', minutes / 60, minutes % 60);
    return text;
}

// The big clock: the time in the chosen zone, its date and UTC offset.
class ClockCard : public View
{
public:
    ClockCard()
    {
        m_time = addText(this, {}, 54, 400);
        m_time->setFontFeatures({{"tnum", 1}});
        m_zone = addText(this, {}, 13, 400, Role::SecondaryText);
        m_date = addText(this, {}, 15, 400);
        m_offsetIcon = addChild<Icon>("globe", 14);
        m_offsetIcon->setColor(ColorRecipe(ColorId::Highlight));
        m_offset = addText(this, {}, 13, 500);
        m_offset->setColor(ColorRecipe(ColorId::Highlight));
        m_timer.onTimeout = [this] { update(); };
        m_timer.start(std::chrono::seconds(1));
        setLayoutHints({.fillWidth = true});
    }

    void setZone(std::string zone, std::string locale)
    {
        m_zoneId = std::move(zone);
        m_locale = std::move(locale);
        update();
    }

    SizeF sizeHint() const override { return {300, 118}; }

protected:
    void layout() override
    {
        const double width = geometry().width;
        const SizeF time = m_time->sizeHint();
        m_time->setGeometry({22, 6, time.width + 2, time.height});
        const SizeF zone = m_zone->sizeHint();
        m_zone->setGeometry({22 + time.width + 10, 6 + m_time->baselineOffset() - m_zone->baselineOffset(),
                             zone.width + 2, zone.height});
        const SizeF date = m_date->sizeHint();
        m_date->setGeometry({22, geometry().height - 18 - date.height, width - 44, date.height});
        const SizeF offset = m_offset->sizeHint();
        m_pill = {width - 20 - (offset.width + 44), 18, offset.width + 44, 28};
        m_offsetIcon->setGeometry({m_pill.x + 14, m_pill.y + 7, 14, 14});
        m_offset->setGeometry({m_pill.x + 34, m_pill.y + (28 - offset.height) / 2, offset.width + 2, offset.height});
    }

    void paint(Painter& painter) override
    {
        painter.drawRoundedRect(localRect(), {style::cardRadius, 0, Corners::Circular},
                                style::color(Role::Card).resolve(), 1, style::color(Role::CardBorder).resolve());
        painter.drawRoundedRect(m_pill, {14, 0, Corners::Circular}, style::color(Role::InfoFill).resolve(), 1,
                                Theme::current().highlight);
    }

private:
    void update()
    {
        if (m_zoneId.empty())
            return;
        const ZoneTime now = zoneTime(m_zoneId);
        const bayu::base::Locale locale(m_locale.substr(0, m_locale.find('.')));
        char time[8];
        snprintf(time, sizeof(time), "%02d:%02d", now.dateTime.time.hour, now.dateTime.time.minute);
        m_time->setText(time);
        m_zone->setText(now.abbreviation);
        m_date->setText(locale.toString(now.dateTime.date, bayu::base::Locale::FormatType::Long));
        m_offset->setText(offsetText(now.offset));
        requestLayout();
    }

    Label* m_time;
    Label* m_zone;
    Label* m_date;
    Icon* m_offsetIcon;
    Label* m_offset;
    RectF m_pill;
    std::string m_zoneId;
    std::string m_locale = "en_US";
    bayu::base::Timer m_timer;
};

// A line under a column: a glyph and a quiet note.
class StatusLine : public Row
{
public:
    StatusLine(std::string glyph)
    {
        setSpacing(8);
        m_icon = addChild<Icon>(std::move(glyph), 14);
        m_icon->setColor(style::color(Role::Success));
        m_text = addText(this, {}, 12, 400, Role::SecondaryText);
    }
    void set(std::string text, bool good)
    {
        m_text->setText(std::move(text));
        m_icon->setColor(good ? style::color(Role::Success) : style::color(Role::TertiaryText));
    }

private:
    Icon* m_icon;
    Label* m_text;
};

class LocalizationPage : public Page
{
public:
    explicit LocalizationPage(Installer& installer)
        : Page(installer)
    {
        Row* body = addChild<Row>();
        body->setSpacing(style::columnSpacing);
        body->setLayoutHints({.fillWidth = true, .fillHeight = true});

        Column* left = body->addChild<Column>();
        left->setSpacing(10);
        left->setLayoutHints({.fillWidth = true, .fillHeight = true, .stretch = 1.05, .preferredWidth = 0});
        addSectionLabel(left, tr("KEYBOARD LAYOUT"));
        m_layouts = left->addChild<ListCard>();
        Label* test = addText(left, tr("Test the layout"), 13, 400, Role::SecondaryText);
        test->setLayoutHints({.margins = {0, 10, 0, 0}});
        Field* field = left->addChild<Field>();
        field->setPlaceholder(tr("Type here to test the layout"));

        // Scrolls on short screens (1366×768).
        ScrollView* rightScroll = body->addChild<ScrollView>();
        rightScroll->setVerticalBarPolicy(ScrollView::BarPolicy::AlwaysOn);
        rightScroll->setLayoutHints({.fillWidth = true, .fillHeight = true, .preferredWidth = 0});
        Column* right = rightScroll->setContent<Column>();
        right->setSpacing(10);
        addSectionLabel(right, tr("TIME & DATE"));
        m_clock = right->addChild<ClockCard>();
        Row* zone = right->addChild<Row>();
        zone->setSpacing(14);
        zone->setLayoutHints({.fillWidth = true, .margins = {0, 8, 0, 4}});
        Column* regionColumn = zone->addChild<Column>();
        regionColumn->setSpacing(8);
        regionColumn->setLayoutHints({.fillWidth = true});
        addText(regionColumn, tr("Region"), 13, 400, Role::SecondaryText);
        m_region = regionColumn->addChild<Dropdown>();
        Column* cityColumn = zone->addChild<Column>();
        cityColumn->setSpacing(8);
        cityColumn->setLayoutHints({.fillWidth = true});
        addText(cityColumn, tr("City"), 13, 400, Role::SecondaryText);
        m_city = cityColumn->addChild<Dropdown>();
        m_ntp = right->addChild<ToggleCard>(tr("Network time (NTP)"), tr("Keep the clock in sync automatically"));
        m_detect = right->addChild<ToggleCard>(tr("Detect time zone from network"),
                                               tr("Uses your IP address, never your location"));
        right->addStretch();
        m_status = right->addChild<StatusLine>("rotate");

        m_zones = installer.backend().timezones();
        std::vector<std::string> regions;
        for (const auto& [region, cities] : m_zones)
            regions.push_back(displayName(region));
        m_region->setItems(regions);
        m_region->onActivated = [this](int index) {
            fillCities(size_t(index), {});
            selectZone();
        };
        m_city->onActivated = [this](int) { selectZone(); };
        m_ntp->onToggled = [this](bool on) {
            m_time.ntp = on;
            updateStatus();
        };
        m_detect->onToggled = [this](bool on) { m_time.geolocation = on; };

        m_time = installer.backend().timeSettings();
        m_layout = installer.backend().keyboardLayout();
    }

    std::string stepName() const override { return tr("Localization"); }
    std::string title() const override { return tr("Keyboard and time."); }
    std::string description() const override
    {
        return tr("Choose the layout you actually type with and confirm your time zone. Both take effect "
                  "immediately, including on this screen.");
    }
    std::string summary() const override { return m_time.timezone; }
    bool canContinue() const override { return !m_layout.empty() && !m_time.timezone.empty(); }

    void enter() override
    {
        fillLayouts();
        showZone(m_time.timezone);
        m_ntp->setChecked(m_time.ntp);
        m_detect->setChecked(m_time.geolocation);
        m_clock->setZone(m_time.timezone, installer().choices().locale.empty() ? "en_US"
                                                                                : installer().choices().locale);
        updateStatus();
    }

    void commit(std::function<void(bool)> done) override
    {
        Choices& choices = installer().choices();
        choices.keyboardLayout = m_layout;
        choices.time = m_time;
        installer().backend().setKeyboardLayout(m_layout);
        installer().backend().setTimeSettings(m_time);
        done(true);
    }

private:
    void fillLayouts()
    {
        m_layouts->clearRows();
        ListRow* selected = nullptr;
        for (const KeyboardLayout& layout : installer().backend().keyboardLayouts(installer().choices().language)) {
            ListRow* row = m_layouts->addRow();
            row->setBadge(layout.code);
            row->setTitle(layout.name);
            row->setDetail(layout.detail);
            const std::string id = layout.id;
            row->onClicked = [this, row, id] {
                m_layout = id;
                m_layouts->select(row);
                // Applied at once, so the test field types with it.
                installer().backend().setKeyboardLayout(id);
                changed();
            };
            if (id == m_layout)
                selected = row;
        }
        if (!selected && !m_layouts->rows().empty())
            selected = m_layouts->rows().front();
        m_layouts->select(selected);
    }

    void showZone(const std::string& zone)
    {
        const size_t slash = zone.find('/');
        const std::string region = slash == std::string::npos ? zone : zone.substr(0, slash);
        const std::string city = slash == std::string::npos ? std::string() : zone.substr(slash + 1);
        for (size_t i = 0; i < m_zones.size(); ++i) {
            if (m_zones[i].first == region) {
                m_region->setCurrentIndex(int(i));
                fillCities(i, city);
                return;
            }
        }
    }

    void fillCities(size_t region, const std::string& city)
    {
        std::vector<std::string> names;
        int current = 0;
        for (size_t i = 0; i < m_zones[region].second.size(); ++i) {
            names.push_back(displayName(m_zones[region].second[i]));
            if (m_zones[region].second[i] == city)
                current = int(i);
        }
        m_city->setItems(names);
        m_city->setCurrentIndex(names.empty() ? -1 : current);
    }

    void selectZone()
    {
        const int region = m_region->currentIndex();
        const int city = m_city->currentIndex();
        if (region < 0 || city < 0)
            return;
        const auto& entry = m_zones[size_t(region)];
        m_time.timezone = entry.first + "/" + entry.second[size_t(city)];
        m_clock->setZone(m_time.timezone,
                         installer().choices().locale.empty() ? "en_US" : installer().choices().locale);
        changed();
    }

    void updateStatus()
    {
        if (!m_time.ntp) {
            m_status->set(tr("Network time is off · the clock is set by hand"), false);
            return;
        }
        const std::string server = m_time.ntpServer.empty() ? "pool.ntp.org" : m_time.ntpServer;
        if (installer().backend().isOnline())
            m_status->set(format(tr("{server} · synchronised"), {{"server", server}}), true);
        else
            m_status->set(format(tr("{server} · waits for a connection"), {{"server", server}}), false);
    }

    ListCard* m_layouts;
    ClockCard* m_clock;
    Dropdown* m_region;
    Dropdown* m_city;
    ToggleCard* m_ntp;
    ToggleCard* m_detect;
    StatusLine* m_status;
    std::vector<std::pair<std::string, std::vector<std::string>>> m_zones;
    TimeSettings m_time;
    std::string m_layout;
};

} // namespace

std::unique_ptr<Page> createLocalizationPage(Installer& installer)
{
    return std::make_unique<LocalizationPage>(installer);
}

} // namespace installer
