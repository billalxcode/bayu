#ifndef WALLPAPERMANAGER_H
#define WALLPAPERMANAGER_H

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "base/settings.h"
#include "base/signal.h"
#include "base/thread_pool.h"
#include "base/timer.h"
#include "scheduleutils.h"

/**
 * The desktop background: the wallpaper in use, the wallpapers the user
 * brought in, and the folders they added as libraries. ThemeManager owns the
 * D-Bus interface and hands this part of it over here.
 *
 * Every wallpaper the user picks is copied into the session's own data
 * directory, so moving or deleting the original leaves the desktop alone.
 *
 * The wallpaper can be dynamic, a folder DynamicWallpaper knows: wallpaper()
 * is then the folder's picture for the time of day or the theme, and changes
 * with it.
 */
class WallpaperManager
{
public:
    using Clock = std::function<ScheduleUtils::DateTime()>;

    explicit WallpaperManager(bayu::base::Settings &settings, Clock clock = ScheduleUtils::currentDateTime);
    ~WallpaperManager();

    WallpaperManager(const WallpaperManager &) = delete;
    WallpaperManager &operator=(const WallpaperManager &) = delete;

    std::string wallpaper() const;
    // A picture, or a dynamic wallpaper's folder.
    void setWallpaper(const std::string &path);

    // The dynamic wallpaper's folder; empty for a picture.
    std::string dynamicWallpaper() const;
    // DynamicWallpaper::Follows.
    int wallpaperFollows() const;
    void setWallpaperFollows(int follows);
    // When today's morning, day, evening and night start, in minutes after
    // midnight, where the user is.
    std::vector<int32_t> wallpaperTimes() const;
    // The theme a dynamic wallpaper that follows it shows.
    void setDarkMode(bool dark);

    std::vector<std::string> userWallpapers() const;
    void removeUserWallpaper(const std::string &path);

    std::vector<std::string> wallpaperFolders() const;
    void addWallpaperFolder(const std::string &path);
    void removeWallpaperFolder(const std::string &path);
    std::vector<std::string> wallpaperFolderImages(const std::string &path);

    int backgroundType() const;
    void setBackgroundType(int type);

    std::string backgroundColor() const;
    void setBackgroundColor(const std::string &color);

    bayu::base::Signal<std::string> wallpaperChanged;
    bayu::base::Signal<> userWallpapersChanged;
    bayu::base::Signal<> wallpaperFoldersChanged;
    bayu::base::Signal<std::string> wallpaperFolderImagesChanged;
    bayu::base::Signal<> backgroundTypeChanged;
    bayu::base::Signal<> backgroundColorChanged;
    // The dynamic wallpaper, what it follows or its times changed.
    bayu::base::Signal<> dynamicWallpaperChanged;

private:
    template<typename T>
    void write(const std::string &key, const T &value);
    void syncWallpaperFolder(const std::string &folder);
    void onWallpaperFolderSynced(const std::string &folder, bool copied);
    void showPicture(const std::string &path);
    void updateDynamic();

    bayu::base::Settings &m_settings;

    std::string m_wallpaperPath;
    std::vector<std::string> m_userWallpapers;
    std::vector<std::string> m_wallpaperFolders;
    // The copies being made, by folder.
    std::map<std::string, bayu::base::Job> m_syncingFolders;

    int m_backgroundType = 0;
    std::string m_backgroundColor;

    Clock m_clock;
    std::string m_dynamic;
    int m_follows = 0;
    bool m_darkMode = false;
    std::array<int, 4> m_times {};
    // The time of day is checked every minute: a timer for the next change
    // would miss the clock being set, a sleep and a new time zone.
    bayu::base::Timer m_minute;
};

#endif // WALLPAPERMANAGER_H
