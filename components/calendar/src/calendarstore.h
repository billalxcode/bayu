// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "color.h"
#include "eventfile.h"

#include <base/file_watcher.h>
#include <base/signal.h>
#include <base/thread_pool.h>
#include <base/timer.h>

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct CalendarInfo
{
    std::string id;
    std::string name;
    Rgb color;
    // Where a subscribed calendar is published; empty for the user's own.
    std::string url;

    bool isReadOnly() const { return !url.empty(); }
};

// A subscribed calendar's address and how it was last fetched.
struct FeedState
{
    std::string url;
    std::string etag;
    std::string lastModified;
    // Seconds since the epoch of the last fetch the server answered.
    int64_t fetched = 0;
    // How often the calendar asks to be fetched; 0 when it does not say.
    int64_t refreshSeconds = 0;
};

// Calendars as vdir directories: one iCalendar file per event, name and colour
// in `displayname` and `color`. Changes made by others on disk are picked up.
// A subscribed calendar keeps its feed in `subscription` and is read-only:
// only replaceEvents() changes its events.
class CalendarStore
{
public:
    explicit CalendarStore(std::string root = defaultRoot());
    ~CalendarStore();

    CalendarStore(const CalendarStore&) = delete;
    CalendarStore& operator=(const CalendarStore&) = delete;

    static std::string defaultRoot();

    const std::string& root() const { return m_root; }
    void load();
    void loadAsync();
    bool isReady() const { return m_ready; }

    std::vector<CalendarInfo> calendars() const;
    bool hasCalendar(const std::string& id) const { return m_calendars.contains(id); }
    CalendarInfo calendar(const std::string& id) const;
    std::string createCalendar(const std::string& name, Rgb color);
    std::string createSubscription(const std::string& name, Rgb color, const FeedState& state);
    bool updateCalendar(const std::string& id, const std::string& name, Rgb color);
    // Moves the calendar's directory to the trash; a subscribed one is deleted.
    bool removeCalendar(const std::string& id);
    bool isReadOnly(const std::string& id) const;
    int eventCount(const std::string& calendarId) const;

    FeedState feedState(const std::string& id) const;
    bool setFeedState(const std::string& id, const FeedState& state);
    // Makes the calendar hold exactly these events, rewriting only the
    // changed ones. How many changed; -1 when there is no such calendar.
    int replaceEvents(const std::string& calendarId, std::vector<std::unique_ptr<EventFile>> files);

    std::vector<Occurrence> occurrences(Date from, Date to,
                                        const std::unordered_set<std::string>& hiddenCalendars = {}) const;
    std::vector<std::string> keys() const;
    const EventFile* event(const std::string& key) const;
    static std::string calendarOf(const std::string& key);

    std::string addEvent(const std::string& calendarId, std::unique_ptr<EventFile> file);
    std::string updateEvent(const std::string& key, std::unique_ptr<EventFile> file);
    std::string moveEvent(const std::string& key, const std::string& calendarId);
    bool removeEvent(const std::string& key);

    // Replaces events with the same UID; -1 when the data holds none.
    int importData(const std::string& calendarId, std::string_view data);
    std::string exportCalendar(const std::string& id) const;

    bayu::base::Signal<> readyChanged;
    bayu::base::Signal<> changed;

private:
    struct Entry
    {
        std::shared_ptr<const EventFile> file;
        std::filesystem::file_time_type modified;
        uintmax_t size = 0;
        // Days a single event touches; repeating events are always expanded.
        Date firstDay;
        Date lastDay;
        bool recurring = false;
    };
    struct Snapshot
    {
        std::unordered_map<std::string, CalendarInfo> calendars;
        std::unordered_map<std::string, Entry> events;
    };

    static Snapshot scan(const std::string& root, const std::unordered_map<std::string, Entry>& known);
    static Entry makeEntry(std::shared_ptr<const EventFile> file);
    void apply(Snapshot snapshot);
    void watch();
    void emitChanged();
    std::string insertEvent(const std::string& calendarId, std::unique_ptr<EventFile> file);
    std::string rewriteEvent(const std::string& key, std::unique_ptr<EventFile> file);
    bool eraseEvent(const std::string& key);
    bool write(const std::string& key, const EventFile& file);
    void stamp(const std::string& key, Entry* entry) const;
    std::string fileNameFor(const std::string& calendarId, const std::string& uid) const;
    std::string pathOf(const std::string& key) const;

    std::string m_root;
    bool m_ready = false;
    // One change for a batch of them, as an import makes.
    bool m_batching = false;
    std::unordered_map<std::string, CalendarInfo> m_calendars;
    std::unordered_map<std::string, Entry> m_events;
    std::map<std::string, bayu::base::FileWatch> m_watches;
    bayu::base::Timer m_rescan;
    bayu::base::Job m_scan;
};
