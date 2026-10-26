// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "base/signal.h"
#include "base/timer.h"
#include "ui/view.h"

namespace Konsole {
class TerminalSession;
class TerminalView;
}

namespace bayu::ui {
class Menu;
class ScrollBar;
}

class FindBar;
class TerminalPage;
class TerminalWindow;

// One shell of a tab: a shell on a pty, its terminal view with the scroll bar
// over its right margin, the find bar and the context menu. A tab split shows
// several side by side.
class TerminalPane : public bayu::ui::View
{
public:
    // `path` is where the shell starts ("$PWD", "$HOME" or a directory),
    // `command` a program and its arguments to run instead.
    TerminalPane(TerminalPage* page, const std::string& path, const std::vector<std::string>& command);
    ~TerminalPane() override;

    Konsole::TerminalSession& session() const { return *m_session; }
    Konsole::TerminalView* terminal() const { return m_terminal; }

    // The settings and the window's zoom and colours, applied again.
    void applySettings();
    // The menu closes and the find bar takes the new language, keeping its
    // search; the shell and its output stay as they are.
    void retranslate();

    void startShell();
    void openFind();
    void focusTerminal();
    // A drop: paths and links as one shell word each, text pasted.
    void dropUrls(const std::vector<std::string>& urls);

    bayu::base::Signal<> titleChanged;
    // The shell ended.
    bayu::base::Signal<> terminalClosed;
    // Took the keyboard focus.
    bayu::base::Signal<> focused;

protected:
    void layout() override;
    bool canDrop(const bayu::ui::DropOffer& offer) const override;
    uint32_t dragUpdate(const bayu::ui::DragEvent& event) override;
    uint32_t drop(const bayu::ui::DragEvent& event) override;

private:
    TerminalWindow* terminalWindow() const;
    bool handleShortcut(const bayu::ui::KeyEvent& event);
    void openMenu(double x, double y);
    void updateScrollBar();

    TerminalPage* m_page;
    std::unique_ptr<Konsole::TerminalSession> m_session;
    Konsole::TerminalView* m_terminal;
    bayu::ui::ScrollBar* m_scrollBar;
    FindBar* m_findBar = nullptr;
    std::unique_ptr<bayu::ui::Menu> m_menu;
    bayu::base::Timer m_scrollActivity;
    std::vector<bayu::base::Subscription> m_connections;

    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);

    // What the menu was opened over.
    std::string m_menuLink;
    std::string m_menuPath;
    bool m_menuCanPaste = false;
};
