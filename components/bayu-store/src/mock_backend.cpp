// SPDX-License-Identifier: GPL-3.0-or-later
// Sample apps shaped like the design, for the pages and screenshots
// without a network: bayu-store --mock.

#include <algorithm>
#include <ctime>
#include <set>

#include "backend.h"
#include "base/timer.h"
#include "catalog.h"

namespace store {

namespace {

constexpr uint64_t MB = 1000ull * 1000;

struct Sample
{
    const char* id;
    Source source;
    const char* name;
    const char* summary;
    const char* developer;
    const char* glyph;
    uint32_t tint;
    std::vector<std::string> categories;
    std::vector<std::string> filters;
    const char* license;
    uint64_t size;
    int64_t installs;
    int daysAgo;
    std::vector<std::string> keywords;
};

App appFrom(const Sample& s, int64_t now)
{
    App app;
    app.id = s.id;
    app.source = s.source;
    app.name = s.name;
    app.summary = s.summary;
    app.developer = s.developer;
    app.verified = s.source == Source::Flathub;
    app.glyph = s.glyph;
    app.tint = s.tint;
    app.categories = s.categories;
    app.filters = s.filters;
    app.license = s.license;
    app.freeLicense = std::string(s.license) != "Proprietary";
    app.downloadSize = s.size;
    app.installedSize = s.size * 2;
    app.installs = s.installs * 12;
    app.installsLastMonth = s.installs;
    app.updated = now - int64_t(s.daysAgo) * 86400;
    app.keywords = s.keywords;
    app.version = "1.0";
    app.description = std::string(s.summary) + ".";
    if (s.source == Source::FDroid)
        app.permissions = {{"wifi", "Network access", "Required"}, {"folder", "Storage", "Read & write"}};
    else
        app.permissions = {{"wifi", "Network access", "Required"}, {"folder", "Home folder", "Read & write"}};
    return app;
}

class MockBackend : public Backend
{
public:
    MockBackend()
    {
        const int64_t now = std::time(nullptr);
        const Source F = Source::Flathub;
        const Source D = Source::FDroid;
        const std::vector<Sample> samples{
            {"org.northlight.Lumen", F, "Lumen Photo Studio", "Non-destructive RAW editor with on-device relighting",
             "Northlight Labs", "aperture", 0x8B5CF6, {"creative", "video"}, {"Photo"}, "GPL-3.0", 214 * MB, 1200000, 2,
             {"raw editor", "retouch", "film look"}},
            {"app.orbit.Mail", F, "Orbit Mail", "Unified inbox with smart triage", "Orbit", "mail", 0xF43F5E,
             {"productivity"}, {}, "MPL-2.0", 96 * MB, 840000, 6, {"email"}},
            {"io.cadence.Player", F, "Cadence", "Lossless player and library", "Cadence Audio", "music", 0x14B8A6,
             {"audio"}, {"Audio"}, "GPL-3.0", 12 * MB, 610000, 3, {"music", "flac"}},
            {"org.vellum.notes", D, "Vellum Notes", "Markdown notes that sync", "Vellum", "notebook-pen", 0xF97316,
             {"productivity"}, {}, "GPL-3.0", 38 * MB, 0, 4, {"notes", "markdown"}},
            {"cloud.nimbus.drive", D, "Nimbus Drive", "Encrypted cloud storage", "Nimbus", "cloud", 0x0EA5E9,
             {"utilities"}, {}, "AGPL-3.0", 51 * MB, 0, 9, {"cloud", "sync"}},
            {"dev.forge.ide", D, "Forge IDE", "Remote containers, faster indexing", "Forge", "code", 0x3F3F46,
             {"developer"}, {}, "Apache-2.0", 186 * MB, 0, 1, {"ide", "code"}},
            {"chat.echo.app", D, "Echo Chat", "Private messages and groups", "Echo", "message-circle", 0xA855F7,
             {"productivity"}, {}, "GPL-3.0", 68 * MB, 0, 5, {"chat"}},
            {"fit.pulse.app", D, "Pulse Fit", "Workouts and heart-rate zones", "Pulse", "activity", 0x22C55E,
             {"utilities"}, {}, "GPL-3.0", 42 * MB, 0, 2, {"health", "fitness"}},
            {"pro.ledger.app", F, "Ledger Pro", "Budgets and accounts, offline", "Ledger", "wallet", 0x16A34A,
             {"productivity"}, {}, "GPL-3.0", 54 * MB, 230000, 7, {"finance", "budget"}},
            {"maps.atlas.app", F, "Atlas Maps", "Offline maps and routes", "Atlas", "map", 0x06B6D4,
             {"utilities"}, {}, "GPL-3.0", 310 * MB, 410000, 8, {"maps", "navigation"}},
            {"cad.prism.app", F, "Prism CAD", "Parametric 3D modelling", "Prism", "box", 0xF43F5E,
             {"creative"}, {"3D"}, "LGPL-2.1", 420 * MB, 380000, 10, {"cad", "3d"}},
            {"art.inkline.app", F, "Inkline", "Vector drawing with live paths", "Inkline", "pen-tool", 0xF43F5E,
             {"creative"}, {"Design"}, "GPL-3.0", 88 * MB, 520000, 12, {"vector", "drawing"}},
            {"video.clipwave.app", F, "Clipwave", "Timeline video editor", "Clipwave", "clapperboard", 0x0EA5E9,
             {"creative", "video"}, {"Video"}, "GPL-3.0", 240 * MB, 470000, 14, {"video editor"}},
            {"audio.resonate.app", F, "Resonate", "Multitrack audio workstation", "Resonate", "audio-lines", 0xF97316,
             {"creative", "audio"}, {"Audio"}, "GPL-3.0", 160 * MB, 300000, 15, {"daw", "audio"}},
            {"design.palette.app", D, "Palette", "Color systems and tokens", "Palette", "palette", 0x22C55E,
             {"creative"}, {"Design"}, "MIT", 18 * MB, 0, 16, {"color"}},
            {"anim.frameloop.app", F, "Frameloop", "Animate photos into short loops", "Studio Nine", "film", 0xA855F7,
             {"creative", "video"}, {"Video", "Photo"}, "MPL-2.0", 180 * MB, 260000, 18, {"animation", "photo editor"}},
            {"type.typeset.app", D, "Typeset", "Font pairing and specimens", "Typeset", "type", 0x3B82F6,
             {"creative"}, {"Design"}, "OFL-1.1", 22 * MB, 0, 20, {"fonts"}},
            {"soft.hanami.Snapfix", F, "Snapfix", "Fast retouching, healing brush, batch export", "Hanami Soft", "wand",
             0xF43F5E, {"creative", "video"}, {"Photo"}, "MIT", 96 * MB, 330000, 11, {"retouch", "photo editor"}},
            {"org.openimaging.grain", D, "Grain", "Film emulation and analog grain profiles", "Open Imaging", "image",
             0xF97316, {"creative", "video"}, {"Photo"}, "GPL-2.0", 62 * MB, 0, 6, {"film look", "photo editor"}},
            {"ai.pixelroot.Cutout", F, "Cutout AI", "Background removal and object masking", "Pixelroot", "scissors",
             0x06B6D4, {"creative", "video"}, {"Photo"}, "Apache-2.0", 148 * MB, 290000, 13,
             {"background remover", "photo editor"}},
            {"games.skyline.Racer", F, "Skyline Racer", "Arcade racing over city rooftops", "Skyline", "gamepad",
             0xF97316, {"games"}, {}, "GPL-3.0", 520 * MB, 350000, 3, {"racing"}},
            {"edu.mentor.app", D, "Mentor", "Flashcards with spaced repetition", "Mentor", "graduation-cap",
             0xEAB308, {"education"}, {}, "GPL-3.0", 24 * MB, 0, 2, {"study"}},
            {"sec.nova.Shield", D, "Nova Shield", "Firewall and tracker blocker", "Nova", "shield", 0x2563EB,
             {"utilities"}, {}, "GPL-3.0", 14 * MB, 0, 1, {"privacy"}},
        };
        std::vector<App> flathub, fdroid;
        for (const Sample& sample : samples) {
            App app = appFrom(sample, now);
            (app.source == Source::Flathub ? flathub : fdroid).push_back(app);
        }
        // The banner's app, as the design has it.
        for (App& app : flathub) {
            if (app.id == "org.northlight.Lumen") {
                app.summary = "Pro-grade RAW editing with on-device relighting — new in 4.2.0.";
                app.description = "Lumen is a non-destructive photo editor built for Linux. Open RAW files from 400+ "
                                  "cameras, relight scenes with on-device AI, and sync edits across machines — "
                                  "without uploading a single frame.";
                app.version = "4.2.0";
                app.releaseNotes = "Relight engine is 2.4× faster on AMD GPUs\nNew masking brush with edge "
                                   "detection\nFixes HEIF import on Wayland sessions";
                app.screenshots = {{"", "Library"}, {"", "Develop"}, {"", "Masks"}, {"", "Export"}};
                app.permissions.push_back({"cpu", "GPU acceleration", "Required"});
                app.ref = "app/org.northlight.Lumen/x86_64/stable";
            }
        }
        m_catalog.setApps(Source::Flathub, flathub);
        m_catalog.setApps(Source::FDroid, fdroid);

        auto installed = [&](const char* id, Source source, const char* version, uint64_t size, int days,
                             const char* newVersion = "") {
            m_installed.push_back({id, source, version, size, now - days * 86400, newVersion,
                                   std::string(newVersion).empty() ? 0 : size});
        };
        installed("org.northlight.Lumen", F, "4.1.3", 214 * MB, 40, "4.2.0");
        installed("dev.forge.ide", D, "11.0.4", 186 * MB, 30, "11.1.0");
        installed("io.cadence.Player", F, "2.8.1", 12 * MB, 20, "2.9.0");
        installed("app.orbit.Mail", F, "3.2", 96 * MB, 2);
        installed("fit.pulse.app", D, "1.4", 42 * MB, 3);
        installed("org.vellum.notes", D, "5.0", 38 * MB, 5);
        installed("chat.echo.app", D, "2.1", 68 * MB, 8);
        m_jobs.push_back({"org.northlight.Lumen", F, Job::Kind::Update, 0.62, 132 * MB, 214 * MB, 2100000, false});
        m_lastChecked = now - 4 * 60;
    }

    void start() override {}
    bool isLoading() const override { return false; }
    std::string syncError() const override { return {}; }
    AndroidState androidState() const override { return AndroidState::Running; }
    void enableAndroid(std::optional<App> then) override
    {
        if (then)
            install(*then);
    }
    AndroidSetup androidSetup() const override { return {}; }
    void disableAndroid() override {}

    int count(Source source) const override { return source == Source::Flathub ? 8420 : 4060; }
    std::vector<Category> categories() const override
    {
        std::vector<Category> out = m_catalog.categories();
        const int counts[] = {2140, 1820, 1265, 3410, 940, 1120, 760, 1025};
        for (size_t i = 0; i < out.size() && i < 8; ++i)
            out[i].count = counts[i];
        return out;
    }
    std::vector<App> featured() const override
    {
        std::vector<App> out;
        for (const char* id : {"org.northlight.Lumen", "video.clipwave.app", "cad.prism.app"}) {
            if (const App* app = m_catalog.find(id, Source::Flathub))
                out.push_back(*app);
        }
        return out;
    }
    Page query(const Query& query) const override
    {
        Page page = m_catalog.query(query);
        // The design's totals.
        if (query.category == "creative" && query.filter.empty() && query.sources.empty())
            page.total = 1820;
        if (!query.text.empty() && query.sources.empty())
            page.total = std::max(page.total, 128);
        return page;
    }
    std::optional<App> app(const std::string& id, Source source) const override
    {
        if (const App* app = m_catalog.find(id, source))
            return *app;
        return std::nullopt;
    }
    void details(const App& app, std::function<void(const App&)> done) override { done(app); }
    std::vector<std::string> relatedSearches(const std::string& text) const override
    {
        return m_catalog.relatedSearches(text);
    }
    std::vector<std::pair<std::string, int>> topDevelopers(const Query& query) const override
    {
        return {{"Northlight Labs", 14}, {"Open Imaging", 9}, {"Hanami Soft", 6}};
    }

    std::vector<InstalledApp> installed() const override { return m_installed; }
    std::optional<InstalledApp> installedApp(const std::string& id, Source source) const override
    {
        for (const InstalledApp& app : m_installed) {
            if (app.id == id && app.source == source)
                return app;
        }
        return std::nullopt;
    }
    std::vector<InstalledApp> updates() const override
    {
        std::vector<InstalledApp> out;
        for (const InstalledApp& app : m_installed) {
            if (!app.newVersion.empty())
                out.push_back(app);
        }
        return out;
    }
    std::vector<InstalledApp> recent() const override
    {
        std::vector<InstalledApp> out = m_installed;
        std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.installed > b.installed; });
        return out;
    }
    void checkUpdates() override { m_lastChecked = std::time(nullptr); }
    int64_t lastChecked() const override { return m_lastChecked; }
    StorageUsage storage() const override
    {
        constexpr uint64_t GB = 1000 * MB;
        return {5200 * MB, 2600 * MB, 2000 * MB, 2200 * MB, 12 * GB};
    }

    std::vector<Job> jobs() const override { return m_jobs; }
    std::optional<Job> job(const std::string& id, Source source) const override
    {
        for (const Job& job : m_jobs) {
            if (job.id == id && job.source == source)
                return job;
        }
        return std::nullopt;
    }
    void install(const App& app) override
    {
        m_jobs.push_back({app.id, app.source, Job::Kind::Install, 0, 0, app.downloadSize, 0, false});
        changed();
    }
    void update(const std::string& id, Source source) override
    {
        if (!job(id, source))
            m_jobs.push_back({id, source, Job::Kind::Update, 0, 0, 0, 0, false});
        changed();
    }
    void updateAll() override
    {
        for (const InstalledApp& app : updates())
            update(app.id, app.source);
    }
    void remove(const std::string& id, Source source) override
    {
        std::erase_if(m_installed, [&](const InstalledApp& app) { return app.id == id && app.source == source; });
        changed();
    }
    void pause(const std::string& id, Source source) override { setPaused(id, source, true); }
    void resume(const std::string& id, Source source) override { setPaused(id, source, false); }
    void cancel(const std::string& id, Source source) override
    {
        std::erase_if(m_jobs, [&](const Job& job) { return job.id == id && job.source == source; });
        changed();
    }
    void launch(const std::string&, Source) override {}
    std::string takeError() override { return {}; }

    bool sourceEnabled(Source source) const override { return m_enabled[int(source)]; }
    void setSourceEnabled(Source source, bool enabled) override
    {
        m_enabled[int(source)] = enabled;
        changed();
    }
    bool automaticUpdates() const override { return m_automatic; }
    void setAutomaticUpdates(bool automatic) override
    {
        m_automatic = automatic;
        changed();
    }
    bool showArmOnly() const override { return m_armOnly; }
    void setShowArmOnly(bool show) override
    {
        m_armOnly = show;
        changed();
    }
    uint64_t cacheSize() const override { return 2000 * MB; }
    void clearCache() override {}
    int64_t lastSynced(Source) const override { return m_lastChecked; }
    void sync() override {}

    bool isFavorite(const std::string& id, Source source) const override
    {
        return m_favorites.count({id, int(source)});
    }
    void setFavorite(const std::string& id, Source source, bool favorite) override
    {
        if (favorite)
            m_favorites.insert({id, int(source)});
        else
            m_favorites.erase({id, int(source)});
        changed();
    }

private:
    void setPaused(const std::string& id, Source source, bool paused)
    {
        for (Job& job : m_jobs) {
            if (job.id == id && job.source == source)
                job.paused = paused;
        }
        changed();
    }
    void changed()
    {
        if (onChanged)
            onChanged();
    }

    Catalog m_catalog;
    std::vector<InstalledApp> m_installed;
    std::vector<Job> m_jobs;
    std::set<std::pair<std::string, int>> m_favorites;
    int64_t m_lastChecked = 0;
    bool m_enabled[2] = {true, true};
    bool m_automatic = true;
    bool m_armOnly = false;
};

} // namespace

std::unique_ptr<Backend> createMockBackend()
{
    return std::make_unique<MockBackend>();
}

} // namespace store
