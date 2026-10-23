// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "ui/painter.h"
#include "ui/cell_layout.h"

namespace settings {

// A rounded panel on a page in FishUI's second background colour: the
// RoundedItem's ColumnLayout, its rows its children. A list card holds
// ListRows edge to edge, without padding, so a row's press background
// reaches the card's edge and its corners follow the card's.
class Card : public bayu::ui::ColumnLayout
{
public:
    enum class Style {
        Padded,
        List,
    };

    explicit Card(Style style = Style::Padded);

protected:
    void paint(bayu::ui::Painter& painter) override;
};


// Kept for pages written before the toolkit had it; new code calls
// bayu::ui::themeCorners().
inline bayu::ui::Corners panelCorners()
{
    return bayu::ui::themeCorners();
}

} // namespace settings
