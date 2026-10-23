// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <functional>
#include <optional>
#include <string>

#include "ui/color_mixer.h"
#include "widgets/listrow.h"

namespace bayu::ui {
class Icon;
}

namespace settings {

class Picture;

// A glyph that is a button, such as a row's info button: it fills its box and
// whitens by half while pressed. A name ending in ".svg" is a path to one of
// settings' pictures instead of a toolkit glyph. At the end of a row its
// drawing, not its box, ends where a switch does.
class IconButton : public bayu::ui::View, public TrailingOverhang
{
public:
    explicit IconButton(std::string glyph, double size = 22);

    // Unset: the theme's text colour.
    void setColor(std::optional<bayu::ui::ColorRecipe> color);

    bayu::ui::SizeF sizeHint() const override { return {m_size, m_size}; }
    bool acceptsPointer() const override { return true; }
    double trailingOverhang() const override;

    std::function<void()> onClicked;

protected:
    void layout() override;
    void pointerPress(const bayu::ui::PointerEvent& event) override;
    void pointerMove(const bayu::ui::PointerEvent& event) override;
    void pointerRelease(const bayu::ui::PointerEvent& event) override;

private:
    void setPressedInside(bool inside);

    std::string m_glyph;
    double m_size;
    bayu::ui::Icon* m_icon = nullptr;
    bayu::ui::Icon* m_overlay = nullptr;
    Picture* m_picture = nullptr;
    Picture* m_pictureOverlay = nullptr;
    bool m_pressed = false;
};

} // namespace settings
