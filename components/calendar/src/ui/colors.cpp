// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "colors.h"

#include <ui/theme.h>

using namespace bayu::ui;

void addCalendarColors(ColorMixer& mixer, const Theme& theme)
{
    const bool dark = theme.darkMode;

    mixer[CalendarColor::WeekendColumn] = withAlpha(ColorId::Text, dark ? 0.03 : 0.02);
    mixer[CalendarColor::SelectedDay] = withAlpha(ColorId::Highlight, dark ? 0.08 : 0.04);
    mixer[CalendarColor::MiniMonthRange] = withAlpha(ColorId::Highlight, dark ? 0.15 : 0.09);
    mixer[CalendarColor::MiniMonthSelected] = withAlpha(ColorId::Highlight, dark ? 0.25 : 0.15);
    mixer[CalendarColor::SearchResultSelected] = withAlpha(ColorId::Highlight, dark ? 0.25 : 0.12);
}
