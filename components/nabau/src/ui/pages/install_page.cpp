// SPDX-License-Identifier: GPL-3.0-or-later
// Step 8: the installation running, then its result.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

#include "base/i18n.h"
#include "base/timer.h"
#include "ui/color_provider.h"
#include "ui/dialog_window.h"
#include "ui/icon.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/path.h"
#include "ui/progress_bar.h"
#include "ui/scroll_view.h"
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

Color resolve(Role role)
{
    return style::color(role).resolve();
}

// The progress bar of the installation: a gradient fill on a quiet track.
class InstallBar : public View
{
public:
    void setValue(double value)
    {
        m_value = std::clamp(value, 0.0, 1.0);
        requestPaint();
    }
    SizeF sizeHint() const override { return {300, 8}; }

protected:
    void paint(Painter& painter) override
    {
        const RectF rect = localRect();
        painter.fillRoundedRect(rect, rect.height / 2, resolve(Role::IconTile));
        if (m_value <= 0)
            return;
        const Color accent = Theme::current().highlight;
        const RectF fill{0, 0, std::max(rect.height, rect.width * m_value), rect.height};
        painter.fillRoundedRect(fill, rect.height / 2,
                                LinearGradient{{0, 0}, {fill.width, 0}, {{0, accent.lighter(1.25)}, {1, accent}}});
    }

private:
    double m_value = 0;
};

// The installation's messages, newest last: "[ ok ]" before what is done,
// "[ .. ]" before what is running.
class LogView : public View
{
public:
    LogView()
    {
        m_scroll = addChild<ScrollView>();
        m_lines = m_scroll->setContent<Column>();
        m_lines->setSpacing(4);
        m_lines->setMargins({22, 18, 22, 18});
        setLayoutHints({.fillWidth = true, .fillHeight = true});
    }

    void add(const InstallLogLine& line)
    {
        const char* mark = line.state == InstallLogLine::State::Done      ? "[ ok ] "
                           : line.state == InstallLogLine::State::Failed ? "[fail] "
                                                                         : "[ .. ] ";
        // The running line before this one is done by now. The backend says
        // so too, for the same step: its line changes, no second one.
        if (m_running) {
            const bool same = line.state != InstallLogLine::State::Running && line.text == m_runningText;
            m_running->setText((same ? mark : "[ ok ] ") + m_runningText);
            m_running->setColor(same && line.state == InstallLogLine::State::Failed ? style::color(Role::Danger)
                                                                                   : style::color(Role::LogText));
            m_running = nullptr;
            if (same)
                return;
        }
        Label* label = addText(m_lines, mark + line.text, 13);
        label->setWrap(true);
        label->setWrapAnywhere(true);
        label->setLayoutHints({.fillWidth = true});
        if (line.state == InstallLogLine::State::Running) {
            label->setColor(ColorRecipe(ColorId::Highlight));
            m_running = label;
            m_runningText = line.text;
        } else {
            label->setColor(line.state == InstallLogLine::State::Failed ? style::color(Role::Danger)
                                                                        : style::color(Role::LogText));
        }
        // Long installations keep their last hundred lines.
        while (m_lines->children().size() > 100) {
            View* first = m_lines->children().front().get();
            if (first == m_running)
                break;
            m_lines->takeChild(first);
        }
        m_follow = true;
        requestLayout();
    }

protected:
    void layout() override
    {
        m_scroll->setGeometry(localRect());
        if (m_follow) {
            m_follow = false;
            const double height = m_lines->heightForWidth(geometry().width);
            m_scroll->setScrollPosition({0, std::max(0.0, height - geometry().height)});
        }
    }
    void paint(Painter& painter) override
    {
        painter.drawRoundedRect(localRect(), {style::cardRadius, 0, Corners::Circular}, resolve(Role::LogBackground),
                                1, resolve(Role::CardBorder));
    }

private:
    ScrollView* m_scroll;
    Column* m_lines;
    Label* m_running = nullptr;
    std::string m_runningText;
    bool m_follow = false;
};

// "While you wait": a card of slides about the system, turning by itself.
class WhileYouWait : public View
{
public:
    struct Slide
    {
        std::string glyph;
        std::string title;
        std::string text;
    };

    explicit WhileYouWait(std::vector<Slide> slides)
        : m_slides(std::move(slides))
    {
        m_icon = addChild<Icon>(std::string(), 46);
        m_icon->setColor(ColorRecipe(Color::rgb(0xFFFFFF)));
        m_eyebrow = addText(this, tr("WHILE YOU WAIT"), 11, 600);
        m_eyebrow->setColor(ColorRecipe(ColorId::Highlight));
        m_eyebrow->setLetterSpacing(2);
        m_title = addText(this, {}, 24, 500);
        m_title->setWrap(true);
        m_text = addText(this, {}, 14, 400, Role::SecondaryText);
        m_text->setWrap(true);
        m_timer.onTimeout = [this] { show((m_index + 1) % m_slides.size()); };
        m_timer.start(std::chrono::seconds(8));
        show(0);
        setLayoutHints({.fillWidth = true, .fillHeight = true});
    }

    bool acceptsPointer() const override { return true; }

protected:
    void pointerPress(const PointerEvent&) override
    {
        show((m_index + 1) % m_slides.size());
        m_timer.start(std::chrono::seconds(8));
    }

    void layout() override
    {
        const double width = geometry().width;
        const double header = std::round(geometry().height * 0.42);
        m_icon->setGeometry({(width - 46) / 2, (header - 46) / 2, 46, 46});
        double y = header + 24;
        const double inner = width - 56;
        m_eyebrow->setGeometry({28, y, inner, m_eyebrow->sizeHint().height});
        y += m_eyebrow->sizeHint().height + 12;
        const double titleHeight = m_title->heightForWidth(inner);
        m_title->setGeometry({28, y, inner, titleHeight});
        y += titleHeight + 10;
        m_text->setGeometry({28, y, inner, m_text->heightForWidth(inner)});
        m_header = header;
    }

    void paint(Painter& painter) override
    {
        const RectF rect = localRect();
        painter.drawRoundedRect(rect, {style::cardRadius, 0, Corners::Circular}, resolve(Role::Card), 1,
                                resolve(Role::CardBorder));
        const Color accent = Theme::current().highlight;
        Path header;
        header.addRoundedRect({0, 0, rect.width, m_header + style::cardRadius}, style::cardRadius, style::cardRadius);
        painter.save();
        painter.clip({0, 0, rect.width, m_header});
        painter.fillPath(header, LinearGradient{{rect.width, 0}, {0, m_header},
                                                {{0, accent.lighter(1.3)}, {1, Color::rgb(0xB58CFF)}}});
        painter.restore();
        // The dots: one per slide, the current one long.
        double x = 28;
        const double y = rect.height - 28;
        for (size_t i = 0; i < m_slides.size(); ++i) {
            const double w = i == m_index ? 22 : 7;
            painter.fillRoundedRect({x, y, w, 7}, 3.5, i == m_index ? accent : accent.withAlpha(0.3));
            x += w + 6;
        }
    }

private:
    void show(size_t index)
    {
        m_index = index;
        const Slide& slide = m_slides[index];
        m_icon->setIconName(slide.glyph);
        m_title->setText(slide.title);
        m_text->setText(slide.text);
        requestLayout();
        requestPaint();
    }

    std::vector<Slide> m_slides;
    size_t m_index = 0;
    Icon* m_icon;
    Label* m_eyebrow;
    Label* m_title;
    Label* m_text;
    double m_header = 0;
    bayu::base::Timer m_timer;
};

// The green check of a finished installation.
class DoneMark : public View
{
public:
    SizeF sizeHint() const override { return {82, 82}; }

protected:
    void paint(Painter& painter) override
    {
        const RectF rect = localRect();
        const Color green = resolve(Role::Success);
        painter.drawShadow(rect, rect.height / 2, 18, green.withAlpha(0.18));
        painter.drawSmoothRectangle(rect, rect.height / 2, resolve(Role::Content), 2, green);
        Path check;
        check.moveTo(rect.x + rect.width * 0.32, rect.y + rect.height * 0.52);
        check.lineTo(rect.x + rect.width * 0.45, rect.y + rect.height * 0.64);
        check.lineTo(rect.x + rect.width * 0.69, rect.y + rect.height * 0.38);
        painter.strokePath(check, green, 4);
    }
};

// A number with a glyph above and a caption below.
class StatCard : public Column
{
public:
    StatCard(std::string glyph, std::string value, std::string caption)
    {
        setSpacing(6);
        setMargins({16, 22, 16, 22});
        Icon* icon = addChild<Icon>(std::move(glyph), 16);
        icon->setColor(style::color(Role::SecondaryText));
        icon->setLayoutHints({.alignment = Alignment::Center});
        Label* number = addText(this, std::move(value), 20, 500);
        number->setLayoutHints({.alignment = Alignment::Center});
        Label* text = addText(this, std::move(caption), 13, 400, Role::SecondaryText);
        text->setLayoutHints({.alignment = Alignment::Center});
        setLayoutHints({.fillWidth = true, .preferredWidth = 0});
    }

protected:
    void paint(Painter& painter) override
    {
        painter.drawRoundedRect(localRect(), {style::cardRadius, 0, Corners::Circular}, resolve(Role::Card), 1,
                                resolve(Role::CardBorder));
    }
};

class InstallPage : public Page
{
public:
    enum class State {
        Installing,
        Complete,
        Failed,
    };

    explicit InstallPage(Installer& installer)
        : Page(installer)
    {
        m_installing = addChild<Row>();
        m_installing->setSpacing(30);
        m_installing->setLayoutHints({.fillWidth = true, .fillHeight = true});
        Column* left = m_installing->addChild<Column>();
        left->setSpacing(14);
        left->setLayoutHints({.fillWidth = true, .fillHeight = true, .stretch = 1.2, .preferredWidth = 0});
        Row* numbers = left->addChild<Row>();
        numbers->setLayoutHints({.fillWidth = true});
        m_percent = addText(numbers, "0", 76, 300);
        m_percent->setFontFeatures({{"tnum", 1}});
        Label* sign = addText(numbers, "%", 30, 400);
        sign->setColor(ColorRecipe(ColorId::Highlight));
        sign->setLayoutHints({.alignment = Alignment::Start, .margins = {6, 18, 0, 0}});
        numbers->addStretch();
        Column* remaining = numbers->addChild<Column>();
        remaining->setSpacing(2);
        Label* remainingLabel = addText(remaining, tr("Estimated remaining"), 13, 400, Role::SecondaryText);
        remainingLabel->setLayoutHints({.alignment = Alignment::End});
        m_remaining = addText(remaining, "—", 15);
        m_remaining->setLayoutHints({.alignment = Alignment::End});
        m_bar = left->addChild<InstallBar>();
        m_bar->setLayoutHints({.fillWidth = true, .margins = {0, 0, 0, 6}});
        Card* status = left->addChild<Card>(0);
        status->setMargins({18, 12, 18, 12});
        Row* statusRow = status->addChild<Row>();
        statusRow->setSpacing(12);
        statusRow->setLayoutHints({.fillWidth = true});
        m_spinner = statusRow->addChild<BusyIndicator>();
        m_spinner->setColor(Theme::current().highlight);
        m_status = addText(statusRow, tr("Starting the installation…"), 14);
        m_status->setLayoutHints({.fillWidth = true});
        m_log = left->addChild<LogView>();
        m_carousel = m_installing->addChild<WhileYouWait>(std::vector<WhileYouWait::Slide>{
            {"terminal", tr("Ten years of updates"),
             tr("Every release is supported until 2035, with security errata published alongside upstream. Switch "
                "between minor versions without reinstalling.")},
            {"monitor", tr("A desktop of its own"),
             tr("Bayu brings Files, Terminal, Settings and a dock that stays out of your way, all drawn by one "
                "toolkit.")},
            {"layers", tr("Android apps, when you want them"),
             tr("Lapis runs Android apps beside your desktop ones. It is installed and stays off until you turn it "
                "on.")},
            {"shield-check", tr("Safe by default"),
             tr("SELinux, a firewall and signed packages from the start; full-disk encryption when you ask for it.")},
        });
        m_carousel->setLayoutHints({.fillWidth = true, .fillHeight = true, .preferredWidth = 0});

        m_done = addChild<Column>();
        m_done->setSpacing(26);
        m_done->setLayoutHints({.fillWidth = true, .fillHeight = true});
        m_done->setVisible(false);
        m_done->addStretch();
        DoneMark* mark = m_done->addChild<DoneMark>();
        mark->setLayoutHints({.alignment = Alignment::Center});
        m_stats = m_done->addChild<Row>();
        m_stats->setSpacing(14);
        m_stats->setLayoutHints({.fillWidth = true, .alignment = Alignment::Center, .preferredWidth = 760,
                                 .maximumWidth = 760});
        m_notes = m_done->addChild<Card>(0);
        m_notes->setMargins({24, 18, 24, 18});
        m_notes->setSpacing(10);
        m_notes->setLayoutHints({.fillWidth = false, .alignment = Alignment::Center, .preferredWidth = 760});
        m_done->addStretch();

        m_failed = addChild<Column>();
        m_failed->setSpacing(14);
        m_failed->setLayoutHints({.fillWidth = true, .fillHeight = true});
        m_failed->setVisible(false);
        m_error = m_failed->addChild<Banner>(Banner::Kind::Error, std::string(), std::string());
    }

    std::string stepName() const override { return tr("Install"); }
    std::string eyebrow() const override
    {
        if (m_state == State::Complete)
            return tr("INSTALLATION COMPLETE");
        if (m_state == State::Failed)
            return tr("INSTALLATION FAILED");
        return tr("STEP 08 — INSTALLING");
    }
    std::string title() const override
    {
        const std::string product = installer().backend().systemInfo().productShortName;
        if (m_state == State::Complete)
            return tr("You're all set.");
        if (m_state == State::Failed)
            return tr("The installation stopped.");
        return format(tr("Installing {product}."), {{"product", product}});
    }
    std::string description() const override
    {
        const std::string product = installer().backend().systemInfo().productShortName;
        if (m_state == State::Complete)
            return format(tr("{product} is on your disk. Remove the installation media, restart, and sign in with "
                             "the account you just created."),
                          {{"product", product}});
        if (m_state == State::Failed)
            return tr("Nothing more is written. The log has the details; you can start the installer again.");
        return tr("Everything is set. Sit back — the installer is writing packages to disk and will tell you when "
                  "it is safe to restart.");
    }
    std::string summary() const override
    {
        return m_state == State::Complete ? style::formatDuration(m_result.seconds) : std::string();
    }
    bool canGoBack() const override { return false; }
    bool ownsFooter() const override { return true; }

    void enter() override
    {
        if (m_started)
            return;
        m_started = true;
        m_startTime = std::chrono::steady_clock::now();
        updateFooter();
        installer().backend().install(
            [this](const InstallProgress& progress) { setProgress(progress); },
            [this](const InstallLogLine& line) { m_log->add(line); },
            [this](const InstallResult& result) { finish(result); });
    }

private:
    void setProgress(const InstallProgress& progress)
    {
        m_percent->setText(std::to_string(int(std::floor(progress.fraction * 100))));
        m_bar->setValue(progress.fraction);
        if (!progress.status.empty())
            m_status->setText(progress.status);
        int remaining = progress.remainingSeconds;
        if (remaining < 0 && progress.fraction > 0.05) {
            const double elapsed =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - m_startTime).count();
            remaining = int(elapsed / progress.fraction * (1 - progress.fraction));
        }
        if (remaining >= 0) {
            char text[32];
            snprintf(text, sizeof(text), remaining >= 60 ? "%d min %d s" : "%d s", remaining >= 60 ? remaining / 60 : remaining,
                     remaining % 60);
            m_remaining->setText(text);
        }
        updateFooter();
    }

    void finish(const InstallResult& result)
    {
        m_result = result;
        m_spinner->setRunning(false);
        if (!result.succeeded) {
            m_state = State::Failed;
            m_installing->setVisible(false);
            m_failed->setVisible(true);
            m_error->setText(tr("The installation could not finish"), result.error);
            updateFooter();
            changed();
            return;
        }
        m_state = State::Complete;
        m_installing->setVisible(false);
        m_done->setVisible(true);
        while (!m_stats->children().empty())
            m_stats->takeChild(m_stats->children().back().get());
        m_stats->addChild<StatCard>("timer", style::formatDuration(result.seconds), tr("Install time"));
        m_stats->addChild<StatCard>("package", style::formatCount(result.packages), tr("Packages written"));
        m_stats->addChild<StatCard>("shield", result.encrypted ? "LUKS2" : tr("None"), tr("Disk encryption"));
        while (!m_notes->children().empty())
            m_notes->takeChild(m_notes->children().back().get());
        auto note = [this](const char* glyph, Role tint, const std::string& text) {
            Row* row = m_notes->addChild<Row>();
            row->setSpacing(12);
            row->setLayoutHints({.fillWidth = true});
            Icon* icon = row->addChild<Icon>(glyph, 16);
            icon->setColor(style::color(tint));
            Label* label = addText(row, text, 14);
            label->setWrap(true);
            label->setLayoutHints({.fillWidth = true});
        };
        note("usb", Role::Warning, tr("Remove the USB installation media before restarting."));
        note("user", Role::SecondaryText,
             format(tr("Sign in as {user}; your settings start fresh."),
                    {{"user", installer().choices().account.userName}}));
        note("terminal", Role::SecondaryText, tr("Run sudo dnf upgrade to pull any errata published since this ISO."));
        installer().setFinished(true);
        updateFooter();
        changed();
    }

    void updateFooter()
    {
        Row* footer = installer().footerActions();
        while (!footer->children().empty())
            footer->takeChild(footer->children().back().get());
        if (m_state == State::Installing) {
            // Anaconda cannot stop halfway: the log instead of a Cancel that
            // would leave the disk half written.
            if (installer().backend().canCancelInstall()) {
                ActionButton* cancel = footer->addChild<ActionButton>(tr("Cancel"));
                cancel->setLeadingGlyph("x");
                cancel->onClicked = [this] { installer().requestQuit(); };
            } else {
                ActionButton* log = footer->addChild<ActionButton>(tr("View log"));
                log->setLeadingGlyph("scroll");
                log->onClicked = [this] { installer().showLog(); };
            }
            ActionButton* busy = footer->addChild<ActionButton>(tr("Installing…"), ActionButton::Kind::Primary);
            busy->setBusy(true);
        } else {
            ActionButton* log = footer->addChild<ActionButton>(tr("View installation log"));
            log->setLeadingGlyph("scroll");
            log->onClicked = [this] { installer().showLog(); };
            if (m_state == State::Complete) {
                ActionButton* restart = footer->addChild<ActionButton>(tr("Restart now"), ActionButton::Kind::Success);
                restart->setTrailingGlyph("power");
                restart->onClicked = [this] { installer().backend().reboot(); };
            } else {
                ActionButton* quit = footer->addChild<ActionButton>(tr("Quit"), ActionButton::Kind::Secondary);
                quit->onClicked = [this] { installer().requestQuit(); };
            }
        }
        footer->requestLayout();
    }

    Row* m_installing;
    Label* m_percent;
    Label* m_remaining;
    InstallBar* m_bar;
    BusyIndicator* m_spinner;
    Label* m_status;
    LogView* m_log;
    WhileYouWait* m_carousel;
    Column* m_done;
    Row* m_stats;
    Card* m_notes;
    Column* m_failed;
    Banner* m_error;
    State m_state = State::Installing;
    InstallResult m_result;
    bool m_started = false;
    std::chrono::steady_clock::time_point m_startTime;
};

} // namespace

std::unique_ptr<Page> createInstallPage(Installer& installer)
{
    return std::make_unique<InstallPage>(installer);
}

} // namespace installer
