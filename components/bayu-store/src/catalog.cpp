// SPDX-License-Identifier: GPL-3.0-or-later

#include "catalog.h"

#include <algorithm>
#include <ctime>
#include <set>

namespace store {

namespace {

bool hasAny(const std::vector<std::string>& values, std::initializer_list<const char*> wanted)
{
    for (const std::string& value : values) {
        for (const char* want : wanted) {
            if (value == want)
                return true;
        }
    }
    return false;
}

bool containsAny(const std::string& text, std::initializer_list<const char*> parts)
{
    for (const char* part : parts) {
        if (text.find(part) != std::string::npos)
            return true;
    }
    return false;
}

void add(std::vector<std::string>& out, const char* value)
{
    if (std::find(out.begin(), out.end(), value) == out.end())
        out.push_back(value);
}

} // namespace

std::string folded(std::string text)
{
    for (char& c : text)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

const std::vector<Category>& storeCategories()
{
    static const std::vector<Category> categories{
        {"productivity", "Productivity", "briefcase", {}, 0},
        {"creative", "Creative", "sparkles", {"Photo", "Design", "Audio", "Video", "3D"}, 0},
        {"developer", "Developer", "terminal", {}, 0},
        {"games", "Games", "gamepad", {}, 0},
        {"audio", "Music & Audio", "headphones", {}, 0},
        {"video", "Photo & Video", "camera", {}, 0},
        {"education", "Education", "graduation-cap", {}, 0},
        {"utilities", "Utilities", "wrench", {}, 0},
    };
    return categories;
}

std::vector<std::string> categoriesForFreedesktop(const std::vector<std::string>& c)
{
    std::vector<std::string> out;
    if (hasAny(c, {"Office", "Email", "Chat", "InstantMessaging", "WebBrowser", "Finance", "Calendar",
                   "ProjectManagement", "ContactManagement", "WordProcessor", "Spreadsheet", "Presentation"}))
        add(out, "productivity");
    if (hasAny(c, {"Graphics", "2DGraphics", "3DGraphics", "VectorGraphics", "RasterGraphics", "Photography",
                   "AudioVideoEditing", "Publishing", "Midi", "Mixer", "Sequencer", "Recorder"}))
        add(out, "creative");
    if (hasAny(c, {"Development", "IDE", "Debugger", "RevisionControl", "Building"}))
        add(out, "developer");
    if (hasAny(c, {"Game"}))
        add(out, "games");
    if (hasAny(c, {"Audio", "Music", "Player"}) && !hasAny(c, {"Video"}))
        add(out, "audio");
    if (hasAny(c, {"Video", "Photography", "TV"}))
        add(out, "video");
    if (hasAny(c, {"Education", "Science", "Math", "Astronomy", "Languages", "Literature"}))
        add(out, "education");
    if (out.empty() || hasAny(c, {"System", "Utility", "Settings", "Security", "FileManager", "Monitor"}))
        add(out, "utilities");
    return out;
}

std::vector<std::string> categoriesForFDroid(const std::vector<std::string>& categories)
{
    std::vector<std::string> out;
    for (const std::string& raw : categories) {
        const std::string c = folded(raw);
        if (containsAny(c, {"game"}))
            add(out, "games");
        if (containsAny(c, {"development"}))
            add(out, "developer");
        if (containsAny(c, {"graphics", "drawing", "camera"}))
            add(out, "creative");
        if (containsAny(c, {"music", "podcast", "audio", "ringtone"}))
            add(out, "audio");
        if (containsAny(c, {"multimedia", "video", "gallery", "photo", "camera"}))
            add(out, "video");
        if (containsAny(c, {"science", "education", "reading", "dictionary", "book"}))
            add(out, "education");
        if (containsAny(c, {"writing", "note", "calendar", "time", "email", "messaging", "office", "money",
                            "finance", "phone", "internet", "browser", "social", "chat"}))
            add(out, "productivity");
        if (containsAny(c, {"system", "security", "theming", "connectivity", "navigation", "sports", "cloud",
                            "file", "calculator", "keyboard", "launcher", "wallpaper", "store", "updater"}))
            add(out, "utilities");
    }
    if (out.empty())
        add(out, "utilities");
    return out;
}

std::vector<std::string> filtersFor(const App& app)
{
    std::vector<std::string> out;
    const std::string text = folded(app.name + " " + app.summary);
    auto has = [&](std::initializer_list<const char*> words) {
        if (containsAny(text, words))
            return true;
        for (const std::string& keyword : app.keywords) {
            if (containsAny(folded(keyword), words))
                return true;
        }
        return false;
    };
    if (has({"photo", "raw", "image", "picture", "camera"}))
        add(out, "Photo");
    if (has({"vector", "design", "draw", "paint", "illustrat", "font", "color", "colour", "typograph"}))
        add(out, "Design");
    if (has({"audio", "music", "sound", "daw", "synth", "midi", "record"}))
        add(out, "Audio");
    if (has({"video", "film", "movie", "animation", "timeline"}))
        add(out, "Video");
    if (has({"3d", "cad", "model", "render", "sculpt"}))
        add(out, "3D");
    return out;
}

void Catalog::setApps(Source source, std::vector<App> apps)
{
    std::erase_if(m_apps, [source](const App& app) { return app.source == source; });
    for (App& app : apps) {
        if (app.filters.empty())
            app.filters = filtersFor(app);
        m_apps.push_back(std::move(app));
    }
    index();
}

void Catalog::index()
{
    m_byId.clear();
    for (size_t i = 0; i < m_apps.size(); ++i)
        m_byId[{m_apps[i].id, int(m_apps[i].source)}] = i;
}

const App* Catalog::find(const std::string& id, Source source) const
{
    const auto it = m_byId.find({id, int(source)});
    return it == m_byId.end() ? nullptr : &m_apps[it->second];
}

App* Catalog::find(const std::string& id, Source source)
{
    const auto it = m_byId.find({id, int(source)});
    return it == m_byId.end() ? nullptr : &m_apps[it->second];
}

int Catalog::count(Source source) const
{
    return int(std::count_if(m_apps.begin(), m_apps.end(), [source](const App& app) { return app.source == source; }));
}

std::vector<Category> Catalog::categories() const
{
    std::vector<Category> out = storeCategories();
    for (Category& category : out) {
        category.count = int(std::count_if(m_apps.begin(), m_apps.end(), [&](const App& app) {
            return std::find(app.categories.begin(), app.categories.end(), category.id) != app.categories.end();
        }));
    }
    return out;
}

// How well an app answers a search: the name most, then its words.
static int relevance(const App& app, const std::string& text)
{
    const std::string name = folded(app.name);
    int score = 0;
    if (name == text)
        score += 100;
    else if (name.starts_with(text))
        score += 60;
    else if (name.find(text) != std::string::npos)
        score += 40;
    if (folded(app.summary).find(text) != std::string::npos)
        score += 20;
    for (const std::string& keyword : app.keywords) {
        if (folded(keyword).find(text) != std::string::npos) {
            score += 15;
            break;
        }
    }
    if (folded(app.developer).find(text) != std::string::npos)
        score += 10;
    if (folded(app.description).find(text) != std::string::npos)
        score += 5;
    if (folded(app.id).find(text) != std::string::npos)
        score += 5;
    return score;
}

// Every word of the search must match somewhere.
static int relevanceAll(const App& app, const std::string& text)
{
    int total = 0;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find(' ', start);
        if (end == std::string::npos)
            end = text.size();
        if (end > start) {
            const int score = relevance(app, text.substr(start, end - start));
            if (score == 0)
                return 0;
            total += score;
        }
        start = end + 1;
    }
    // The whole phrase in the name or summary counts again.
    if (text.find(' ') != std::string::npos)
        total += relevance(app, text);
    return total;
}

std::vector<const App*> Catalog::matching(const Query& query) const
{
    const std::string text = folded(query.text);
    const int64_t monthAgo = int64_t(std::time(nullptr)) - 30 * 86400;
    std::vector<const App*> out;
    for (const App& app : m_apps) {
        if (!query.sources.empty()
            && std::find(query.sources.begin(), query.sources.end(), app.source) == query.sources.end())
            continue;
        if (!query.category.empty()
            && std::find(app.categories.begin(), app.categories.end(), query.category) == app.categories.end())
            continue;
        if (!query.filter.empty()
            && std::find(app.filters.begin(), app.filters.end(), query.filter) == app.filters.end())
            continue;
        if (query.games && std::find(app.categories.begin(), app.categories.end(), "games") == app.categories.end())
            continue;
        if (query.openSource && !app.freeLicense)
            continue;
        if (query.verified && !app.verified)
            continue;
        if (query.recentlyUpdated && app.updated < monthAgo)
            continue;
        if (!text.empty() && relevanceAll(app, text) == 0)
            continue;
        out.push_back(&app);
    }
    return out;
}

Page Catalog::query(const Query& query) const
{
    std::vector<const App*> apps = matching(query);
    const std::string text = folded(query.text);
    // Flathub counts installs, F-Droid does not: each source is ranked on
    // its own (installs, or how recently it was updated) and the two take
    // turns.
    auto popularity = [](const App* app) {
        return app->source == Source::Flathub ? double(app->installsLastMonth) : double(app->updated);
    };
    switch (query.sort) {
    case Sort::Popular: {
        std::vector<const App*> flathub, fdroid;
        for (const App* app : apps)
            (app->source == Source::Flathub ? flathub : fdroid).push_back(app);
        auto byPopularity = [&](const App* a, const App* b) { return popularity(a) > popularity(b); };
        std::stable_sort(flathub.begin(), flathub.end(), byPopularity);
        std::stable_sort(fdroid.begin(), fdroid.end(), byPopularity);
        apps.clear();
        for (size_t i = 0; i < std::max(flathub.size(), fdroid.size()); ++i) {
            if (i < flathub.size())
                apps.push_back(flathub[i]);
            if (i < fdroid.size())
                apps.push_back(fdroid[i]);
        }
        break;
    }
    case Sort::Recent:
        std::stable_sort(apps.begin(), apps.end(), [](const App* a, const App* b) { return a->updated > b->updated; });
        break;
    case Sort::Name:
        std::stable_sort(apps.begin(), apps.end(),
                         [](const App* a, const App* b) { return folded(a->name) < folded(b->name); });
        break;
    case Sort::Relevance:
        std::stable_sort(apps.begin(), apps.end(), [&](const App* a, const App* b) {
            const int ra = relevanceAll(*a, text);
            const int rb = relevanceAll(*b, text);
            if (ra != rb)
                return ra > rb;
            return a->installsLastMonth > b->installsLastMonth;
        });
        break;
    }
    Page page;
    page.total = int(apps.size());
    for (int i = query.offset; i < int(apps.size()) && i < query.offset + query.limit; ++i)
        page.apps.push_back(*apps[size_t(i)]);
    return page;
}

std::vector<std::string> Catalog::relatedSearches(const std::string& text) const
{
    Query query;
    query.text = text;
    query.sort = Sort::Relevance;
    query.limit = 40;
    const std::string asked = folded(text);
    std::map<std::string, int> counts;
    for (const App& app : this->query(query).apps) {
        for (const std::string& keyword : app.keywords) {
            const std::string word = folded(keyword);
            if (word.size() > 2 && word != asked && asked.find(word) == std::string::npos)
                ++counts[word];
        }
    }
    std::vector<std::pair<std::string, int>> ranked(counts.begin(), counts.end());
    std::stable_sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    std::vector<std::string> out;
    for (size_t i = 0; i < ranked.size() && out.size() < 5; ++i)
        out.push_back(ranked[i].first);
    return out;
}

std::vector<std::pair<std::string, int>> Catalog::topDevelopers(const Query& query) const
{
    std::map<std::string, int> found;
    for (const App* app : matching(query)) {
        if (!app->developer.empty())
            found[app->developer] = 0;
    }
    // Each developer's count is their apps in the whole store.
    for (const App& app : m_apps) {
        const auto it = found.find(app.developer);
        if (it != found.end())
            ++it->second;
    }
    std::vector<std::pair<std::string, int>> ranked(found.begin(), found.end());
    std::stable_sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    if (ranked.size() > 3)
        ranked.resize(3);
    return ranked;
}

std::string plainText(const std::string& html)
{
    std::string out;
    for (size_t i = 0; i < html.size(); ++i) {
        if (html[i] != '<') {
            out += html[i];
            continue;
        }
        const size_t end = html.find('>', i);
        if (end == std::string::npos)
            break;
        const std::string tag = html.substr(i + 1, end - i - 1);
        if (tag == "br" || tag == "br/" || tag == "/p" || tag == "/li" || tag == "/ul")
            out += '\n';
        else if (tag == "li")
            out += "• ";
        i = end;
    }
    // Entities, the common ones.
    for (const auto& [from, to] : std::initializer_list<std::pair<const char*, const char*>>{
             {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&#39;", "'"}, {"&nbsp;", " "}}) {
        for (size_t at; (at = out.find(from)) != std::string::npos;)
            out.replace(at, std::string_view(from).size(), to);
    }
    // At most one blank line in a row, none at the ends.
    std::string tidy;
    int newlines = 0;
    for (char c : out) {
        if (c == '\n') {
            if (++newlines <= 2 && !tidy.empty())
                tidy += c;
            continue;
        }
        newlines = 0;
        tidy += c;
    }
    while (!tidy.empty() && (tidy.back() == '\n' || tidy.back() == ' '))
        tidy.pop_back();
    return tidy;
}

} // namespace store
