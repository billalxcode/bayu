// Copyright 2026 Reion Wong <reionwong@gmail.com>
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BAYU_SHELL_MENUBAR_MENU_BAR_VIEW_H_
#define BAYU_SHELL_MENUBAR_MENU_BAR_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "base/signal.h"
#include "shell/menubar/menu_bar_model.h"
#include "shell/statusbar/status_bar_menu_bar.h"

namespace bayu::ui {
class Menu;
}

namespace bayu::shell {

class BarItem;
class Shell;

// The status bar's menu bar: the user's menu, then the
// active application's name and its menus, entries abutting each other from
// the bar's left edge. A click opens an entry's menu under its plate; while
// one is open, moving onto another entry or Left and Right switch to it.
//
// Goes into the bar's first slot: `bar->setMenuBar(std::make_unique<
// MenuBarView>(shell, bar->menuBarHost()))`. It owns the bar's model and
// with it the application menu backend.
class MenuBarView final : public StatusBarMenuBar
{
public:
    // The popup's distance below the bar (PanelStyle.popupMargin).
    static constexpr double kPopupMargin = 6;

    MenuBarView(Shell *shell, StatusBarMenuBarHost host);
    ~MenuBarView() override;

    // StatusBarMenuBar
    void closeMenu() override { close(); }
    void barChanged() override;

    // Opens the menu of the entry at `index` (a plain action entry is
    // triggered instead), closing the bar's panels and the tray's menu.
    void open(int index, bool keyboard = false);
    void close();
    // Left and Right: the neighbouring entry, wrapping around.
    void step(int step);

    // The entry whose menu is open, or -1.
    int currentIndex() const { return m_currentIndex; }
    ui::Menu *openMenu() const { return m_openMenu; }
    MenuBarModel &model() { return *m_model; }
    BarItem *entry(size_t index) const;

protected:
    void layout() override;

private:
    class Entry;

    void rebuildEntries();
    void onMenuClosed();

    StatusBarMenuBarHost m_host;
    std::unique_ptr<MenuBarModel> m_model;
    std::vector<Entry *> m_entries;
    // The first entry's picture.
    std::string m_logo;
    int m_currentIndex = -1;
    ui::Menu *m_openMenu = nullptr;
    // A switch closes one menu to open the next: that close is not the
    // user's.
    bool m_switching = false;
    std::vector<base::Subscription> m_subscriptions;
};

} // namespace bayu::shell

#endif // BAYU_SHELL_MENUBAR_MENU_BAR_VIEW_H_
