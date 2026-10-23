// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "calendarmodel.h"

#include <base/signal.h>
#include <ui/view.h>

#include <string>
#include <vector>

namespace bayu::ui {
class Column;
class Label;
class ScrollView;
}

class CalendarWindow;

// Events whose title, location or notes match, coming ones first, under
// the day they fall on.
class SearchResults : public bayu::ui::View
{
public:
    explicit SearchResults(CalendarWindow* window);
    ~SearchResults() override;

    void setText(const std::string& text);
    const std::vector<EventItem>& results() const { return m_results; }

protected:
    void layout() override;
    void paint(bayu::ui::Painter& painter) override;

private:
    void update();

    CalendarWindow* m_window;
    std::string m_text;
    std::vector<EventItem> m_results;
    bayu::ui::ScrollView* m_scroll;
    bayu::ui::Column* m_list;
    bayu::ui::Label* m_empty;
    std::vector<bayu::base::Subscription> m_subscriptions;
};
