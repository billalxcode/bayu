// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <functional>
#include <string>

#include "ui/view.h"

namespace bayu::ui {

class Label;

class Button : public View
{
public:
    explicit Button(std::string text = {});
    ~Button() override;

    void setText(std::string text);

    SizeF sizeHint() const override;
    bool acceptsPointer() const override { return true; }
    bool acceptsFocus() const override { return true; }

    std::function<void()> onClicked;

protected:
    void layout() override;
    void paint(Painter& painter) override;
    void enabledChanged() override;

    void pointerEnter() override;
    void pointerLeave() override;
    void pointerPress(const PointerEvent& event) override;
    void pointerRelease(const PointerEvent& event) override;
    bool keyPress(const KeyEvent& event) override;
    bool keyRelease(const KeyEvent& event) override;

private:
    Label* m_label;
    bool m_hovered = false;
    bool m_pressed = false;
};

} // namespace bayu::ui
