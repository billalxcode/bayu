// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "datetime.h"

#include <ui/view.h>

#include <functional>
#include <optional>
#include <vector>

namespace bayu::ui {
class Label;
}

class CalendarModel;

// Weekday initials over six weeks of days, as the sidebar, the date picker
// and the year view show a month.
class MiniMonth : public bayu::ui::View
{
public:
    explicit MiniMonth(CalendarModel* model);

    void setMonth(Date month);
    Date month() const { return m_month; }
    void setSelectedDate(std::optional<Date> date);
    // A run of days the main view shows, tinted behind the numbers.
    void setRange(std::optional<Date> start, std::optional<Date> end);
    // Days of the neighbouring months: shown dimmed, or left out.
    void setShowOutsideDays(bool show);
    // One entry per day of the grid; days with events get a dot.
    void setBusy(std::vector<bool> busy);
    void setCellHeight(double height);
    void setHeaderHeight(double height);
    // In points.
    void setFontSize(double size);
    // After today or the first day of the week changed.
    void refresh();

    Date gridStart() const { return m_gridStart; }

    bayu::ui::SizeF sizeHint() const override;
    bool acceptsPointer() const override { return true; }

    std::function<void(Date date)> onDayClicked;
    std::function<void(Date date)> onDayDoubleClicked;

protected:
    void layout() override;
    void paint(bayu::ui::Painter& painter) override;
    void pointerPress(const bayu::ui::PointerEvent& event) override;
    void pointerRelease(const bayu::ui::PointerEvent& event) override;

private:
    int indexOf(std::optional<Date> date) const;
    bool inMonth(int index) const { return index >= m_monthFirst && index <= m_monthLast; }
    double cellWidth() const;
    double plateSize() const;
    bayu::ui::RectF cellRect(int index) const;
    std::optional<Date> dateAt(bayu::ui::PointF point) const;
    void update();

    CalendarModel* m_model;
    Date m_month;
    std::optional<Date> m_selected;
    std::optional<Date> m_rangeStart;
    std::optional<Date> m_rangeEnd;
    bool m_showOutsideDays = true;
    std::vector<bool> m_busy;
    double m_cellHeight = 26;
    double m_headerHeight = -1;
    double m_fontSize = 8;

    Date m_gridStart;
    int m_monthFirst = 0;
    int m_monthLast = 0;
    std::vector<bayu::ui::Label*> m_weekdays;
    std::vector<bayu::ui::Label*> m_days;
    // A second press inside the double-click time is not a click.
    bool m_doublePress = false;
};
