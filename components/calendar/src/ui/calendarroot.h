// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <base/timer.h>
#include <ui/view.h>

#include <memory>
#include <string>
#include <vector>

namespace bayu::ui {
class WindowHeader;
}

class CalendarWindow;
class HeaderBar;
class MonthView;
class NavigationRow;
class SearchResults;
class Sidebar;
class TimeGridView;
class YearView;

// The calendar's side: the period's title and buttons, the views and the
// search results; it takes the keys that move through the days.
class CalendarContent : public bayu::ui::View
{
public:
    explicit CalendarContent(CalendarWindow* window);
    ~CalendarContent() override;

    void showView(const std::string& name);
    void setSearchText(const std::string& text);
    bool isSearching() const { return m_searching; }
    SearchResults* searchResults() const { return m_search; }
    bayu::ui::View* currentView() const;

    bool acceptsFocus() const override { return true; }

protected:
    void layout() override;
    bool keyPress(const bayu::ui::KeyEvent& event) override;

private:
    void ensureViews();

    CalendarWindow* m_window;
    NavigationRow* m_navigation;
    bayu::ui::View* m_viewArea;
    SearchResults* m_search;
    TimeGridView* m_day = nullptr;
    TimeGridView* m_week = nullptr;
    MonthView* m_month = nullptr;
    YearView* m_year = nullptr;
    std::string m_current;
    bool m_searching = false;
    // The views not shown yet are built once the window is idle.
    bayu::base::Timer m_preload;
};

// Everything in the main window: the calendar's background, the sidebar
// docked or over the calendar, and the title bar.
class CalendarRoot : public bayu::ui::View
{
public:
    explicit CalendarRoot(CalendarWindow* window);
    ~CalendarRoot() override;

    CalendarContent* content() const { return m_content; }
    HeaderBar* headerBar() const { return m_headerBar; }
    Sidebar* sidebar() const { return m_sidebar; }

    bool canDrop(const bayu::ui::DropOffer& offer) const override;
    void dragEnter(const bayu::ui::DragEvent& event) override;
    uint32_t dragUpdate(const bayu::ui::DragEvent& event) override;
    void dragLeave() override;
    uint32_t drop(const bayu::ui::DragEvent& event) override;

protected:
    void layout() override;
    void paint(bayu::ui::Painter& painter) override;

private:
    class OverlayBackground;

    CalendarWindow* m_window;
    CalendarContent* m_content;
    OverlayBackground* m_overlay;
    Sidebar* m_sidebar;
    bayu::ui::WindowHeader* m_header;
    HeaderBar* m_headerBar;
    double m_buttonMargin = -1;
    // The calendar files of the drag over the window, once read.
    std::vector<std::string> m_dropFiles;
    std::shared_ptr<bool> m_dragAlive;
};
