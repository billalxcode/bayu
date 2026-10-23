// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "yearview.h"
#include "calendarwindow.h"
#include "dates.h"
#include "localedata.h"
#include "minimonth.h"
#include "style.h"

#include <ui/label.h>

#include <algorithm>
#include <cmath>

using namespace bayu::ui;

static constexpr double Padding = 16;
static constexpr double ColumnSpacing = 16;
static constexpr double RowSpacing = 16;
static constexpr double TitleHeight = 24;
static constexpr double WeekdayHeight = 20;

// A month's name in the accent colour; a click opens the month.
class MonthTitle : public View
{
public:
    MonthTitle(CalendarWindow* window, int month)
        : m_window(window)
        , m_month(month)
        , m_label(addChild<Label>(LocaleData::monthName(month)))
    {
        m_label->setFontSize(Style::points(10));
        m_label->setWeight(600);
        m_label->setVerticallyCentered(true);
    }

    bool acceptsPointer() const override { return true; }

    // Only the text takes the pointer, not the rest of the line.
    void setLineGeometry(const RectF& rect)
    {
        m_line = rect;
        const double width = std::min(rect.width, m_label->sizeHint().width);
        setGeometry({rect.x, rect.y, width, rect.height});
    }

protected:
    void layout() override
    {
        m_label->setColor(Theme::current().highlight);
        m_label->setOpacity(m_hovered ? 0.75 : 1);
        m_label->setGeometry({0, 0, m_line.width, geometry().height});
    }

    void pointerEnter() override
    {
        m_hovered = true;
        requestLayout();
    }

    void pointerLeave() override
    {
        m_hovered = false;
        requestLayout();
    }

    void pointerRelease(const PointerEvent& event) override
    {
        if (event.button == MouseButton::Left && localRect().contains(event.position))
            m_window->showMonth(Date(m_window->currentDate().year(), m_month, 1));
    }

private:
    CalendarWindow* m_window;
    int m_month;
    Label* m_label;
    RectF m_line;
    bool m_hovered = false;
};

YearView::YearView(CalendarWindow* window)
    : m_window(window)
{
    setName("yearView");
    for (int month = 1; month <= 12; ++month) {
        m_titles.push_back(addChild<MonthTitle>(window, month));
        MiniMonth* mini = addChild<MiniMonth>(window->model());
        mini->setHeaderHeight(WeekdayHeight);
        mini->onDayClicked = [window](Date date) { window->setCurrentDate(date); };
        mini->onDayDoubleClicked = [window](Date date) { window->showDay(date); };
        m_months.push_back(mini);
    }

    CalendarModel* model = window->model();
    m_subscriptions.push_back(model->revisionChanged.connect([this] { refresh(); }));
    m_subscriptions.push_back(model->todayChanged.connect([this] {
        for (MiniMonth* mini : m_months)
            mini->refresh();
    }));
    m_subscriptions.push_back(model->weekStartChanged.connect([this] {
        for (MiniMonth* mini : m_months)
            mini->refresh();
        refresh();
    }));
    m_subscriptions.push_back(window->currentDateChanged.connect([this] {
        refresh();
        requestLayout();
    }));
    // Laid out once while preloaded, so the first switch to it is quick too.
    refresh(true);
}

YearView::~YearView() = default;

int YearView::year() const
{
    return m_window->currentDate().year();
}

// One lookup for the whole year, from January's grid to the end of December's.
Date YearView::busyStart() const
{
    return Dates::monthGridStart(Date(year(), 1, 1), m_window->model()->firstDayOfWeek());
}

void YearView::setLive(bool live)
{
    if (live == m_live)
        return;
    m_live = live;
    refresh();
}

void YearView::refresh(bool force)
{
    const std::string key = std::to_string(m_window->model()->revision()) + ',' + busyStart().toCompactString();
    if ((!m_live && !force) || key == m_busyKey)
        return;
    m_busyKey = key;
    const Date start = busyStart();
    const Date end = Dates::monthGridStart(Date(year(), 12, 1), m_window->model()->firstDayOfWeek()).addDays(42);
    m_busy = m_window->model()->busyDays(start, int(start.daysTo(end)));
    requestLayout();
}

void YearView::layout()
{
    const double width = geometry().width;
    const double height = geometry().height;
    // The whole year always fits: four columns unless a day would get under 15 px.
    const int columns = (width - 2 * Padding - 3 * ColumnSpacing) / 4 >= 7 * 15 ? 4 : 3;
    const int rows = 12 / columns;
    const double monthWidth = (width - 2 * Padding - (columns - 1) * ColumnSpacing) / columns;
    const double monthHeight = (height - 2 * Padding - (rows - 1) * RowSpacing) / rows;
    // The month name and the weekday row keep their size; the day rows take what is left.
    const double cellHeight = std::min(30.0, (monthHeight - TitleHeight - WeekdayHeight) / 6);
    const Date start = busyStart();

    for (int index = 0; index < 12; ++index) {
        const double x = Padding + (index % columns) * (monthWidth + ColumnSpacing);
        const double y = Padding + (index / columns) * (monthHeight + RowSpacing);
        m_titles[index]->setLineGeometry({x + 4, y, monthWidth - 8, TitleHeight});

        MiniMonth* mini = m_months[index];
        mini->setMonth(Date(year(), index + 1, 1));
        mini->setSelectedDate(m_window->currentDate());
        mini->setCellHeight(cellHeight);
        mini->setFontSize(cellHeight >= 18 && monthWidth / 7 >= 19 ? 8 : 7);
        mini->setGeometry({x, y + TitleHeight, monthWidth, monthHeight - TitleHeight});
        const int64_t offset = start.daysTo(mini->gridStart());
        std::vector<bool> busy;
        for (int64_t day = offset; day < offset + 42; ++day)
            busy.push_back(day >= 0 && size_t(day) < m_busy.size() && m_busy[size_t(day)]);
        mini->setBusy(std::move(busy));
    }
}
