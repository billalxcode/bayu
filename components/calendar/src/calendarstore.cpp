// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "calendarstore.h"
#include "sha1.h"
#include "text.h"
#include "trash.h"
#include "uuid.h"

#include <base/file.h>
#include <base/paths.h>
#include <base/settings.h>

#include <algorithm>
#include <regex>

namespace fs = std::filesystem;
namespace base = bayu::base;

static const Rgb DefaultColor{0x3c, 0x81, 0xf6};
static const char SubscriptionFile[] = "/subscription";

static FeedState readFeedState(const std::string& directory)
{
    FeedState state;
    std::error_code error;
    if (!fs::is_regular_file(directory + SubscriptionFile, error))
        return state;
    const base::Settings settings(directory + SubscriptionFile);
    state.url = settings.stringValue("url");
    state.etag = settings.stringValue("etag");
    state.lastModified = settings.stringValue("lastModified");
    state.fetched = settings.intValue("fetched");
    state.refreshSeconds = settings.intValue("refreshSeconds");
    return state;
}

static bool writeFeedState(const std::string& directory, const FeedState& state)
{
    base::Settings settings(directory + SubscriptionFile);
    settings.setValue("url", state.url);
    settings.setValue("etag", state.etag);
    settings.setValue("lastModified", state.lastModified);
    settings.setValue("fetched", state.fetched);
    settings.setValue("refreshSeconds", state.refreshSeconds);
    return settings.sync();
}

static std::string readSmallFile(const std::string& path)
{
    const std::optional<std::string> contents = base::file::read(path);
    return contents ? trimmed(std::string_view(*contents).substr(0, 4096)) : std::string();
}

static bool writeSmallFile(const std::string& path, const std::string& text)
{
    return base::file::writeAtomically(path, text + '\n');
}

CalendarStore::CalendarStore(std::string root)
    : m_root(std::move(root))
{
    m_rescan.setSingleShot(true);
    m_rescan.setInterval(std::chrono::milliseconds(250));
    m_rescan.onTimeout = [this] {
        apply(scan(m_root, m_events));
    };
}

CalendarStore::~CalendarStore() = default;

std::string CalendarStore::defaultRoot()
{
    return base::paths::dataHome() + "/bayu-calendar/calendars";
}

CalendarStore::Entry CalendarStore::makeEntry(std::shared_ptr<const EventFile> file)
{
    Entry entry;
    entry.recurring = file->isRecurring();
    if (!entry.recurring) {
        std::vector<Occurrence> occurrences;
        file->expand(Date(1, 1, 1), Date(9999, 12, 31), &occurrences);
        if (!occurrences.empty()) {
            entry.firstDay = occurrences.front().firstDay;
            entry.lastDay = occurrences.front().lastDay;
        }
    }
    entry.file = std::move(file);
    return entry;
}

CalendarStore::Snapshot CalendarStore::scan(const std::string& root, const std::unordered_map<std::string, Entry>& known)
{
    Snapshot snapshot;
    std::error_code error;
    for (const fs::directory_entry& directory : fs::directory_iterator(root, error)) {
        if (!directory.is_directory(error))
            continue;
        const std::string id = directory.path().filename().string();
        if (id.starts_with('.'))
            continue;
        const std::string path = directory.path().string();
        CalendarInfo info;
        info.id = id;
        info.name = readSmallFile(path + "/displayname");
        if (info.name.empty())
            info.name = id;
        info.color = Rgb::fromName(std::string_view(readSmallFile(path + "/color")).substr(0, 7)).value_or(DefaultColor);
        info.url = readFeedState(path).url;
        snapshot.calendars.emplace(id, info);

        for (const fs::directory_entry& file : fs::directory_iterator(path, error)) {
            const std::string name = file.path().filename().string();
            if (name.starts_with('.') || file.path().extension() != ".ics" || !file.is_regular_file(error))
                continue;
            const std::string key = id + '/' + name;
            const fs::file_time_type modified = file.last_write_time(error);
            const uintmax_t size = file.file_size(error);
            const auto it = known.find(key);
            if (it != known.end() && it->second.modified == modified && it->second.size == size) {
                snapshot.events.emplace(key, it->second);
                continue;
            }
            const std::optional<std::string> data = base::file::read(file.path().string());
            if (!data)
                continue;
            std::shared_ptr<const EventFile> event = EventFile::fromData(*data);
            if (!event)
                continue;
            Entry entry = makeEntry(std::move(event));
            entry.modified = modified;
            entry.size = size;
            snapshot.events.emplace(key, entry);
        }
    }
    return snapshot;
}

void CalendarStore::load()
{
    apply(scan(m_root, {}));
}

void CalendarStore::loadAsync()
{
    m_scan = base::runInThread([root = m_root] { return scan(root, {}); },
                               [this](Snapshot snapshot) { apply(std::move(snapshot)); });
}

void CalendarStore::apply(Snapshot snapshot)
{
    m_calendars = std::move(snapshot.calendars);
    m_events = std::move(snapshot.events);
    watch();
    if (!m_ready) {
        m_ready = true;
        readyChanged.emit();
    }
    emitChanged();
}

void CalendarStore::emitChanged()
{
    if (!m_batching)
        changed.emit();
}

void CalendarStore::watch()
{
    std::error_code error;
    fs::create_directories(m_root, error);
    std::vector<std::string> paths{m_root};
    for (const auto& [id, info] : m_calendars)
        paths.push_back(m_root + '/' + id);

    for (auto it = m_watches.begin(); it != m_watches.end();) {
        if (std::find(paths.begin(), paths.end(), it->first) == paths.end())
            it = m_watches.erase(it);
        else
            ++it;
    }
    for (const std::string& path : paths) {
        if (m_watches.contains(path))
            continue;
        m_watches.emplace(path, base::FileWatch::directory(path, [this](const std::string&) {
            m_rescan.start();
        }));
    }
}

std::vector<CalendarInfo> CalendarStore::calendars() const
{
    std::vector<CalendarInfo> list;
    list.reserve(m_calendars.size());
    for (const auto& [id, info] : m_calendars)
        list.push_back(info);
    std::sort(list.begin(), list.end(), [](const CalendarInfo& a, const CalendarInfo& b) {
        const int order = naturalCompare(a.name, b.name);
        return order != 0 ? order < 0 : a.id < b.id;
    });
    return list;
}

CalendarInfo CalendarStore::calendar(const std::string& id) const
{
    const auto it = m_calendars.find(id);
    return it == m_calendars.end() ? CalendarInfo() : it->second;
}

std::string CalendarStore::createCalendar(const std::string& name, Rgb color)
{
    const std::string id = createUuid();
    const std::string path = m_root + '/' + id;
    std::error_code error;
    fs::create_directories(path, error);
    if (error)
        return {};
    writeSmallFile(path + "/displayname", name);
    writeSmallFile(path + "/color", color.name());
    m_calendars[id] = CalendarInfo{id, name, color};
    watch();
    emitChanged();
    return id;
}

std::string CalendarStore::createSubscription(const std::string& name, Rgb color, const FeedState& state)
{
    if (state.url.empty())
        return {};
    const std::string id = createUuid();
    const std::string path = m_root + '/' + id;
    std::error_code error;
    fs::create_directories(path, error);
    if (error)
        return {};
    // The feed first: a rescan in between never sees it as one of the user's own.
    if (!writeFeedState(path, state)) {
        fs::remove_all(path, error);
        return {};
    }
    writeSmallFile(path + "/displayname", name);
    writeSmallFile(path + "/color", color.name());
    m_calendars[id] = CalendarInfo{id, name, color, state.url};
    watch();
    emitChanged();
    return id;
}

bool CalendarStore::updateCalendar(const std::string& id, const std::string& name, Rgb color)
{
    const auto it = m_calendars.find(id);
    if (it == m_calendars.end())
        return false;
    const std::string path = m_root + '/' + id;
    if (!writeSmallFile(path + "/displayname", name) || !writeSmallFile(path + "/color", color.name()))
        return false;
    it->second.name = name;
    it->second.color = color;
    emitChanged();
    return true;
}

bool CalendarStore::removeCalendar(const std::string& id)
{
    const auto it = m_calendars.find(id);
    if (it == m_calendars.end())
        return false;
    const std::string path = m_root + '/' + id;
    std::error_code error;
    // A subscribed calendar's events come back from its feed.
    if (it->second.isReadOnly()) {
        if (fs::remove_all(path, error), error)
            return false;
    } else if (!moveToTrash(path) && (fs::remove_all(path, error), error)) {
        return false;
    }
    m_calendars.erase(id);
    std::erase_if(m_events, [&id](const auto& item) {
        return calendarOf(item.first) == id;
    });
    watch();
    emitChanged();
    return true;
}

bool CalendarStore::isReadOnly(const std::string& id) const
{
    const auto it = m_calendars.find(id);
    return it != m_calendars.end() && it->second.isReadOnly();
}

FeedState CalendarStore::feedState(const std::string& id) const
{
    if (!isReadOnly(id))
        return {};
    return readFeedState(m_root + '/' + id);
}

bool CalendarStore::setFeedState(const std::string& id, const FeedState& state)
{
    if (!isReadOnly(id) || state.url.empty())
        return false;
    if (!writeFeedState(m_root + '/' + id, state))
        return false;
    m_calendars[id].url = state.url;
    return true;
}

int CalendarStore::replaceEvents(const std::string& calendarId, std::vector<std::unique_ptr<EventFile>> files)
{
    if (!m_calendars.contains(calendarId))
        return -1;

    std::unordered_map<std::string, std::string> byUid;
    for (const auto& [key, entry] : m_events) {
        if (calendarOf(key) == calendarId)
            byUid[entry.file->uid()] = key;
    }

    int count = 0;
    std::unordered_set<std::string> kept;
    m_batching = true;
    for (std::unique_ptr<EventFile>& file : files) {
        const auto existing = byUid.find(file->uid());
        if (existing == byUid.end()) {
            if (!insertEvent(calendarId, std::move(file)).empty())
                ++count;
            continue;
        }
        kept.insert(existing->second);
        if (m_events[existing->second].file->toData() == file->toData())
            continue;
        if (!rewriteEvent(existing->second, std::move(file)).empty())
            ++count;
    }
    for (const auto& [uid, key] : byUid) {
        if (!kept.contains(key) && eraseEvent(key))
            ++count;
    }
    m_batching = false;
    if (count > 0)
        emitChanged();
    return count;
}

int CalendarStore::eventCount(const std::string& calendarId) const
{
    int count = 0;
    for (const auto& [key, entry] : m_events) {
        if (calendarOf(key) == calendarId)
            ++count;
    }
    return count;
}

std::vector<Occurrence> CalendarStore::occurrences(Date from, Date to,
                                                   const std::unordered_set<std::string>& hiddenCalendars) const
{
    std::vector<Occurrence> result;
    for (const auto& [key, entry] : m_events) {
        const std::string calendarId = calendarOf(key);
        if (hiddenCalendars.contains(calendarId) || !m_calendars.contains(calendarId))
            continue;
        if (!entry.recurring && (!entry.firstDay.isValid() || entry.firstDay >= to || entry.lastDay < from))
            continue;
        const size_t start = result.size();
        entry.file->expand(from, to, &result);
        for (size_t i = start; i < result.size(); ++i) {
            result[i].key = key;
            result[i].calendarId = calendarId;
        }
    }
    return result;
}

std::vector<std::string> CalendarStore::keys() const
{
    std::vector<std::string> list;
    list.reserve(m_events.size());
    for (const auto& [key, entry] : m_events)
        list.push_back(key);
    return list;
}

const EventFile* CalendarStore::event(const std::string& key) const
{
    const auto it = m_events.find(key);
    return it == m_events.end() ? nullptr : it->second.file.get();
}

std::string CalendarStore::calendarOf(const std::string& key)
{
    return key.substr(0, key.find('/'));
}

std::string CalendarStore::pathOf(const std::string& key) const
{
    return m_root + '/' + key;
}

std::string CalendarStore::fileNameFor(const std::string& calendarId, const std::string& uid) const
{
    static const std::regex safe("^[A-Za-z0-9@._-]{1,96}$");
    const std::string base = std::regex_match(uid, safe) ? uid : sha1Hex(uid);
    // A file this store could not read is on disk all the same.
    const auto taken = [&](const std::string& name) {
        std::error_code error;
        return m_events.contains(calendarId + '/' + name) || fs::exists(pathOf(calendarId + '/' + name), error);
    };
    std::string name = base + ".ics";
    for (int i = 2; taken(name); ++i)
        name = base + '-' + std::to_string(i) + ".ics";
    return name;
}

bool CalendarStore::write(const std::string& key, const EventFile& file)
{
    return base::file::writeAtomically(pathOf(key), file.toData());
}

void CalendarStore::stamp(const std::string& key, Entry* entry) const
{
    std::error_code error;
    entry->modified = fs::last_write_time(pathOf(key), error);
    entry->size = fs::file_size(pathOf(key), error);
}

std::string CalendarStore::insertEvent(const std::string& calendarId, std::unique_ptr<EventFile> file)
{
    if (!m_calendars.contains(calendarId) || !file)
        return {};
    const std::string key = calendarId + '/' + fileNameFor(calendarId, file->uid());
    if (!write(key, *file))
        return {};
    Entry entry = makeEntry(std::move(file));
    stamp(key, &entry);
    m_events[key] = std::move(entry);
    emitChanged();
    return key;
}

std::string CalendarStore::rewriteEvent(const std::string& key, std::unique_ptr<EventFile> file)
{
    if (!m_events.contains(key) || !file || !write(key, *file))
        return {};
    Entry entry = makeEntry(std::move(file));
    stamp(key, &entry);
    m_events[key] = std::move(entry);
    emitChanged();
    return key;
}

bool CalendarStore::eraseEvent(const std::string& key)
{
    if (!m_events.contains(key))
        return false;
    std::error_code error;
    if (!fs::remove(pathOf(key), error) && fs::exists(pathOf(key), error))
        return false;
    m_events.erase(key);
    emitChanged();
    return true;
}

std::string CalendarStore::addEvent(const std::string& calendarId, std::unique_ptr<EventFile> file)
{
    if (isReadOnly(calendarId))
        return {};
    return insertEvent(calendarId, std::move(file));
}

std::string CalendarStore::updateEvent(const std::string& key, std::unique_ptr<EventFile> file)
{
    if (isReadOnly(calendarOf(key)))
        return {};
    return rewriteEvent(key, std::move(file));
}

std::string CalendarStore::moveEvent(const std::string& key, const std::string& calendarId)
{
    const EventFile* file = event(key);
    if (!file || !m_calendars.contains(calendarId) || isReadOnly(calendarOf(key)) || isReadOnly(calendarId))
        return {};
    if (calendarOf(key) == calendarId)
        return key;
    const std::string moved = addEvent(calendarId, file->clone());
    if (moved.empty())
        return {};
    removeEvent(key);
    return moved;
}

bool CalendarStore::removeEvent(const std::string& key)
{
    if (isReadOnly(calendarOf(key)))
        return false;
    return eraseEvent(key);
}

int CalendarStore::importData(const std::string& calendarId, std::string_view data)
{
    if (!m_calendars.contains(calendarId) || isReadOnly(calendarId))
        return -1;
    std::vector<std::unique_ptr<EventFile>> files = EventFile::split(data);
    if (files.empty())
        return -1;

    std::unordered_map<std::string, std::string> byUid;
    for (const auto& [key, entry] : m_events) {
        if (calendarOf(key) == calendarId)
            byUid[entry.file->uid()] = key;
    }

    int count = 0;
    m_batching = true;
    for (std::unique_ptr<EventFile>& file : files) {
        const auto existing = byUid.find(file->uid());
        const std::string key = existing == byUid.end() ? addEvent(calendarId, std::move(file))
                                                        : updateEvent(existing->second, std::move(file));
        if (!key.empty())
            ++count;
    }
    m_batching = false;
    emitChanged();
    return count;
}

std::string CalendarStore::exportCalendar(const std::string& id) const
{
    std::vector<std::pair<std::string, const EventFile*>> files;
    for (const auto& [key, entry] : m_events) {
        if (calendarOf(key) == id)
            files.emplace_back(key, entry.file.get());
    }
    // Stable output, whatever order the events are kept in.
    std::sort(files.begin(), files.end());
    std::vector<const EventFile*> list;
    for (const auto& [key, file] : files)
        list.push_back(file);
    return EventFile::join(list);
}
