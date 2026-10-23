// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <ui/color_provider.h>

#include <cstdint>

// The calendar's own colours, after the toolkit's; colors.cpp gives them
// for each theme. Events keep their calendars' colours.
enum class CalendarColor : uint16_t {
    WeekendColumn = uint16_t(bayu::ui::ColorId::ApplicationFirst),
    SelectedDay,
    MiniMonthRange,
    MiniMonthSelected,
    SearchResultSelected,
};

void addCalendarColors(bayu::ui::ColorMixer& mixer, const bayu::ui::Theme& theme);

// Added before main by every program that names one of the colours.
[[maybe_unused]] inline const bool calendarColorsAdded =
    (bayu::ui::ColorProvider::addMixer(addCalendarColors), true);
