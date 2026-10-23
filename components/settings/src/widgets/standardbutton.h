// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <functional>
#include <optional>
#include <string>

#include "ui/view.h"

namespace bayu::ui {
class Label;
}

namespace settings {

// A full-width button standing alone between cards: a flat plate that only
// darkens while pressed.
class StandardButton : public bayu::ui::View
{
public:
    explicit StandardButton(std::string text = {});

    void setText(std::string text);
    // Unset: #363636 in dark mode, white otherwise.
    void setBackgroundColor(std::optional<bayu::ui::Color> color);

    bayu::ui::SizeF sizeHint() const override;
    bool acceptsPointer() const override { return true; }
    bool acceptsFocus() const override { return true; }

    std::function<void()> onClicked;

protected:
    void layout() override;
    void paint(bayu::ui::Painter& painter) override;
    void pointerPress(const bayu::ui::PointerEvent& event) override;
    void pointerRelease(const bayu::ui::PointerEvent& event) override;
    bool keyPress(const bayu::ui::KeyEvent& event) override;

private:
    bayu::ui::Color backgroundColor() const;

    bayu::ui::Label* m_label;
    std::optional<bayu::ui::Color> m_background;
    bool m_pressed = false;
};

} // namespace settings
