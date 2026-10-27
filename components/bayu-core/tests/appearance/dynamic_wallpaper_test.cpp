// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include <memory>

#include "base/event_loop.h"
#include "base/file.h"
#include "base/settings.h"
#include "dynamicwallpaper.h"
#include "scheduleutils.h"
#include "wallpapermanager.h"

using namespace bayu::base;
using namespace ScheduleUtils;
using namespace std::chrono;

static DateTime at(const char *zoneId, int hour, int minute = 0)
{
    return ScheduleUtils::at(year(2026) / October / 2, timeOfDay(hour, minute), zone(zoneId));
}

TEST(DynamicWallpaperTimes, FollowTheSunWhereTheUserIs)
{
    // Jakarta, WIB: the sun rises at about 5:35 and sets at about 17:45.
    const auto jakarta = DynamicWallpaper::times(at("Asia/Jakarta", 12));
    EXPECT_NEAR(jakarta[DynamicWallpaper::Morning], 5 * 60 + 5, 15);
    EXPECT_NEAR(jakarta[DynamicWallpaper::Day], 8 * 60 + 35, 15);
    EXPECT_NEAR(jakarta[DynamicWallpaper::Evening], 15 * 60 + 45, 15);
    EXPECT_NEAR(jakarta[DynamicWallpaper::Night], 18 * 60 + 15, 15);

    // Jayapura, WIT, lies further east in its zone than Jakarta in its own:
    // its clocks see the sun earlier.
    const auto jayapura = DynamicWallpaper::times(at("Asia/Jayapura", 12));
    EXPECT_LT(jayapura[DynamicWallpaper::Morning], jakarta[DynamicWallpaper::Morning]);
    EXPECT_LT(jayapura[DynamicWallpaper::Night], jakarta[DynamicWallpaper::Night]);
}

TEST(DynamicWallpaperTimes, FixedWhereTheLocationIsUnknown)
{
    const auto times = DynamicWallpaper::times(at("UTC", 12));
    EXPECT_EQ(times[DynamicWallpaper::Morning], 5 * 60 + 30);
    EXPECT_EQ(times[DynamicWallpaper::Day], 9 * 60);
    EXPECT_EQ(times[DynamicWallpaper::Evening], 16 * 60);
    EXPECT_EQ(times[DynamicWallpaper::Night], 18 * 60 + 30);
}

TEST(DynamicWallpaperPhase, ByTheTimeOfDay)
{
    EXPECT_EQ(DynamicWallpaper::phase(at("UTC", 0)), DynamicWallpaper::Night);
    EXPECT_EQ(DynamicWallpaper::phase(at("UTC", 5, 29)), DynamicWallpaper::Night);
    EXPECT_EQ(DynamicWallpaper::phase(at("UTC", 5, 30)), DynamicWallpaper::Morning);
    EXPECT_EQ(DynamicWallpaper::phase(at("UTC", 8, 59)), DynamicWallpaper::Morning);
    EXPECT_EQ(DynamicWallpaper::phase(at("UTC", 9)), DynamicWallpaper::Day);
    EXPECT_EQ(DynamicWallpaper::phase(at("UTC", 16)), DynamicWallpaper::Evening);
    EXPECT_EQ(DynamicWallpaper::phase(at("UTC", 18, 29)), DynamicWallpaper::Evening);
    EXPECT_EQ(DynamicWallpaper::phase(at("UTC", 18, 30)), DynamicWallpaper::Night);
    EXPECT_EQ(DynamicWallpaper::phase(at("UTC", 23, 59)), DynamicWallpaper::Night);
}

class WallpaperManagerTest : public testing::Test
{
protected:
    EventLoop loop;
    file::TemporaryDirectory directory {"/tmp", "dynamic-wallpaper-test"};
    DateTime now = at("UTC", 12);

    void SetUp() override
    {
        ASSERT_TRUE(directory.isValid());
        for (const char *name : {"morning.jpg", "day.jpg", "evening.jpg", "night.jpg"})
            ASSERT_TRUE(file::writeAtomically(set() + "/" + name, "jpg"));
        ASSERT_TRUE(DynamicWallpaper::isSet(set()));
    }

    std::string set() const { return directory.path() + "/sunny"; }

    std::unique_ptr<WallpaperManager> start()
    {
        m_settings = std::make_unique<Settings>(directory.path() + "/services.conf");
        return std::make_unique<WallpaperManager>(*m_settings, [this] {
            return now;
        });
    }

private:
    std::unique_ptr<Settings> m_settings;
};

TEST_F(WallpaperManagerTest, ShowsThePictureForTheTimeAndKeepsTheSet)
{
    auto wallpapers = start();
    wallpapers->setWallpaper(set());
    EXPECT_EQ(wallpapers->dynamicWallpaper(), set());
    EXPECT_EQ(wallpapers->wallpaper(), set() + "/day.jpg");
    EXPECT_EQ(wallpapers->wallpaperTimes(), (std::vector<int32_t> {330, 540, 960, 1110}));

    // The next login, in the evening.
    wallpapers.reset();
    now = at("UTC", 17);
    wallpapers = start();
    EXPECT_EQ(wallpapers->dynamicWallpaper(), set());
    EXPECT_EQ(wallpapers->wallpaper(), set() + "/evening.jpg");
}

TEST_F(WallpaperManagerTest, FollowsTheTheme)
{
    auto wallpapers = start();
    wallpapers->setWallpaper(set());
    wallpapers->setWallpaperFollows(DynamicWallpaper::Theme);
    EXPECT_EQ(wallpapers->wallpaper(), set() + "/day.jpg");
    wallpapers->setDarkMode(true);
    EXPECT_EQ(wallpapers->wallpaper(), set() + "/night.jpg");
    wallpapers->setDarkMode(false);
    EXPECT_EQ(wallpapers->wallpaper(), set() + "/day.jpg");

    // Back to the time: the dark theme no longer counts.
    now = at("UTC", 7);
    wallpapers->setDarkMode(true);
    wallpapers->setWallpaperFollows(DynamicWallpaper::Time);
    EXPECT_EQ(wallpapers->wallpaper(), set() + "/morning.jpg");
}

TEST_F(WallpaperManagerTest, APictureEndsTheDynamicWallpaper)
{
    auto wallpapers = start();
    wallpapers->setWallpaper(set());
    const std::string picture = directory.path() + "/plain.jpg";
    ASSERT_TRUE(file::writeAtomically(picture, "jpg"));
    wallpapers->setWallpaper(picture);
    EXPECT_TRUE(wallpapers->dynamicWallpaper().empty());
    EXPECT_NE(wallpapers->wallpaper().find("plain"), std::string::npos);
}
