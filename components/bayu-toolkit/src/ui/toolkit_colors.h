// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "ui/color_mixer.h"

namespace bayu::ui {

// The toolkit's recipes, which every provider starts from.
void addToolkitColors(ColorMixer& mixer, const Theme& theme);

} // namespace bayu::ui
