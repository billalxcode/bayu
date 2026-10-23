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
}

class CalendarWindow;
class EventLine;
class MoreLabel;

// Six weeks around the current month: events that last a day or more as bars
// across their days, timed ones as lines under them.
class MonthView : public bayu::ui::View
{
public:
    static constexpr double HeaderHeight = 30;
    static constexpr double DayLabelHeight = 28;
    static constexpr double LineHeight = 20;

    explicit MonthView(CalendarWindow* window);
    ~MonthView() override;

    void setLive(bool live);
    void refresh(bool force = false);

    Date gridStart() const;
    // The day under a point of the view, or none outside the grid.
    std::optional<Date> dateAt(bayu::ui::PointF point) const;

    bool acceptsPointer() const override { return true; }

protected:
    void layout() override;
    void paint(bayu::ui::Painter& painter) override;
    void pointerPress(const bayu::ui::PointerEvent& event) override;
    void pointerRelease(const bayu::ui::PointerEvent& event) override;
    bool scroll(const bayu::ui::ScrollEvent& event) override;

private:
    double gutter() const;
    double cellWidth() const;
    double rowHeight() const;
    int lanes() const;
    int indexOf(Date date) const;
    std::string dayText(int index) const;
    double plateWidth(double textWidth) const { return std::max(22.0, textWidth + 10); }
    void rebuild();

    CalendarWindow* m_window;
    bool m_live = false;
    std::string m_laidOutKey;
    MonthLayout m_layout;
    std::vector<bayu::ui::Label*> m_weekdays;
    std::vector<bayu::ui::Label*> m_days;
    std::vector<bayu::ui::Label*> m_weekNumbers;
    std::vector<EventLine*> m_bars;
    std::vector<MoreLabel*> m_more;
    // Where a dragged event would land.
    std::optional<Date> m_dropTarget;
    bool m_doublePress = false;

    // A wheel step or a touchpad swipe turns the month.
    double m_travel = 0;
    bool m_spent = false;
    bayu::base::Timer m_settle;

    std::vector<bayu::base::Subscription> m_subscriptions;
};
