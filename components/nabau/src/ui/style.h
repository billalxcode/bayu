// SPDX-License-Identifier: GPL-3.0-or-later
// The installer's look: metrics, colours and type, in one place.

#pragma once

#include <cstdint>
#include <string>

#include "ui/color_id.h"
#include "ui/color_mixer.h"
#include "ui/geometry.h"

namespace bayu::ui {
class Label;
class TextField;
}

namespace installer::style {

// The typeface of the whole installer: a monospaced one, as designed.
// Text the family lacks falls back to the desktop font.
inline constexpr const char* kFontFamily = "Red Hat Mono";

// Window and columns, logical pixels.
inline constexpr double windowWidth = 1120;
inline constexpr double windowHeight = 728;
inline constexpr double sidebarWidth = 280;
inline constexpr double pageMarginLeft = 44;
inline constexpr double pageMarginRight = 44;
inline constexpr double pageMarginTop = 40;
inline constexpr double columnSpacing = 20;
inline constexpr double footerHeight = 72;

// Type sizes.
inline constexpr double eyebrowSize = 11;
inline constexpr double titleSize = 26;
inline constexpr double bodySize = 14;
inline constexpr double rowTitleSize = 14;
inline constexpr double rowDetailSize = 12;
inline constexpr double sectionSize = 11;

// Shapes.
inline constexpr double cardRadius = 12;
inline constexpr double rowRadius = 10;
inline constexpr double tileRadius = 8;
inline constexpr double buttonRadius = 8;
inline constexpr double buttonHeight = 40;
inline constexpr double fieldHeight = 40;
inline constexpr double listRowHeight = 52;
inline constexpr double optionCardHeight = 64;

// The installer's colours, on top of the toolkit's.
enum class Role : uint16_t {
    // The content pane, lighter than the sidebar.
    Content = uint16_t(bayu::ui::ColorId::ApplicationFirst),
    Sidebar,
    // Cards and fields.
    Card,
    CardBorder,
    // The selected row or card: a soft accent fill and an accent edge.
    SelectedFill,
    SelectedBorder,
    // The current step in the sidebar.
    StepCurrent,
    SecondaryText,
    TertiaryText,
    Success,
    Warning,
    WarningFill,
    Danger,
    InfoFill,
    IconTile,
    LogBackground,
    LogText,
};

// Registers the colours and the glyphs; once, before any window.
void install();

bayu::ui::ColorRecipe color(Role role);

// A label in the installer's typeface.
void setFont(bayu::ui::Label* label, double size, int weight = 400);
void setFont(bayu::ui::TextField* field, double size, int weight = 400);

// "1.0 TB", "500 GB", "16 GB" as disk makers count.
std::string formatSize(uint64_t bytes);
// "6 m 12 s", "3 min 10 s".
std::string formatDuration(int seconds);
// "1,842".
std::string formatCount(int64_t count);

} // namespace installer::style
