// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <string>

#include "ui/view.h"

namespace bayu::ui {
class Icon;
}

namespace settings {

class Picture;

// The sidebar's tile: a coloured rounded square with a white glyph, for the
// sidebar and a list row's leading slot. The glyph is one of settings'
// sidebar pictures, or a toolkit glyph when the name has no ".svg".
class RowIcon : public bayu::ui::View
{
public:
    RowIcon(std::string glyph, bayu::ui::Color tint);

    void set(std::string glyph, bayu::ui::Color tint);

    bayu::ui::SizeF sizeHint() const override { return {24, 24}; }

protected:
    void layout() override;
    void paint(bayu::ui::Painter& painter) override;

private:
    bayu::ui::Color m_tint;
    Picture* m_picture;
    bayu::ui::Icon* m_glyph;
};

} // namespace settings
