// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "calendarstore.h"
#include "subscriptions.h"

#include <base/settings.h>
#include <base/signal.h>
#include <base/timer.h>

#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

struct CalendarItem
{
    std::string id;
    std::string name;
    Rgb color;
    bool visible = true;
    // A subscription: its events are shown, not edited.
    bool readOnly = false;
};

// An occurrence as the views show it, with its place in a layout.
struct EventItem
{
    std::string key;
    std::string recurrenceKey;
    std::string title;
    std::string location;
    bool allDay = false;
    bool recurring = false;
    bool readOnly = false;
    Rgb color;
    std::string calendarId;
    Date firstDay;
    Date lastDay;
    // Timed events only.
    DateTime start;
    DateTime end;
    std::string startText;
    std::string endText;

    // In a row of days: the month's rows, or the all-day strip.
    int row = 0;
    int column = 0;
    int span = 1;
    int lane = 0;
    bool continuesBefore = false;
    bool continuesAfter = false;
    // Drawn as a bar: all day, or over more than one day.
    bool bar = false;
    // On the hour grid.
    int startMinute = 0;
    int endMinute = 0;
    int slot = 0;
    int slots = 1;
    // Search results: still to come.
    bool upcoming = false;
};

struct MoreItem
{
    int row = 0;
    int column = 0;
    int count = 0;
    Date date;
};

struct MonthLayout
{
    std::vector<EventItem> bars;
    std::vector<MoreItem> more;
};

struct DayLayout
{
    std::vector<EventItem> bars;
    std::vector<MoreItem> more;
    int lanes = 0;
    std::vector<EventItem> blocks;
};

// The fields the event editor shows and saves.
struct EventForm
{
    // Empty for a new event.
    std::string key;
    std::string recurrenceKey;
    std::string calendarId;
    bool recurring = false;
    bool firstOccurrence = true;
    // In a subscription: shown, not saved.
    bool readOnly = false;

    std::string title;
    std::string location;
    std::string notes;
    bool allDay = false;
    // Midnights of the first and last day for an all-day event.
    DateTime start;
    DateTime end;
    int repeat = Repeat::None;
    // 0 never, 1 on repeatUntil, 2 after repeatCount times.
    int repeatEnd = 0;
    Date repeatUntil;
    int repeatCount = 10;
    int alarm = EventData::NoAlarm;
    int alarmOffset = 0;
};

// The calendars, their events laid out for each view, the edits made to them
// and the calendar's own settings.
class CalendarModel
{
public:
    // The settings live in ~/.config/bayu/calendar.conf unless a path is given.
    explicit CalendarModel(CalendarStore* store, std::string settingsPath = {});

    CalendarModel(const CalendarModel&) = delete;
    CalendarModel& operator=(const CalendarModel&) = delete;

    CalendarStore* store() const { return m_store; }
    Subscriptions* subscriptions() { return &m_subscriptions; }

    bool isReady() const { return m_store->isReady(); }
    // Bumped on every change to calendars or events; layouts depend on it.
    int revision() const { return m_revision; }
    std::vector<CalendarItem> calendars() const;
    // The calendars events can be added to: the user's own, not subscriptions.
    std::vector<CalendarItem> writableCalendars() const;
    // Always one of the writable calendars.
    std::string defaultCalendar() const;
    void setDefaultCalendar(const std::string& id);
    Date today() const { return m_today; }
    // 0 follows the language; 1 to 7 is Monday to Sunday.
    int weekStart() const { return m_weekStart; }
    void setWeekStart(int day);
    int firstDayOfWeek() const;
    bool showWeekNumbers() const { return m_showWeekNumbers; }
    void setShowWeekNumbers(bool show);
    bool use24Hour() const;
    // Days of the weekend as the language counts them, 0 for Sunday to 6 for Saturday.
    std::vector<int> weekendDays() const;
    const std::string& view() const { return m_view; }
    void setView(const std::string& view);
    // Whether the sidebar is wanted; a narrow window hides it anyway.
    bool sidebarVisible() const { return m_sidebarVisible; }
    void setSidebarVisible(bool visible);
    bool canUndo() const { return !m_undo.empty(); }
    std::string undoText() const;

    MonthLayout monthLayout(Date first, int rows, int lanes) const;
    DayLayout dayLayout(Date first, int days, int lanes) const;
    std::vector<bool> busyDays(Date first, int days) const;
    std::vector<EventItem> search(const std::string& text) const;
    int weekNumber(Date rowStart) const;

    std::optional<EventForm> eventData(const std::string& key, const std::string& recurrenceKey) const;
    EventForm newEventData(const DateTime& start, const DateTime& end, bool allDay) const;
    bool isFirstOccurrence(const std::string& key, const std::string& recurrenceKey) const;
    // Scope is EventFile::Scope: 0 this event, 1 this and later ones, 2 all.
    bool saveEvent(const EventForm& form, int scope);
    bool removeEvent(const std::string& key, const std::string& recurrenceKey, int scope);
    // Moves an occurrence's start and end by whole days and minutes, as a drag does.
    bool moveOccurrence(const std::string& key, const std::string& recurrenceKey, int startDays, int startMinutes,
                        int endDays, int endMinutes, int scope);
    void undo();

    std::string createCalendar(const std::string& name, Rgb color);
    bool updateCalendar(const std::string& id, const std::string& name, Rgb color);
    // The last of the user's own calendars stays; subscriptions can always go.
    bool canRemoveCalendar(const std::string& id) const;
    bool removeCalendar(const std::string& id);
    void setCalendarVisible(const std::string& id, bool visible);
    int eventCount(const std::string& id) const;
    // A path or a file URL. How many events the file had, -1 when it was not a calendar.
    int importFile(const std::string& file, const std::string& calendarId);
    bool exportCalendar(const std::string& id, const std::string& file) const;
    std::string exportFileName(const std::string& id) const;

    // From a second start or the D-Bus interface.
    void activateWindow();
    void open(const std::vector<std::string>& files, const std::string& date);

    bayu::base::Signal<> readyChanged;
    bayu::base::Signal<> revisionChanged;
    bayu::base::Signal<> defaultCalendarChanged;
    bayu::base::Signal<> todayChanged;
    bayu::base::Signal<> weekStartChanged;
    bayu::base::Signal<> showWeekNumbersChanged;
    bayu::base::Signal<> viewChanged;
    bayu::base::Signal<> sidebarVisibleChanged;
    bayu::base::Signal<> undoChanged;
    bayu::base::Signal<> activationRequested;
    bayu::base::Signal<Date> dateRequested;
    bayu::base::Signal<std::vector<std::string>> importRequested;

private:
    struct Change
    {
        std::string key;
        // The file as it was; null when the change created it.
        std::shared_ptr<EventFile> before;
        // Where it is now; empty when the change removed it.
        std::string after;
    };
    struct UndoStep
    {
        // Untranslated, so that it is shown in the language of the moment.
        const char* text;
        std::vector<Change> changes;
    };

    EventItem itemFor(const Occurrence& occurrence) const;
    void pushUndo(const char* text, std::vector<Change> changes);
    void ensureCalendar();
    void saveHidden();

    CalendarStore* m_store;
    Subscriptions m_subscriptions;
    bayu::base::Settings m_settings;
    int m_revision = 0;
    Date m_today;
    bayu::base::Timer m_dayTimer;
    int m_weekStart = 0;
    bool m_showWeekNumbers = false;
    std::string m_view;
    bool m_sidebarVisible = true;
    std::unordered_set<std::string> m_hidden;
    std::vector<UndoStep> m_undo;
    bayu::base::Subscription m_readySubscription;
    bayu::base::Subscription m_changedSubscription;
};
