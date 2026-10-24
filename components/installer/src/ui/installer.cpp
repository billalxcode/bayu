// SPDX-License-Identifier: BSD-3-Clause

#include "ui/installer.h"

#include <cmath>
#include <cstdio>

#include "base/event_loop.h"
#include "base/i18n.h"
#include "base/process.h"
#include "ui/application.h"
#include "ui/color_provider.h"
#include "ui/dialog_window.h"
#include "ui/header_button.h"
#include "ui/icon.h"
#include "ui/keys.h"
#include "ui/label.h"
#include "ui/menu.h"
#include "ui/painter.h"
#include "ui/path.h"
#include "ui/progress_bar.h"
#include "ui/scroll_view.h"
#include "ui/theme.h"
#include "ui/window.h"
#include "ui/window_header.h"
#include "ui/page.h"
#include "ui/pages.h"
#include "ui/widgets.h"

using namespace bayu::ui;
using bayu::i18n::format;
using bayu::i18n::tr;

namespace installer {

using style::Role;

static constexpr uint32_t kF12 = 0xffc9;

namespace {

Color resolve(Role role)
{
    return style::color(role).resolve();
}

// A step in the sidebar: its state mark, its name and, once done, what was
// chosen.
class StepItem : public Pressable
{
public:
    enum class State {
        Pending,
        Current,
        Done,
    };

    StepItem()
    {
        m_name = addText(this, {}, 15, 400);
        m_summary = addText(this, {}, 12, 400, Role::SecondaryText);
        // Steps not reached yet cannot be clicked, but read as plainly.
        m_name->setFadesWhenDisabled(false);
        m_summary->setFadesWhenDisabled(false);
        setLayoutHints({.fillWidth = true});
    }

    void set(const std::string& name, const std::string& summary, State state)
    {
        m_state = state;
        m_name->setText(name);
        m_name->setWeight(state == State::Current ? 600 : 400);
        const bool showSummary = state == State::Done && !summary.empty();
        m_summary->setText(showSummary ? summary : std::string());
        m_summary->setVisible(showSummary);
        requestLayout();
        requestPaint();
    }

    State state() const { return m_state; }

    SizeF sizeHint() const override { return {200, m_summary->isVisible() ? 50 : 34}; }

protected:
    void layout() override
    {
        const double width = geometry().width;
        const double height = geometry().height;
        const double x = 40;
        const double nameHeight = m_name->sizeHint().height;
        if (m_summary->isVisible()) {
            const double summaryHeight = m_summary->sizeHint().height;
            const double top = std::round((height - nameHeight - summaryHeight) / 2);
            m_name->setGeometry({x, top, width - x - 8, nameHeight});
            m_summary->setGeometry({x, top + nameHeight, width - x - 8, summaryHeight});
        } else {
            m_name->setGeometry({x, std::round((height - nameHeight) / 2), width - x - 8, nameHeight});
        }
    }

    void paint(Painter& painter) override
    {
        const RectF rect = localRect();
        const Color accent = Theme::current().highlight;
        if (m_state == State::Current)
            painter.drawRoundedRect(rect, {8, 0, Corners::Circular}, resolve(Role::StepCurrent));
        else if (isEnabled() && (isHovered() || isPressed()) && m_state == State::Done)
            painter.drawRoundedRect(rect, {8, 0, Corners::Circular}, Theme::current().text.withAlpha(0.05));
        const RectF mark{14, std::round((rect.height - 17) / 2), 17, 17};
        switch (m_state) {
        case State::Pending:
            painter.drawSmoothRectangle(mark, mark.height / 2, Color{0, 0, 0, 0}, 1.5,
                                        resolve(Role::TertiaryText));
            break;
        case State::Current:
            painter.drawSmoothRectangle(mark, mark.height / 2, Color{0, 0, 0, 0}, 1.5, accent);
            painter.fillEllipse({mark.x + 6.5, mark.y + 6.5, 4, 4}, accent);
            break;
        case State::Done: {
            const Color green = resolve(Role::Success);
            painter.drawSmoothRectangle(mark, mark.height / 2, Color{0, 0, 0, 0}, 1.5, green);
            Path check;
            check.moveTo(mark.x + 5, mark.y + 8.8);
            check.lineTo(mark.x + 7.6, mark.y + 11.2);
            check.lineTo(mark.x + 12.2, mark.y + 6.2);
            painter.strokePath(check, green, 1.5);
            break;
        }
        }
        if (hasVisibleFocus())
            painter.drawRoundedRect(rect, {8, 0, Corners::Circular}, Color{0, 0, 0, 0}, 2, accent.withAlpha(0.5));
    }

private:
    Label* m_name;
    Label* m_summary;
    State m_state = State::Pending;
};

// The product's mark: a letter on an accent tile.
class LogoTile : public View
{
public:
    explicit LogoTile(std::string letter)
    {
        m_letter = addText(this, std::move(letter), 17, 500);
        m_letter->setColor(ColorRecipe(Color::rgb(0xFFFFFF)));
        m_letter->setTextAlignment(Label::TextAlignment::Center);
        m_letter->setVerticallyCentered(true);
    }
    SizeF sizeHint() const override { return {30, 30}; }

protected:
    void layout() override { m_letter->setGeometry(localRect()); }
    void paint(Painter& painter) override
    {
        const Color accent = Theme::current().highlight;
        Path tile;
        tile.addRoundedRect(localRect(), 7, 7);
        painter.fillPath(tile, LinearGradient{{0, 0}, {30, 30}, {{0, accent.lighter(1.2)}, {1, accent.darker(1.15)}}});
    }

private:
    Label* m_letter;
};

class Sidebar : public View
{
public:
    explicit Sidebar(Installer& installer)
        : m_installer(installer)
    {
        const SystemInfo info = installer.backend().systemInfo();
        std::string name = info.productShortName.empty() ? info.productName : info.productShortName;
        m_logo = addChild<LogoTile>(name.empty() ? "I" : name.substr(0, 1));
        m_product = addText(this, name, 15, 600);
        m_subtitle = addText(this, tr("Guided installation"), 12, 400, Role::SecondaryText);
        m_section = addSectionLabel(this, tr("SETUP"));
        m_steps = addChild<Column>();
        m_steps->setSpacing(4);
        m_stepCount = addText(this, {}, 12, 400, Role::SecondaryText);
        m_percent = addText(this, {}, 12, 500);
        m_percent->setColor(ColorRecipe(ColorId::Highlight));
        m_percent->setTextAlignment(Label::TextAlignment::Right);
        m_progress = addChild<ProgressBar>();
        std::string build = format("anaconda {version} · {arch} · {firmware}",
                                   {{"version", info.backendVersion},
                                    {"arch", info.architecture},
                                    {"firmware", info.uefi ? "UEFI" : "BIOS"}});
        m_build = addText(this, std::move(build), 11, 400, Role::TertiaryText);
    }

    void update()
    {
        const size_t count = m_installer.pageCount();
        while (m_items.size() < count) {
            StepItem* item = m_steps->addChild<StepItem>();
            const size_t index = m_items.size();
            item->onClicked = [this, index] { m_installer.goTo(index); };
            m_items.push_back(item);
        }
        const size_t current = m_installer.currentIndex();
        const bool finished = m_installer.isFinished();
        for (size_t i = 0; i < count; ++i) {
            Page* page = m_installer.page(i);
            StepItem::State state = StepItem::State::Pending;
            if (finished || (m_installer.isDone(i) && i != current))
                state = StepItem::State::Done;
            else if (i == current)
                state = StepItem::State::Current;
            m_items[i]->set(page->stepName(), page->summary(), state);
            // Steps done can be gone back to, until the installation starts.
            m_items[i]->setEnabled(state == StepItem::State::Done && !finished
                                   && m_installer.page(current)->canGoBack());
        }
        const double fraction = finished ? 1.0 : double(current + 1) / double(count);
        m_stepCount->setText(finished ? tr("All steps complete")
                                      : format(tr("Step {n} of {count}"),
                                               {{"n", std::to_string(current + 1)}, {"count", std::to_string(count)}}));
        m_percent->setText(std::to_string(int(std::floor(fraction * 100))) + "%");
        m_percent->setColor(finished ? style::color(Role::Success) : ColorRecipe(ColorId::Highlight));
        m_progress->setValue(fraction);
        requestLayout();
    }

protected:
    void layout() override
    {
        const double width = geometry().width;
        const double height = geometry().height;
        const double inner = width - 2 * 22;
        m_logo->setGeometry({30, 30, 30, 30});
        const double productHeight = m_product->sizeHint().height;
        m_product->setGeometry({70, 27, width - 90, productHeight});
        m_subtitle->setGeometry({70, 27 + productHeight, width - 90, m_subtitle->sizeHint().height});
        m_section->setGeometry({22, 92, inner, m_section->sizeHint().height});
        m_steps->setGeometry({22, 108, inner, m_steps->heightForWidth(inner)});
        const double bottom = height - 30;
        const double buildHeight = m_build->sizeHint().height;
        m_build->setGeometry({22, bottom - buildHeight, inner, buildHeight});
        m_ruleY = bottom - buildHeight - 18;
        m_progress->setGeometry({30, m_ruleY - 22, width - 60, 4});
        const double countHeight = m_stepCount->sizeHint().height;
        m_stepCount->setGeometry({30, m_ruleY - 30 - countHeight, inner, countHeight});
        m_percent->setGeometry({30, m_ruleY - 30 - countHeight, width - 60, countHeight});
    }

    void paint(Painter& painter) override
    {
        painter.fillRect(localRect(), resolve(Role::Sidebar));
        painter.drawHorizontalLine(22, m_ruleY, geometry().width - 44, Theme::current().text.withAlpha(0.08));
        painter.drawVerticalLine(geometry().width - 1, 0, geometry().height, Theme::current().text.withAlpha(0.06));
    }

private:
    Installer& m_installer;
    LogoTile* m_logo;
    Label* m_product;
    Label* m_subtitle;
    Label* m_section;
    Column* m_steps;
    std::vector<StepItem*> m_items;
    Label* m_stepCount;
    Label* m_percent;
    ProgressBar* m_progress;
    Label* m_build;
    double m_ruleY = 0;
};

// The page's heading: "STEP 01 — LANGUAGE", the title and the description.
class Heading : public Column
{
public:
    Heading()
    {
        setSpacing(0);
        m_eyebrow = addText(this, {}, style::eyebrowSize, 600);
        m_eyebrow->setColor(ColorRecipe(ColorId::Highlight));
        m_eyebrow->setLetterSpacing(2);
        m_title = addText(this, {}, style::titleSize, 500);
        m_title->setLayoutHints({.margins = {0, 16, 0, 0}});
        m_description = addText(this, {}, style::bodySize, 400, Role::SecondaryText);
        m_description->setWrap(true);
        m_description->setLayoutHints({.fillWidth = true, .maximumWidth = 600, .margins = {0, 16, 0, 0}});
        setLayoutHints({.fillWidth = true});
    }

    void set(const std::string& eyebrow, const std::string& title, const std::string& description, bool complete)
    {
        m_eyebrow->setText(eyebrow);
        m_eyebrow->setColor(complete ? style::color(Role::Success) : ColorRecipe(ColorId::Highlight));
        m_title->setText(title);
        m_description->setText(description);
    }

private:
    Label* m_eyebrow;
    Label* m_title;
    Label* m_description;
};

class Footer : public View
{
public:
    Footer()
    {
        m_hints = addText(this, tr("F1 Help   ·   F12 Console"), 12, 400, Role::SecondaryText);
        m_extra = addChild<Row>();
        m_extra->setSpacing(12);
        back = addChild<ActionButton>(tr("Back"), ActionButton::Kind::Secondary);
        back->setLeadingGlyph("arrow-left");
        next = addChild<ActionButton>(tr("Continue"), ActionButton::Kind::Primary);
        next->setTrailingGlyph("arrow-right");
        setLayoutHints({.fillWidth = true});
    }

    // Buttons of a page that owns the footer, in place of Back and Continue.
    Row* extra() const { return m_extra; }
    void setOwned(bool owned)
    {
        back->setVisible(!owned);
        next->setVisible(!owned);
        m_extra->setVisible(owned);
        requestLayout();
    }

    SizeF sizeHint() const override { return {400, style::footerHeight}; }

    ActionButton* back;
    ActionButton* next;

protected:
    void layout() override
    {
        const double width = geometry().width;
        const double height = geometry().height;
        const double buttonY = height - style::buttonHeight - 0;
        const double hintsHeight = m_hints->sizeHint().height;
        m_hints->setGeometry({0, buttonY + (style::buttonHeight - hintsHeight) / 2, 260, hintsHeight});
        double x = width;
        if (next->isVisible()) {
            const double w = next->sizeHint().width;
            x -= w;
            next->setGeometry({x, buttonY, w, style::buttonHeight});
            x -= 12;
        }
        if (back->isVisible()) {
            const double w = back->sizeHint().width;
            x -= w;
            back->setGeometry({x, buttonY, w, style::buttonHeight});
        }
        const SizeF extra = m_extra->sizeHint();
        m_extra->setGeometry({width - extra.width, buttonY, extra.width, style::buttonHeight});
    }

    void paint(Painter& painter) override
    {
        painter.drawHorizontalLine(0, 0, geometry().width, Theme::current().text.withAlpha(0.08));
    }

private:
    Label* m_hints;
    Row* m_extra;
};

class PageHost : public View
{
protected:
    void layout() override
    {
        for (const auto& child : children())
            child->setGeometry(localRect());
    }
};

} // namespace

// The window's content: the sidebar, and the page with its heading and footer.
class Root : public View
{
public:
    explicit Root(Installer& installer)
    {
        sidebar = addChild<Sidebar>(installer);
        heading = addChild<Heading>();
        pages = addChild<PageHost>();
        footer = addChild<Footer>();
        header = addChild<WindowHeader>(56);
        header->setButtonMargin(14);
        Row* tools = header->setContent<Row>();
        tools->addStretch();
        menu = tools->addChild<HeaderButton>(std::string(), 32);
        menu->setGlyph("menu", 18);
        menu->setLayoutHints({.margins = {0, 14, 4, 0}});
    }

    Sidebar* sidebar;
    Heading* heading;
    PageHost* pages;
    Footer* footer;
    WindowHeader* header;
    HeaderButton* menu;

protected:
    void layout() override
    {
        const double width = geometry().width;
        const double height = geometry().height;
        const double left = style::sidebarWidth;
        sidebar->setGeometry({0, 0, left, height});
        header->setGeometry({left, 0, width - left, 56});
        const double x = left + style::pageMarginLeft;
        const double contentWidth = std::max(0.0, width - x - style::pageMarginRight);
        const double headingHeight = heading->heightForWidth(contentWidth);
        heading->setGeometry({x, style::pageMarginTop, contentWidth, headingHeight});
        const double footerTop = height - 30 - style::footerHeight;
        footer->setGeometry({x, footerTop, contentWidth, style::footerHeight});
        const double pagesTop = style::pageMarginTop + headingHeight + 30;
        pages->setGeometry({x, pagesTop, contentWidth, std::max(0.0, footerTop - 20 - pagesTop)});
    }

    void paint(Painter& painter) override
    {
        painter.fillRect({style::sidebarWidth, 0, geometry().width - style::sidebarWidth, geometry().height},
                         resolve(Role::Content));
    }
};

// What the window shows while the backend starts, or why it could not.
class Splash : public Column
{
public:
    Splash()
    {
        setSpacing(12);
        addStretch();
        m_spinner = addChild<BusyIndicator>();
        m_spinner->setLayoutHints({.alignment = Alignment::Center});
        m_title = addText(this, tr("Preparing the installer"), 20, 600);
        m_title->setTextAlignment(Label::TextAlignment::Center);
        m_title->setLayoutHints({.fillWidth = true});
        m_status = addText(this, {}, 14, 400, Role::SecondaryText);
        m_status->setTextAlignment(Label::TextAlignment::Center);
        m_status->setWrap(true);
        m_status->setLayoutHints({.fillWidth = true, .margins = {80, 0, 80, 0}});
        m_quit = addChild<ActionButton>(tr("Quit"));
        m_quit->setLayoutHints({.alignment = Alignment::Center, .margins = {0, 12, 0, 0}});
        m_quit->setVisible(false);
        m_quit->onClicked = [] { Application::instance()->quit(1); };
        addStretch();
    }

    void setStatus(const std::string& status) { m_status->setText(status + "…"); }

    void setError(const std::string& error)
    {
        m_spinner->setVisible(false);
        m_title->setText(tr("The installer cannot start"));
        m_status->setText(error);
        m_quit->setVisible(true);
    }

protected:
    void paint(Painter& painter) override { painter.fillRect(localRect(), resolve(Role::Content)); }

private:
    BusyIndicator* m_spinner;
    Label* m_title;
    Label* m_status;
    ActionButton* m_quit;
};

Installer::Installer(std::unique_ptr<Backend> backend)
    : m_backend(std::move(backend))
    , m_window(std::make_unique<Window>())
{
    m_window->setTitle(tr("Install"));
    m_window->resize({style::windowWidth, style::windowHeight});
    m_window->setMinimumSize({960, 660});
    m_window->setBackgroundColor(ColorRecipe(ColorId::TranslucentWindowBackground));
}

void Installer::build()
{
    auto root = std::make_unique<Root>(*this);
    m_root = root.get();
    m_window->setContentView(std::move(root));
    m_window->onCloseRequested = [this] { requestQuit(); };
    m_window->addShortcut(NoModifier, key::F1, [this] { showHelp(); });
    m_window->addShortcut(NoModifier, kF12, [this] { openConsole(); });
    m_window->addShortcut(AltModifier, key::Left, [this] { back(); });
    m_root->footer->back->onClicked = [this] { back(); };
    m_root->footer->next->onClicked = [this] { next(); };
    m_root->menu->onClicked = [this] {
        auto* menu = new Menu;
        menu->addItem(tr("Help"), [this] { showHelp(); }, "F1");
        menu->addItem(tr("Installation log"), [this] { showLog(); });
        menu->addItem(tr("Open a terminal"), [this] { openConsole(); }, "F12");
        menu->addSeparator();
        menu->addItem(tr("Quit the installer"), [this] { requestQuit(); });
        menu->onClosed = [menu] { bayu::base::EventLoop::current()->post([menu] { delete menu; }); };
        menu->popupFor(m_root->menu, 4);
    };

    for (std::unique_ptr<Page>& page : createPages(*this)) {
        Page* raw = page.get();
        raw->setVisible(false);
        raw->onChanged = [this, raw] { pageChanged(raw); };
        m_pages.push_back(raw);
        m_root->pages->addChild(std::move(page));
    }
}

Installer::~Installer() = default;

Row* Installer::footerActions() const
{
    return m_root->footer->extra();
}

Page* Installer::page(size_t index) const
{
    return index < m_pages.size() ? m_pages[index] : nullptr;
}

bool Installer::isDone(size_t index) const
{
    return index < m_reached;
}

void Installer::start()
{
    auto splash = std::make_unique<Splash>();
    Splash* raw = splash.get();
    m_window->setContentView(std::move(splash));
    m_window->onCloseRequested = [] { Application::instance()->quit(1); };
    show();
    m_backend->start([raw](const std::string& status) { raw->setStatus(status); },
                     [this, raw](const std::string& error) {
                         if (!error.empty()) {
                             raw->setError(error);
                             return;
                         }
                         build();
                         show(0);
                     });
}

void Installer::show()
{
    const SizeF size = m_window->size();
    m_window->setBackgroundBlur({0, 0, size.width, size.height}, Theme::current().windowRadius);
    m_window->show();
}

void Installer::show(size_t index)
{
    if (index >= m_pages.size())
        return;
    m_current = index;
    for (size_t i = 0; i < m_pages.size(); ++i)
        m_pages[i]->setVisible(i == index);
    Page* page = m_pages[index];
    page->enter();
    updateChrome();
}

void Installer::updateChrome()
{
    Page* page = m_pages[m_current];
    char number[8];
    snprintf(number, sizeof(number), "%02zu", m_current + 1);
    std::string eyebrow = page->eyebrow();
    if (eyebrow.empty())
        eyebrow = format(tr("STEP {n} — {name}"), {{"n", number}, {"name", page->stepName()}});
    for (char& c : eyebrow)
        c = char(toupper(static_cast<unsigned char>(c)));
    m_root->heading->set(eyebrow, page->title(), page->description(), m_finished);
    Footer* footer = m_root->footer;
    footer->setOwned(page->ownsFooter());
    footer->back->setEnabled(m_current > 0 && page->canGoBack() && !m_busy);
    footer->back->setVisible(!page->ownsFooter());
    const std::string text = page->continueText();
    footer->next->setText(text.empty() ? tr("Continue") : text);
    footer->next->setTrailingGlyph(page->continueGlyph());
    footer->next->setBusy(m_busy);
    footer->next->setEnabled(page->canContinue() && !m_busy);
    m_root->sidebar->update();
    m_root->requestLayout();
}

void Installer::pageChanged(Page*)
{
    updateChrome();
}

void Installer::next()
{
    if (m_busy || m_current >= m_pages.size())
        return;
    Page* page = m_pages[m_current];
    if (!page->canContinue())
        return;
    m_busy = true;
    updateChrome();
    page->commit([this, page](bool ok) {
        m_busy = false;
        if (!ok || page != m_pages[m_current]) {
            updateChrome();
            return;
        }
        m_reached = std::max(m_reached, m_current + 1);
        size_t target = m_current + 1;
        if (m_editing) {
            m_editing = false;
            target = std::max(target, m_reached);
            // Back to Review, the step the user came from.
            for (size_t i = 0; i < m_pages.size(); ++i) {
                if (m_pages[i]->isReview())
                    target = i;
            }
        }
        show(std::min(target, m_pages.size() - 1));
    });
}

void Installer::back()
{
    if (m_busy || !m_pages[m_current]->canGoBack())
        return;
    if (m_pages[m_current]->handleBack()) {
        updateChrome();
        return;
    }
    if (m_current == 0)
        return;
    show(m_current - 1);
}

void Installer::goTo(size_t index)
{
    if (m_busy || index == m_current || !m_pages[m_current]->canGoBack())
        return;
    if (index < m_reached || index == m_reached)
        show(index);
}

void Installer::edit(size_t index)
{
    m_editing = true;
    show(index);
}

void Installer::setFinished(bool finished)
{
    m_finished = finished;
    updateChrome();
}

void Installer::showHelp()
{
    Page* page = m_pages[m_current];
    m_dialog = std::make_unique<DialogWindow>(m_window.get(), page->stepName(), 460);
    auto content = std::make_unique<Column>();
    content->setSpacing(10);
    Label* text = addText(content.get(), page->help(), 14);
    text->setWrap(true);
    text->setLayoutHints({.fillWidth = true});
    m_dialog->setContentView(std::move(content));
    m_dialog->show();
}

void Installer::openConsole()
{
    for (const char* terminal : {"bayu-terminal", "foot", "xterm"}) {
        if (bayu::base::Process::startDetached(terminal) > 0)
            return;
    }
}

void Installer::showLog()
{
    m_dialog = std::make_unique<DialogWindow>(m_window.get(), tr("Installation log"), 720);
    auto scroll = std::make_unique<ScrollView>();
    scroll->setLayoutHints({.fillWidth = true, .preferredHeight = 460});
    Label* text = scroll->setContent<Label>(m_backend->logText());
    text->setFontFamily("Red Hat Mono");
    text->setFontSize(12);
    text->setWrap(true);
    text->setWrapAnywhere(true);
    m_dialog->setContentView(std::move(scroll));
    m_dialog->show();
}

void Installer::requestQuit()
{
    if (m_finished) {
        m_backend->quit();
        Application::instance()->quit();
        return;
    }
    m_confirm = std::make_unique<ConfirmDialog>(m_window.get(), tr("Quit the installer?"),
                                                m_backend->canCancelInstall()
                                                    ? tr("Nothing has been written to your disks yet.")
                                                    : tr("The installation is running; stopping it now leaves the "
                                                         "disk unusable until you install again."));
    m_confirm->setConfirmText(tr("Quit"));
    m_confirm->setDestructive(true);
    m_confirm->onAccepted = [this] {
        m_backend->quit();
        Application::instance()->quit();
    };
    m_confirm->show();
}

int Installer::capture(const std::string& directory)
{
    build();
    m_window->resize({style::windowWidth, style::windowHeight});
    m_window->show();
    int failures = 0;
    for (size_t i = 0; i < m_pages.size(); ++i) {
        m_reached = i;
        show(i);
        char file[64];
        snprintf(file, sizeof(file), "/step-%02zu.png", i + 1);
        if (!m_window->grab().savePng(directory + file))
            ++failures;
    }
    return failures ? 1 : 0;
}

} // namespace installer
