// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "color.h"

#include <ui/geometry.h>
#include <ui/theme.h>

#include <cmath>

// Colours and sizes the calendar's views share.
namespace Style
{
using bayu::ui::Color;
using bayu::ui::Theme;

// Point sizes in logical pixels, four thirds of a pixel a point.
inline double points(double size)
{
    return size * 4 / 3;
}

inline Color withAlpha(Color color, double alpha)
{
    return color.withAlpha(alpha * color.a / 255.0);
}

inline Color toColor(Rgb rgb)
{
    return Color{rgb.r, rgb.g, rgb.b, 255};
}

// Controls on the calendar's surface get a track, as the segmented control has on cards.
inline Color controlTint()
{
    return Theme::current().darkMode ? Color{255, 255, 255, 15} : Color{0, 0, 0, 10};
}

inline Color lineColor()
{
    return withAlpha(Theme::current().text, 0.07);
}

inline Color separatorColor()
{
    return withAlpha(Theme::current().text, 0.08);
}

inline const Color& red()
{
    static const Color color = bayu::ui::accent::red;
    return color;
}

// On the device pixel grid of the scale.
inline double snap(double value, double scale)
{
    return std::round(value * scale) / scale;
}

// Where an item of this length is centred in its parent: each half rounded
// on its own, so odd lengths lean the same way everywhere.
inline double centred(double parent, double item)
{
    return std::round(parent / 2) - std::round(item / 2);
}
}
