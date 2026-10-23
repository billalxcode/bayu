// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "include/core/SkPath.h"
#include "include/core/SkRect.h"

namespace bayu::ui {

// A rounded rect whose corners are circular arcs, or the continuous corners
// the compositor gives window silhouettes: a cubic superellipse meeting the
// straight edges with zero curvature, over 1.5 times the nominal radius. Keep
// it in step with FishUI's roundedpath.cpp and bayu-compositor's
// rounded_shape.h.
SkPath roundedRectPath(const SkRect& rect, double radius, bool continuous);

} // namespace bayu::ui
