// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "eventviews.h"
#include "calendarwindow.h"
#include "style.h"
#include "timegridview.h"

#include <base/i18n.h>
#include <ui/label.h>
#include <ui/painter.h>

#include <algorithm>
#include <cmath>

using namespace bayu::ui;
using bayu::i18n::format;
using bayu::i18n::tr;
using bayu::i18n::trn;

static std::string titleOf(const EventItem& item)
{
    return item.title.empty() ? std::string(tr("New Event")) : item.title;
}

EventPointer::EventPointer(CalendarWindow* window, View* view)
    : m_window(window)
    , m_view(view)
{
}

void EventPointer::press(const PointerEvent& event)
{
    m_pressPoint = event.position;
    m_pressed = event.button == MouseButton::Left && !(m_item && m_item->readOnly);
    if (m_dragging) {
        m_dragging = false;
        if (onDraggingChanged)
            onDraggingChanged();
    }
    if (!m_item)
        return;
    m_window->selectEvent(m_item);
    m_window->focusView();
    if (event.button == MouseButton::Left && event.clickCount == 2) {
        m_pressed = false;
        m_window->editEvent(m_item->key, m_item->recurrenceKey);
    }
}

void EventPointer::move(const PointerEvent& event)
{
    if (!m_pressed)
        return;
    if (!m_dragging && std::hypot(event.position.x - m_pressPoint.x, event.position.y - m_pressPoint.y) >= 6) {
        m_dragging = true;
        if (onDraggingChanged)
            onDraggingChanged();
    }
    if (m_dragging && onDragMoved)
        onDragMoved(event.position);
}

void EventPointer::release(const PointerEvent& event)
{
    m_pressed = false;
    if (m_dragging) {
        m_dragging = false;
        if (onDraggingChanged)
            onDraggingChanged();
        if (onDragFinished)
            onDragFinished(m_pressPoint, event.position);
        return;
    }
    if (event.button == MouseButton::Right && m_item && m_view->localRect().contains(event.position))
        m_window->showEventMenu(*m_item, m_view->mapToWindow(event.position));
}

EventLine::EventLine(CalendarWindow* window, EventItem item)
    : m_window(window)
    , m_item(std::move(item))
    , m_pointer(window, this)
    , m_title(addChild<Label>(titleOf(m_item)))
    , m_time(addChild<Label>(m_item.startText))
{
    setName("eventLine");
    m_pointer.setItem(&m_item);
    m_pointer.onDragMoved = [this](PointF point) {
        if (onDragMoved)
            onDragMoved(point);
    };
    m_pointer.onDragFinished = [this](PointF start, PointF point) {
        if (onDragFinished)
            onDragFinished(start, point);
    };
    m_pointer.onDraggingChanged = [this] { setOpacity(m_pointer.isDragging() ? 0.5 : 1); };
    m_title->setFontSize(Style::points(8));
    m_time->setFontSize(Style::points(8));
    m_time->setElide(Label::Elide::None);
}

void EventLine::layout()
{
    const Theme& theme = Theme::current();
    const bool selected = m_window->isSelected(m_item);
    const double width = geometry().width;
    const double height = geometry().height;

    m_title->setWeight(isBar() ? 500 : 400);
    m_title->setColor(selected ? theme.highlightedText : theme.text);
    m_time->setColor(selected ? theme.highlightedText : theme.disabledText);
    m_time->setVisible(!isBar() && !m_item.startText.empty() && width >= 150);

    // As a RowLayout places them: whole pixels, centred across.
    double left = isBar() ? 6 : 4;
    double right = width - 6;
    if (!isBar())
        left += 6 + 5;
    if (m_time->isVisible()) {
        const SizeF time = {std::ceil(m_time->sizeHint().width), m_time->sizeHint().height};
        m_time->setGeometry({std::round(right - time.width), std::round((height - time.height) / 2), time.width,
                             time.height});
        right -= time.width + 5;
    }
    const double titleHeight = m_title->sizeHint().height;
    m_title->setGeometry({left, std::round((height - titleHeight) / 2), std::max(0.0, right - left), titleHeight});
}

void EventLine::paint(Painter& painter)
{
    const Theme& theme = Theme::current();
    const bool selected = m_window->isSelected(m_item);
    const Color color = Style::toColor(m_item.color);
    if (selected)
        painter.fillRoundedRect(localRect(), 4, color);
    else if (isBar())
        painter.fillRoundedRect(localRect(), 4, Style::withAlpha(color, theme.darkMode ? 0.32 : 0.2));
    else if (m_hovered)
        painter.fillRoundedRect(localRect(), 4, Style::withAlpha(theme.text, 0.05));

    if (!isBar())
        painter.fillEllipse({4, std::round((geometry().height - 6) / 2), 6, 6}, selected ? theme.highlightedText : color);
}

void EventLine::pointerEnter()
{
    m_hovered = true;
    requestPaint();
}

void EventLine::pointerLeave()
{
    m_hovered = false;
    requestPaint();
}

void EventLine::pointerPress(const PointerEvent& event)
{
    m_pointer.press(event);
}

void EventLine::pointerMove(const PointerEvent& event)
{
    m_pointer.move(event);
}

void EventLine::pointerRelease(const PointerEvent& event)
{
    m_pointer.release(event);
}

MoreLabel::MoreLabel(CalendarWindow* window, MoreItem item)
    // TRANSLATORS: the events of a day that do not fit in it, as in "3 more".
    : Label(trn("{n} more", "{n} more", item.count))
    , m_window(window)
    , m_item(item)
{
    setFontSize(Style::points(8));
    setWeight(500);
    setVerticallyCentered(true);
}

void MoreLabel::layout()
{
    const Theme& theme = Theme::current();
    setColor(m_hovered ? theme.highlight : theme.disabledText);
}

void MoreLabel::pointerEnter()
{
    m_hovered = true;
    requestLayout();
}

void MoreLabel::pointerLeave()
{
    m_hovered = false;
    requestLayout();
}

void MoreLabel::pointerRelease(const PointerEvent& event)
{
    if (event.button == MouseButton::Left && localRect().contains(event.position))
        m_window->showDay(m_item.date);
}

// The end of an event that ends on this day can be dragged.
class EventBlock::Resizer : public View
{
public:
    explicit Resizer(EventBlock* block)
        : m_block(block)
    {
    }

    bool acceptsPointer() const override { return true; }
    CursorShape cursorShape() const override { return CursorShape::ResizeVertical; }

protected:
    void pointerPress(const PointerEvent& event) override
    {
        if (event.button != MouseButton::Left)
            return;
        m_pressed = true;
        m_startY = m_block->m_grid->mapFromWindow(mapToWindow(event.position)).y;
        m_block->m_window->selectEvent(&m_block->m_item);
    }

    void pointerMove(const PointerEvent& event) override
    {
        if (!m_pressed)
            return;
        TimeGridView* grid = m_block->m_grid;
        const EventItem& item = m_block->m_item;
        const double y = grid->mapFromWindow(mapToWindow(event.position)).y;
        const int minutes = grid->snap((y - m_startY) / TimeGridView::HourHeight * 60);
        const int length = item.endMinute - item.startMinute;
        // At least one step long, and not past midnight.
        grid->dragResize(item, std::max(TimeGridView::Step - length, std::min(24 * 60 - item.endMinute, minutes)));
    }

    void pointerRelease(const PointerEvent&) override
    {
        if (!m_pressed)
            return;
        m_pressed = false;
        m_block->m_grid->finishDrag(m_block->m_item);
    }

private:
    EventBlock* m_block;
    bool m_pressed = false;
    double m_startY = 0;
};

EventBlock::EventBlock(CalendarWindow* window, TimeGridView* grid, EventItem item)
    : m_window(window)
    , m_grid(grid)
    , m_item(std::move(item))
    , m_pointer(window, this)
{
    setName("eventBlock");
    View* column = addChild<View>();
    column->setClipsChildren(true);
    m_title = column->addChild<Label>();
    // TRANSLATORS: an event that began on an earlier day, such as "until 10:00".
    const std::string time = m_item.continuesBefore ? format(tr("until {time}"), {{"time", m_item.endText}})
                                                    : format(tr("{start} – {end}"),
                                                             {{"start", m_item.startText}, {"end", m_item.endText}});
    m_time = column->addChild<Label>(time);
    m_location = column->addChild<Label>(m_item.location);
    for (Label* label : {m_title, m_time, m_location})
        label->setFontSize(Style::points(8));
    m_title->setWeight(500);
    m_resizer = addChild<Resizer>(this);
    m_resizer->setVisible(!m_item.continuesAfter && !m_item.allDay && !m_item.readOnly);

    m_pointer.setItem(&m_item);
    m_pointer.onDragMoved = [this](PointF point) {
        const PointF start = m_grid->mapFromWindow(mapToWindow(m_pointer.pressPoint()));
        const PointF now = m_grid->mapFromWindow(mapToWindow(point));
        m_grid->dragMove(m_item, m_grid->columnAt(now.x) - m_grid->columnAt(start.x),
                         m_grid->snap((now.y - start.y) / TimeGridView::HourHeight * 60));
    };
    m_pointer.onDragFinished = [this](PointF, PointF) { m_grid->finishDrag(m_item); };
}

void EventBlock::layout()
{
    const Theme& theme = Theme::current();
    const bool selected = m_window->isSelected(m_item);
    const double width = geometry().width;
    const double height = geometry().height;
    const bool compact = isCompact();
    const bool narrow = width < 80;

    View* column = m_title->parent();
    const double top = compact ? 0 : 3;
    column->setGeometry({8, top, std::max(0.0, width - 8 - 4), std::max(0.0, height - top)});
    const double columnWidth = column->geometry().width;

    std::string title = titleOf(m_item);
    if (compact && !m_item.startText.empty() && width >= 140)
        title += "  " + m_item.startText;
    m_title->setText(title);
    // A word wider than a narrow block is elided, not broken or cut.
    m_title->setWrap(!compact && !narrow);
    m_title->setMaximumLineCount(compact || narrow ? 1 : 2);
    if (compact != m_compact) {
        m_compact = compact;
        m_title->setVerticallyCentered(compact);
    }
    m_title->setColor(selected ? theme.highlightedText : theme.text);
    m_time->setColor(selected ? theme.highlightedText : theme.disabledText);
    m_location->setColor(selected ? theme.highlightedText : theme.disabledText);
    m_time->setVisible(!compact && height >= 40);
    m_location->setVisible(!compact && height >= 58 && !m_item.location.empty());

    const double titleHeight = compact ? height : m_title->heightForWidth(columnWidth);
    m_title->setGeometry({0, 0, columnWidth, titleHeight});
    double y = titleHeight;
    for (Label* label : {m_time, m_location}) {
        if (!label->isVisible())
            continue;
        const double labelHeight = label->sizeHint().height;
        label->setGeometry({0, y, columnWidth, labelHeight});
        y += labelHeight;
    }
    m_resizer->setGeometry({0, height - 6, width, 6});
}

void EventBlock::paint(Painter& painter)
{
    const Theme& theme = Theme::current();
    const bool selected = m_window->isSelected(m_item);
    const Color color = Style::toColor(m_item.color);
    painter.fillRoundedRect(localRect(), 4, selected ? color : Style::withAlpha(color, theme.darkMode ? 0.32 : 0.18));
    if (!selected)
        painter.fillRoundedRect({0, 0, 3, geometry().height}, 1.5, color);
}

void EventBlock::pointerPress(const PointerEvent& event)
{
    m_pointer.press(event);
}

void EventBlock::pointerMove(const PointerEvent& event)
{
    m_pointer.move(event);
}

void EventBlock::pointerRelease(const PointerEvent& event)
{
    m_pointer.release(event);
}
