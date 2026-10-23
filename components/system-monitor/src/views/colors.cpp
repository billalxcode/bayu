// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "colors.h"

#include "ui/theme.h"

using namespace bayu::ui;

void addMonitorColors(ColorMixer& mixer, const Theme& theme)
{
    mixer[MonitorColor::MutedPlate] =
        theme.darkMode ? Color::rgb(0xFFFFFF).withAlpha(0.05) : Color::rgb(0x000000).withAlpha(0.03);
}
