// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "base/signal.h"
#include "ui/list_view.h"
#include "ui/view.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class Monitor;
class Plate;
class ProcessList;
struct ProcessRecord;

namespace bayu::ui {
class Icon;
class Label;
class SearchField;
class SegmentedControl;
} // namespace bayu::ui

// The process the window acts on: its PID and start time, as the list it was
// picked from names it.
struct Selection
{
    int pid = 0;
    uint64_t startTicks = 0;
    bool application = true;
};

// The process list's rows; Up and Down move the selection, Return opens it
// and Delete asks to quit it.
class ProcessListView : public bayu::ui::ListView
{
public:
    bool acceptsFocus() const override { return true; }

    std::function<void(int step)> onStep;
    std::function<void()> onOpen;
    std::function<void()> onQuit;

protected:
    bool keyPress(const bayu::ui::KeyEvent& event) override;
};

class ProcessesPage : public bayu::ui::View
{
public:
    ProcessesPage(Monitor& monitor, Selection& selection);

    bool applicationsMode() const;
    ProcessList& list() const;
    bool isEditingSearch() const;
    void focusSearch();
    void clearSearch();
    // After the selection changed elsewhere.
    void selectionChanged();
    // Where the list's scroll bar goes, at the window's edge.
    ProcessListView* listView() const { return m_list; }

    std::function<void()> onSelectionChanged;
    std::function<void()> onOpenDetails;
    std::function<void(bool force)> onConfirmEnd;
    std::function<void(bayu::ui::PointF position)> onContextMenu;

protected:
    void layout() override;

private:
    class QuitButton;
    class ColumnHeader;
    class Row;

    void select(size_t row);
    void clearSelection();
    void reload();
    void updateSorting();
    double nameWidth() const;

    Monitor& m_monitor;
    Selection& m_selection;
    bayu::ui::SegmentedControl* m_scope;
    bayu::ui::SearchField* m_search;
    QuitButton* m_quit;
    Plate* m_table;
    std::vector<ColumnHeader*> m_headers;
    View* m_hairline;
    ProcessListView* m_list;
    View* m_empty;
    bayu::ui::Label* m_emptyTitle;
    bayu::ui::Label* m_emptyText;
    std::vector<bayu::base::Subscription> m_subscriptions;
};
