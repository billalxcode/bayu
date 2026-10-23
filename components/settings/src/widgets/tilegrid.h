// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "ui/view.h"

namespace settings {

// Choices in cells of one size, as many to a row as fit, each at the cell's
// left and centred in its height, with some room below the last row.
class TileGrid : public bayu::ui::View
{
public:
    TileGrid(bayu::ui::SizeF cell, double bottomMargin = 0);

    bayu::ui::SizeF sizeHint() const override;
    double heightForWidth(double width) const override;

protected:
    void layout() override;

private:
    int columns(double width) const;
    int visibleCount() const;

    bayu::ui::SizeF m_cell;
    double m_bottomMargin;
};

} // namespace settings
