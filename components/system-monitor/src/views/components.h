// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "ui/geometry.h"
#include "ui/label.h"
#include "ui/layout.h"
#include "ui/painter.h"
#include "ui/cell_layout.h"
#include "ui/view.h"

#include <optional>
#include <string>

// FishUI's point sizes in logical pixels.
inline double points(double size)
{
    return size * 4 / 3;
}

// The colour laid over the base, as Qt.tint does.
bayu::ui::Color tint(bayu::ui::Color base, bayu::ui::Color overlay);

bayu::ui::Label* addLabel(bayu::ui::View* parent, std::string text, double pointSize = 0, int weight = 400,
                              std::optional<bayu::ui::Color> color = std::nullopt);

// A rounded plate in a colour, its corners the theme's unless told otherwise.
class Plate : public bayu::ui::View
{
public:
    explicit Plate(double radius = 0);

    void setRadius(double radius);
    // Unset: the theme's second background.
    void setColor(std::optional<bayu::ui::Color> color);
    void setContinuousCorners(std::optional<bool> continuous);

protected:
    void paint(bayu::ui::Painter& painter) override;

    bayu::ui::Corners corners() const;

private:
    double m_radius;
    std::optional<bayu::ui::Color> m_color;
    std::optional<bool> m_continuous;
};

// One device pixel at any scale, across the bottom of its rect.
void paintSeparator(bayu::ui::Painter& painter, double x, double bottom, double width);

// One read-only row of a ListCard: the label, then its value in secondary text.
class InfoRow : public bayu::ui::View
{
public:
    InfoRow(std::string text = {}, std::string value = {});

    void setText(std::string text);
    void setValue(std::string value);
    // Drawn below every visible row but the card's last.
    void setSeparatorVisible(bool visible);

    bayu::ui::SizeF sizeHint() const override { return {0, 44}; }

protected:
    void layout() override;
    void paint(bayu::ui::Painter& painter) override;

private:
    bayu::ui::RowLayout* m_row;
    bayu::ui::Label* m_text;
    bayu::ui::Label* m_value;
    bool m_separator = false;
};

// A card of InfoRows: no inner padding, the rows carry their own.
class ListCard : public Plate
{
public:
    ListCard();

    InfoRow* addRow(std::string text, std::string value = {});
    // After rows were shown or hidden.
    void updateSeparators();

    bayu::ui::SizeF sizeHint() const override;
    double heightForWidth(double width) const override;

protected:
    void layout() override;
};

// Names the card below it; padded to the card's text edge.
class SectionLabel : public bayu::ui::View
{
public:
    explicit SectionLabel(std::string text = {});

    void setText(std::string text);
    void setElide(bayu::ui::Label::Elide elide);

    bayu::ui::SizeF sizeHint() const override;
    double heightForWidth(double width) const override;

protected:
    void layout() override;

private:
    bayu::ui::Label* m_label;
};
