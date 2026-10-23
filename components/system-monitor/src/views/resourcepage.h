// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "base/signal.h"
#include "ui/scroll_view.h"

#include <vector>

class ChartCard;
class InfoRow;
class ListCard;
class Monitor;
class SectionLabel;

namespace bayu::ui {
class Column;
class ColumnLayout;
class GridLayout;
class Label;
class ProgressBar;
} // namespace bayu::ui

// A resource's chart and details: CPU (1), memory (2), network (3), disks (4)
// or GPUs (5).
class ResourcePage : public bayu::ui::ScrollView
{
public:
    ResourcePage(Monitor& monitor, int page);

    void setPage(int page);
    int page() const { return m_page; }

protected:
    void layout() override;

private:
    struct Section
    {
        bayu::ui::ColumnLayout* column = nullptr;
        SectionLabel* title = nullptr;
        ListCard* card = nullptr;
        bayu::ui::ProgressBar* usage = nullptr;
        std::vector<InfoRow*> rows;
    };
    struct Tile;

    void rebuild();
    void update();
    void updateChart();
    Section addSection(bayu::ui::View* parent, size_t rows);
    bayu::ui::Label* addHeading(const char* text);

    Monitor& m_monitor;
    int m_page;
    bayu::ui::Column* m_content;
    // As the Qt version's ColumnLayout, inside the scrolled margins.
    bayu::ui::ColumnLayout* m_layout;
    ChartCard* m_chart = nullptr;
    bayu::ui::GridLayout* m_cores = nullptr;
    std::vector<Tile*> m_tiles;
    ListCard* m_card = nullptr;
    std::vector<Section> m_sections;
    std::vector<Section> m_volumes;
    size_t m_built[3] = {0, 0, 0};
    bayu::base::Subscription m_updated;
};
