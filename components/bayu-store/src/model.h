// SPDX-License-Identifier: GPL-3.0-or-later
// What the store shows: apps from Flathub and F-Droid, the ones installed,
// and the work on them, independent of where the data comes from.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace store {

enum class Source {
    Flathub,
    FDroid,
};

struct Screenshot
{
    std::string url;
    std::string caption;
    // Phones' screenshots stand upright.
    bool portrait = false;
};

struct Permission
{
    // "wifi", "folder", "cpu"...: a glyph.
    std::string glyph;
    std::string name;
    // "Required", "Read & write".
    std::string detail;
};

struct App
{
    // "org.gimp.GIMP", "org.fdroid.fdroid".
    std::string id;
    Source source = Source::Flathub;
    std::string name;
    std::string summary;
    // Plain text paragraphs, separated by blank lines.
    std::string description;
    std::string developer;
    bool verified = false;
    // The store's categories ("creative", "developer"...) and its filters
    // within them ("Photo", "3D"...).
    std::vector<std::string> categories;
    std::vector<std::string> filters;
    std::vector<std::string> keywords;
    // A file on disk, or empty for the placeholder tile.
    std::string iconPath;
    std::string iconUrl;
    // The placeholder: a glyph on a gradient of this colour (0: one from
    // the name).
    std::string glyph;
    uint32_t tint = 0;
    std::string license;
    bool freeLicense = true;
    std::string version;
    // Seconds since the epoch.
    int64_t updated = 0;
    int64_t added = 0;
    uint64_t downloadSize = 0;
    uint64_t installedSize = 0;
    // Flathub's counts; F-Droid publishes none.
    int64_t installs = 0;
    int64_t installsLastMonth = 0;
    double trending = 0;
    std::vector<Screenshot> screenshots;
    std::string releaseNotes;
    std::vector<Permission> permissions;
    // F-Droid's anti-features: "Ads", "Tracking", "NonFreeDep"...
    std::vector<std::string> antiFeatures;
    std::string homepage;
    // Flatpak: the ref. F-Droid: the APK and its checksum.
    std::string ref;
    std::string apkUrl;
    std::string apkSha256;
    int64_t versionCode = 0;
    // F-Droid: needs ARM translation on this PC.
    bool armOnly = false;
};

struct InstalledApp
{
    std::string id;
    Source source = Source::Flathub;
    std::string version;
    uint64_t size = 0;
    int64_t installed = 0;
    // An update is waiting: its version.
    std::string newVersion;
    uint64_t updateSize = 0;
};

// Work on an app, queued or running.
struct Job
{
    enum class Kind {
        Install,
        Update,
        Remove,
    };
    std::string id;
    Source source = Source::Flathub;
    Kind kind = Kind::Install;
    // 0 to 1, negative while unknown.
    double progress = -1;
    uint64_t bytesDone = 0;
    uint64_t bytesTotal = 0;
    uint64_t bytesPerSecond = 0;
    bool paused = false;
};

struct StorageUsage
{
    uint64_t apps = 0;
    uint64_t runtimes = 0;
    uint64_t cache = 0;
    uint64_t free = 0;
    uint64_t total = 0;
};

// A category of the store, with the apps in it.
struct Category
{
    std::string id;
    std::string name;
    std::string glyph;
    // Sub-filters: "Photo", "Design"...
    std::vector<std::string> filters;
    int count = 0;
};

enum class Sort {
    Popular,
    Recent,
    Name,
    Relevance,
};

struct Query
{
    std::string text;
    std::string category;
    std::string filter;
    // Empty: both.
    std::vector<Source> sources;
    bool openSource = false;
    bool verified = false;
    bool recentlyUpdated = false;
    bool games = false;
    Sort sort = Sort::Popular;
    int offset = 0;
    int limit = 24;
};

struct Page
{
    std::vector<App> apps;
    int total = 0;
};

} // namespace store
