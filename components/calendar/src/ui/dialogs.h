// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "calendarmodel.h"
#include "holidayfeeds.h"

#include <ui/view.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace bayu::ui {
class Column;
class ColumnLayout;
class ComboBox;
class DialogButton;
class DialogWindow;
class DropDownPopup;
class Label;
class Switch;
class TextField;
class Window;
}

class CalendarWindow;
class MiniMonth;

// A small frameless dialog over its parent: a close button in the corner,
// the content in a column.
class SmallDialog
{
public:
    SmallDialog(bayu::ui::Window* parent, std::string title, double width);
    virtual ~SmallDialog();

    SmallDialog(const SmallDialog&) = delete;
    SmallDialog& operator=(const SmallDialog&) = delete;

    bayu::ui::Window* window() const { return m_window.get(); }
    bayu::ui::ColumnLayout* column() const { return m_column; }
    // The title in the dialog's bold, clear of the close button.
    bayu::ui::Label* addTitle(const std::string& text);
    // A quieter line under the title.
    bayu::ui::Label* addText(const std::string& text);

    void setWidth(double width);
    void show();
    // Goes once the event being handled is over.
    void close();

    std::function<void()> onClosed;

private:
    class Root;

    bayu::ui::Window* m_parent;
    std::unique_ptr<bayu::ui::Window> m_window;
    bayu::ui::ColumnLayout* m_column;
    bayu::ui::Column* m_texts = nullptr;
    double m_width;
};

// Asks which occurrences of a repeating event a change or a removal is for,
// in the confirmation dialog's layout with a third button.
class ScopeDialog : public SmallDialog
{
public:
    struct Question
    {
        std::string title;
        std::string text;
        std::string thisText;
        std::string laterText;
        // Without "only this one", as when the rule itself changed.
        bool seriesOnly = false;
        bool destructive = false;
    };

    ScopeDialog(bayu::ui::Window* parent, const Question& question);

    // EventFile::Scope: 0 this event, 1 this and later ones.
    std::function<void(int scope)> onChosen;
};

class CalendarDialog : public SmallDialog
{
public:
    // A new calendar without an id.
    CalendarDialog(bayu::ui::Window* parent, CalendarModel* model, std::optional<CalendarItem> calendar);

private:
    void accept();
    void updateSwatches();

    CalendarModel* m_model;
    std::string m_id;
    Rgb m_color;
    bayu::ui::TextField* m_name;
    bayu::ui::DialogButton* m_save;
    std::vector<bayu::ui::View*> m_swatches;
};

// Asks for the address of a calendar published on the web and subscribes to
// it, staying open with the reason when that fails.
class SubscriptionDialog : public SmallDialog
{
public:
    SubscriptionDialog(bayu::ui::Window* parent, CalendarModel* model);
    ~SubscriptionDialog() override;

    bool isBusy() const { return m_pending >= 0; }
    bool holidaysShown() const;
    void setHolidaysShown(bool shown);

    std::function<void(const std::string& id)> onSubscribed;

private:
    class HolidayRow;
    class HolidayToggle;

    void accept();
    void update();
    void updateSwatches();

    CalendarModel* m_model;
    Rgb m_color;
    bayu::ui::TextField* m_url;
    bayu::ui::Label* m_error;
    HolidayToggle* m_holidayToggle;
    bayu::ui::View* m_holidayList;
    std::vector<HolidayFeed> m_feeds;
    std::vector<HolidayRow*> m_holidayRows;
    bayu::ui::DialogButton* m_subscribe;
    std::vector<bayu::ui::View*> m_swatches;
    int m_pending = -1;
};

class ImportDialog : public SmallDialog
{
public:
    ImportDialog(bayu::ui::Window* parent, CalendarModel* model, std::vector<std::string> files);

    const std::vector<std::string>& files() const { return m_files; }
    // In the list of writable calendars.
    int calendarIndex() const;
    void setCalendarIndex(int index);

    std::function<void(const std::vector<std::string>& files, const std::string& calendarId)> onImport;

private:
    void accept();

    CalendarModel* m_model;
    std::vector<std::string> m_files;
    bayu::ui::ComboBox* m_calendar;
};

// Week start and week numbers.
class SettingsDialog
{
public:
    SettingsDialog(bayu::ui::Window* parent, CalendarModel* model);
    ~SettingsDialog();

    void show();
    bayu::ui::Window* window() const;
    std::function<void()> onClosed;

private:
    std::unique_ptr<bayu::ui::DialogWindow> m_dialog;
};

// A card of rows: a label on the left, its controls on the right, a
// hairline between rows.
class FormCard : public bayu::ui::View
{
public:
    FormCard() = default;

    // The row, to add its controls to, right-aligned.
    bayu::ui::View* addRow(const std::string& label, double height = 44);
    // A full-width child, such as a text field.
    void addWide(std::unique_ptr<bayu::ui::View> view, double height);
    void setRowVisible(bayu::ui::View* row, bool visible);

    double heightForWidth(double width) const override;
    bayu::ui::SizeF sizeHint() const override { return {0, heightForWidth(0)}; }

protected:
    void layout() override;
    void paint(bayu::ui::Painter& painter) override;

private:
    std::vector<bayu::ui::View*> visibleRows() const;
};

// A month to pick a day from, dropped down from a date field the way a
// combo box drops its list.
class DatePopup
{
public:
    DatePopup(CalendarModel* model, bayu::ui::View* anchor, Date selected);
    ~DatePopup();

    bool isOpen() const;
    void close();
    bayu::ui::Window* window() const;

    std::function<void(Date date)> onPicked;
    std::function<void()> onClosed;

private:
    class Content;

    CalendarModel* m_model;
    std::unique_ptr<bayu::ui::DropDownPopup> m_popup;
};

// The day of a date field, opening a month to pick another from.
class DateButton : public bayu::ui::View
{
public:
    DateButton(CalendarModel* model, Date date);
    ~DateButton() override;

    Date date() const { return m_date; }
    void setDate(Date date);
    bool isPopupOpen() const;
    // The open month's window, for tests.
    bayu::ui::Window* popupWindow() const;
    void dismiss();

    bayu::ui::SizeF sizeHint() const override;
    bool acceptsPointer() const override { return true; }
    bool acceptsFocus() const override { return true; }

    std::function<void(Date date)> onPicked;

protected:
    void layout() override;
    void paint(bayu::ui::Painter& painter) override;
    void pointerPress(const bayu::ui::PointerEvent& event) override;
    void pointerRelease(const bayu::ui::PointerEvent& event) override;
    bool keyPress(const bayu::ui::KeyEvent& event) override;

private:
    void open();

    CalendarModel* m_model;
    Date m_date;
    bayu::ui::Label* m_label;
    std::unique_ptr<DatePopup> m_popup;
    bool m_pressed = false;
};
