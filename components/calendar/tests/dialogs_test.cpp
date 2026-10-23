// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "calendarmodel.h"
#include "calendarstore.h"
#include "localedata.h"
#include "ui/calendarwindow.h"
#include "ui/dialogs.h"
#include "ui/eventdialog.h"
#include "ui/glyphs.h"
#include "ui/minimonth.h"

#include <base/event_loop.h>
#include <ui/application.h>
#include <ui/combo_box.h>
#include <ui/keys.h>
#include <ui/label.h>
#include <ui/switch.h>
#include <ui/test_input.h>
#include <ui/text_field.h>
#include <ui/theme.h>
#include <ui/view.h>
#include <ui/window.h>

#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace bayu::ui;

static uint64_t g_frameTime = 1000000000;

static void advance(Window& window)
{
    g_frameTime += 16666667;
    TestInput::presentFrame(window, g_frameTime);
}

static void runPosted()
{
    bayu::base::EventLoop& loop = Application::instance()->loop();
    loop.post([&loop] { loop.quit(); });
    loop.run();
}

static void collect(View* view, const std::function<bool(View*)>& match, std::vector<View*>* out)
{
    if (!view->isVisible())
        return;
    if (match(view))
        out->push_back(view);
    for (const auto& child : view->children())
        collect(child.get(), match, out);
}

static std::vector<View*> find(Window* window, const std::string& name)
{
    std::vector<View*> views;
    collect(window->contentView(), [&name](View* view) { return view->name() == name; }, &views);
    return views;
}

template<class T>
static T* one(Window* window, const std::string& name)
{
    const std::vector<View*> views = find(window, name);
    return views.size() == 1 ? dynamic_cast<T*>(views.front()) : nullptr;
}

template<class T>
static T* firstOfType(Window* window)
{
    std::vector<View*> views;
    collect(window->contentView(), [](View* view) { return dynamic_cast<T*>(view) != nullptr; }, &views);
    return views.empty() ? nullptr : static_cast<T*>(views.front());
}

static PointF centreOf(const View* view)
{
    return view->mapToWindow({view->geometry().width / 2, view->geometry().height / 2});
}

static void activate(ComboBox* box, int index)
{
    box->setCurrentIndex(index);
    if (box->onActivated)
        box->onActivated(index);
}

namespace {

class DialogsTest : public testing::Test
{
protected:
    void SetUp() override
    {
        std::string pattern = (fs::temp_directory_path() / "calendar-dialogs-XXXXXX").string();
        m_dir = mkdtemp(pattern.data());
        m_store = std::make_unique<CalendarStore>(m_dir + "/calendars");
        m_model = std::make_unique<CalendarModel>(m_store.get(), m_dir + "/calendar.conf");
        m_store->load();
        m_window = std::make_unique<CalendarWindow>(m_model.get(), false);
        m_window->show();
        advance(*m_window->window());
    }

    void TearDown() override
    {
        m_window.reset();
        m_model.reset();
        m_store.reset();
        Theme::setCurrent(Theme::light());
        std::error_code error;
        fs::remove_all(m_dir, error);
    }

    // A new timed event on the day, its editor laid out.
    Window* newEvent(Date day, int startHour = 14)
    {
        const DateTime start(day, Time(startHour, 0));
        m_window->newEvent(start, start.addSecs(3600), false);
        EXPECT_TRUE(m_window->editor());
        Window* dialog = m_window->editor()->window();
        advance(*dialog);
        return dialog;
    }

    std::vector<Occurrence> eventsOn(Date day, int days = 1) const { return m_store->occurrences(day, day.addDays(days)); }

    std::string m_dir;
    std::unique_ptr<CalendarStore> m_store;
    std::unique_ptr<CalendarModel> m_model;
    std::unique_ptr<CalendarWindow> m_window;
};

}

TEST_F(DialogsTest, EditorLaysOutItsCards)
{
    Window* dialog = newEvent(Date(2026, 10, 14));
    // Cards and spacings as the Qt dialog: whole pixels, 12 apart.
    View* content = find(dialog, "eventDialog").front();
    std::vector<RectF> rects;
    for (const auto& child : content->children()) {
        if (child->isVisible())
            rects.push_back(child->geometry());
    }
    ASSERT_EQ(rects.size(), 5u);
    // Title and location: two fields of 44 and a separator of one device pixel.
    EXPECT_EQ(rects[0].y, 0);
    EXPECT_EQ(rects[0].height, 89);
    EXPECT_EQ(rects[1].y, 101);
    EXPECT_EQ(rects[1].height, 132);
    EXPECT_EQ(rects[2].y, 245);
    EXPECT_EQ(rects[2].height, 132);
    EXPECT_EQ(rects[3].y, 389);
    EXPECT_EQ(rects[3].height, 84);
    EXPECT_EQ(rects[4].y, 489);
}

TEST_F(DialogsTest, RepeatEndsAfterTheCountTyped)
{
    const Date day(2026, 10, 14);
    Window* dialog = newEvent(day);
    TestInput::typeText(*dialog, "Standup");
    EXPECT_TRUE(find(dialog, "repeatEndBox").empty());

    activate(one<ComboBox>(dialog, "repeatBox"), 1);
    advance(*dialog);
    auto* ends = one<ComboBox>(dialog, "repeatEndBox");
    ASSERT_TRUE(ends);
    EXPECT_TRUE(find(dialog, "repeatCountField").empty());
    activate(ends, 2);
    advance(*dialog);
    auto* count = one<TextField>(dialog, "repeatCountField");
    ASSERT_TRUE(count);
    EXPECT_EQ(count->text(), "10");
    count->setFocus();
    count->selectAll();
    // Only digits, at most three.
    TestInput::typeText(*dialog, "x3");
    EXPECT_EQ(count->text(), "3");

    one<TextField>(dialog, "titleField")->setFocus();
    TestInput::typeKey(*dialog, key::Return, ControlModifier);
    runPosted();
    EXPECT_FALSE(m_window->editor());
    const std::vector<Occurrence> found = eventsOn(day, 10);
    ASSERT_EQ(found.size(), 3u);
    EXPECT_EQ(found.front().summary, "Standup");
    EXPECT_EQ(found.back().start.date(), day.addDays(2));
}

TEST_F(DialogsTest, AnEndBeforeTheStartCannotBeSaved)
{
    const Date day(2026, 10, 14);
    Window* dialog = newEvent(day);
    const auto errorShown = [dialog] {
        std::vector<View*> labels;
        collect(dialog->contentView(), [](View* view) {
            auto* label = dynamic_cast<Label*>(view);
            return label && label->text() == "The event ends before it starts.";
        }, &labels);
        return labels.size() == 1;
    };
    EXPECT_FALSE(errorShown());
    auto* save = one<View>(dialog, "saveButton");
    ASSERT_TRUE(save);
    EXPECT_TRUE(save->isEnabled());

    auto* end = one<ComboBox>(dialog, "endTime");
    end->field()->setFocus();
    end->field()->selectAll();
    TestInput::typeText(*dialog, "8");
    TestInput::typeKey(*dialog, key::Return);
    advance(*dialog);
    EXPECT_EQ(end->editText(), LocaleData::timeText(Time(8, 0)));
    EXPECT_FALSE(save->isEnabled());
    EXPECT_TRUE(errorShown());

    TestInput::typeKey(*dialog, key::Return, ControlModifier);
    runPosted();
    ASSERT_TRUE(m_window->editor());
    EXPECT_TRUE(eventsOn(day).empty());

    // A later end from the list makes it whole again.
    activate(end, 15 * 4);
    advance(*dialog);
    EXPECT_TRUE(save->isEnabled());
    EXPECT_FALSE(errorShown());
}

TEST_F(DialogsTest, AllDayHidesTheTimesAndListsDayAlerts)
{
    Window* dialog = newEvent(Date(2026, 10, 14));
    auto* alerts = one<ComboBox>(dialog, "alarmBox");
    ASSERT_TRUE(alerts);
    EXPECT_EQ(alerts->items().size(), 11u);
    EXPECT_EQ(find(dialog, "startTime").size(), 1u);

    auto* allDay = one<Switch>(dialog, "allDaySwitch");
    ASSERT_TRUE(allDay);
    TestInput::click(*dialog, centreOf(allDay));
    advance(*dialog);
    EXPECT_TRUE(find(dialog, "startTime").empty());
    EXPECT_TRUE(find(dialog, "endTime").empty());
    ASSERT_EQ(alerts->items().size(), 5u);
    EXPECT_EQ(alerts->items()[1], "On the Day (9:00)");
    activate(alerts, 2);

    // The switch took the focus, as buttons do on a click.
    EXPECT_TRUE(allDay->hasFocus());
    one<TextField>(dialog, "titleField")->setFocus();
    TestInput::typeText(*dialog, "Holiday");
    TestInput::typeKey(*dialog, key::Return);
    runPosted();
    EXPECT_FALSE(m_window->editor());
    const std::vector<Occurrence> found = eventsOn(Date(2026, 10, 14));
    ASSERT_EQ(found.size(), 1u);
    EXPECT_TRUE(found.front().allDay);
    const std::optional<EventForm> form = m_model->eventData(found.front().key, found.front().recurrenceKey);
    ASSERT_TRUE(form);
    EXPECT_EQ(form->alarm, int(EventData::OffsetAlarm));
    EXPECT_EQ(form->alarmOffset, -15 * 60);
}

TEST_F(DialogsTest, TheDatePopupPicksADayAndKeepsTheLength)
{
    const Date day(2026, 10, 14);
    Window* dialog = newEvent(day, 23);
    auto* start = one<DateButton>(dialog, "startDate");
    auto* end = one<DateButton>(dialog, "endDate");
    ASSERT_TRUE(start && end);
    // From 23:00 to midnight: the end is on the next day.
    EXPECT_EQ(end->date(), day.addDays(1));

    TestInput::click(*dialog, centreOf(start));
    Window* popup = start->popupWindow();
    ASSERT_TRUE(popup);
    advance(*popup);
    EXPECT_EQ(popup->contentView()->geometry().width, 252);
    auto* month = firstOfType<MiniMonth>(popup);
    ASSERT_TRUE(month);
    EXPECT_EQ(month->month(), Date(2026, 10, 1));
    const int index = int(month->gridStart().daysTo(Date(2026, 10, 20)));
    const double cell = month->geometry().width / 7;
    TestInput::click(*popup, month->mapToWindow({(index % 7 + 0.5) * cell, 28 + (index / 7 + 0.5) * 28}));
    runPosted();
    EXPECT_FALSE(start->isPopupOpen());
    EXPECT_EQ(start->date(), Date(2026, 10, 20));
    EXPECT_EQ(end->date(), Date(2026, 10, 21));
}

TEST_F(DialogsTest, ChangingTheRuleOfARepeatingEventAsksOnlyForLaterOnes)
{
    const Date day = Date::currentDate();
    const DateTime start(day, Time(9, 0));
    EventForm form = m_model->newEventData(start, start.addSecs(1800), false);
    form.title = "Standup";
    form.repeat = Repeat::Daily;
    ASSERT_TRUE(m_model->saveEvent(form, EventFile::AllEvents));
    const std::vector<Occurrence> found = eventsOn(day.addDays(1));
    ASSERT_EQ(found.size(), 1u);

    m_window->editEvent(found.front().key, found.front().recurrenceKey);
    ASSERT_TRUE(m_window->editor());
    Window* dialog = m_window->editor()->window();
    advance(*dialog);
    EXPECT_EQ(find(dialog, "deleteButton").size(), 1u);
    activate(one<ComboBox>(dialog, "repeatBox"), 3);
    TestInput::typeKey(*dialog, key::Return, ControlModifier);
    runPosted();
    ScopeDialog* scope = m_window->scopeDialog();
    ASSERT_TRUE(scope);
    advance(*scope->window());
    EXPECT_TRUE(find(scope->window(), "thisEventButton").empty());
    EXPECT_EQ(scope->window()->size().width, 360);
    auto* later = one<View>(scope->window(), "laterEventsButton");
    ASSERT_TRUE(later);
    EXPECT_TRUE(later->hasFocus());
    TestInput::typeKey(*scope->window(), key::Escape);
    runPosted();
    EXPECT_FALSE(m_window->scopeDialog());
    EXPECT_TRUE(m_window->editor());
}

TEST_F(DialogsTest, TheCalendarDialogRenamesAndRecolours)
{
    const CalendarItem calendar = m_model->calendars().front();
    m_window->editCalendar(calendar);
    CalendarDialog* dialog = m_window->calendarDialog();
    ASSERT_TRUE(dialog);
    Window* frame = dialog->window();
    advance(*frame);
    EXPECT_EQ(frame->size().width, 360);
    auto* name = one<TextField>(frame, "calendarName");
    auto* save = one<View>(frame, "calendarSave");
    ASSERT_TRUE(name && save);
    EXPECT_EQ(name->text(), calendar.name);
    // The name starts selected, so typing replaces it; an empty one cannot be saved.
    TestInput::typeKey(*frame, key::BackSpace);
    EXPECT_FALSE(save->isEnabled());
    TestInput::typeText(*frame, "Home");
    EXPECT_TRUE(save->isEnabled());

    const std::vector<View*> swatches = find(frame, "swatch");
    ASSERT_EQ(swatches.size(), 7u);
    TestInput::click(*frame, centreOf(swatches[5]));
    TestInput::typeKey(*frame, key::Return);
    runPosted();
    EXPECT_FALSE(m_window->calendarDialog());
    const CalendarItem changed = m_model->calendars().front();
    EXPECT_EQ(changed.name, "Home");
    EXPECT_EQ(changed.color, (Rgb{accent::green.r, accent::green.g, accent::green.b}));
}

TEST_F(DialogsTest, TheImportDialogAddsToTheChosenCalendar)
{
    const std::string other = m_model->createCalendar("Work", Rgb{1, 2, 3});
    m_model->setDefaultCalendar(other);
    std::vector<std::string> files;
    for (const char* name : {"a.ics", "b%20c.ics"}) {
        files.push_back(m_dir + "/" + name);
        std::ofstream(files.back()) << "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nBEGIN:VEVENT\r\nUID:" << name
                                    << "\r\nDTSTAMP:20261001T000000Z\r\nSUMMARY:Trip\r\nDTSTART;VALUE=DATE:20261020\r\n"
                                       "DTEND;VALUE=DATE:20261021\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n";
    }

    m_window->importFiles(files);
    ImportDialog* dialog = m_window->importDialog();
    ASSERT_TRUE(dialog);
    Window* frame = dialog->window();
    advance(*frame);
    EXPECT_EQ(frame->size().width, 380);
    std::vector<View*> texts;
    collect(frame->contentView(), [](View* view) {
        auto* label = dynamic_cast<Label*>(view);
        return label && label->text() == "2 files";
    }, &texts);
    EXPECT_EQ(texts.size(), 1u);
    auto* box = one<ComboBox>(frame, "importCalendarBox");
    ASSERT_TRUE(box);
    EXPECT_EQ(box->currentText(), "Work");

    TestInput::typeKey(*frame, key::Return);
    runPosted();
    EXPECT_FALSE(m_window->importDialog());
    const std::vector<Occurrence> found = eventsOn(Date(2026, 10, 20));
    ASSERT_EQ(found.size(), 2u);
    EXPECT_EQ(found.front().calendarId, other);
    EXPECT_EQ(found.back().calendarId, other);
}

TEST_F(DialogsTest, OneFileIsNamedWithoutItsEscapes)
{
    m_window->importFiles({"file:///tmp/My%20Trip.ics"});
    ImportDialog* dialog = m_window->importDialog();
    ASSERT_TRUE(dialog);
    advance(*dialog->window());
    std::vector<View*> texts;
    collect(dialog->window()->contentView(), [](View* view) {
        auto* label = dynamic_cast<Label*>(view);
        return label && label->text() == "My Trip.ics";
    }, &texts);
    EXPECT_EQ(texts.size(), 1u);
    TestInput::typeKey(*dialog->window(), key::Escape);
    runPosted();
    EXPECT_FALSE(m_window->importDialog());
}

// Runs the loop until the condition holds, at most for the timeout.
static bool runUntil(const std::function<bool()>& condition, std::chrono::milliseconds timeout = std::chrono::seconds(5))
{
    bayu::base::EventLoop& loop = Application::instance()->loop();
    if (condition())
        return true;
    const int check = loop.startTimer(std::chrono::milliseconds(10), true, [&] {
        if (condition())
            loop.quit();
    });
    const int expire = loop.startTimer(timeout, false, [&] { loop.quit(); });
    loop.run();
    loop.stopTimer(check);
    loop.stopTimer(expire);
    return condition();
}

static const char HolidayEvents[] = "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nX-WR-CALNAME:Holidays\r\n"
                                  "BEGIN:VEVENT\r\nUID:national-day\r\nDTSTART;VALUE=DATE:20261001\r\n"
                                  "DTEND;VALUE=DATE:20261008\r\nSUMMARY:National Day\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n";

TEST_F(DialogsTest, TheSubscriptionDialogSubscribesOrSaysWhyNot)
{
    long status = 404;
    m_model->subscriptions()->setFetch([&status](const std::string&, const std::string&, const std::string&) {
        Download answer;
        answer.status = status;
        answer.body = status == 200 ? HolidayEvents : "";
        return answer;
    });
    EXPECT_TRUE(find(m_window->window(), "subscriptionList").empty());

    m_window->newSubscription();
    SubscriptionDialog* dialog = m_window->subscriptionDialog();
    ASSERT_TRUE(dialog);
    Window* frame = dialog->window();
    advance(*frame);
    EXPECT_EQ(frame->size().width, 380);
    auto* url = one<TextField>(frame, "subscriptionUrl");
    auto* subscribe = one<View>(frame, "subscribeButton");
    ASSERT_TRUE(url && subscribe);
    EXPECT_FALSE(subscribe->isEnabled());
    EXPECT_TRUE(find(frame, "subscriptionError").empty());

    TestInput::typeText(*frame, "not an address");
    TestInput::typeKey(*frame, key::Return);
    advance(*frame);
    auto* error = one<Label>(frame, "subscriptionError");
    ASSERT_TRUE(error);
    EXPECT_EQ(error->text(), "This is not a web address.");

    // The server's answer keeps the dialog open; typing puts the reason away.
    url->setFocus();
    url->selectAll();
    TestInput::typeText(*frame, "webcal://example.com/holidays.ics");
    EXPECT_TRUE(find(frame, "subscriptionError").empty());
    TestInput::click(*frame, centreOf(subscribe));
    EXPECT_TRUE(dialog->isBusy());
    EXPECT_FALSE(subscribe->isEnabled());
    ASSERT_TRUE(runUntil([&] { return !dialog->isBusy(); }));
    advance(*frame);
    error = one<Label>(frame, "subscriptionError");
    ASSERT_TRUE(error);
    EXPECT_EQ(error->text(), "The server answered with error 404.");
    EXPECT_TRUE(subscribe->isEnabled());

    status = 200;
    TestInput::click(*frame, centreOf(subscribe));
    ASSERT_TRUE(runUntil([&] { return !m_window->subscriptionDialog(); }));
    std::string id;
    for (const CalendarItem& calendar : m_model->calendars()) {
        if (calendar.readOnly)
            id = calendar.id;
    }
    ASSERT_FALSE(id.empty());
    EXPECT_EQ(m_store->calendar(id).name, "Holidays");
    EXPECT_EQ(m_store->eventCount(id), 1);

    // Its own section in the sidebar, under the user's calendars.
    runPosted();
    advance(*m_window->window());
    View* calendars = one<View>(m_window->window(), "calendarList");
    View* heading = one<View>(m_window->window(), "subscriptionsHeading");
    View* subscriptions = one<View>(m_window->window(), "subscriptionList");
    ASSERT_TRUE(calendars && heading && subscriptions);
    const double calendarsBottom = calendars->mapToWindow({0, calendars->geometry().height}).y;
    const double headingTop = heading->parent()->mapToWindow({0, 0}).y;
    const double headingBottom = heading->parent()->mapToWindow({0, heading->parent()->geometry().height}).y;
    EXPECT_EQ(headingTop - calendarsBottom, 12);
    EXPECT_EQ(subscriptions->mapToWindow({0, 0}).y - headingBottom, 12);
    EXPECT_EQ(subscriptions->children().front()->children().size(), 1u);
}

TEST_F(DialogsTest, PublicHolidaysArePickedFromAList)
{
    const std::vector<HolidayFeed> feeds = HolidayFeeds::list();
    ASSERT_GE(feeds.size(), 2u);
    m_store->createSubscription(feeds[1].name, Rgb{0xff, 0, 0}, {.url = feeds[1].url});
    std::string fetched;
    m_model->subscriptions()->setFetch([&fetched](const std::string& url, const std::string&, const std::string&) {
        fetched = url;
        Download answer;
        answer.status = 200;
        answer.body = HolidayEvents;
        return answer;
    });

    m_window->newSubscription();
    SubscriptionDialog* dialog = m_window->subscriptionDialog();
    ASSERT_TRUE(dialog);
    Window* frame = dialog->window();
    advance(*frame);
    const double closedHeight = frame->size().height;
    EXPECT_TRUE(find(frame, "holidayList").empty());

    TestInput::click(*frame, centreOf(one<View>(frame, "holidaysToggle")));
    advance(*frame);
    advance(*frame);
    EXPECT_TRUE(dialog->holidaysShown());
    View* list = one<View>(frame, "holidayList");
    ASSERT_TRUE(list);
    EXPECT_EQ(list->geometry().height, 6 * 34 + 12);
    EXPECT_EQ(frame->size().height, closedHeight + 16 + list->geometry().height);
    const std::vector<View*> rows = find(frame, "holidayRow");
    ASSERT_EQ(rows.size(), feeds.size());
    // One gap of 6 to the card's edges.
    const PointF card = list->mapToWindow({0, 0});
    const PointF first = rows[0]->mapToWindow({0, 0});
    EXPECT_EQ(first.x - card.x, 6);
    EXPECT_EQ(first.y - card.y, 6);
    EXPECT_EQ(list->geometry().width - rows[0]->geometry().width, 12);

    auto* url = one<TextField>(frame, "subscriptionUrl");
    ASSERT_TRUE(url);
    // A region already subscribed to is not picked again.
    TestInput::click(*frame, centreOf(rows[1]));
    EXPECT_EQ(url->text(), "");
    TestInput::click(*frame, centreOf(rows[0]));
    EXPECT_EQ(url->text(), feeds[0].url);
    EXPECT_TRUE(one<View>(frame, "subscribeButton")->isEnabled());

    TestInput::click(*frame, centreOf(one<View>(frame, "subscribeButton")));
    ASSERT_TRUE(runUntil([&] { return !m_window->subscriptionDialog(); }));
    EXPECT_EQ(fetched, feeds[0].url);
    int subscriptions = 0;
    for (const CalendarInfo& calendar : m_store->calendars())
        subscriptions += calendar.isReadOnly() && calendar.url == feeds[0].url;
    EXPECT_EQ(subscriptions, 1);
}

TEST_F(DialogsTest, ASubscribedEventOpensReadOnly)
{
    const std::string id = m_store->createSubscription("Holidays", Rgb{0xff, 0, 0},
                                                       {.url = "https://example.com/holidays.ics"});
    ASSERT_EQ(m_store->replaceEvents(id, EventFile::split(HolidayEvents)), 1);
    const std::string key = m_store->keys().front().starts_with(id) ? m_store->keys().front() : m_store->keys().back();

    m_window->editEvent(key, {});
    ASSERT_TRUE(m_window->editor());
    Window* frame = m_window->editor()->window();
    advance(*frame);
    EXPECT_TRUE(find(frame, "saveButton").empty());
    EXPECT_TRUE(find(frame, "deleteButton").empty());
    ASSERT_TRUE(one<View>(frame, "closeButton"));
    auto* title = one<TextField>(frame, "titleField");
    ASSERT_TRUE(title);
    EXPECT_EQ(title->text(), "National Day");
    TestInput::typeText(*frame, "Changed");
    EXPECT_EQ(title->text(), "National Day");
    EXPECT_FALSE(one<View>(frame, "allDaySwitch")->isEnabled());
    auto* calendar = one<ComboBox>(frame, "calendarBox");
    ASSERT_TRUE(calendar);
    EXPECT_FALSE(calendar->isEnabled());
    EXPECT_EQ(calendar->currentText(), "Holidays");

    TestInput::typeKey(*frame, key::Return, ControlModifier);
    TestInput::click(*frame, centreOf(one<View>(frame, "closeButton")));
    runPosted();
    EXPECT_FALSE(m_window->editor());
    EXPECT_EQ(m_store->event(key)->summary(), "National Day");

    // Nor does the keyboard delete it.
    m_window->deleteEvent(key, {});
    runPosted();
    EXPECT_TRUE(m_store->event(key));
    EXPECT_FALSE(m_window->scopeDialog());
}

TEST_F(DialogsTest, SettingsChangeTheWeek)
{
    SettingsDialog settings(m_window->window(), m_model.get());
    bool closed = false;
    settings.onClosed = [&closed] { closed = true; };
    settings.show();
    Window* frame = settings.window();
    advance(*frame);
    EXPECT_EQ(frame->size().width, 420);
    auto* weekStart = one<ComboBox>(frame, "weekStartBox");
    ASSERT_TRUE(weekStart);
    ASSERT_EQ(weekStart->items().size(), 4u);
    EXPECT_EQ(weekStart->items()[0], "System Default");
    EXPECT_EQ(weekStart->items()[2], "Monday");
    EXPECT_EQ(weekStart->currentIndex(), 0);
    activate(weekStart, 2);
    EXPECT_EQ(m_model->weekStart(), 1);

    auto* numbers = one<Switch>(frame, "weekNumbersSwitch");
    ASSERT_TRUE(numbers);
    TestInput::click(*frame, centreOf(numbers));
    EXPECT_TRUE(m_model->showWeekNumbers());

    TestInput::typeKey(*frame, key::Escape);
    runPosted();
    EXPECT_TRUE(closed);
}

TEST(EventDialogTimesTest, EveryListedTimeReadsBack)
{
    // The list's texts, with the locale's narrow space before am and pm.
    for (int minute = 0; minute < 24 * 60; minute += 15)
        EXPECT_EQ(EventDialog::parseTime(LocaleData::timeText(Time(minute / 60, minute % 60)), -1), minute);
    EXPECT_EQ(EventDialog::parseTime("8:00\u202fam", -1), 8 * 60);
    EXPECT_EQ(EventDialog::parseTime("8:30\u00a0PM", -1), 20 * 60 + 30);
    EXPECT_EQ(EventDialog::parseTime("20:30:00", -1), 20 * 60 + 30);
}

static void save(Window* window, double scale, const std::string& file)
{
    window->setScale(scale);
    advance(*window);
    if (!window->grab().savePng(file + ".png"))
        std::fprintf(stderr, "cannot write %s.png\n", file.c_str());
    std::ofstream(file + ".txt") << window->layoutDump();
}

// Renders the dialogs over a calendar directory, for comparing with the live
// application: calendar-dialogs-tests --capture ROOT OUTDIR [SCALE].
static int capture(const std::string& root, const std::string& out, double scale)
{
    fs::create_directories(out);
    const std::string settings = out + "/calendar.conf";
    std::ofstream(settings) << "[General]\ndefaultCalendar=personal\nsidebarVisible=true\nview=month\n";
    CalendarStore store(root);
    CalendarModel model(&store, settings);
    store.load();
    const Date day(2026, 10, 14);
    for (const auto& [name, theme] : {std::pair{"light", Theme::light()}, std::pair{"dark", Theme::dark()}}) {
        Theme::setCurrent(theme);
        const std::string prefix = out + "/" + name + "-";
        CalendarWindow window(&model, false);
        window.window()->setScale(scale);
        window.show();
        advance(*window.window());

        window.newEvent(DateTime(day, Time(9, 0)), DateTime(day, Time(10, 0)), false);
        Window* editor = window.editor()->window();
        save(editor, scale, prefix + "new-event");
        auto* start = one<DateButton>(editor, "startDate");
        TestInput::click(*editor, centreOf(start));
        if (Window* popup = start->popupWindow())
            save(popup, scale, prefix + "date-popup");
        start->dismiss();
        runPosted();
        activate(one<ComboBox>(editor, "repeatBox"), 1);
        activate(one<ComboBox>(editor, "repeatEndBox"), 2);
        one<ComboBox>(editor, "endTime")->setEditText("8:00 am");
        one<ComboBox>(editor, "endTime")->onAccepted();
        save(editor, scale, prefix + "repeat-after");
        activate(one<ComboBox>(editor, "repeatEndBox"), 1);
        save(editor, scale, prefix + "repeat-until");
        window.editor()->close();
        runPosted();

        for (const Occurrence& occurrence : store.occurrences(day, day.addDays(1))) {
            if (occurrence.recurring) {
                window.deleteEvent(occurrence.key, occurrence.recurrenceKey);
                break;
            }
        }
        if (ScopeDialog* scope = window.scopeDialog()) {
            save(scope->window(), scale, prefix + "scope-delete");
            scope->close();
        }
        window.newCalendar();
        save(window.calendarDialog()->window(), scale, prefix + "new-calendar");
        window.calendarDialog()->close();
        window.importFiles({"file:///tmp/Holidays%202026.ics"});
        save(window.importDialog()->window(), scale, prefix + "import");
        window.importDialog()->close();
        SettingsDialog settingsDialog(window.window(), &model);
        settingsDialog.show();
        save(settingsDialog.window(), scale, prefix + "settings");
        runPosted();
    }
    return 0;
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    Application application(argc, argv, Application::Platform::Headless);
    registerCalendarGlyphs();
    if (argc >= 4 && std::string(argv[1]) == "--capture")
        return capture(argv[2], argv[3], argc > 4 ? std::stod(argv[4]) : 1.5);
    return RUN_ALL_TESTS();
}
