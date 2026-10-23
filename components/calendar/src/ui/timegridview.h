// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "calendarmodel.h"

#include <base/timer.h>
#include <ui/view.h>

#include <optional>
#include <string>
#include <vector>

namespace bayu::ui {
class Label;
class ScrollView;
}

class CalendarWindow;
class EventBlock;
class EventLine;

// Days side by side over the hours, the week view with seven and the day
// view with one: all-day events in a strip on top, timed ones as blocks.
class TimeGridView : public bayu::ui::View
{
public:
    static constexpr double Gutter = 56;
    static constexpr double HourHeight = 48;
    static constexpr double LineHeight = 20;
    static constexpr int MaximumLanes = 3;
    // Snapped to this many minutes while creating, moving and resizing.
    static constexpr int Step = 15;

    TimeGridView(CalendarWindow* window, int days);
    ~TimeGridView() override;

    // Shown: laid out again only while shown and when something it shows changed.
    void setLive(bool live);
    void refresh(bool force = false);
    void scrollToWorkingHours();
    // The hour grid's scroll position.
    double scrollTop() const;
    void setScrollTop(double top);

    double headerHeight() const { return m_days == 1 ? 0 : 44; }
    double columnWidth() const;
    int columnAt(double x) const;
    int snap(double minutes) const;
    Date rangeStart() const;

    // The event being moved or resized, and by how much.
    void dragMove(const EventItem& item, int days, int minutes);
    void dragResize(const EventItem& item, int minutes);
    void finishDrag(const EventItem& item);

protected:
    void layout() override;
    void paint(bayu::ui::Painter& painter) override;

private:
    class Canvas;
    class Header;
    class Strip;

    int stripLanes() const;
    double stripHeight() const;
    void rebuild();

    CalendarWindow* m_window;
    int m_days;
    bool m_live = false;
    std::string m_laidOutKey;
    DayLayout m_layout;

    std::vector<Header*> m_headers;
    bayu::ui::Label* m_allDayLabel;
    Strip* m_strip;
    bayu::ui::ScrollView* m_scroll;
    Canvas* m_canvas;
    std::vector<EventLine*> m_bars;
    std::vector<bayu::ui::Label*> m_more;
    std::vector<EventBlock*> m_blocks;

    std::string m_dragKey;
    std::string m_dragRecurrenceKey;
    int m_dragDays = 0;
    int m_dragMinutes = 0;
    int m_resizeMinutes = 0;
    std::optional<double> m_pendingScroll;

    bayu::base::Timer m_nowTimer;
    std::vector<bayu::base::Subscription> m_subscriptions;
};
