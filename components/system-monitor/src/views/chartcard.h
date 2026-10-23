// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "components.h"

#include <cstdint>
#include <string>
#include <vector>

class HistoryGraph;

namespace bayu::ui {
class ColumnLayout;
}

// A resource's minute of history at full width, with its current value.
class ChartCard : public Plate
{
public:
    ChartCard();

    void setTitle(std::string title);
    void setValue(std::string value);
    void setSubtitle(std::string subtitle);
    void setColors(bayu::ui::Color primary, bayu::ui::Color secondary);
    void setSeries(std::vector<double> values, std::vector<double> secondaryValues, std::vector<int64_t> timestamps,
                   int64_t duration);
    void setMaximum(double maximum);
    // Unset: 100 % for a fixed maximum, the scale's rate otherwise.
    void setScaleLabel(std::string label);
    void setLegends(std::string primary, std::string secondary);
    void setDurationText(std::string text);

    HistoryGraph* graph() const { return m_graph; }

    bayu::ui::SizeF sizeHint() const override { return {0, 276}; }

protected:
    void layout() override;

private:
    void updateScale();

    bayu::ui::ColumnLayout* m_column;
    bayu::ui::Label* m_title;
    bayu::ui::Label* m_value;
    bayu::ui::Label* m_subtitle;
    bayu::ui::Label* m_scale;
    HistoryGraph* m_graph;
    bayu::ui::Label* m_duration;
    bayu::ui::Label* m_primaryLegend;
    bayu::ui::Label* m_secondaryLegend;
    bayu::ui::Label* m_now;
    std::string m_scaleText;
    double m_maximum = 100;
    bayu::ui::Color m_primary;
    bayu::ui::Color m_secondary;
};
