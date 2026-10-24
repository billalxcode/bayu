// SPDX-License-Identifier: BSD-3-Clause
// The installer's building blocks, drawn as designed: cards, selectable
// rows and option cards, buttons, fields, drop-downs and the layout bar.

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ui/layout.h"
#include "ui/view.h"
#include "ui/style.h"

namespace bayu::ui {
class Icon;
class ImageView;
class Label;
class Menu;
class Switch;
class TextField;
class BusyIndicator;
}

namespace installer {

using bayu::ui::Column;
using bayu::ui::Row;
using bayu::ui::View;

// A label in the installer's typeface.
bayu::ui::Label* addText(View* parent, std::string text, double size = style::bodySize, int weight = 400,
                         std::optional<style::Role> color = std::nullopt);

// "REGIONAL FORMAT": small capitals, spaced, in the tertiary text colour.
bayu::ui::Label* addSectionLabel(View* parent, std::string text);

// A rounded white panel with a hairline edge; its children in a column.
class Card : public Column
{
public:
    explicit Card(double padding = 0);
    void setFill(std::optional<style::Role> fill, std::optional<style::Role> border);

protected:
    void paint(bayu::ui::Painter& painter) override;

private:
    std::optional<style::Role> m_fill = style::Role::Card;
    std::optional<style::Role> m_border = style::Role::CardBorder;
};

// A clickable view with hover and press states and keyboard activation.
class Pressable : public View
{
public:
    bool acceptsPointer() const override { return true; }
    bool acceptsFocus() const override { return true; }
    std::function<void()> onClicked;

protected:
    bool isHovered() const { return m_hovered; }
    bool isPressed() const { return m_pressed; }
    void pointerEnter() override;
    void pointerLeave() override;
    void pointerPress(const bayu::ui::PointerEvent& event) override;
    void pointerRelease(const bayu::ui::PointerEvent& event) override;
    bool keyPress(const bayu::ui::KeyEvent& event) override;
    void focusIn(bayu::ui::FocusReason) override { requestPaint(); }
    void focusOut() override { requestPaint(); }

private:
    bool m_hovered = false;
    bool m_pressed = false;
};

// A row of a list card: a badge ("EN") or glyph, a title and a detail, and a
// check while selected.
class ListRow : public Pressable
{
public:
    ListRow();
    void setBadge(std::string badge);
    void setGlyph(std::string glyph);
    void setTitle(std::string title);
    void setDetail(std::string detail);
    // Text or a glyph at the end, such as a signal meter.
    void setTrailingGlyph(std::string glyph);
    void setSelected(bool selected);
    bool isSelected() const { return m_selected; }

    bayu::ui::SizeF sizeHint() const override;

protected:
    void layout() override;
    void paint(bayu::ui::Painter& painter) override;

private:
    void updateColors();
    bayu::ui::Label* m_badge;
    bayu::ui::Icon* m_glyph;
    bayu::ui::Label* m_title;
    bayu::ui::Label* m_detail;
    bayu::ui::Icon* m_trailing;
    bayu::ui::Icon* m_check;
    bool m_selected = false;
};

// A list card of ListRows, at most one selected.
class ListCard : public Card
{
public:
    ListCard();
    ListRow* addRow();
    void clearRows();
    void select(ListRow* row);
    const std::vector<ListRow*>& rows() const { return m_rows; }

private:
    std::vector<ListRow*> m_rows;
};

// A card to choose: an icon tile, a title and a detail, a note at the end
// ("1.0 TB", "recommended") and a radio mark.
class OptionCard : public Pressable
{
public:
    explicit OptionCard(std::string glyph = {});
    void setGlyph(std::string glyph);
    void setTitle(std::string title);
    void setDetail(std::string detail);
    void setNote(std::string note, bool accent = false);
    void setSelected(bool selected);
    bool isSelected() const { return m_selected; }
    // A check box mark instead of the radio mark.
    void setCheckStyle(bool check) { m_checkStyle = check; requestPaint(); }

    bayu::ui::SizeF sizeHint() const override;
    double heightForWidth(double width) const override;

protected:
    void layout() override;
    void paint(bayu::ui::Painter& painter) override;
    void enabledChanged() override;

private:
    void updateColors();
    bayu::ui::Icon* m_icon;
    bayu::ui::Label* m_title;
    bayu::ui::Label* m_detail;
    bayu::ui::Label* m_note;
    bool m_selected = false;
    bool m_checkStyle = false;
    bool m_noteAccent = false;
};

// A row with a check box: a title and a detail under it.
class CheckRow : public Pressable
{
public:
    CheckRow(std::string title, std::string detail);
    void setChecked(bool checked);
    bool isChecked() const { return m_checked; }
    std::function<void(bool checked)> onToggled;

    bayu::ui::SizeF sizeHint() const override;

protected:
    void layout() override;
    void paint(bayu::ui::Painter& painter) override;

private:
    bayu::ui::Label* m_title;
    bayu::ui::Label* m_detail;
    bool m_checked = false;
};

// A card with a title, a detail and a switch at its end.
class ToggleCard : public View
{
public:
    ToggleCard(std::string title, std::string detail);
    void setChecked(bool checked);
    bool isChecked() const;
    void setDetail(std::string detail);
    std::function<void(bool checked)> onToggled;

    bayu::ui::SizeF sizeHint() const override;
    double heightForWidth(double width) const override;

protected:
    void layout() override;
    void paint(bayu::ui::Painter& painter) override;

private:
    bayu::ui::Label* m_title;
    bayu::ui::Label* m_detail;
    bayu::ui::Switch* m_switch;
};

// A card of "Key ........ value" lines, with an optional heading (a glyph,
// a title and an Edit link).
class DetailsCard : public Card
{
public:
    DetailsCard();
    // A rule under the heading unless `plain`: Preview's heading has none,
    // its glyph in the secondary text colour.
    void setHeading(std::string glyph, std::string title, std::function<void()> edit = {}, bool plain = false);
    void addLine(std::string key, std::string value, std::optional<style::Role> valueColor = std::nullopt);
    void clearLines();

private:
    Column* m_lines;
    View* m_heading = nullptr;
};

// A coloured note: a glyph and wrapped text, as for a warning.
class Banner : public View
{
public:
    enum class Kind {
        Info,
        Warning,
        Error,
    };
    Banner(Kind kind, std::string title, std::string text);
    void setText(std::string title, std::string text);
    void setKind(Kind kind);

    bayu::ui::SizeF sizeHint() const override;
    double heightForWidth(double width) const override;

protected:
    void layout() override;
    void paint(bayu::ui::Painter& painter) override;

private:
    void updateColors();
    Kind m_kind;
    bayu::ui::Icon* m_icon;
    bayu::ui::Label* m_title;
    bayu::ui::Label* m_text;
};

// The footer's buttons: Back (outlined), Continue (accent), Restart (green).
class ActionButton : public Pressable
{
public:
    enum class Kind {
        Secondary,
        Primary,
        Success,
        Danger,
        // Text in the accent colour, no frame: "Edit".
        Link,
    };
    ActionButton(std::string text, Kind kind = Kind::Secondary);
    void setText(std::string text);
    void setKind(Kind kind);
    void setLeadingGlyph(std::string glyph);
    void setTrailingGlyph(std::string glyph);
    // A spinner after the text, the button greyed and inert.
    void setBusy(bool busy);

    bayu::ui::SizeF sizeHint() const override;

protected:
    void layout() override;
    void paint(bayu::ui::Painter& painter) override;
    void enabledChanged() override;

private:
    void updateColors();
    Kind m_kind;
    bool m_busy = false;
    bayu::ui::Label* m_label;
    bayu::ui::Icon* m_leading;
    bayu::ui::Icon* m_trailing;
    bayu::ui::BusyIndicator* m_spinner;
};

// A text field drawn as a white rounded box, the accent edge while focused,
// and an optional eye button that shows a password.
class Field : public View
{
public:
    explicit Field(std::string text = {});
    bayu::ui::TextField* edit() const { return m_edit; }
    const std::string& text() const;
    void setText(std::string text);
    void setPlaceholder(std::string placeholder);
    void setPassword(bool password);
    // A search glyph before the text.
    void setSearch(bool search);
    // The edge in the danger colour.
    void setInvalid(bool invalid);
    std::function<void()> onChanged;

    bayu::ui::SizeF sizeHint() const override;

protected:
    void layout() override;
    void paint(bayu::ui::Painter& painter) override;

private:
    class Eye;
    bayu::ui::TextField* m_edit;
    bayu::ui::Icon* m_search = nullptr;
    Eye* m_eye = nullptr;
    bool m_invalid = false;
};

// A label over a field: "Full name" then the box.
View* addLabeledField(View* parent, std::string label, Field** field);

// A drop-down drawn like a Field: the current item and a chevron, a menu of
// the items below it.
class Dropdown : public Pressable
{
public:
    Dropdown();
    ~Dropdown() override;
    void setItems(std::vector<std::string> items);
    const std::vector<std::string>& items() const { return m_items; }
    void setCurrentIndex(int index);
    int currentIndex() const { return m_current; }
    std::string currentText() const;
    std::function<void(int index)> onActivated;

    bayu::ui::SizeF sizeHint() const override;

protected:
    void layout() override;
    void paint(bayu::ui::Painter& painter) override;

private:
    void open();
    std::vector<std::string> m_items;
    int m_current = -1;
    bayu::ui::Label* m_label;
    bayu::ui::Icon* m_chevron;
    std::unique_ptr<bayu::ui::Menu> m_menu;
};

// How a disk will be laid out: coloured segments in proportion, and a
// legend of mount points and sizes under them.
class LayoutBar : public View
{
public:
    struct Part
    {
        std::string name;
        std::string size;
        double fraction = 0;
        bayu::ui::Color color;
        // Hatched: space kept as it is.
        bool kept = false;
    };
    LayoutBar();
    void setParts(std::vector<Part> parts);
    // The legend on or off.
    void setLegendVisible(bool visible);

    bayu::ui::SizeF sizeHint() const override;
    double heightForWidth(double width) const override;

protected:
    void layout() override;
    void paint(bayu::ui::Painter& painter) override;

private:
    std::vector<Part> m_parts;
    View* m_legend;
    bool m_legendVisible = true;
};

// The colour a mount point is drawn in, on bars and legends.
bayu::ui::Color mountColor(const std::string& mountPoint);

// A password's strength: four segments and a caption.
class StrengthMeter : public View
{
public:
    StrengthMeter();
    // 0 (none) to 4 (strong); the caption and the length beside it.
    void setStrength(int strength, std::string caption, std::string length);

    bayu::ui::SizeF sizeHint() const override;

protected:
    void layout() override;
    void paint(bayu::ui::Painter& painter) override;

private:
    int m_strength = 0;
    bayu::ui::Label* m_caption;
    bayu::ui::Label* m_length;
};

// A round avatar with initials on an accent gradient.
class Avatar : public Pressable
{
public:
    explicit Avatar(double size = 56);
    void setInitials(std::string initials);
    // A picture instead of the initials.
    void setPicture(std::string path);

    bayu::ui::SizeF sizeHint() const override { return {m_size, m_size}; }

protected:
    void layout() override;
    void paint(bayu::ui::Painter& painter) override;

private:
    double m_size;
    std::string m_picture;
    bayu::ui::ImageView* m_image;
    bayu::ui::Label* m_initials;
};

// An icon tile: a glyph on a rounded square, grey or in the accent.
class IconTile : public View
{
public:
    IconTile(std::string glyph, double size = 36);
    void setGlyph(std::string glyph);
    void setAccent(bool accent);

    bayu::ui::SizeF sizeHint() const override { return {m_size, m_size}; }

protected:
    void layout() override;
    void paint(bayu::ui::Painter& painter) override;

private:
    double m_size;
    bool m_accent = false;
    bayu::ui::Icon* m_icon;
};

} // namespace installer
