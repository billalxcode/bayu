// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#ifndef CONTROLS_H
#define CONTROLS_H

#include "ui/view.h"

#include <functional>
#include <optional>
#include <string>

namespace bayu::ui {
class Icon;
class Label;
class Painter;
}

// A round plate inset a tenth of its size, tinted
// with the text colour when hovered or pressed, around a glyph or a label.
class RoundButton : public bayu::ui::View
{
public:
    explicit RoundButton(double size = 32);

    void setButtonSize(double size);
    double buttonSize() const { return m_size; }
    // A glyph of the icon set, drawn at iconSize in the middle.
    void setGlyph(std::string name, double iconSize = 16);
    void setIconColor(std::optional<bayu::ui::Color> color);
    // A label in the middle instead of a glyph, which the button is as wide
    // as, plus `padding` on each side, unless laid out otherwise.
    bayu::ui::Label *setLabel(std::string text, double padding = 8);
    bayu::ui::Label *label() const { return m_label; }
    void setBackgroundColor(bayu::ui::Color color);
    // The pressed plate while set, as while the button's menu is open.
    void setChecked(bool checked);

    bayu::ui::SizeF sizeHint() const override;
    bool acceptsPointer() const override { return true; }

    // The click and where it was, in window coordinates.
    std::function<void()> onClicked;
    std::function<void(bayu::ui::PointF windowPosition)> onClickedAt;

protected:
    void layout() override;
    void paint(bayu::ui::Painter &painter) override;
    void enabledChanged() override;
    void pointerEnter() override;
    void pointerLeave() override;
    void pointerPress(const bayu::ui::PointerEvent &event) override;
    void pointerRelease(const bayu::ui::PointerEvent &event) override;

private:
    double iconSize() const;

    double m_size;
    double m_iconSize = -1;
    double m_padding = 8;
    bayu::ui::Icon *m_icon = nullptr;
    bayu::ui::Label *m_label = nullptr;
    std::optional<bayu::ui::Color> m_iconColor;
    bayu::ui::Color m_background{0, 0, 0, 0};
    bool m_checked = false;
    bool m_hovered = false;
    bool m_pressed = false;
};

// A text button, plain or flat (the accent's tint, its text in the accent).
class TextButton : public bayu::ui::View
{
public:
    explicit TextButton(std::string text = {}, bool flat = false);

    void setText(std::string text);
    void setFlat(bool flat);

    bayu::ui::SizeF sizeHint() const override;
    bool acceptsPointer() const override { return true; }
    bool acceptsFocus() const override { return true; }

    std::function<void()> onClicked;

protected:
    void layout() override;
    void paint(bayu::ui::Painter &painter) override;
    void enabledChanged() override;
    void pointerEnter() override;
    void pointerLeave() override;
    void pointerPress(const bayu::ui::PointerEvent &event) override;
    void pointerRelease(const bayu::ui::PointerEvent &event) override;
    bool keyPress(const bayu::ui::KeyEvent &event) override;

private:
    bayu::ui::Label *m_label;
    bool m_flat;
    bool m_hovered = false;
    bool m_pressed = false;
};

// The menus' surface colour, rounded, a hairline edge
// and a soft shadow under the plate; its content inset by `padding`.
class FloatingCard : public bayu::ui::View
{
public:
    static constexpr double padding = 6;

    void setRadius(double radius) { m_radius = radius; }
    bayu::ui::RectF visualRect() const override;
    bool acceptsPointer() const override { return true; }

protected:
    void paint(bayu::ui::Painter &painter) override;

private:
    double m_radius = 20;
};

// One device pixel tall, snapped as a RoundedRectangle of height 1 / dpr.
void drawHairline(bayu::ui::Painter &painter, double x, double y, double width, bayu::ui::Color color);

#endif
