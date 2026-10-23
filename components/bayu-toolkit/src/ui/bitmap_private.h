// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "include/core/SkColorType.h"
#include "include/core/SkPixmap.h"
#include "ui/bitmap.h"

namespace bayu::ui {

SkColorType skColorType(Bitmap::Format format);
// Read-only view of the pixels.
SkPixmap skPixmap(const Bitmap& bitmap);

} // namespace bayu::ui
