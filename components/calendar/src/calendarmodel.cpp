// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "calendarmodel.h"
#include "eventlayout.h"
#include "localedata.h"
#include "text.h"
#include "userdirs.h"

#include <base/file.h>
#include <base/i18n.h>
#include <base/paths.h>

#include <algorithm>

using bayu::i18n::tr;
using bayu::i18n::trNoop;

static const size_t UndoDepth = 20;
// How far ahead search looks for a repeating event's next occurrence, in days.
static const int SearchHorizon = 3 * 366;

static DateTime startOfDay(Date date)
{
    return DateTime(date, Time(0, 0));
}

static EventData eventDataFrom(const EventForm& form)
{
    EventData data;
    data.summary = form.title;
    data.location = form.location;
    data.description = form.notes;
    data.allDay = form.allDay;
    const DateTime start = form.start.toLocalTime();
    const DateTime end = form.end.toLocalTime();
    if (data.allDay) {
        data.startDate = start.date();
        data.endDate = std::max(end.date(), start.date());
    } else {
        data.start = start;
        data.end = std::max(end, start);
    }

    data.repeat.frequency = Repeat::Frequency(form.repeat);
    switch (form.repeatEnd) {
    case 1:
        data.repeat.until = form.repeatUntil;
        break;
    case 2:
        data.repeat.count = std::max(1, form.repeatCount);
        break;
    default:
        break;
    }
    data.alarm = EventData::Alarm(form.alarm);
    data.alarmOffset = form.alarmOffset;
    return data;
}

static EventForm formFrom(const EventData& data)
{
    EventForm form;
    form.title = data.summary;
    form.location = data.location;
    form.notes = data.description;
    form.allDay = data.allDay;
    form.start = data.allDay ? startOfDay(data.startDate) : data.start;
    form.end = data.allDay ? startOfDay(data.endDate) : data.end;
    form.repeat = int(data.repeat.frequency);
    form.repeatEnd = data.repeat.count > 0 ? 2 : data.repeat.until.isValid() ? 1 : 0;
    form.repeatUntil = data.repeat.until.isValid() ? data.repeat.until
                                                   : (data.allDay ? data.startDate : data.start.date()).addMonths(1);
    form.repeatCount = data.repeat.count > 0 ? data.repeat.count : 10;
    form.alarm = int(data.alarm);
    form.alarmOffset = data.alarmOffset;
    return form;
}

// The fields of the whole series when `data` is what one occurrence should become.
static EventData seriesData(const EventFile& file, const std::string& recurrenceKey, const EventData& data)
{
    EventData series = file.data();
    if (recurrenceKey.empty()) {
        series = data;
        return series;
    }

    std::unique_ptr<EventFile> original = file.clone();
    original->resetOccurrence(recurrenceKey);
    const EventData instance = original->data(recurrenceKey);

    EventData result = data;
    const Date instanceDay = instance.allDay ? instance.startDate : instance.start.date();
    const Date seriesDay = series.allDay ? series.startDate : series.start.date();
    if (data.allDay) {
        result.startDate = seriesDay.addDays(instanceDay.daysTo(data.startDate));
        result.endDate = result.startDate.addDays(data.startDate.daysTo(data.endDate));
    } else if (instance.allDay) {
        result.start = DateTime(seriesDay.addDays(instanceDay.daysTo(data.start.date())), data.start.time());
        result.end = result.start.addSecs(data.start.secsTo(data.end));
    } else {
        result.start = series.start.addSecs(instance.start.secsTo(data.start));
        result.end = result.start.addSecs(data.start.secsTo(data.end));
    }
    return result;
}

// A file URL as a path; anything else as it is.
static std::string localPath(const std::string& file)
{
    if (!file.starts_with("file://"))
        return file;
    std::string path;
    const std::string_view encoded = std::string_view(file).substr(7);
    for (size_t i = 0; i < encoded.size(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.size()) {
            path.push_back(char(std::stoi(std::string(encoded.substr(i + 1, 2)), nullptr, 16)));
            i += 2;
        } else {
            path.push_back(encoded[i]);
        }
    }
    return path;
}

CalendarModel::CalendarModel(CalendarStore* store, std::string settingsPath)
    : m_store(store)
    , m_subscriptions(store)
    , m_settings(settingsPath.empty() ? bayu::base::paths::bayuConfig("calendar") : std::move(settingsPath))
    , m_today(Date::currentDate())
{
    m_weekStart = std::clamp(int(m_settings.intValue("weekStart", 0)), 0, 7);
    m_showWeekNumbers = m_settings.boolValue("showWeekNumbers", false);
    m_view = m_settings.stringValue("view", "month");
    m_sidebarVisible = m_settings.boolValue("sidebarVisible", true);
    for (const std::string& id : m_settings.stringListValue("Calendars/hidden"))
        m_hidden.insert(id);

    m_readySubscription = m_store->readyChanged.connect([this] {
        ensureCalendar();
        readyChanged.emit();
    });
    m_changedSubscription = m_store->changed.connect([this] {
        ++m_revision;
        revisionChanged.emit();
    });

    // A day changes at midnight, after a suspend or when the clock is set.
    m_dayTimer.setInterval(std::chrono::seconds(30));
    m_dayTimer.onTimeout = [this] {
        if (Date::currentDate() != m_today) {
            m_today = Date::currentDate();
            todayChanged.emit();
        }
    };
    m_dayTimer.start();
}

void CalendarModel::ensureCalendar()
{
    if (writableCalendars().empty())
        m_store->createCalendar(tr("Personal"), Rgb{0x33, 0x85, 0xff});
}

std::vector<CalendarItem> CalendarModel::calendars() const
{
    std::vector<CalendarItem> list;
    for (const CalendarInfo& calendar : m_store->calendars())
        list.push_back({calendar.id, calendar.name, calendar.color, !m_hidden.contains(calendar.id),
                        calendar.isReadOnly()});
    return list;
}

std::vector<CalendarItem> CalendarModel::writableCalendars() const
{
    std::vector<CalendarItem> list = calendars();
    std::erase_if(list, [](const CalendarItem& calendar) { return calendar.readOnly; });
    return list;
}

std::string CalendarModel::defaultCalendar() const
{
    const std::string id = m_settings.stringValue("defaultCalendar");
    if (m_store->hasCalendar(id) && !m_store->isReadOnly(id))
        return id;
    const std::vector<CalendarItem> calendars = writableCalendars();
    return calendars.empty() ? std::string() : calendars.front().id;
}

void CalendarModel::setDefaultCalendar(const std::string& id)
{
    if (id == m_settings.stringValue("defaultCalendar"))
        return;
    m_settings.setValue("defaultCalendar", id);
    m_settings.sync();
    defaultCalendarChanged.emit();
}

void CalendarModel::setWeekStart(int day)
{
    day = std::clamp(day, 0, 7);
    if (day == m_weekStart)
        return;
    m_weekStart = day;
    m_settings.setValue("weekStart", day);
    m_settings.sync();
    weekStartChanged.emit();
}

int CalendarModel::firstDayOfWeek() const
{
    return m_weekStart > 0 ? m_weekStart : LocaleData::firstDayOfWeek();
}

void CalendarModel::setShowWeekNumbers(bool show)
{
    if (show == m_showWeekNumbers)
        return;
    m_showWeekNumbers = show;
    m_settings.setValue("showWeekNumbers", show);
    m_settings.sync();
    showWeekNumbersChanged.emit();
}

bool CalendarModel::use24Hour() const
{
    return LocaleData::use24Hour();
}

std::vector<int> CalendarModel::weekendDays() const
{
    return LocaleData::weekendDays();
}

void CalendarModel::setView(const std::string& view)
{
    if (view == m_view)
        return;
    m_view = view;
    m_settings.setValue("view", view);
    m_settings.sync();
    viewChanged.emit();
}

void CalendarModel::setSidebarVisible(bool visible)
{
    if (visible == m_sidebarVisible)
        return;
    m_sidebarVisible = visible;
    m_settings.setValue("sidebarVisible", visible);
    m_settings.sync();
    sidebarVisibleChanged.emit();
}

EventItem CalendarModel::itemFor(const Occurrence& occurrence) const
{
    EventItem item;
    item.key = occurrence.key;
    item.recurrenceKey = occurrence.recurrenceKey;
    item.title = occurrence.summary;
    item.location = occurrence.location;
    item.allDay = occurrence.allDay;
    item.recurring = occurrence.recurring;
    item.readOnly = m_store->isReadOnly(occurrence.calendarId);
    item.color = m_store->calendar(occurrence.calendarId).color;
    item.calendarId = occurrence.calendarId;
    item.firstDay = occurrence.firstDay;
    item.lastDay = occurrence.lastDay;
    if (!occurrence.allDay) {
        item.start = occurrence.start;
        item.end = occurrence.end;
        item.startText = LocaleData::timeText(occurrence.start.time());
        item.endText = LocaleData::timeText(occurrence.end.time());
    }
    return item;
}

MonthLayout CalendarModel::monthLayout(Date first, int rows, int lanes) const
{
    const std::vector<Occurrence> occurrences = m_store->occurrences(first, first.addDays(int64_t(rows) * 7), m_hidden);
    const EventLayout::Rows layout = EventLayout::layoutRows(occurrences, first, rows, lanes);

    MonthLayout result;
    for (const EventLayout::Bar& bar : layout.bars) {
        const Occurrence& occurrence = occurrences[bar.index];
        EventItem item = itemFor(occurrence);
        item.row = bar.row;
        item.column = bar.column;
        item.span = bar.span;
        item.lane = bar.lane;
        item.continuesBefore = bar.continuesBefore;
        item.continuesAfter = bar.continuesAfter;
        item.bar = occurrence.allDay || occurrence.firstDay != occurrence.lastDay;
        result.bars.push_back(std::move(item));
    }
    for (const EventLayout::More& more : layout.more)
        result.more.push_back({more.row, more.column, more.count, first.addDays(int64_t(more.row) * 7 + more.column)});
    return result;
}

DayLayout CalendarModel::dayLayout(Date first, int days, int lanes) const
{
    const std::vector<Occurrence> occurrences = m_store->occurrences(first, first.addDays(days), m_hidden);
    const EventLayout::Rows strip = EventLayout::layoutRows(occurrences, first, 1, lanes, true, days);

    DayLayout result;
    for (const EventLayout::Bar& bar : strip.bars) {
        EventItem item = itemFor(occurrences[bar.index]);
        item.column = bar.column;
        item.span = bar.span;
        item.lane = bar.lane;
        item.continuesBefore = bar.continuesBefore;
        item.continuesAfter = bar.continuesAfter;
        item.bar = true;
        result.bars.push_back(std::move(item));
    }
    for (const EventLayout::More& more : strip.more)
        result.more.push_back({0, more.column, more.count, first.addDays(more.column)});
    result.lanes = strip.lanes;

    for (const EventLayout::Block& block : EventLayout::layoutDays(occurrences, first, days, 30)) {
        EventItem item = itemFor(occurrences[block.index]);
        item.column = block.column;
        item.startMinute = block.startMinute;
        item.endMinute = block.endMinute;
        item.slot = block.slot;
        item.slots = block.slotCount;
        item.span = block.span;
        item.continuesBefore = block.continuesBefore;
        item.continuesAfter = block.continuesAfter;
        result.blocks.push_back(std::move(item));
    }
    return result;
}

std::vector<bool> CalendarModel::busyDays(Date first, int days) const
{
    std::vector<bool> busy(size_t(std::max(0, days)), false);
    for (const Occurrence& occurrence : m_store->occurrences(first, first.addDays(days), m_hidden)) {
        const int64_t from = std::max<int64_t>(0, first.daysTo(occurrence.firstDay));
        const int64_t to = std::min<int64_t>(days - 1, first.daysTo(occurrence.lastDay));
        for (int64_t day = from; day <= to; ++day)
            busy[size_t(day)] = true;
    }
    return busy;
}

std::vector<EventItem> CalendarModel::search(const std::string& text) const
{
    const std::string needle = trimmed(text);
    if (needle.empty())
        return {};

    struct Match
    {
        Occurrence occurrence;
        DateTime start;
        bool upcoming = false;
    };
    std::vector<Match> matches;
    const DateTime now = DateTime::currentDateTime();
    for (const std::string& key : m_store->keys()) {
        const std::string calendarId = CalendarStore::calendarOf(key);
        if (m_hidden.contains(calendarId))
            continue;
        const EventFile* file = m_store->event(key);
        if (!containsIgnoringCase(file->summary(), needle) && !containsIgnoringCase(file->location(), needle)
                && !containsIgnoringCase(file->description(), needle))
            continue;

        Match match;
        match.upcoming = file->nextOccurrence(startOfDay(now.date()), SearchHorizon, &match.occurrence);
        if (!match.upcoming) {
            // Over by now: shown at its first occurrence, which may come after a removed or moved one.
            const EventData data = file->data();
            const Date day = data.allDay ? data.startDate : data.start.date();
            if (!file->nextOccurrence(startOfDay(day), SearchHorizon, &match.occurrence))
                continue;
        }
        match.occurrence.key = key;
        match.occurrence.calendarId = calendarId;
        match.start = match.occurrence.allDay ? startOfDay(match.occurrence.firstDay) : match.occurrence.start;
        matches.push_back(std::move(match));
    }

    // What is coming first, soonest first, then what is over, latest first.
    std::stable_sort(matches.begin(), matches.end(), [](const Match& a, const Match& b) {
        if (a.upcoming != b.upcoming)
            return a.upcoming;
        return a.upcoming ? a.start < b.start : a.start > b.start;
    });
    std::vector<EventItem> list;
    for (const Match& match : matches) {
        EventItem item = itemFor(match.occurrence);
        item.upcoming = match.upcoming;
        list.push_back(std::move(item));
    }
    return list;
}

int CalendarModel::weekNumber(Date rowStart) const
{
    // The ISO week of the row's Thursday, whichever day the row starts on.
    for (int i = 0; i < 7; ++i) {
        if (rowStart.addDays(i).dayOfWeek() == 4)
            return rowStart.addDays(i).weekNumber();
    }
    return rowStart.weekNumber();
}

std::optional<EventForm> CalendarModel::eventData(const std::string& key, const std::string& recurrenceKey) const
{
    const EventFile* file = m_store->event(key);
    if (!file)
        return std::nullopt;
    EventForm form = formFrom(file->data(recurrenceKey));
    form.key = key;
    form.recurrenceKey = recurrenceKey;
    form.calendarId = CalendarStore::calendarOf(key);
    form.recurring = file->isRecurring();
    form.firstOccurrence = recurrenceKey.empty() || file->isFirstOccurrence(recurrenceKey);
    form.readOnly = m_store->isReadOnly(form.calendarId);
    return form;
}

EventForm CalendarModel::newEventData(const DateTime& start, const DateTime& end, bool allDay) const
{
    EventData data;
    data.allDay = allDay;
    if (allDay) {
        data.startDate = start.toLocalTime().date();
        data.endDate = std::max(end.toLocalTime().date(), data.startDate);
    } else {
        data.start = start.toLocalTime();
        data.end = std::max(end.toLocalTime(), data.start);
    }
    EventForm form = formFrom(data);
    form.calendarId = defaultCalendar();
    return form;
}

bool CalendarModel::isFirstOccurrence(const std::string& key, const std::string& recurrenceKey) const
{
    const EventFile* file = m_store->event(key);
    return !file || recurrenceKey.empty() || file->isFirstOccurrence(recurrenceKey);
}

bool CalendarModel::saveEvent(const EventForm& form, int scope)
{
    const std::string& key = form.key;
    const std::string& recurrenceKey = form.recurrenceKey;
    if (!key.empty() && m_store->isReadOnly(CalendarStore::calendarOf(key)))
        return false;
    std::string calendarId = form.calendarId;
    if (!m_store->hasCalendar(calendarId) || m_store->isReadOnly(calendarId))
        calendarId = defaultCalendar();
    const EventData data = eventDataFrom(form);

    if (key.empty()) {
        const std::string added = m_store->addEvent(calendarId, EventFile::create(data));
        if (added.empty())
            return false;
        setDefaultCalendar(calendarId);
        pushUndo(trNoop("Undo New Event"), {{added, nullptr, added}});
        return true;
    }

    const EventFile* file = m_store->event(key);
    if (!file)
        return false;
    std::shared_ptr<EventFile> before = file->clone();
    std::unique_ptr<EventFile> changed = file->clone();
    std::vector<Change> changes;

    if (!file->isRecurring() || recurrenceKey.empty() || scope == EventFile::AllEvents
            || (scope == EventFile::FutureEvents && file->isFirstOccurrence(recurrenceKey))) {
        const EventData series = seriesData(*file, recurrenceKey, data);
        if (!recurrenceKey.empty())
            changed->resetOccurrence(recurrenceKey);
        changed->setData(series);
    } else if (scope == EventFile::ThisEvent) {
        changed->setOccurrenceData(recurrenceKey, data);
    } else {
        std::unique_ptr<EventFile> rest = changed->splitAt(recurrenceKey, data);
        if (!rest)
            return false;
        const std::string added = m_store->addEvent(calendarId, std::move(rest));
        if (added.empty())
            return false;
        changes.push_back({added, nullptr, added});
        // The earlier part stays in its calendar.
        calendarId = CalendarStore::calendarOf(key);
    }

    std::string updated = m_store->updateEvent(key, std::move(changed));
    if (updated.empty())
        return false;
    if (CalendarStore::calendarOf(updated) != calendarId)
        updated = m_store->moveEvent(updated, calendarId);
    changes.insert(changes.begin(), {key, before, updated});
    pushUndo(trNoop("Undo Edit"), std::move(changes));
    return true;
}

bool CalendarModel::removeEvent(const std::string& key, const std::string& recurrenceKey, int scope)
{
    const EventFile* file = m_store->event(key);
    if (!file || m_store->isReadOnly(CalendarStore::calendarOf(key)))
        return false;
    std::shared_ptr<EventFile> before = file->clone();

    if (file->isRecurring() && !recurrenceKey.empty() && scope != EventFile::AllEvents) {
        std::unique_ptr<EventFile> changed = file->clone();
        bool left = true;
        if (scope == EventFile::ThisEvent)
            changed->excludeOccurrence(recurrenceKey);
        else
            left = changed->endBefore(recurrenceKey);
        if (left) {
            if (m_store->updateEvent(key, std::move(changed)).empty())
                return false;
            pushUndo(trNoop("Undo Delete"), {{key, before, key}});
            return true;
        }
    }

    if (!m_store->removeEvent(key))
        return false;
    pushUndo(trNoop("Undo Delete"), {{key, before, std::string()}});
    return true;
}

bool CalendarModel::moveOccurrence(const std::string& key, const std::string& recurrenceKey, int startDays,
                                   int startMinutes, int endDays, int endMinutes, int scope)
{
    const EventFile* file = m_store->event(key);
    if (!file || m_store->isReadOnly(CalendarStore::calendarOf(key)))
        return false;
    EventData data = file->data(recurrenceKey);
    if (data.allDay) {
        data.startDate = data.startDate.addDays(startDays);
        data.endDate = std::max(data.startDate, data.endDate.addDays(endDays));
    } else {
        data.start = data.start.addDays(startDays).addSecs(int64_t(startMinutes) * 60);
        data.end = std::max(data.start, data.end.addDays(endDays).addSecs(int64_t(endMinutes) * 60));
    }
    // Moving changes the times only; the rule and alarms stay as they are.
    data.repeat.frequency = Repeat::Custom;
    data.alarm = EventData::CustomAlarm;

    EventForm form = formFrom(data);
    form.key = key;
    form.recurrenceKey = recurrenceKey;
    form.calendarId = CalendarStore::calendarOf(key);
    if (!saveEvent(form, scope))
        return false;
    if (!m_undo.empty())
        m_undo.back().text = trNoop("Undo Move");
    undoChanged.emit();
    return true;
}

std::string CalendarModel::undoText() const
{
    return m_undo.empty() ? std::string() : std::string(tr(m_undo.back().text));
}

void CalendarModel::pushUndo(const char* text, std::vector<Change> changes)
{
    m_undo.push_back({text, std::move(changes)});
    while (m_undo.size() > UndoDepth)
        m_undo.erase(m_undo.begin());
    undoChanged.emit();
}

void CalendarModel::undo()
{
    if (m_undo.empty())
        return;
    const UndoStep step = std::move(m_undo.back());
    m_undo.pop_back();
    for (auto it = step.changes.crbegin(); it != step.changes.crend(); ++it) {
        const Change& change = *it;
        if (!change.before) {
            m_store->removeEvent(change.after);
            continue;
        }
        if (!change.after.empty() && change.after != change.key)
            m_store->removeEvent(change.after);
        if (m_store->event(change.key))
            m_store->updateEvent(change.key, change.before->clone());
        else
            m_store->addEvent(CalendarStore::calendarOf(change.key), change.before->clone());
    }
    undoChanged.emit();
}

std::string CalendarModel::createCalendar(const std::string& name, Rgb color)
{
    return m_store->createCalendar(name, color);
}

bool CalendarModel::updateCalendar(const std::string& id, const std::string& name, Rgb color)
{
    return m_store->updateCalendar(id, name, color);
}

void CalendarModel::saveHidden()
{
    std::vector<std::string> hidden(m_hidden.begin(), m_hidden.end());
    std::sort(hidden.begin(), hidden.end());
    m_settings.setValue("Calendars/hidden", hidden);
    m_settings.sync();
}

bool CalendarModel::canRemoveCalendar(const std::string& id) const
{
    if (!m_store->hasCalendar(id))
        return false;
    return m_store->isReadOnly(id) || writableCalendars().size() > 1;
}

bool CalendarModel::removeCalendar(const std::string& id)
{
    const bool readOnly = m_store->isReadOnly(id);
    if (!canRemoveCalendar(id) || !m_store->removeCalendar(id))
        return false;
    // Undo would bring back events of a calendar that is gone; a subscription's were never edited.
    if (!readOnly) {
        m_undo.clear();
        undoChanged.emit();
    }
    if (m_hidden.erase(id))
        saveHidden();
    defaultCalendarChanged.emit();
    return true;
}

void CalendarModel::setCalendarVisible(const std::string& id, bool visible)
{
    if (visible == !m_hidden.contains(id))
        return;
    if (visible)
        m_hidden.erase(id);
    else
        m_hidden.insert(id);
    saveHidden();
    ++m_revision;
    revisionChanged.emit();
}

int CalendarModel::eventCount(const std::string& id) const
{
    return m_store->eventCount(id);
}

int CalendarModel::importFile(const std::string& file, const std::string& calendarId)
{
    const std::optional<std::string> data = bayu::base::file::read(localPath(file));
    if (!data)
        return -1;
    const int count = m_store->importData(calendarId, *data);
    if (count >= 0) {
        // Undo cannot tell imported events from the ones they replaced.
        m_undo.clear();
        undoChanged.emit();
    }
    return count;
}

bool CalendarModel::exportCalendar(const std::string& id, const std::string& file) const
{
    return bayu::base::file::writeAtomically(localPath(file), m_store->exportCalendar(id));
}

std::string CalendarModel::exportFileName(const std::string& id) const
{
    std::string name = m_store->calendar(id).name;
    std::replace(name.begin(), name.end(), '/', '-');
    return documentsDirectory() + '/' + (name.empty() ? std::string("Calendar") : name) + ".ics";
}

void CalendarModel::activateWindow()
{
    activationRequested.emit();
}

void CalendarModel::open(const std::vector<std::string>& files, const std::string& date)
{
    const Date day = Date::fromIsoString(date);
    if (day.isValid())
        dateRequested.emit(day);
    if (!files.empty())
        importRequested.emit(files);
    activationRequested.emit();
}
