// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "ui/geometry.h"

class SkCanvas;

namespace bayu::ui {

// An antialiased rectangle with a radius, drawn as vertex geometry: the
// corners as ceil(r·π/6) chords (3 to 18), the edges ramped across
// about a device pixel by vertices the vertex shader pushes apart, and
// triangles filled where they cover pixel centres. rect is in logical
// pixels, already offset; the border lies inside the outline.
void drawSmoothRoundedRectangle(SkCanvas* canvas, const RectF& rect, double radius, double scale, Color fill,
                               double borderWidth, Color border, float opacity);

} // namespace bayu::ui
