// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "model.h"

namespace store {

// Where the store's apps come from and where they go: the catalog of
// Flathub and F-Droid, Flatpak and lapis (Android) to install with. Calls
// answer from the catalog at hand; work reports through onChanged.
class Backend
{
public:
    virtual ~Backend() = default;

    // Opens the catalog and refreshes it in the background.
    virtual void start() = 0;
    // The catalog is being fetched for the first time: nothing to show yet.
    virtual bool isLoading() const = 0;
    // Why the catalog could not be refreshed, for a quiet note.
    virtual std::string syncError() const = 0;

    // Android apps install through lapis, when it runs.
    virtual bool androidAvailable() const = 0;
    // Starts Android for this session (lapis), or sets it up.
    virtual void enableAndroid(std::function<void(const std::string& error)> done) = 0;

    virtual int count(Source source) const = 0;
    virtual std::vector<Category> categories() const = 0;
    // The banner: Flathub's app of the day and of the week.
    virtual std::vector<App> featured() const = 0;
    virtual Page query(const Query& query) const = 0;
    virtual std::optional<App> app(const std::string& id, Source source) const = 0;
    // Fills in what the catalog lacks (screenshots, install counts, sizes),
    // fetched once and kept.
    virtual void details(const App& app, std::function<void(const App& app)> done) = 0;
    // Searches related to a query, and the developers with most results.
    virtual std::vector<std::string> relatedSearches(const std::string& text) const = 0;
    virtual std::vector<std::pair<std::string, int>> topDevelopers(const Query& query) const = 0;

    // The library.
    virtual std::vector<InstalledApp> installed() const = 0;
    virtual std::optional<InstalledApp> installedApp(const std::string& id, Source source) const = 0;
    virtual std::vector<InstalledApp> updates() const = 0;
    // Installs and updates, newest first.
    virtual std::vector<InstalledApp> recent() const = 0;
    virtual void checkUpdates() = 0;
    // Seconds since the epoch; 0 before the first check.
    virtual int64_t lastChecked() const = 0;
    virtual StorageUsage storage() const = 0;

    // Work on apps: queued, one at a time, reported through jobs().
    virtual std::vector<Job> jobs() const = 0;
    virtual std::optional<Job> job(const std::string& id, Source source) const = 0;
    virtual void install(const App& app) = 0;
    virtual void update(const std::string& id, Source source) = 0;
    virtual void updateAll() = 0;
    virtual void remove(const std::string& id, Source source) = 0;
    virtual void pause(const std::string& id, Source source) = 0;
    virtual void resume(const std::string& id, Source source) = 0;
    virtual void cancel(const std::string& id, Source source) = 0;
    virtual void launch(const std::string& id, Source source) = 0;
    // Why the last job failed, once.
    virtual std::string takeError() = 0;

    // Settings.
    virtual bool sourceEnabled(Source source) const = 0;
    virtual void setSourceEnabled(Source source, bool enabled) = 0;
    virtual bool automaticUpdates() const = 0;
    virtual void setAutomaticUpdates(bool automatic) = 0;
    // F-Droid apps built for ARM only, which need translation here.
    virtual bool showArmOnly() const = 0;
    virtual void setShowArmOnly(bool show) = 0;
    virtual uint64_t cacheSize() const = 0;
    virtual void clearCache() = 0;
    // Seconds since the epoch of the catalog's last refresh.
    virtual int64_t lastSynced(Source source) const = 0;
    virtual void sync() = 0;

    // Favourites, kept on this computer.
    virtual bool isFavorite(const std::string& id, Source source) const = 0;
    virtual void setFavorite(const std::string& id, Source source, bool favorite) = 0;

    // The catalog, the library or a job changed.
    std::function<void()> onChanged;
};

std::unique_ptr<Backend> createMockBackend();
std::unique_ptr<Backend> createBackend();

} // namespace store
