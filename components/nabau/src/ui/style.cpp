// SPDX-License-Identifier: GPL-3.0-or-later

#include "ui/style.h"

#include <cmath>
#include <cstdio>

#include "ui/color_provider.h"
#include "ui/icon.h"
#include "ui/label.h"
#include "ui/text_field.h"
#include "ui/theme.h"

using namespace bayu::ui;

namespace installer::style {

namespace {

// Glyphs from Lucide (https://lucide.dev, ISC license), as the toolkit's own:
// a 24 px box stroked in the current colour.
struct Glyph
{
    const char* name;
    const char* body;
};

constexpr Glyph kGlyphs[] = {
    {"hard-drive", R"(<line x1="22" x2="2" y1="12" y2="12"/><path d="M5.45 5.11 2 12v6a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2v-6l-3.45-6.89A2 2 0 0 0 16.76 4H7.24a2 2 0 0 0-1.79 1.11z"/><line x1="6" x2="6.01" y1="16" y2="16"/><line x1="10" x2="10.01" y1="16" y2="16"/>)"},
    {"usb", R"(<circle cx="10" cy="7" r="1"/><circle cx="4" cy="20" r="1"/><path d="M4.7 19.3 19 5"/><path d="m21 3-3 1 2 2Z"/><path d="M9.26 7.68 5 12l2 5"/><path d="m10 14 5 2 3.5-3.5"/><path d="m18 12 1-1 1 1-1 1Z"/>)"},
    {"wand", R"(<path d="m21.64 3.64-1.28-1.28a1.21 1.21 0 0 0-1.72 0L2.36 18.64a1.21 1.21 0 0 0 0 1.72l1.28 1.28a1.2 1.2 0 0 0 1.72 0L21.64 5.36a1.2 1.2 0 0 0 0-1.72"/><path d="m14 7 3 3"/><path d="M5 6v4"/><path d="M19 14v4"/><path d="M10 2v2"/><path d="M7 8H3"/><path d="M21 16h-4"/><path d="M11 3H9"/>)"},
    {"sliders", R"(<line x1="21" x2="14" y1="4" y2="4"/><line x1="10" x2="3" y1="4" y2="4"/><line x1="21" x2="12" y1="12" y2="12"/><line x1="8" x2="3" y1="12" y2="12"/><line x1="21" x2="16" y1="20" y2="20"/><line x1="12" x2="3" y1="20" y2="20"/><line x1="14" x2="14" y1="2" y2="6"/><line x1="8" x2="8" y1="10" y2="14"/><line x1="16" x2="16" y1="18" y2="22"/>)"},
    {"terminal", R"(<polyline points="4 17 10 11 4 5"/><line x1="12" x2="20" y1="19" y2="19"/>)"},
    {"monitor", R"(<rect width="20" height="14" x="2" y="3" rx="2"/><line x1="8" x2="16" y1="21" y2="21"/><line x1="12" x2="12" y1="17" y2="21"/>)"},
    {"layout-grid", R"(<rect width="7" height="7" x="3" y="3" rx="1"/><rect width="7" height="7" x="14" y="3" rx="1"/><rect width="7" height="7" x="14" y="14" rx="1"/><rect width="7" height="7" x="3" y="14" rx="1"/>)"},
    {"server", R"(<rect width="20" height="8" x="2" y="2" rx="2" ry="2"/><rect width="20" height="8" x="2" y="14" rx="2" ry="2"/><line x1="6" x2="6.01" y1="6" y2="6"/><line x1="6" x2="6.01" y1="18" y2="18"/>)"},
    {"box", R"(<path d="M21 8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73l7 4a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16Z"/><path d="m3.3 7 8.7 5 8.7-5"/><path d="M12 22V12"/>)"},
    {"layers", R"(<path d="m12.83 2.18a2 2 0 0 0-1.66 0L2.6 6.08a1 1 0 0 0 0 1.83l8.58 3.91a2 2 0 0 0 1.66 0l8.58-3.9a1 1 0 0 0 0-1.83Z"/><path d="m22 17.65-9.17 4.16a2 2 0 0 1-1.66 0L2 17.65"/><path d="m22 12.65-9.17 4.16a2 2 0 0 1-1.66 0L2 12.65"/>)"},
    {"padlock", R"(<rect width="18" height="11" x="3" y="11" rx="2" ry="2"/><path d="M7 11V7a5 5 0 0 1 10 0v4"/>)"},
    {"key", R"(<path d="M2.586 17.414A2 2 0 0 0 2 18.828V21a1 1 0 0 0 1 1h3a1 1 0 0 0 1-1v-1a1 1 0 0 1 1-1h1a1 1 0 0 0 1-1v-1a1 1 0 0 1 1-1h.172a2 2 0 0 0 1.414-.586l.814-.814a6.5 6.5 0 1 0-4-4z"/><circle cx="16.5" cy="7.5" r=".5" fill="currentColor"/>)"},
    {"eye", R"(<path d="M2.062 12.348a1 1 0 0 1 0-.696 10.75 10.75 0 0 1 19.876 0 1 1 0 0 1 0 .696 10.75 10.75 0 0 1-19.876 0"/><circle cx="12" cy="12" r="3"/>)"},
    {"eye-off", R"(<path d="M10.733 5.076a10.744 10.744 0 0 1 11.205 6.575 1 1 0 0 1 0 .696 10.747 10.747 0 0 1-1.444 2.49"/><path d="M14.084 14.158a3 3 0 0 1-4.242-4.242"/><path d="M17.479 17.499a10.75 10.75 0 0 1-15.417-5.151 1 1 0 0 1 0-.696 10.75 10.75 0 0 1 4.446-5.143"/><path d="m2 2 20 20"/>)"},
    {"shield", R"(<path d="M20 13c0 5-3.5 7.5-7.66 8.95a1 1 0 0 1-.67-.01C7.5 20.5 4 18 4 13V6a1 1 0 0 1 1-1c2 0 4.5-1.2 6.24-2.72a1.17 1.17 0 0 1 1.52 0C14.51 3.81 17 5 19 5a1 1 0 0 1 1 1z"/>)"},
    {"shield-check", R"(<path d="M20 13c0 5-3.5 7.5-7.66 8.95a1 1 0 0 1-.67-.01C7.5 20.5 4 18 4 13V6a1 1 0 0 1 1-1c2 0 4.5-1.2 6.24-2.72a1.17 1.17 0 0 1 1.52 0C14.51 3.81 17 5 19 5a1 1 0 0 1 1 1z"/><path d="m9 12 2 2 4-4"/>)"},
    {"globe", R"(<circle cx="12" cy="12" r="10"/><path d="M12 2a14.5 14.5 0 0 0 0 20 14.5 14.5 0 0 0 0-20"/><path d="M2 12h20"/>)"},
    {"ethernet", R"(<path d="m15 20 3-3h2a2 2 0 0 0 2-2V6a2 2 0 0 0-2-2H4a2 2 0 0 0-2 2v9a2 2 0 0 0 2 2h2l3 3z"/><path d="M6 8v1"/><path d="M10 8v1"/><path d="M14 8v1"/><path d="M18 8v1"/>)"},
    {"wifi", R"(<path d="M12 20h.01"/><path d="M2 8.82a15 15 0 0 1 20 0"/><path d="M5 12.859a10 10 0 0 1 14 0"/><path d="M8.5 16.429a5 5 0 0 1 7 0"/>)"},
    {"smartphone", R"(<rect width="14" height="20" x="5" y="2" rx="2" ry="2"/><path d="M12 18h.01"/>)"},
    {"plus", R"(<path d="M5 12h14"/><path d="M12 5v14"/>)"},
    {"arrow-left", R"(<path d="m12 19-7-7 7-7"/><path d="M19 12H5"/>)"},
    {"arrow-right", R"(<path d="M5 12h14"/><path d="m12 5 7 7-7 7"/>)"},
    {"drive-download", R"(<path d="M12 2v8"/><path d="m16 6-4 4-4-4"/><rect width="20" height="8" x="2" y="14" rx="2"/><path d="M6 18h.01"/><path d="M10 18h.01"/>)"},
    {"power", R"(<path d="M12 2v10"/><path d="M18.4 6.6a9 9 0 1 1-12.77.04"/>)"},
    {"menu", R"(<line x1="4" x2="20" y1="12" y2="12"/><line x1="4" x2="20" y1="6" y2="6"/><line x1="4" x2="20" y1="18" y2="18"/>)"},
    {"x", R"(<path d="M18 6 6 18"/><path d="m6 6 12 12"/>)"},
    {"alert", R"(<path d="m21.73 18-8-14a2 2 0 0 0-3.48 0l-8 14A2 2 0 0 0 4 21h16a2 2 0 0 0 1.73-3"/><path d="M12 9v4"/><path d="M12 17h.01"/>)"},
    {"languages", R"(<path d="m5 8 6 6"/><path d="m4 14 6-6 2-3"/><path d="M2 5h12"/><path d="M7 2h1"/><path d="m22 22-5-10-5 10"/><path d="M14 18h6"/>)"},
    {"user", R"(<path d="M19 21v-2a4 4 0 0 0-4-4H9a4 4 0 0 0-4 4v2"/><circle cx="12" cy="7" r="4"/>)"},
    {"package", R"(<path d="M11 21.73a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16V8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73z"/><path d="M12 22V12"/><polyline points="3.29 7 12 12 20.71 7"/><path d="m7.5 4.27 9 5.15"/>)"},
    {"timer", R"(<line x1="10" x2="14" y1="2" y2="2"/><line x1="12" x2="15" y1="14" y2="11"/><circle cx="12" cy="14" r="8"/>)"},
    {"scroll", R"(<path d="M15 12h-5"/><path d="M15 8h-5"/><path d="M19 17V5a2 2 0 0 0-2-2H4"/><path d="M8 21h12a2 2 0 0 0 2-2v-1a1 1 0 0 0-1-1H11a1 1 0 0 0-1 1v1a2 2 0 1 1-4 0V5a2 2 0 1 0-4 0v2a1 1 0 0 0 1 1h3"/>)"},
    {"bulb", R"(<path d="M15 14c.2-1 .7-1.7 1.5-2.5 1-.9 1.5-2.2 1.5-3.5A6 6 0 0 0 6 8c0 1 .2 2.2 1.5 3.5.7.7 1.3 1.5 1.5 2.5"/><path d="M9 18h6"/><path d="M10 22h4"/>)"},
    {"trash", R"(<path d="M3 6h18"/><path d="M19 6v14c0 1-1 2-2 2H7c-1 0-2-1-2-2V6"/><path d="M8 6V4c0-1 1-2 2-2h4c1 0 2 1 2 2v2"/><line x1="10" x2="10" y1="11" y2="17"/><line x1="14" x2="14" y1="11" y2="17"/>)"},
    {"pencil", R"(<path d="M21.174 6.812a1 1 0 0 0-3.986-3.987L3.842 16.174a2 2 0 0 0-.5.83l-1.321 4.352a.5.5 0 0 0 .623.622l4.353-1.32a2 2 0 0 0 .83-.497z"/><path d="m15 5 4 4"/>)"},
    {"resize", R"(<path d="m18 8 4 4-4 4"/><path d="M2 12h20"/><path d="m6 8-4 4 4 4"/>)"},
    {"undo", R"(<path d="M9 14 4 9l5-5"/><path d="M4 9h10.5a5.5 5.5 0 0 1 5.5 5.5a5.5 5.5 0 0 1-5.5 5.5H11"/>)"},
    {"rotate", R"(<path d="M21 12a9 9 0 1 1-9-9c2.52 0 4.93 1 6.74 2.74L21 8"/><path d="M21 3v5h-5"/>)"},
    {"clock", R"(<circle cx="12" cy="12" r="10"/><polyline points="12 6 12 12 16 14"/>)"},
    {"circle-check", R"(<circle cx="12" cy="12" r="10"/><path d="m9 12 2 2 4-4"/>)"},
    {"windows", R"(<path d="M3 5.5 10.5 4.5v7H3z"/><path d="M12 4.3 21 3v8.5h-9z"/><path d="M3 12.5h7.5v7L3 18.5z"/><path d="M12 12.5h9V21l-9-1.3z"/>)"},
    {"linux", R"(<path d="M12 3c-2.2 0-3.5 1.8-3.5 4.2 0 1.7.6 2.6-.4 4.3C6.9 13.6 5 15.7 5 18c0 1.7 1.5 3 3.5 3h7c2 0 3.5-1.3 3.5-3 0-2.3-1.9-4.4-3.1-6.5-1-1.7-.4-2.6-.4-4.3C15.5 4.8 14.2 3 12 3z"/>)"},
};

void mix(ColorMixer& mixer, const Theme& theme)
{
    const bool dark = theme.darkMode;
    const Color accent = theme.highlight;
    mixer[Role::Content] = dark ? theme.background.withAlpha(0.92) : Color::rgb(0xFBFBFC).withAlpha(0.94);
    mixer[Role::Sidebar] =
        withAlpha(dark ? lighter(ColorId::Background, 1.5) : darker(ColorId::Background, 1.05), 0.7);
    mixer[Role::Card] = dark ? Color::rgb(0x2A2A2C) : Color::rgb(0xFFFFFF);
    mixer[Role::CardBorder] = dark ? Color::rgb(0x3A3A3D) : Color::rgb(0xE6E7EA);
    mixer[Role::SelectedFill] = accent.withAlpha(dark ? 0.2 : 0.12);
    mixer[Role::SelectedBorder] = accent;
    mixer[Role::StepCurrent] = accent.withAlpha(dark ? 0.24 : 0.16);
    mixer[Role::SecondaryText] = dark ? Color::rgb(0xA0A0A8) : Color::rgb(0x64646E);
    mixer[Role::TertiaryText] = dark ? Color::rgb(0x76767E) : Color::rgb(0x9A9AA3);
    mixer[Role::Success] = dark ? Color::rgb(0x3CCB6A) : Color::rgb(0x1F9D55);
    mixer[Role::Warning] = dark ? Color::rgb(0xF2B44B) : Color::rgb(0xB7790B);
    mixer[Role::WarningFill] = dark ? Color::rgb(0x3A3020) : Color::rgb(0xFBF4E8);
    mixer[Role::Danger] = accent::red;
    mixer[Role::InfoFill] = accent.withAlpha(dark ? 0.18 : 0.1);
    mixer[Role::IconTile] = dark ? Color::rgb(0x3A3A3D) : Color::rgb(0xEEEFF2);
    mixer[Role::LogBackground] = dark ? Color::rgb(0x202022) : Color::rgb(0xF2F3F4);
    mixer[Role::LogText] = dark ? Color::rgb(0x3CCB6A) : Color::rgb(0x23A060);
}

} // namespace

void install()
{
    for (const Glyph& glyph : kGlyphs) {
        Icon::registerGlyph(glyph.name,
                            std::string("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"24\" height=\"24\" "
                                        "viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" "
                                        "stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">")
                                + glyph.body + "</svg>");
    }
    ColorProvider::addMixer(mix);
}

ColorRecipe color(Role role)
{
    return ColorRecipe(role);
}

void setFont(Label* label, double size, int weight)
{
    label->setFontSize(size);
    label->setWeight(weight);
}

void setFont(TextField* field, double size, int weight)
{
    field->setFontSize(size);
    field->setFontWeight(weight);
}

std::string formatSize(uint64_t bytes)
{
    static const char* units[] = {"B", "kB", "MB", "GB", "TB", "PB"};
    double value = double(bytes);
    int unit = 0;
    while (value >= 1000 && unit < 5) {
        value /= 1000;
        ++unit;
    }
    char text[32];
    if (unit == 0)
        snprintf(text, sizeof(text), "%llu B", static_cast<unsigned long long>(bytes));
    else if (value < 10)
        snprintf(text, sizeof(text), "%.1f %s", value, units[unit]);
    else
        snprintf(text, sizeof(text), "%.0f %s", value, units[unit]);
    return text;
}

std::string formatDuration(int seconds)
{
    char text[32];
    if (seconds < 60)
        snprintf(text, sizeof(text), "%d s", seconds);
    else if (seconds < 3600)
        snprintf(text, sizeof(text), "%d m %d s", seconds / 60, seconds % 60);
    else
        snprintf(text, sizeof(text), "%d h %d m", seconds / 3600, seconds / 60 % 60);
    return text;
}

std::string formatCount(int64_t count)
{
    std::string digits = std::to_string(count < 0 ? -count : count);
    std::string out;
    for (size_t i = 0; i < digits.size(); ++i) {
        if (i && (digits.size() - i) % 3 == 0)
            out += ',';
        out += digits[i];
    }
    return count < 0 ? "-" + out : out;
}

} // namespace installer::style
