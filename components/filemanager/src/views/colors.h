// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>

#include "ui/color_provider.h"

namespace fm {

// The file manager's own colours, after the toolkit's; colors.cpp gives
// them for each theme.
enum class FmColor : uint16_t {
    ChooserSidebarBackground = uint16_t(bayu::ui::ColorId::ApplicationFirst),
    AlternateRow,
    RubberBandFill,
    RubberBandBorder,
    PathEditorText,
    PathEditorBackground,
    SearchScopeSelected,
};

void addFmColors(bayu::ui::ColorMixer& mixer, const bayu::ui::Theme& theme);

// Added before main by every program that names one of the colours.
[[maybe_unused]] inline const bool fmColorsAdded = (bayu::ui::ColorProvider::addMixer(addFmColors), true);

} // namespace fm
