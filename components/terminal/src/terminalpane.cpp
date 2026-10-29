// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "terminalpane.h"

#include <algorithm>
#include <cmath>

#include "base/i18n.h"
#include "base/strings.h"
#include "base/url.h"
#include "ui/clipboard.h"
#include "ui/drag.h"
#include "ui/keys.h"
#include "ui/menu.h"
#include "ui/scroll_bar.h"
#include "ui/theme.h"
#include "ui/window.h"

#include "TerminalSession.h"
#include "TerminalView.h"
#include "Vt102Emulation.h"

#include "findbar.h"
#include "processhelper.h"
#include "terminalpage.h"
#include "terminalsettings.h"
#include "terminalwindow.h"

using namespace bayu::ui;
using bayu::i18n::format;
using bayu::i18n::tr;
namespace strings = bayu::base::strings;

// The terminal's margins inside the pane; the top is the header's line.
static constexpr double kTerminalMargin = 6;

TerminalPane::TerminalPane(TerminalPage* page, const std::string& path, const std::vector<std::string>& command)
    : m_page(page)
    , m_session(std::make_unique<Konsole::TerminalSession>())
    , m_terminal(addChild<Konsole::TerminalView>())
    , m_scrollBar(addChild<ScrollBar>(ScrollBar::Orientation::Vertical))
{
    setName("terminalPane");
    m_session->setInitialWorkingDirectory(path);
    if (!command.empty()) {
        m_session->setShellProgram(command.front());
        m_session->setShellProgramArguments(std::vector<std::string>(command.begin() + 1, command.end()));
    }

    m_terminal->setFullCursorHeight(true);
    m_terminal->setBackgroundOpacity(0);
    m_terminal->setSession(m_session.get());

    m_terminal->onShortcut = [this](const bayu::ui::KeyEvent& event) { return handleShortcut(event); };
    m_terminal->onContextMenu = [this](double x, double y) { openMenu(x, y); };
    m_terminal->onLinkActivated = [this](const std::string& url) { terminalWindow()->openLink(url); };

    m_connections.push_back(m_session->finished.connect([this] { terminalClosed(); }));
    m_connections.push_back(m_session->titleChanged.connect([this] { titleChanged(); }));
    m_connections.push_back(m_terminal->notifyBell.connect([this] { terminalWindow()->flashBell(); }));
    m_connections.push_back(m_terminal->focused.connect([this] { focused(); }));
    m_connections.push_back(m_terminal->multilinePasteRequested.connect([this](const std::string& text) {
        // The shell can exit and close the pane while the dialog is open.
        terminalWindow()->confirmPaste(text, [this, alive = std::weak_ptr<bool>(m_alive)] {
            if (!alive.expired())
                m_terminal->confirmPaste();
        });
    }));
    m_connections.push_back(m_terminal->scrollbarParamsChanged.connect([this] { updateScrollBar(); }));
    m_connections.push_back(m_terminal->sizeChanged.connect([this] { updateScrollBar(); }));
    m_connections.push_back(m_terminal->scrollbarValueChanged.connect([this] {
        m_scrollActivity.start();
        m_scrollBar->setMoving(true);
    }));

    // Shown while scrolling or under the pointer.
    m_scrollActivity.setSingleShot(true);
    m_scrollActivity.setInterval(std::chrono::milliseconds(800));
    m_scrollActivity.onTimeout = [this] { m_scrollBar->setMoving(false); };
    m_scrollBar->onPositionRequested = [this](double position) {
        m_terminal->setScrollbarValue(int(std::lround(position)) + m_terminal->scrollbarMinimum());
    };

    applySettings();
}

TerminalPane::~TerminalPane()
{
    m_connections.clear();
    // The view goes before the session it shows.
    takeChild(m_terminal).reset();
}

TerminalWindow* TerminalPane::terminalWindow() const
{
    return m_page->terminalWindow();
}

void TerminalPane::applySettings()
{
    TerminalWindow* window = terminalWindow();
    const TerminalSettings& settings = window->settings();
    m_terminal->setColorScheme(window->colorScheme());
    m_terminal->setTerminalFont(settings.fontName(), window->fontPointSize());
    m_terminal->setBlinkingCursor(settings.blinkingCursor());
    m_terminal->setTerminalCursorShape(settings.cursorShape());
    // NotifyBell hands the bell to the window; NoBell ignores it.
    m_terminal->setBellMode(settings.visualBell() ? Konsole::TerminalView::NotifyBell : Konsole::TerminalView::NoBell);
    m_terminal->setConfirmMultilinePaste(settings.confirmMultilinePaste());
    m_terminal->setLinkColor(window->chromeForeground());
    m_scrollBar->setDarkMode(window->chromeDark());
    m_session->setHistorySize(settings.scrollbackLines());
    if (m_findBar)
        m_findBar->setColors(window->terminalBackground(), window->chromeForeground(), window->chromeDark(),
                             window->schemeColor(1, accent::red), window->schemeColor(4, Theme::current().highlight));
}

void TerminalPane::retranslate()
{
    if (m_menu)
        m_menu->close();
    m_menu.reset();
    if (m_findBar)
        m_findBar->retranslate();
}

void TerminalPane::startShell()
{
    m_session->startShellProgram();
    m_terminal->setFocus();
}

void TerminalPane::focusTerminal()
{
    m_terminal->setFocus();
}

void TerminalPane::layout()
{
    const RectF bounds = localRect();
    m_terminal->setGeometry({kTerminalMargin, 0, std::max(0.0, bounds.width - 2 * kTerminalMargin),
                             std::max(0.0, bounds.height - kTerminalMargin)});
    // Overlays the right margin, the terminal's height.
    m_scrollBar->setGeometry({bounds.width - ScrollBar::thickness, 0, ScrollBar::thickness,
                              std::max(0.0, bounds.height - kTerminalMargin)});
    if (m_findBar) {
        const Theme& theme = Theme::current();
        const SizeF size = m_findBar->sizeHint();
        m_findBar->setGeometry({bounds.width - theme.largeSpacing - size.width, theme.smallSpacing, size.width,
                                size.height});
    }
    updateScrollBar();
}

void TerminalPane::updateScrollBar()
{
    const int lines = m_terminal->lines();
    const int totalLines = lines + m_terminal->scrollbarMaximum() - m_terminal->scrollbarMinimum();
    m_scrollBar->setRange(lines, totalLines, m_terminal->scrollbarValue() - m_terminal->scrollbarMinimum());
}

// Dropped files and links arrive as one shell word each.
static std::string shellQuote(const std::string& text)
{
    const bool plain = !text.empty() && std::all_of(text.begin(), text.end(), [](char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
                || std::string_view("_@%+=:,./-").find(c) != std::string_view::npos;
    });
    if (plain)
        return text;
    return "'" + strings::replaceAll(text, "'", "'\\''") + "'";
}

void TerminalPane::dropUrls(const std::vector<std::string>& urls)
{
    std::vector<std::string> words;
    for (const std::string& url : urls) {
        // A local file as its path, "file://localhost/" too; other links as they are.
        words.push_back(shellQuote(bayu::base::url::toLocalFile(url).value_or(url)));
    }
    // As a paste: bracketed for a shell that asks, so control characters in
    // a name are inserted rather than run as editing keys.
    m_terminal->pasteText(strings::join(words, " ") + " ");
}

static bool hasUrls(const DropOffer& offer)
{
    return offer.hasMimeType("text/uri-list");
}

static std::string textType(const DropOffer& offer)
{
    for (const char* type : {"text/plain;charset=utf-8", "text/plain"}) {
        if (offer.hasMimeType(type))
            return type;
    }
    return std::string();
}

// A tab dragged over the terminal is not something to paste.
bool TerminalPane::canDrop(const DropOffer& offer) const
{
    return hasUrls(offer) || !textType(offer).empty();
}

uint32_t TerminalPane::dragUpdate(const DragEvent&)
{
    return CopyAction;
}

uint32_t TerminalPane::drop(const DragEvent& event)
{
    std::weak_ptr<bool> alive = m_alive;
    if (hasUrls(*event.offer)) {
        event.offer->read("text/uri-list", [this, alive](std::string data) {
            if (!alive.lock())
                return;
            // RFC 2483: one URI a line, CRLF ends, # starts a comment.
            std::vector<std::string> urls;
            for (std::string_view line : strings::split(data, '\n')) {
                line = strings::trimmed(line);
                if (!line.empty() && line.front() != '#')
                    urls.emplace_back(line);
            }
            if (!urls.empty())
                dropUrls(urls);
        });
        return CopyAction;
    }

    event.offer->read(textType(*event.offer), [this, alive](std::string data) {
        if (alive.lock())
            m_terminal->pasteText(data);
    });
    return CopyAction;
}

// Starts from the selection when it is a single line, as a search term would be.
void TerminalPane::openFind()
{
    const std::string selection = m_terminal->hasSelection()
            ? std::string(strings::trimmed(m_terminal->selectedText()))
            : std::string();
    if (!m_findBar) {
        m_findBar = addChild<FindBar>(m_terminal);
        m_findBar->onClosed = [this] { m_terminal->setFocus(); };
        applySettings();
        requestLayout();
    }
    m_findBar->open(selection.find('\n') == std::string::npos ? selection : std::string());
}

bool TerminalPane::handleShortcut(const bayu::ui::KeyEvent& event)
{
    const uint32_t mods = event.modifiers;
    const bool ctrl = mods & bayu::ui::ControlModifier;
    const bool shift = mods & bayu::ui::ShiftModifier;
    const bool alt = mods & bayu::ui::AltModifier;
    const uint32_t key = event.key;
    bool accepted = false;

    if (key == key::letter('a') && ctrl && shift) {
        m_terminal->selectAll();
        accepted = true;
    }
    if (key == key::letter('c') && ctrl && shift) {
        m_terminal->copyClipboard();
        accepted = true;
    }
    if (key == key::letter('v') && ctrl && shift) {
        m_terminal->pasteClipboard();
        accepted = true;
    }
    if (key == key::letter('q') && ctrl && shift) {
        terminalWindow()->close();
        return true;
    }
    if (key == key::letter('n') && ctrl && shift) {
        terminalWindow()->newWindow();
        accepted = true;
    }
    if (key == key::letter('t') && ctrl && shift) {
        terminalWindow()->openNewTab();
        accepted = true;
    }
    if (key == key::letter('w') && ctrl && shift) {
        terminalWindow()->closeCurrentPane();
        return true;
    }

    // Splits: Ctrl+Shift+E to the right, Ctrl+Shift+O below; Alt and an
    // arrow moves between the panes, and goes to the shell with only one.
    if (key == key::letter('e') && ctrl && shift) {
        m_page->split(this, TerminalPage::Direction::Right);
        accepted = true;
    }
    if (key == key::letter('o') && ctrl && shift) {
        m_page->split(this, TerminalPage::Direction::Down);
        accepted = true;
    }
    if (alt && !ctrl && !shift && m_page->paneCount() > 1) {
        if (key == key::Left)
            accepted = m_page->focusNeighbour(this, TerminalPage::Direction::Left);
        else if (key == key::Right)
            accepted = m_page->focusNeighbour(this, TerminalPage::Direction::Right);
        else if (key == key::Up)
            accepted = m_page->focusNeighbour(this, TerminalPage::Direction::Up);
        else if (key == key::Down)
            accepted = m_page->focusNeighbour(this, TerminalPage::Direction::Down);
    }

    // Tabs: Ctrl+Tab and Ctrl+PgDn go right, Ctrl+Shift+Tab and Ctrl+PgUp left;
    // with Shift, Ctrl+PgUp and Ctrl+PgDn move the tab instead.
    if (ctrl) {
        if (key == key::Tab && !shift) {
            terminalWindow()->cycleTab(1);
            accepted = true;
        } else if (key == key::Tab && shift) {
            terminalWindow()->cycleTab(-1);
            accepted = true;
        } else if (key == key::PageDown) {
            shift ? terminalWindow()->moveCurrentTab(1) : terminalWindow()->cycleTab(1);
            accepted = true;
        } else if (key == key::PageUp) {
            shift ? terminalWindow()->moveCurrentTab(-1) : terminalWindow()->cycleTab(-1);
            accepted = true;
        }
    }

    // Alt+1 to Alt+8 pick a tab, Alt+9 the last one.
    if (alt && !ctrl && key >= '1' && key <= '9' && terminalWindow()->tabCount() > 1) {
        terminalWindow()->selectTab(key == '9' ? terminalWindow()->tabCount() - 1 : int(key - '1'));
        accepted = true;
    }

    if (key == key::letter('f') && ctrl && shift) {
        openFind();
        accepted = true;
    }

    if (key == key::letter('k') && ctrl && shift)
        accepted = m_session->clearScreenAndScrollback();

    // Zoom: Ctrl+= (or Ctrl++), Ctrl+-, Ctrl+0.
    if (ctrl && !alt) {
        if (key == '=' || key == '+') {
            terminalWindow()->zoom(1);
            accepted = true;
        } else if (key == '-' && !shift) {
            terminalWindow()->zoom(-1);
            accepted = true;
        } else if (key == '0' && !shift) {
            terminalWindow()->zoom(0);
            accepted = true;
        }
    }

    if (key == key::F1 + 10) {
        terminalWindow()->toggleFullScreen();
        accepted = true;
    }

    return accepted;
}

static std::string elided(const std::string& text, size_t length)
{
    const std::u16string utf16 = strings::utf8ToUtf16(text);
    if (utf16.size() <= length)
        return text;
    return strings::utf16ToUtf8(utf16.substr(0, length - 1)) + "\xe2\x80\xa6";
}

// /^[a-z][a-z0-9+.-]*:\/\/\S+$/i
static bool isLink(const std::string& text)
{
    const size_t scheme = text.find("://");
    if (scheme == std::string::npos || scheme == 0 || scheme + 3 >= text.size())
        return false;
    if (!std::isalpha(static_cast<unsigned char>(text[0])))
        return false;
    for (size_t i = 1; i < scheme; ++i) {
        const char c = text[i];
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '+' && c != '.' && c != '-')
            return false;
    }
    for (size_t i = scheme + 3; i < text.size(); ++i) {
        if (std::isspace(static_cast<unsigned char>(text[i])))
            return false;
    }
    return true;
}

void TerminalPane::openMenu(double x, double y)
{
    // A right click on a word outside the selection selects that word first,
    // so Copy and the file and link items act on what was clicked.
    if (!m_terminal->isSelectedAt(x, y))
        m_terminal->selectWordAt(x, y);

    const std::string selection = m_terminal->hasSelection()
            ? std::string(strings::trimmed(m_terminal->selectedText()))
            : std::string();
    const bool selectionIsLink = isLink(selection);

    std::string link = m_terminal->linkAt(x, y);
    if (link.empty() && selectionIsLink)
        link = selection;
    m_menuLink = link;
    m_menuPath = link.empty() ? ProcessHelper::existingPath(selection, m_session->currentDir()) : std::string();

    const PointF position = m_terminal->mapToWindow({x, y});
    std::weak_ptr<bool> alive = m_alive;
    Clipboard::requestText([this, alive, position](std::string text) {
        if (!alive.lock())
            return;
        m_menuCanPaste = !text.empty();

        // The menu is a window of its own, made on the first right click.
        if (!m_menu)
            m_menu = std::make_unique<Menu>();
        m_menu->clear();

        if (!m_menuLink.empty()) {
            m_menu->addItem(tr("Open Link"), [this] { terminalWindow()->openLink(m_menuLink); });
            m_menu->addItem(tr("Copy Link"), [this] { Clipboard::setText(m_menuLink); });
            m_menu->addSeparator();
        }
        if (!m_menuPath.empty()) {
            const std::string name = m_menuPath.substr(m_menuPath.rfind('/') + 1);
            m_menu->addItem(format(tr("Open “{name}”"), {{"name", elided(name.empty() ? m_menuPath : name, 30)}}),
                            [this] { ProcessHelper::openUrl(window(), m_menuPath); });
            m_menu->addItem(tr("Show in File Manager"), [this] { ProcessHelper::showInFileManager(m_menuPath); });
            m_menu->addSeparator();
        }
        if (m_terminal->hasSelection())
            m_menu->addItem(tr("Copy"), [this] { m_terminal->copyClipboard(); });
        if (m_menuCanPaste)
            m_menu->addItem(tr("Paste"), [this] { m_terminal->pasteClipboard(); });
        m_menu->addItem(tr("Select All"), [this] { m_terminal->selectAll(); });
        addWindowItems(*m_menu);

        if (Window* window = this->window())
            m_menu->popup(window, position);
    });
}

void TerminalPane::addWindowItems(Menu& menu)
{
    menu.addItem(tr("Find…"), [this] { openFind(); });
    if (m_terminal->scrollbarMaximum() > 0)
        menu.addItem(tr("Clear Scrollback"), [this] { m_session->clearScrollback(); });
    menu.addItem(tr("Reset Terminal"), [this] { m_session->resetTerminal(); });
    menu.addSeparator();
    menu.addItem(tr("Split Right"), [this] { m_page->split(this, TerminalPage::Direction::Right); });
    menu.addItem(tr("Split Down"), [this] { m_page->split(this, TerminalPage::Direction::Down); });
    if (m_page->paneCount() > 1)
        menu.addItem(tr("Close Pane"), [this] { terminalWindow()->closePane(m_page, this); });
    menu.addSeparator();
    menu.addItem(tr("New Tab"), [this] { terminalWindow()->openNewTab(); });
    menu.addItem(tr("New Window"), [this] { terminalWindow()->newWindow(); });
    menu.addItem(tr("Open File Manager"), [this] { ProcessHelper::openFileManager(m_session->currentDir()); });
    menu.addSeparator();
    menu.addItem(terminalWindow()->isFullScreen() ? tr("Exit full screen") : tr("Full screen"),
                 [this] { terminalWindow()->toggleFullScreen(); });
    menu.addItem(tr("Settings"), [this] { terminalWindow()->showSettings(); });
}
