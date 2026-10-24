// SPDX-License-Identifier: BSD-3-Clause

#include "flathub.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <future>

#include "base/json.h"
#include "catalog.h"
#include "net.h"

using bayu::base::json::Value;

namespace store::flathub {

namespace {

std::string trimmed(std::string text)
{
    // Collapse the indentation AppStream texts carry.
    std::string out;
    bool space = false;
    for (char c : text) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            space = !out.empty();
            continue;
        }
        if (space)
            out += ' ';
        space = false;
        out += c;
    }
    return out;
}

std::string canonicalCategory(const std::string& main)
{
    static const std::vector<std::pair<const char*, const char*>> names{
        {"audiovideo", "AudioVideo"}, {"development", "Development"}, {"education", "Education"},
        {"game", "Game"},             {"graphics", "Graphics"},       {"network", "Network"},
        {"office", "Office"},         {"science", "Science"},         {"system", "System"},
        {"utility", "Utility"},
    };
    const std::string key = folded(main);
    for (const auto& [lower, name] : names) {
        if (key == lower)
            return name;
    }
    return main;
}

std::vector<std::string> strings(const Value& value)
{
    std::vector<std::string> out;
    if (value.isArray()) {
        for (size_t i = 0; i < value.size(); ++i) {
            if (value[i].isString())
                out.push_back(value[i].toString());
        }
    } else if (value.isString()) {
        out.push_back(value.toString());
    }
    return out;
}

App appFromHit(const Value& hit)
{
    App app;
    app.id = hit["app_id"].toString();
    app.source = Source::Flathub;
    app.name = hit["name"].toString(app.id);
    app.summary = hit["summary"].toString();
    app.description = trimmed(hit["description"].toString());
    app.developer = hit["developer_name"].toString();
    app.verified = hit["verification_verified"].toBool();
    std::vector<std::string> categories;
    for (const std::string& main : strings(hit["main_categories"]))
        categories.push_back(canonicalCategory(main));
    for (const std::string& sub : strings(hit["sub_categories"]))
        categories.push_back(sub);
    app.categories = categoriesForFreedesktop(categories);
    app.keywords = strings(hit["keywords"]);
    app.iconUrl = hit["icon"].toString();
    app.glyph = "package";
    app.license = hit["project_license"].toString();
    app.freeLicense = hit["is_free_license"].toBool(true);
    app.installsLastMonth = hit["installs_last_month"].toInt();
    app.trending = hit["trending"].toDouble();
    app.updated = hit["updated_at"].toInt();
    app.added = hit["added_at"].toInt();
    app.ref = "app/" + app.id + "/x86_64/stable";
    return app;
}

Value parse(const net::Response& response)
{
    if (!response.ok())
        return Value();
    return Value::parse(response.body).value_or(Value());
}

std::string today(int daysBack = 0)
{
    const std::time_t now = std::time(nullptr) - std::time_t(daysBack) * 86400;
    std::tm utc{};
    gmtime_r(&now, &utc);
    char text[16];
    std::strftime(text, sizeof(text), "%Y-%m-%d", &utc);
    return text;
}

} // namespace

std::vector<App> fetchCatalog(std::string* error)
{
    auto page = [](int number) {
        const std::string body = "{\"query\":\"\",\"filters\":[],\"hits_per_page\":250,\"page\":"
                                 + std::to_string(number) + "}";
        return net::post(std::string(kApi) + "/search", body, 180);
    };
    const net::Response first = page(1);
    const Value firstPage = parse(first);
    if (!firstPage.isObject()) {
        if (error)
            *error = "Flathub: " + (first.error.empty() ? std::string("unexpected answer") : first.error);
        return {};
    }
    const int pages = int(std::min<int64_t>(60, firstPage["totalPages"].toInt(1)));
    // The rest at once: a page takes seconds to come.
    std::vector<std::future<net::Response>> rest;
    for (int number = 2; number <= pages; ++number)
        rest.push_back(std::async(std::launch::async, page, number));
    std::vector<App> apps;
    auto take = [&apps](const Value& result) {
        const Value& hits = result["hits"];
        for (size_t i = 0; i < hits.size(); ++i) {
            if (hits[i]["type"].toString() == "desktop-application" || hits[i]["type"].toString() == "console-application")
                apps.push_back(appFromHit(hits[i]));
        }
    };
    take(firstPage);
    int number = 1;
    for (auto& future : rest) {
        ++number;
        net::Response response = future.get();
        // A page that failed gets one more try.
        if (!response.ok())
            response = page(number);
        const Value result = parse(response);
        if (!result.isObject()) {
            if (error)
                *error = "Flathub: " + response.error;
            continue;
        }
        take(result);
    }
    return apps;
}

void fetchDetails(App& app)
{
    auto appstream = std::async(std::launch::async, [&] { return net::get(std::string(kApi) + "/appstream/" + app.id); });
    auto summary = std::async(std::launch::async, [&] { return net::get(std::string(kApi) + "/summary/" + app.id); });
    auto stats = std::async(std::launch::async, [&] { return net::get(std::string(kApi) + "/stats/" + app.id); });

    const Value meta = parse(appstream.get());
    if (meta.isObject()) {
        const std::string description = plainText(meta["description"].toString());
        if (!description.empty())
            app.description = description;
        app.screenshots.clear();
        const Value& shots = meta["screenshots"];
        for (size_t i = 0; i < shots.size() && app.screenshots.size() < 4; ++i) {
            const Value& sizes = shots[i]["sizes"];
            std::string best;
            int bestWidth = 0;
            for (size_t j = 0; j < sizes.size(); ++j) {
                const int width = std::atoi(sizes[j]["width"].toString().c_str());
                // The smallest that is at least 600 wide, else the largest.
                if (best.empty() || (width >= 600 && (bestWidth < 600 || width < bestWidth))
                    || (bestWidth < 600 && width > bestWidth)) {
                    best = sizes[j]["src"].toString();
                    bestWidth = width;
                }
            }
            if (!best.empty())
                app.screenshots.push_back({best, trimmed(shots[i]["caption"].toString()), false});
        }
        const Value& releases = meta["releases"];
        if (releases.size() > 0) {
            const Value& release = releases[0];
            if (release["version"].isString())
                app.version = release["version"].toString();
            app.releaseNotes = plainText(release["description"].toString());
            // List items become lines; drop their bullets.
            std::string notes;
            size_t start = 0;
            while (start < app.releaseNotes.size()) {
                size_t end = app.releaseNotes.find('\n', start);
                if (end == std::string::npos)
                    end = app.releaseNotes.size();
                std::string line = trimmed(app.releaseNotes.substr(start, end - start));
                if (line.starts_with("• "))
                    line = line.substr(4);
                if (!line.empty())
                    notes += line + "\n";
                start = end + 1;
            }
            app.releaseNotes = notes;
        }
        if (meta["urls"].isObject())
            app.homepage = meta["urls"]["homepage"].toString();
    }

    const Value sizes = parse(summary.get());
    if (sizes.isObject()) {
        app.downloadSize = uint64_t(std::max<int64_t>(0, sizes["download_size"].toInt()));
        app.installedSize = uint64_t(std::max<int64_t>(0, sizes["installed_size"].toInt()));
        const Value& permissions = sizes["metadata"]["permissions"];
        app.permissions.clear();
        const std::vector<std::string> shared = strings(permissions["shared"]);
        if (std::find(shared.begin(), shared.end(), "network") != shared.end())
            app.permissions.push_back({"wifi", "Network access", "Required"});
        std::string files;
        for (const std::string& filesystem : strings(permissions["filesystems"])) {
            const bool readOnly = filesystem.ends_with(":ro");
            if (filesystem.starts_with("host") && files.empty())
                app.permissions.push_back({"folder", "All files", readOnly ? "Read only" : "Read & write"});
            else if (filesystem.starts_with("home") && files.empty())
                app.permissions.push_back({"folder", "Home folder", readOnly ? "Read only" : "Read & write"});
            else if (filesystem.starts_with("xdg-download"))
                app.permissions.push_back({"download", "Downloads folder", readOnly ? "Read only" : "Read & write"});
            else
                continue;
            files = filesystem;
        }
        const std::vector<std::string> devices = strings(permissions["devices"]);
        if (std::find(devices.begin(), devices.end(), "all") != devices.end())
            app.permissions.push_back({"cpu", "All devices", "Camera, USB, GPU"});
        else if (std::find(devices.begin(), devices.end(), "dri") != devices.end())
            app.permissions.push_back({"cpu", "GPU acceleration", "Required"});
        const std::vector<std::string> sockets = strings(permissions["sockets"]);
        if (std::find(sockets.begin(), sockets.end(), "pulseaudio") != sockets.end())
            app.permissions.push_back({"music", "Sound", "Play & record"});
    }

    const Value counts = parse(stats.get());
    if (counts.isObject())
        app.installs = counts["installs_total"].toInt();
}

std::vector<std::string> fetchFeatured()
{
    std::vector<std::string> ids;
    // Today's pick may not be published yet early in the day.
    for (int back = 0; back < 2 && ids.empty(); ++back) {
        const Value day = parse(net::get(std::string(kApi) + "/app-picks/app-of-the-day/" + today(back), 30));
        if (day.isObject() && day["app_id"].isString())
            ids.push_back(day["app_id"].toString());
    }
    const Value week = parse(net::get(std::string(kApi) + "/app-picks/apps-of-the-week/" + today(), 30));
    const Value& apps = week["apps"];
    for (size_t i = 0; i < apps.size() && ids.size() < 5; ++i) {
        const std::string id = apps[i]["app_id"].toString();
        if (!id.empty() && std::find(ids.begin(), ids.end(), id) == ids.end())
            ids.push_back(id);
    }
    return ids;
}

} // namespace store::flathub
