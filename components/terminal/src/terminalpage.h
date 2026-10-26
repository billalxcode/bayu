// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/signal.h"
#include "ui/view.h"

namespace Konsole {
class TerminalSession;
class TerminalView;
}

class TerminalPane;
class TerminalWindow;

// One tab: a shell, or several split side by side and above one another. One
// of them is current, the one that last had the keyboard; the tab's title and
// what the window asks of the tab are its.
class TerminalPage : public bayu::ui::View
{
public:
    enum class Direction { Left, Right, Up, Down };

    // `path` is where the shell starts ("$PWD", "$HOME" or a directory),
    // `command` a program and its arguments to run instead.
    TerminalPage(TerminalWindow* window, const std::string& path, const std::vector<std::string>& command);
    ~TerminalPage() override;

    // The current pane's.
    Konsole::TerminalSession& session() const;
    Konsole::TerminalView* terminal() const;

    // A program runs in one of the panes.
    bool hasActiveProcess() const;
    int paneCount() const { return int(m_panes.size()); }
    TerminalPane* currentPane() const { return m_current; }

    // The tab is in this window now, moved there with its shells.
    void setTerminalWindow(TerminalWindow* window);
    TerminalWindow* terminalWindow() const { return m_window; }

    // Set by renaming the tab; empty follows the current pane's session.
    void setCustomTitle(const std::string& title);
    std::string title() const;

    // The settings and the window's zoom and colours, applied again.
    void applySettings();
    // The menus close and the find bars take the new language.
    void retranslate();

    void startShell();
    void openFind();
    void focusTerminal();
    // A drop: paths and links as one shell word each, text pasted.
    void dropUrls(const std::vector<std::string>& urls);

    // A new shell next to `pane`, in its directory, half of its space.
    void split(TerminalPane* pane, Direction direction);
    // The pane goes, its neighbour takes its space; the last one closes the tab.
    void closePane(TerminalPane* pane);
    // The keyboard to the pane on that side of `pane`; false without one.
    bool focusNeighbour(TerminalPane* pane, Direction direction);

    bayu::base::Signal<> titleChanged;
    // The last shell ended.
    bayu::base::Signal<> terminalClosed;

protected:
    void layout() override;

private:
    struct Split;
    class Divider;

    TerminalPane* addPane(const std::string& path, const std::vector<std::string>& command);
    void attach(TerminalPane* pane);
    void setCurrent(TerminalPane* pane);
    Split* leafOf(const TerminalPane* pane) const;
    void layoutSplit(Split& split, bayu::ui::RectF rect);

    TerminalWindow* m_window;
    std::string m_customTitle;
    std::unique_ptr<Split> m_root;
    std::vector<TerminalPane*> m_panes;
    TerminalPane* m_current = nullptr;
    // Dropped when the pane closes.
    std::map<const TerminalPane*, std::vector<bayu::base::Subscription>> m_paneConnections;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};
