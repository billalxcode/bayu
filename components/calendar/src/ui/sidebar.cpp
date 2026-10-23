// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "sidebar.h"
#include "calendarwindow.h"
#include "dates.h"
#include "localedata.h"
#include "minimonth.h"
#include "style.h"

#include <base/event_loop.h>
#include <base/i18n.h>
#include <ui/cell_layout.h>
#include <ui/icon.h>
#include <ui/label.h>
#include <ui/layout.h>
#include <ui/painter.h>
#include <ui/round_image_button.h>

#include <cmath>

using namespace bayu::ui;
using bayu::i18n::tr;

// A rounded card in the calendar's second background colour.
class Sidebar::Card : public View
{
public:
    // A filling content is anchored on every side, the bottom by the side
    // margin, and gets whatever the card has beyond its own height.
    Card(double top, double bottom, double sides, bool fills = false)
        : m_top(top)
        , m_bottom(bottom)
        , m_sides(sides)
        , m_fills(fills)
    {
    }

    View* content() const { return children().empty() ? nullptr : children().front().get(); }

    double heightForWidth(double width) const override
    {
        View* view = content();
        return m_top + (view ? view->heightForWidth(width - 2 * m_sides) : 0) + m_bottom;
    }
    SizeF sizeHint() const override { return {0, heightForWidth(geometry().width)}; }

protected:
    void layout() override
    {
        if (View* view = content()) {
            const double width = geometry().width - 2 * m_sides;
            const double height = m_fills ? geometry().height - m_top - m_sides : view->heightForWidth(width);
            view->setGeometry({m_sides, m_top, width, height});
        }
    }

    void paint(Painter& painter) override
    {
        const Theme& theme = Theme::current();
        painter.fillRoundedRect(localRect(), theme.mediumRadius, theme.secondBackground);
    }

private:
    double m_top;
    double m_bottom;
    double m_sides;
    bool m_fills;
};

// A calendar with its colour box: a click shows or hides it.
class CalendarRow : public View
{
public:
    CalendarRow(CalendarWindow* window, CalendarItem calendar)
        : m_window(window)
        , m_calendar(std::move(calendar))
        , m_check(addChild<Icon>("check", 12))
        , m_name(addChild<Label>(m_calendar.name))
    {
        m_check->setColor(Color{255, 255, 255, 255});
    }

    SizeF sizeHint() const override { return {0, 34}; }
    double heightForWidth(double) const override { return 34; }
    bool acceptsPointer() const override { return true; }

protected:
    void layout() override
    {
        const double height = geometry().height;
        m_check->setVisible(m_calendar.visible);
        m_check->setGeometry({10 + 2, (height - 12) / 2, 12, 12});
        // As the delegate's RowLayout places it: whole pixels, centred.
        const double nameHeight = m_name->sizeHint().height;
        m_name->setGeometry({10 + 16 + 10, std::round((height - nameHeight) / 2),
                             std::max(0.0, geometry().width - 36 - 10), nameHeight});
    }

    void paint(Painter& painter) override
    {
        const Theme& theme = Theme::current();
        // Inset 6 into the radius 10 card.
        if (m_pressed)
            painter.fillRoundedRect(localRect(), theme.mediumRadius - 6, Style::withAlpha(theme.text, 0.08));
        else if (m_hovered)
            painter.fillRoundedRect(localRect(), theme.mediumRadius - 6, Style::withAlpha(theme.text, 0.04));

        const Color color = Style::toColor(m_calendar.color);
        const RectF box{10, (geometry().height - 16) / 2, 16, 16};
        if (m_calendar.visible)
            painter.fillRoundedRect(box, 5, color);
        else
            painter.strokeRoundedRect(box, 5, color, painter.borderWidth(1.5));
    }

    void pointerEnter() override
    {
        m_hovered = true;
        requestPaint();
    }

    void pointerLeave() override
    {
        m_hovered = false;
        requestPaint();
    }

    void pointerPress(const PointerEvent& event) override
    {
        if (event.button == MouseButton::Left) {
            m_pressed = true;
            requestPaint();
        }
    }

    void pointerRelease(const PointerEvent& event) override
    {
        const bool inside = localRect().contains(event.position);
        if (event.button == MouseButton::Right) {
            if (inside)
                m_window->showCalendarMenu(m_calendar, mapToWindow(event.position));
            return;
        }
        if (!m_pressed)
            return;
        m_pressed = false;
        requestPaint();
        if (inside)
            m_window->model()->setCalendarVisible(m_calendar.id, !m_calendar.visible);
    }

private:
    CalendarWindow* m_window;
    CalendarItem m_calendar;
    Icon* m_check;
    Label* m_name;
    bool m_hovered = false;
    bool m_pressed = false;
};

Sidebar::Sidebar(CalendarWindow* window)
    : m_window(window)
    , m_shownMonth(Dates::monthStart(window->currentDate()))
{
    setName("sidebar");
    Column* content = setContent<Column>();
    content->setMargins({12, 4, 12, 16});
    content->setSpacing(12);

    Card* monthCard = content->addChild<Card>(10, 14, 12, true);
    monthCard->setName("miniMonth");
    monthCard->setLayoutHints({.fillWidth = true});
    // Layouts, which round sizes and positions to whole pixels.
    ColumnLayout* monthColumn = monthCard->addChild<ColumnLayout>();
    monthColumn->setSpacing(4);
    RowLayout* monthRow = monthColumn->addChild<RowLayout>();
    monthRow->setSpacing(0);
    monthRow->setLayoutHints({.fillWidth = true});
    m_monthLabel = monthRow->addChild<Label>();
    m_monthLabel->setFontSize(Style::points(9));
    m_monthLabel->setWeight(500);
    m_monthLabel->setLayoutHints({.fillWidth = true, .margins = {4, 0, 0, 0}});
    RoundImageButton* previous = monthRow->addChild<RoundImageButton>("chevron-left", 24);
    previous->setIconSize(16);
    previous->setName("miniPrevious");
    previous->onClicked = [this] {
        m_shownMonth = m_shownMonth.addMonths(-1);
        updateMonth();
    };
    RoundImageButton* next = monthRow->addChild<RoundImageButton>("chevron-right", 24);
    next->setIconSize(16);
    next->setName("miniNext");
    next->onClicked = [this] {
        m_shownMonth = m_shownMonth.addMonths(1);
        updateMonth();
    };
    m_miniMonth = monthColumn->addChild<MiniMonth>(window->model());
    m_miniMonth->setLayoutHints({.fillWidth = true});
    m_miniMonth->onDayClicked = [window](Date date) { window->setCurrentDate(date); };
    m_miniMonth->onDayDoubleClicked = [window](Date date) { window->showDay(date); };

    RowLayout* heading = addHeading(tr("Calendars"), "addCalendar");
    heading->children()[0]->setName("calendarsHeading");
    static_cast<RoundImageButton*>(heading->children()[1].get())->onClicked = [window] { window->newCalendar(); };

    m_calendarList = content->addChild<Card>(6, 6, 6);
    m_calendarList->setName("calendarList");
    m_calendarList->setLayoutHints({.fillWidth = true});
    m_calendarList->addChild<Column>();

    // Shown once there is a subscription; the main menu adds the first.
    m_subscriptionHeading = addHeading(tr("Subscriptions"), "addSubscription");
    m_subscriptionHeading->children()[0]->setName("subscriptionsHeading");
    static_cast<RoundImageButton*>(m_subscriptionHeading->children()[1].get())->onClicked = [window] {
        window->newSubscription();
    };

    m_subscriptionList = content->addChild<Card>(6, 6, 6);
    m_subscriptionList->setName("subscriptionList");
    m_subscriptionList->setLayoutHints({.fillWidth = true});
    m_subscriptionList->addChild<Column>();

    CalendarModel* model = window->model();
    m_subscriptions.push_back(model->revisionChanged.connect([this] {
        // Later: a row's own click hides its calendar, and the rows are rebuilt.
        if (m_updatePosted)
            return;
        m_updatePosted = true;
        bayu::base::EventLoop::current()->post([this, alive = std::weak_ptr<bool>(m_alive)] {
            if (alive.expired())
                return;
            m_updatePosted = false;
            updateCalendars();
            m_miniMonth->refresh();
        });
    }));
    m_subscriptions.push_back(model->todayChanged.connect([this] { m_miniMonth->refresh(); }));
    m_subscriptions.push_back(model->weekStartChanged.connect([this] {
        m_miniMonth->refresh();
        updateMonth();
    }));
    m_subscriptions.push_back(window->currentDateChanged.connect([this] {
        m_shownMonth = Dates::monthStart(m_window->currentDate());
        updateMonth();
    }));
    m_subscriptions.push_back(window->viewChanged.connect([this] { updateMonth(); }));
    updateMonth();
    updateCalendars();
}

Sidebar::~Sidebar() = default;

RowLayout* Sidebar::addHeading(const std::string& text, const std::string& buttonName)
{
    RowLayout* heading = contentView()->addChild<RowLayout>();
    heading->setSpacing(0);
    heading->setLayoutHints({.fillWidth = true});
    Label* label = heading->addChild<Label>(text);
    label->setFontSize(Style::points(9));
    label->setWeight(500);
    label->setLayoutHints({.fillWidth = true, .margins = {4, 0, 0, 0}});
    m_headings.push_back(label);
    RoundImageButton* add = heading->addChild<RoundImageButton>("calendar-add", 24);
    add->setIconSize(16);
    add->setName(buttonName);
    return heading;
}

void Sidebar::layout()
{
    for (Label* heading : m_headings)
        heading->setColor(Theme::current().disabledText);
    ScrollView::layout();
}

void Sidebar::setShownMonth(Date month)
{
    m_shownMonth = month;
    updateMonth();
}

void Sidebar::updateMonth()
{
    // The name stands alone, as a heading.
    m_monthLabel->setText(LocaleData::monthName(m_shownMonth.month()) + " " + std::to_string(m_shownMonth.year()));
    m_miniMonth->setMonth(m_shownMonth);
    m_miniMonth->setSelectedDate(m_window->currentDate());
    if (m_window->view() == "week")
        m_miniMonth->setRange(m_window->rangeStart(), m_window->rangeStart().addDays(6));
    else
        m_miniMonth->setRange(std::nullopt, std::nullopt);
}

void Sidebar::updateCalendars()
{
    auto* calendars = static_cast<Column*>(m_calendarList->content());
    auto* subscriptions = static_cast<Column*>(m_subscriptionList->content());
    for (CalendarRow* row : m_rows)
        static_cast<Column*>(row->parent())->takeChild(row);
    m_rows.clear();
    bool subscribed = false;
    for (const CalendarItem& calendar : m_window->model()->calendars()) {
        subscribed = subscribed || calendar.readOnly;
        CalendarRow* row = (calendar.readOnly ? subscriptions : calendars)->addChild<CalendarRow>(m_window, calendar);
        row->setLayoutHints({.fillWidth = true});
        m_rows.push_back(row);
    }
    m_subscriptionHeading->setVisible(subscribed);
    m_subscriptionList->setVisible(subscribed);
    requestLayout();
}
