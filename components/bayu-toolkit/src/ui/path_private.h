// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "include/core/SkPath.h"
#include "base/transform.h"
#include "ui/path.h"

namespace bayu::ui {

struct PathAccess
{
    // Points mapped in double precision before they go to single precision.
    static SkPath toSkPath(const Path& path);
    static SkPath toSkPath(const Path& path, double scale, PointF offset);
    static SkPath toSkPath(const Path& path, const base::AffineTransform& transform);
    static Path fromSkPath(const SkPath& path);

private:
    template<typename Map>
    static SkPath convert(const Path& path, Map map);
};

} // namespace bayu::ui
