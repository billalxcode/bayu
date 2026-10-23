// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "ui/button.h"

#include <algorithm>
#include <utility>

#include "ui/color_provider.h"
#include "ui/keys.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/theme.h"

namespace bayu::ui {

static constexpr double kMinimumWidth = 64;
static constexpr double kMinimumHeight = 32;

Button::Button(std::string text)
    : m_label(addChild<Label>(std::move(text)))
{
    m_label->setTextAlignment(Label::TextAlignment::Center);
    m_label->setVerticallyCentered(true);
    // A button colours its icon and label instead of fading them.
    m_label->setFadesWhenDisabled(false);
}

Button::~Button() = default;

void Button::setText(std::string text)
{
    m_label->setText(std::move(text));
}

SizeF Button::sizeHint() const
{
    const Theme& theme = Theme::current();
    const SizeF text = m_label->sizeHint();
    return {std::max(kMinimumWidth, text.width + 2 * theme.largeSpacing),
            std::max(kMinimumHeight, text.height + 2 * theme.smallSpacing)};
}

void Button::layout()
{
    const Theme& theme = Theme::current();
    const double padding = theme.largeSpacing;
    m_label->setGeometry(localRect().adjusted(padding, 0, -padding, 0));
    m_label->setColor(isEnabled() ? std::nullopt : std::optional<Color>(theme.disabledText));
}

void Button::enabledChanged()
{
    requestLayout();
    requestPaint();
}

void Button::paint(Painter& painter)
{
    const Theme& theme = Theme::current();
    const bool enabled = isEnabled();
    ColorId background = ColorId::ButtonBackground;
    if (enabled && m_pressed)
        background = ColorId::ButtonBackgroundPressed;
    else if (enabled && m_hovered)
        background = ColorId::ButtonBackgroundHovered;
    painter.drawRoundedRect(localRect(), {theme.mediumRadius, 0, themeCorners()}, color(background),
                            1 / painter.scale(),
                            hasVisibleFocus() ? color(ColorId::ButtonFocusRing) : Color{0, 0, 0, 0});
}

void Button::pointerEnter()
{
    m_hovered = true;
    requestPaint();
}

void Button::pointerLeave()
{
    m_hovered = false;
    requestPaint();
}

void Button::pointerPress(const PointerEvent& event)
{
    if (event.button != MouseButton::Left)
        return;
    m_pressed = true;
    requestPaint();
}

void Button::pointerRelease(const PointerEvent& event)
{
    if (!m_pressed)
        return;
    m_pressed = false;
    requestPaint();
    if (localRect().contains(event.position) && onClicked)
        onClicked();
}

bool Button::keyPress(const KeyEvent& event)
{
    if (event.modifiers != NoModifier)
        return false;
    if (event.key == key::Space) {
        m_pressed = true;
        requestPaint();
        return true;
    }
    if (event.key == key::Return || event.key == key::KeypadEnter) {
        if (onClicked)
            onClicked();
        return true;
    }
    return false;
}

bool Button::keyRelease(const KeyEvent& event)
{
    if (event.key != key::Space || !m_pressed)
        return false;
    m_pressed = false;
    requestPaint();
    if (onClicked)
        onClicked();
    return true;
}

} // namespace bayu::ui
