// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "ui/view.h"

#include <cstdint>
#include <optional>
#include <vector>

// The last minute of a measurement: a filled curve over a grid, and an
// optional second series as a line.
class HistoryGraph : public bayu::ui::View
{
public:
    void setValues(std::vector<double> values);
    void setSecondaryValues(std::vector<double> values);
    // Milliseconds; the newest sample is at the right edge.
    void setTimestamps(std::vector<int64_t> timestamps);
    void setDuration(int64_t duration);
    void setColor(bayu::ui::Color color);
    void setSecondaryColor(bayu::ui::Color color);
    // Unset: no grid.
    void setGridColor(std::optional<bayu::ui::Color> color);
    // Positive: a fixed full scale; 0 picks the next power of two above the peak.
    void setMaximum(double maximum);
    // The smallest full scale an automatic maximum may pick.
    void setScaleFloor(double floor);
    void setCurved(bool curved);
    // Logical pixels.
    void setLineWidth(double width);
    double effectiveMaximum() const;

    struct Segment
    {
        bayu::ui::PointF control1;
        bayu::ui::PointF control2;
        bayu::ui::PointF end;
    };
    // The series as drawn in the rect: its first point, then cubic segments.
    static std::vector<Segment> curve(const std::vector<bayu::ui::PointF>& points, bool curved);

protected:
    void paint(bayu::ui::Painter& painter) override;

private:
    std::vector<bayu::ui::PointF> points(const std::vector<double>& values, const bayu::ui::RectF& area,
                                             bool filled) const;

    std::vector<double> m_values;
    std::vector<double> m_secondaryValues;
    std::vector<int64_t> m_timestamps;
    int64_t m_duration = 60000;
    bayu::ui::Color m_color;
    bayu::ui::Color m_secondaryColor;
    std::optional<bayu::ui::Color> m_gridColor;
    double m_maximum = 100;
    double m_scaleFloor = 1024;
    bool m_curved = false;
    double m_lineWidth = 1.5;
};
