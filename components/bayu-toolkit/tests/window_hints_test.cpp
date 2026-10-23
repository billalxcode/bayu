// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include "ui/header_button.h"
#include "ui/layout.h"
#include "ui/path.h"
#include "ui/scroll_bar.h"
#include "ui/subsurface.h"
#include "ui/test_input.h"
#include "ui/theme.h"
#include "ui/window.h"
#include "ui/window_header.h"

using namespace bayu::ui;

TEST(SubsurfaceTest, HeadlessWindowsHaveNone)
{
    Window window;
    window.resize({200, 100});
    window.setContentView(std::make_unique<View>());
    window.show();
    Subsurface subsurface(&window);
    EXPECT_FALSE(subsurface.isValid());
    subsurface.setGeometry({0, 0, 200, 100});
    EXPECT_EQ(subsurface.geometry(), (RectF{0, 0, 200, 100}));
    EXPECT_TRUE(subsurface.beginFrame().isEmpty());
    EXPECT_FALSE(subsurface.endFrame());
    EXPECT_FALSE(subsurface.waitForFrame(0));
    EXPECT_FALSE(subsurface.makeCurrent());
    EXPECT_EQ(Subsurface::waylandDisplay(), nullptr);
    // A frame of the window still renders.
    EXPECT_GT(window.grab().width(), 0);
}

TEST(WindowHintsTest, GlassAndShapeAreAcceptedEverywhere)
{
    Window window;
    window.resize({300, 200});
    window.setContentView(std::make_unique<View>());
    window.show();
    window.setBackgroundBlur({0, 0, 300, 200}, 0);
    Path shape;
    shape.addRoundedRect({10, 120, 280, 70}, 12, 12);
    shape.addRoundedRect({200, 10, 90, 100}, 12, 12);
    window.setBackgroundBlurShape(shape);
    GlassMaterial material;
    material.blurSigma = 40;
    material.displacement = 0;
    material.chroma = 0;
    material.saturation = 0.65;
    material.tintRed = 0.07;
    material.tintGreen = 0.07;
    material.tintBlue = 0.08;
    material.tintAlpha = 0.62;
    window.setBackgroundGlass(material);
    window.setBackgroundGlass(std::nullopt);
    window.setResizeAroundCenter(true);
    window.setRestoreId("main");
    EXPECT_GT(window.grab().width(), 0);
}

// The close button's glyph, sampled at its middle stroke.
static Color closeGlyphColor(Window& window, double headerHeight)
{
    const Image image = window.grab();
    // The close glyph is a cross: its centre is inked.
    const int x = int(300 - 4 - 16);
    const int y = int(headerHeight == 44 ? 6 + 16 : 4 + 16);
    return image.pixel(x, y);
}

TEST(WindowHintsTest, HeaderDarkModeTintsTheButtonsWhateverTheTheme)
{
    const bool wasDark = Theme::current().darkMode;
    Window window;
    auto column = std::make_unique<Column>();
    WindowHeader* header = column->addChild<WindowHeader>(44);
    header->setButtonsCentered(true);
    header->setLayoutHints({.fillWidth = true});
    window.resize({300, 200});
    window.setBackgroundColor(Color::rgb(0x808080));
    window.setContentView(std::move(column));
    window.show();

    EXPECT_EQ(window.headerDarkMode(), wasDark);
    window.setHeaderDarkMode(true);
    EXPECT_TRUE(window.headerDarkMode());
    const Color dark = closeGlyphColor(window, 44);
    window.setHeaderDarkMode(false);
    EXPECT_FALSE(window.headerDarkMode());
    const Color light = closeGlyphColor(window, 44);
    // White ink on the grey for a dark header, near-black for a light one.
    EXPECT_GT(dark.r, 0x90);
    EXPECT_LT(light.r, 0x70);
    window.setHeaderDarkMode(std::nullopt);
    EXPECT_EQ(window.headerDarkMode(), wasDark);
}

TEST(WindowHintsTest, CenteredHeaderButtons)
{
    Window window;
    auto column = std::make_unique<Column>();
    WindowHeader* header = column->addChild<WindowHeader>(44);
    header->setButtonsCentered(true);
    header->setLayoutHints({.fillWidth = true});
    window.resize({300, 200});
    window.setContentView(std::move(column));
    window.show();
    window.grab();
    // Close, maximize and minimize from the end, 4 px from it, 6 px down.
    const std::string dump = window.layoutDump();
    EXPECT_NE(dump.find("logical 264 6 32 32"), std::string::npos) << dump;
    EXPECT_NE(dump.find("logical 231 6 32 32"), std::string::npos) << dump;
    EXPECT_NE(dump.find("logical 198 6 32 32"), std::string::npos) << dump;
}

TEST(WindowHintsTest, ScrollBarDarkModeOverridesTheTheme)
{
    Window window;
    window.resize({40, 200});
    window.setBackgroundColor(Color::rgb(0x000000));
    auto bar = std::make_unique<ScrollBar>(ScrollBar::Orientation::Vertical);
    ScrollBar* scrollBar = bar.get();
    window.setContentView(std::move(bar));
    window.show();
    scrollBar->setRange(100, 200, 0);
    scrollBar->setAlwaysShown(true);
    // Let the bar fade in.
    for (int frame = 0; frame < 120; ++frame)
        window.grab(uint64_t(frame) * 16666667 + 1000000000);
    scrollBar->setDarkMode(true);
    const Color light = window.grab(5000000000).pixel(4, 40);
    scrollBar->setDarkMode(false);
    const Color dark = window.grab(5000000000).pixel(4, 40);
    // A white handle shows on the black background; a black one does not.
    EXPECT_GT(light.r, 0x60);
    EXPECT_LT(dark.r, 0x10);
}

TEST(WindowHintsTest, HeaderButtonGlyphColour)
{
    Window window;
    window.resize({32, 32});
    window.setBackgroundColor(Color::rgb(0xFFFFFF));
    auto button = std::make_unique<HeaderButton>("close");
    HeaderButton* close = button.get();
    window.setContentView(std::move(button));
    window.show();
    close->setGlyphColor(Color::rgb(0xFF0000));
    const Color red = window.grab().pixel(16, 16);
    EXPECT_GT(red.r, 0xC0);
    EXPECT_LT(red.g, 0x80);
    close->setGlyphColor(std::nullopt);
    const Color tint = window.grab().pixel(16, 16);
    EXPECT_LT(std::abs(int(tint.r) - int(tint.b)), 0x20);
}
