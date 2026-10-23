// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "minimonth.h"
#include "calendarmodel.h"
#include "colors.h"
#include "dates.h"
#include "localedata.h"
#include "style.h"

#include <ui/label.h>
#include <ui/painter.h>

#include <algorithm>
#include <cmath>

using namespace bayu::ui;

MiniMonth::MiniMonth(CalendarModel* model)
    : m_model(model)
    , m_month(Dates::monthStart(Date::currentDate()))
{
    for (int i = 0; i < 7; ++i) {
        Label* label = addChild<Label>();
        label->setTextAlignment(Label::TextAlignment::Center);
        label->setVerticallyCentered(true);
        label->setElide(Label::Elide::None);
        m_weekdays.push_back(label);
    }
    for (int i = 0; i < 42; ++i) {
        Label* label = addChild<Label>();
        label->setTextAlignment(Label::TextAlignment::Center);
        label->setVerticallyCentered(true);
        label->setElide(Label::Elide::None);
        m_days.push_back(label);
    }
    update();
}

void MiniMonth::setMonth(Date month)
{
    if (month == m_month)
        return;
    m_month = month;
    update();
}

void MiniMonth::setSelectedDate(std::optional<Date> date)
{
    if (date == m_selected)
        return;
    m_selected = date;
    update();
}

void MiniMonth::setRange(std::optional<Date> start, std::optional<Date> end)
{
    if (start == m_rangeStart && end == m_rangeEnd)
        return;
    m_rangeStart = start;
    m_rangeEnd = end;
    update();
}

void MiniMonth::setShowOutsideDays(bool show)
{
    m_showOutsideDays = show;
    update();
}

void MiniMonth::setBusy(std::vector<bool> busy)
{
    if (busy == m_busy)
        return;
    m_busy = std::move(busy);
    requestPaint();
}

void MiniMonth::setCellHeight(double height)
{
    if (height == m_cellHeight)
        return;
    m_cellHeight = height;
    requestLayout();
}

void MiniMonth::setHeaderHeight(double height)
{
    if (height == m_headerHeight)
        return;
    m_headerHeight = height;
    requestLayout();
}

void MiniMonth::setFontSize(double size)
{
    if (size == m_fontSize)
        return;
    m_fontSize = size;
    update();
}

void MiniMonth::refresh()
{
    update();
}

SizeF MiniMonth::sizeHint() const
{
    const double header = m_headerHeight >= 0 ? m_headerHeight : m_cellHeight;
    return {7 * 28, header + 6 * m_cellHeight};
}

double MiniMonth::cellWidth() const
{
    return geometry().width / 7;
}

// Packed rows let the plate reach a little into the rows around it.
double MiniMonth::plateSize() const
{
    return std::min({cellWidth() - 2, m_cellHeight + 4, 22.0});
}

RectF MiniMonth::cellRect(int index) const
{
    const double header = m_headerHeight >= 0 ? m_headerHeight : m_cellHeight;
    return {(index % 7) * cellWidth(), header + (index / 7) * m_cellHeight, cellWidth(), m_cellHeight};
}

int MiniMonth::indexOf(std::optional<Date> date) const
{
    if (!date || !date->isValid())
        return -1;
    const int64_t index = m_gridStart.daysTo(*date);
    return index >= 0 && index < 42 ? int(index) : -1;
}

void MiniMonth::update()
{
    const int firstDay = m_model->firstDayOfWeek();
    m_gridStart = Dates::monthGridStart(m_month, firstDay);
    m_monthFirst = int(m_gridStart.daysTo(Dates::monthStart(m_month)));
    m_monthLast = m_monthFirst + m_month.daysInMonth() - 1;
    for (int i = 0; i < 7; ++i)
        m_weekdays[i]->setText(LocaleData::dayName(Dates::weekdayOfColumn(i, firstDay), LocaleData::Width::Narrow));
    for (int i = 0; i < 42; ++i)
        m_days[i]->setText(std::to_string(m_gridStart.addDays(i).day()));
    requestLayout();
    requestPaint();
}

// Styled here, so a theme change, which lays every window out again, restyles them.
void MiniMonth::layout()
{
    const Theme& theme = Theme::current();
    const double fontSize = Style::points(m_fontSize);
    const double header = m_headerHeight >= 0 ? m_headerHeight : m_cellHeight;
    for (int i = 0; i < 7; ++i) {
        Label* label = m_weekdays[i];
        label->setFontSize(fontSize);
        label->setColor(theme.disabledText);
        label->setGeometry({i * cellWidth(), 0, cellWidth(), header});
    }

    const int today = indexOf(m_model->today());
    const int selected = indexOf(m_selected);
    for (int i = 0; i < 42; ++i) {
        Label* label = m_days[i];
        const bool own = inMonth(i);
        const bool isToday = own && i == today;
        const bool isSelected = own && i == selected && !m_rangeStart;
        label->setFontSize(fontSize);
        label->setWeight(isToday || isSelected ? 600 : 400);
        label->setColor(isToday ? theme.highlightedText : isSelected ? theme.highlight
                        : own ? theme.text : theme.disabledText);
        label->setOpacity(own || isToday ? 1 : 0.6);
        label->setVisible(m_showOutsideDays || own);
        label->setGeometry(cellRect(i));
    }
}

void MiniMonth::paint(Painter& painter)
{
    const Theme& theme = Theme::current();
    const double plate = plateSize();

    // The shown range as one band per row, round at its ends.
    const int first = indexOf(m_rangeStart);
    const int last = indexOf(m_rangeEnd);
    if (first >= 0 && last > first) {
        for (int start = first; start <= last; start = (start / 7 + 1) * 7) {
            const int end = std::min(last, start / 7 * 7 + 6);
            const RectF cell = cellRect(start);
            const RectF band{cell.x + 2, cell.y + (m_cellHeight - plate) / 2, (end - start + 1) * cellWidth() - 4, plate};
            painter.fillRoundedRect(band, plate / 2, color(CalendarColor::MiniMonthRange));
        }
    }

    const auto platedCell = [&](int index, Color color) {
        const RectF cell = cellRect(index);
        painter.fillEllipse({cell.x + (cell.width - plate) / 2, cell.y + (m_cellHeight - plate) / 2, plate, plate},
                            color);
    };
    const int today = indexOf(m_model->today());
    const int selected = indexOf(m_selected);
    if (selected >= 0 && selected != today && inMonth(selected) && !m_rangeStart)
        platedCell(selected, color(CalendarColor::MiniMonthSelected));
    if (today >= 0 && inMonth(today))
        platedCell(today, theme.highlight);

    // Dots in cells too short for them would sit on the next row's numbers.
    if (m_cellHeight >= 20) {
        for (int i = m_monthFirst; i <= m_monthLast; ++i) {
            if (size_t(i) >= m_busy.size() || !m_busy[size_t(i)])
                continue;
            const RectF cell = cellRect(i);
            const double y = cell.y + m_cellHeight - 4 - std::max(1.0, (m_cellHeight - 22) / 2 - 1);
            painter.fillEllipse({cell.x + (cell.width - 4) / 2, y, 4, 4},
                                i == today ? theme.highlightedText : theme.disabledText);
        }
    }
}

std::optional<Date> MiniMonth::dateAt(PointF point) const
{
    const double header = m_headerHeight >= 0 ? m_headerHeight : m_cellHeight;
    if (point.y < header)
        return std::nullopt;
    const int row = std::min(6, int(std::floor((point.y - header) / m_cellHeight)));
    const int column = std::min(6, int(std::floor(point.x / cellWidth())));
    const int index = row * 7 + column;
    if (index < 0 || index >= 42 || (!m_showOutsideDays && !inMonth(index)))
        return std::nullopt;
    return m_gridStart.addDays(index);
}

void MiniMonth::pointerPress(const PointerEvent& event)
{
    m_doublePress = event.button == MouseButton::Left && event.clickCount == 2;
    if (!m_doublePress)
        return;
    if (const std::optional<Date> date = dateAt(event.position); date && onDayDoubleClicked)
        onDayDoubleClicked(*date);
}

void MiniMonth::pointerRelease(const PointerEvent& event)
{
    if (event.button != MouseButton::Left || m_doublePress || !localRect().contains(event.position))
        return;
    if (const std::optional<Date> date = dateAt(event.position); date && onDayClicked)
        onDayClicked(*date);
}
