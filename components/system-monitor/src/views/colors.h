// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>

#include "ui/color_provider.h"

// The system monitor's own colours, after the toolkit's; colors.cpp gives
// them for each theme.
enum class MonitorColor : uint16_t {
    // A plate that is there but takes no part: a disabled button, the
    // search scope's track.
    MutedPlate = uint16_t(bayu::ui::ColorId::ApplicationFirst),
};

void addMonitorColors(bayu::ui::ColorMixer& mixer, const bayu::ui::Theme& theme);

// Added before main by every program that names one of the colours.
[[maybe_unused]] inline const bool monitorColorsAdded = (bayu::ui::ColorProvider::addMixer(addMonitorColors), true);
