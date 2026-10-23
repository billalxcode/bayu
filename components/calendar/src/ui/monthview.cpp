// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "monthview.h"
#include "calendarwindow.h"
#include "colors.h"
#include "dates.h"
#include "eventviews.h"
#include "localedata.h"
#include "style.h"

#include <ui/label.h>
#include <ui/painter.h>
#include <ui/window.h>

#include <algorithm>
#include <cmath>

using namespace bayu::ui;

namespace {

// Where a dragged event would land.
class DropTarget : public View
{
protected:
    void paint(Painter& painter) override
    {
        const Color highlight = Theme::current().highlight;
        painter.fillRect(localRect(), Style::withAlpha(highlight, 0.12));
        painter.strokeRoundedRect(localRect(), 0, Style::withAlpha(highlight, 0.5), painter.borderWidth(1));
    }
};

}

MonthView::MonthView(CalendarWindow* window)
    : m_window(window)
{
    setName("monthView");
    for (int i = 0; i < 7; ++i) {
        Label* label = addChild<Label>();
        label->setFontSize(Style::points(9));
        label->setTextAlignment(Label::TextAlignment::Right);
        label->setVerticallyCentered(true);
        m_weekdays.push_back(label);
    }
    for (int i = 0; i < 42; ++i) {
        Label* label = addChild<Label>();
        label->setFontSize(Style::points(9));
        label->setElide(Label::Elide::None);
        m_days.push_back(label);
    }
    for (int i = 0; i < 6; ++i) {
        Label* label = addChild<Label>();
        label->setFontSize(Style::points(8));
        label->setTextAlignment(Label::TextAlignment::Center);
        label->setVerticallyCentered(true);
        label->setElide(Label::Elide::None);
        m_weekNumbers.push_back(label);
    }
    addChild<DropTarget>()->setVisible(false);

    m_settle.setSingleShot(true);
    m_settle.setInterval(std::chrono::milliseconds(200));
    m_settle.onTimeout = [this] {
        m_travel = 0;
        m_spent = false;
    };

    CalendarModel* model = window->model();
    m_subscriptions.push_back(model->revisionChanged.connect([this] { refresh(); }));
    m_subscriptions.push_back(model->todayChanged.connect([this] { requestLayout(); requestPaint(); }));
    m_subscriptions.push_back(model->weekStartChanged.connect([this] { refresh(); requestLayout(); }));
    m_subscriptions.push_back(model->showWeekNumbersChanged.connect([this] { requestLayout(); requestPaint(); }));
    m_subscriptions.push_back(window->currentDateChanged.connect([this] {
        refresh();
        requestLayout();
        requestPaint();
    }));
    m_subscriptions.push_back(window->selectionChanged.connect([this] {
        for (EventLine* bar : m_bars) {
            bar->requestLayout();
            bar->requestPaint();
        }
    }));
}

MonthView::~MonthView() = default;

Date MonthView::gridStart() const
{
    return Dates::monthGridStart(m_window->currentDate(), m_window->model()->firstDayOfWeek());
}

double MonthView::gutter() const
{
    return m_window->model()->showWeekNumbers() ? 28 : 0;
}

double MonthView::cellWidth() const
{
    return (geometry().width - gutter()) / 7;
}

double MonthView::rowHeight() const
{
    return (geometry().height - HeaderHeight) / 6;
}

int MonthView::lanes() const
{
    return std::max(1, int(std::floor((rowHeight() - DayLabelHeight - 4) / LineHeight)));
}

int MonthView::indexOf(Date date) const
{
    const int64_t index = gridStart().daysTo(date);
    return index >= 0 && index < 42 ? int(index) : -1;
}

// The first of a month carries the month's name.
std::string MonthView::dayText(int index) const
{
    const Date date = gridStart().addDays(index);
    if (date.day() != 1)
        return std::to_string(date.day());
    const std::string language = LocaleData::language();
    if (language == "zh" || language == "ja")
        return std::to_string(date.month()) + "月1日";
    return LocaleData::monthName(date.month(), LocaleData::Width::Short) + " 1";
}

std::optional<Date> MonthView::dateAt(PointF point) const
{
    if (point.x < gutter() || point.y < HeaderHeight || point.x >= geometry().width || point.y >= geometry().height)
        return std::nullopt;
    const int column = std::min(6, int(std::floor((point.x - gutter()) / cellWidth())));
    const int row = std::min(5, int(std::floor((point.y - HeaderHeight) / rowHeight())));
    return gridStart().addDays(row * 7 + column);
}

void MonthView::setLive(bool live)
{
    if (live == m_live)
        return;
    m_live = live;
    refresh();
}

void MonthView::refresh(bool force)
{
    if (geometry().height <= 0)
        return;
    const std::string key = std::to_string(m_window->model()->revision()) + ',' + gridStart().toCompactString() + ','
            + std::to_string(lanes());
    if ((!m_live && !force) || key == m_laidOutKey)
        return;
    m_laidOutKey = key;
    m_layout = m_window->model()->monthLayout(gridStart(), 6, lanes());
    rebuild();
}

void MonthView::rebuild()
{
    for (EventLine* bar : m_bars)
        takeChild(bar);
    for (MoreLabel* more : m_more)
        takeChild(more);
    m_bars.clear();
    m_more.clear();

    for (const EventItem& item : m_layout.bars) {
        EventLine* bar = addChild<EventLine>(m_window, item);
        bar->onDragMoved = [this, bar](PointF point) {
            m_dropTarget = dateAt(mapFromWindow(bar->mapToWindow(point)));
            requestLayout();
        };
        bar->onDragFinished = [this, bar](PointF start, PointF) {
            const std::optional<Date> from = dateAt(mapFromWindow(bar->mapToWindow(start)));
            const std::optional<Date> to = m_dropTarget;
            m_dropTarget.reset();
            requestLayout();
            if (from && to && *from != *to) {
                const int days = int(from->daysTo(*to));
                m_window->moveEvent(bar->item(), days, 0, days, 0);
            }
        };
        m_bars.push_back(bar);
    }
    for (const MoreItem& more : m_layout.more)
        m_more.push_back(addChild<MoreLabel>(m_window, more));
    requestLayout();
}

void MonthView::layout()
{
    // Lanes follow the height; the events are laid out again when they change.
    refresh(m_laidOutKey.empty());

    const Theme& theme = Theme::current();
    CalendarModel* model = m_window->model();
    const int firstDay = model->firstDayOfWeek();
    const double gutter = this->gutter();
    const double cellWidth = this->cellWidth();
    const double rowHeight = this->rowHeight();
    const Date gridStart = this->gridStart();
    const Date current = m_window->currentDate();
    const int monthFirst = int(gridStart.daysTo(Dates::monthStart(current)));
    const int monthLast = monthFirst + current.daysInMonth() - 1;
    const int today = indexOf(model->today());
    const double scale = window() ? window()->scale() : 1;
    // Of the regular weight, as every day but today has.
    const double capHeight = m_days[today == 0 ? 1 : 0]->capHeight();

    for (int i = 0; i < 7; ++i) {
        Label* label = m_weekdays[i];
        label->setText(LocaleData::dayName(Dates::weekdayOfColumn(i, firstDay), LocaleData::Width::Short));
        label->setColor(theme.disabledText);
        label->setGeometry({gutter + i * cellWidth, 0, std::max(0.0, cellWidth - 12), HeaderHeight});
    }

    for (int i = 0; i < 42; ++i) {
        Label* label = m_days[i];
        const bool inMonth = i >= monthFirst && i <= monthLast;
        const bool isToday = i == today;
        label->setText(dayText(i));
        label->setWeight(isToday ? 600 : 400);
        label->setColor(isToday ? theme.highlightedText : inMonth ? theme.text : theme.disabledText);
        label->setOpacity(inMonth || isToday ? 1 : 0.6);
        // Centred on where the today plate would be, capitals on its middle.
        const SizeF size = label->sizeHint();
        const double cellRight = gutter + (i % 7 + 1) * cellWidth;
        const double plateTop = HeaderHeight + (i / 7) * rowHeight + 4;
        const double y = std::round((plateTop + 11 + capHeight / 2 - label->baselineOffset()) * scale) / scale;
        label->setGeometry({cellRight - 6 - plateWidth(size.width) / 2 - size.width / 2, y, size.width, size.height});
    }

    const bool numbers = model->showWeekNumbers();
    for (int i = 0; i < 6; ++i) {
        Label* label = m_weekNumbers[i];
        label->setVisible(numbers);
        label->setText(std::to_string(model->weekNumber(gridStart.addDays(i * 7))));
        label->setColor(theme.disabledText);
        label->setGeometry({0, HeaderHeight + i * rowHeight + 4, gutter, 22});
    }

    for (const auto& child : children()) {
        if (auto* target = dynamic_cast<DropTarget*>(child.get())) {
            target->setVisible(m_dropTarget.has_value());
            if (m_dropTarget) {
                const int index = int(gridStart.daysTo(*m_dropTarget));
                target->setGeometry({gutter + (index % 7) * cellWidth, HeaderHeight + (index / 7) * rowHeight,
                                     cellWidth, rowHeight});
            }
        }
    }

    for (EventLine* bar : m_bars) {
        const EventItem& item = bar->item();
        bar->setGeometry({gutter + item.column * cellWidth + (item.continuesBefore ? 0 : 4),
                          HeaderHeight + item.row * rowHeight + DayLabelHeight + 2 + item.lane * LineHeight,
                          item.span * cellWidth - (item.continuesBefore ? 0 : 4) - (item.continuesAfter ? 0 : 4),
                          LineHeight - 2});
    }
    for (MoreLabel* more : m_more) {
        const MoreItem& item = more->item();
        more->setGeometry({gutter + item.column * cellWidth + 10,
                           HeaderHeight + item.row * rowHeight + DayLabelHeight + 2 + (lanes() - 1) * LineHeight,
                           cellWidth - 20, LineHeight - 2});
    }
}

void MonthView::paint(Painter& painter)
{
    const Theme& theme = Theme::current();
    CalendarModel* model = m_window->model();
    const double width = geometry().width;
    const double height = geometry().height;
    const double gutter = this->gutter();
    const double cellWidth = this->cellWidth();
    const double rowHeight = this->rowHeight();
    const Color line = Style::lineColor();
    const int firstDay = model->firstDayOfWeek();

    painter.drawHorizontalLine(0, HeaderHeight, width, line);

    // Weekend columns, as the language counts them.
    const std::vector<int> weekend = model->weekendDays();
    for (int i = 0; i < 7; ++i) {
        if (std::find(weekend.begin(), weekend.end(), Dates::weekdayOfColumn(i, firstDay)) != weekend.end()) {
            painter.fillRect({gutter + i * cellWidth, HeaderHeight, cellWidth, height - HeaderHeight},
                             color(CalendarColor::WeekendColumn));
        }
    }

    const int selected = indexOf(m_window->currentDate());
    if (selected >= 0) {
        painter.fillRect({gutter + (selected % 7) * cellWidth, HeaderHeight + (selected / 7) * rowHeight, cellWidth,
                          rowHeight},
                         color(CalendarColor::SelectedDay));
    }

    for (int i = 0; i < 5; ++i)
        painter.drawHorizontalLine(0, HeaderHeight + (i + 1) * rowHeight, width, line);
    for (int i = 0; i < (gutter > 0 ? 7 : 6); ++i)
        painter.drawVerticalLine(gutter + (i + (gutter > 0 ? 0 : 1)) * cellWidth, HeaderHeight, height - HeaderHeight, line);

    const int today = indexOf(model->today());
    if (today >= 0) {
        const double plate = plateWidth(m_days[today]->sizeHint().width);
        painter.fillRoundedRect({gutter + (today % 7 + 1) * cellWidth - 6 - plate,
                                 HeaderHeight + (today / 7) * rowHeight + 4, plate, 22},
                                11, theme.highlight);
    }
}

void MonthView::pointerPress(const PointerEvent& event)
{
    m_doublePress = false;
    const std::optional<Date> date = dateAt(event.position);
    if (!date)
        return;
    m_window->setCurrentDate(*date);
    m_window->clearSelection();
    if (event.button == MouseButton::Left && event.clickCount == 2) {
        m_doublePress = true;
        m_window->newEventOn(*date);
    }
}

void MonthView::pointerRelease(const PointerEvent& event)
{
    if (event.button != MouseButton::Right || !localRect().contains(event.position))
        return;
    if (const std::optional<Date> date = dateAt(event.position))
        m_window->showDayMenu(*date, mapToWindow(event.position));
}

bool MonthView::scroll(const ScrollEvent& event)
{
    const double delta = event.precise || event.value120Y == 0 ? -event.deltaY : -event.value120Y / 120.0 * 60;
    m_settle.start();
    if (m_spent || delta == 0)
        return true;
    m_travel += delta;
    if (std::abs(m_travel) >= 60) {
        m_window->step(m_travel > 0 ? -1 : 1);
        m_travel = 0;
        m_spent = event.precise;
    }
    return true;
}
