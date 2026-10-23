// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "calendarmodel.h"

#include <ui/label.h>
#include <ui/view.h>

#include <functional>

class CalendarWindow;

// Pointer handling every event shape shares: a press selects, a double click
// edits, a right click opens the event menu, and a press that moves drags.
class EventPointer
{
public:
    EventPointer(CalendarWindow* window, bayu::ui::View* view);

    void setItem(const EventItem* item) { m_item = item; }
    bool isDragging() const { return m_dragging; }
    bayu::ui::PointF pressPoint() const { return m_pressPoint; }

    void press(const bayu::ui::PointerEvent& event);
    void move(const bayu::ui::PointerEvent& event);
    void release(const bayu::ui::PointerEvent& event);

    // In the view's coordinates.
    std::function<void(bayu::ui::PointF point)> onDragMoved;
    std::function<void(bayu::ui::PointF start, bayu::ui::PointF point)> onDragFinished;
    std::function<void()> onDraggingChanged;

private:
    CalendarWindow* m_window;
    bayu::ui::View* m_view;
    const EventItem* m_item = nullptr;
    bayu::ui::PointF m_pressPoint;
    bool m_pressed = false;
    bool m_dragging = false;
};

// One event on a line of a day: a tinted bar for one that lasts a day or
// more, a dot, title and time for a timed one.
class EventLine : public bayu::ui::View
{
public:
    EventLine(CalendarWindow* window, EventItem item);

    const EventItem& item() const { return m_item; }
    bool isDragging() const { return m_pointer.isDragging(); }

    bool acceptsPointer() const override { return true; }

    std::function<void(bayu::ui::PointF point)> onDragMoved;
    std::function<void(bayu::ui::PointF start, bayu::ui::PointF point)> onDragFinished;

protected:
    void layout() override;
    void paint(bayu::ui::Painter& painter) override;
    void pointerEnter() override;
    void pointerLeave() override;
    void pointerPress(const bayu::ui::PointerEvent& event) override;
    void pointerMove(const bayu::ui::PointerEvent& event) override;
    void pointerRelease(const bayu::ui::PointerEvent& event) override;

private:
    bool isBar() const { return m_item.allDay || m_item.bar; }

    CalendarWindow* m_window;
    EventItem m_item;
    EventPointer m_pointer;
    bayu::ui::Label* m_title;
    bayu::ui::Label* m_time;
    bool m_hovered = false;
};

// "N more" under a day's lines; a click opens the day.
class MoreLabel : public bayu::ui::Label
{
public:
    MoreLabel(CalendarWindow* window, MoreItem item);

    const MoreItem& item() const { return m_item; }

    bool acceptsPointer() const override { return true; }

protected:
    void layout() override;
    void pointerEnter() override;
    void pointerLeave() override;
    void pointerRelease(const bayu::ui::PointerEvent& event) override;

private:
    CalendarWindow* m_window;
    MoreItem m_item;
    bool m_hovered = false;
};

class TimeGridView;

// A timed event on the hour grid: moved by dragging it, lengthened by
// dragging its bottom edge.
class EventBlock : public bayu::ui::View
{
public:
    EventBlock(CalendarWindow* window, TimeGridView* grid, EventItem item);

    const EventItem& item() const { return m_item; }

    bool acceptsPointer() const override { return true; }

protected:
    void layout() override;
    void paint(bayu::ui::Painter& painter) override;
    void pointerPress(const bayu::ui::PointerEvent& event) override;
    void pointerMove(const bayu::ui::PointerEvent& event) override;
    void pointerRelease(const bayu::ui::PointerEvent& event) override;

private:
    class Resizer;

    bool isCompact() const { return geometry().height < 34; }

    CalendarWindow* m_window;
    TimeGridView* m_grid;
    EventItem m_item;
    EventPointer m_pointer;
    bayu::ui::Label* m_title;
    bayu::ui::Label* m_time;
    bayu::ui::Label* m_location;
    Resizer* m_resizer;
    bool m_compact = false;
};
