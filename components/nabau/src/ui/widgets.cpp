// SPDX-License-Identifier: BSD-3-Clause

#include "ui/widgets.h"

#include <algorithm>
#include <cmath>

#include "base/i18n.h"
#include "ui/color_provider.h"
#include "ui/icon.h"
#include "ui/image_view.h"
#include "ui/keys.h"
#include "ui/label.h"
#include "ui/menu.h"
#include "ui/painter.h"
#include "ui/path.h"
#include "ui/progress_bar.h"
#include "ui/scroll_view.h"
#include "ui/switch.h"
#include "ui/text_field.h"
#include "ui/theme.h"

using namespace bayu::ui;
using bayu::i18n::tr;

namespace installer {

using style::Role;

namespace {

Color resolve(Role role)
{
    return style::color(role).resolve();
}

Color accent()
{
    return Theme::current().highlight;
}

// The focus ring keyboard focus draws around a control.
void paintFocusRing(Painter& painter, const RectF& rect, double radius)
{
    painter.drawRoundedRect(rect.adjusted(-3, -3, 3, 3), {radius + 3, 0, Corners::Circular}, Color{0, 0, 0, 0}, 2,
                            accent().withAlpha(0.35));
}

// The radio mark of option cards: a ring, or a ring with a check while on.
void paintRadio(Painter& painter, const RectF& rect, bool on)
{
    const Color edge = on ? accent() : resolve(Role::TertiaryText);
    painter.drawSmoothRectangle(rect, rect.height / 2, Color{0, 0, 0, 0}, 1.5, edge);
    if (on) {
        Path check;
        const double x = rect.x;
        const double y = rect.y;
        const double s = rect.width / 18;
        check.moveTo(x + 5.2 * s, y + 9.2 * s);
        check.lineTo(x + 7.9 * s, y + 11.8 * s);
        check.lineTo(x + 12.8 * s, y + 6.6 * s);
        painter.strokePath(check, accent(), 1.6);
    }
}

void paintCheckBox(Painter& painter, const RectF& rect, bool on)
{
    const Color edge = on ? accent() : resolve(Role::TertiaryText);
    painter.drawSmoothRectangle(rect, 4, Color{0, 0, 0, 0}, 1.5, edge);
    if (on) {
        Path check;
        const double s = rect.width / 18;
        check.moveTo(rect.x + 4.8 * s, rect.y + 9.4 * s);
        check.lineTo(rect.x + 7.7 * s, rect.y + 12.2 * s);
        check.lineTo(rect.x + 13.2 * s, rect.y + 6.2 * s);
        painter.strokePath(check, accent(), 1.6);
    }
}

} // namespace

Label* addText(View* parent, std::string text, double size, int weight, std::optional<Role> color)
{
    Label* label = parent->addChild<Label>(std::move(text));
    style::setFont(label, size, weight);
    if (color)
        label->setColor(style::color(*color));
    return label;
}

Label* addSectionLabel(View* parent, std::string text)
{
    Label* label = addText(parent, std::move(text), style::sectionSize, 500, Role::SecondaryText);
    label->setLetterSpacing(1.6);
    return label;
}

// --- Card -------------------------------------------------------------

Card::Card(double padding)
{
    setMargins(Margins::all(padding));
    setLayoutHints({.fillWidth = true});
}

void Card::setFill(std::optional<Role> fill, std::optional<Role> border)
{
    m_fill = fill;
    m_border = border;
    requestPaint();
}

void Card::paint(Painter& painter)
{
    const Color fill = m_fill ? resolve(*m_fill) : Color{0, 0, 0, 0};
    painter.drawRoundedRect(localRect(), {style::cardRadius, 0, Corners::Circular}, fill, m_border ? 1 : 0,
                            m_border ? resolve(*m_border) : Color{});
}

// --- Pressable --------------------------------------------------------

void Pressable::pointerEnter()
{
    m_hovered = true;
    requestPaint();
}

void Pressable::pointerLeave()
{
    m_hovered = false;
    requestPaint();
}

void Pressable::pointerPress(const PointerEvent& event)
{
    if (event.button != MouseButton::Left)
        return;
    m_pressed = true;
    requestPaint();
}

void Pressable::pointerRelease(const PointerEvent& event)
{
    const bool clicked = m_pressed && localRect().contains(event.position);
    m_pressed = false;
    requestPaint();
    if (clicked && onClicked)
        onClicked();
}

bool Pressable::keyPress(const KeyEvent& event)
{
    if (event.key == key::Return || event.key == key::KeypadEnter || event.key == key::Space) {
        if (onClicked)
            onClicked();
        return true;
    }
    return false;
}

// --- ListRow ----------------------------------------------------------

ListRow::ListRow()
{
    m_badge = addText(this, {}, 11, 500, Role::SecondaryText);
    m_glyph = addChild<Icon>(std::string(), 18);
    m_glyph->setVisible(false);
    m_title = addText(this, {}, style::rowTitleSize, 500);
    m_detail = addText(this, {}, style::rowDetailSize, 400, Role::SecondaryText);
    m_trailing = addChild<Icon>(std::string(), 18);
    m_trailing->setColor(style::color(Role::SecondaryText));
    m_trailing->setVisible(false);
    m_check = addChild<Icon>("check", 16);
    m_check->setColor(ColorRecipe(ColorId::Highlight));
    m_check->setVisible(false);
    setLayoutHints({.fillWidth = true});
}

void ListRow::setBadge(std::string badge)
{
    m_badge->setText(std::move(badge));
}

void ListRow::setGlyph(std::string glyph)
{
    m_glyph->setIconName(std::move(glyph));
    m_glyph->setVisible(true);
    m_badge->setVisible(false);
    requestLayout();
}

void ListRow::setTitle(std::string title)
{
    m_title->setText(std::move(title));
}

void ListRow::setDetail(std::string detail)
{
    m_detail->setText(std::move(detail));
    m_detail->setVisible(!m_detail->text().empty());
    requestLayout();
}

void ListRow::setTrailingGlyph(std::string glyph)
{
    m_trailing->setIconName(glyph);
    m_trailing->setVisible(!glyph.empty());
    requestLayout();
}

void ListRow::setSelected(bool selected)
{
    if (selected == m_selected)
        return;
    m_selected = selected;
    updateColors();
    m_check->setVisible(selected);
    requestPaint();
}

void ListRow::updateColors()
{
    m_badge->setColor(m_selected ? ColorRecipe(ColorId::Highlight) : style::color(Role::SecondaryText));
    m_glyph->setColor(m_selected ? ColorRecipe(ColorId::Highlight) : style::color(Role::SecondaryText));
    m_title->setWeight(m_selected ? 600 : 500);
}

SizeF ListRow::sizeHint() const
{
    return {200, style::listRowHeight};
}

void ListRow::layout()
{
    const double width = geometry().width;
    const double height = geometry().height;
    const double textX = 46;
    m_badge->setGeometry({12, 0, 30, height});
    m_badge->setVerticallyCentered(true);
    m_glyph->setGeometry({13, (height - 18) / 2, 18, 18});
    const double end = width - 18;
    m_check->setGeometry({end - 16, (height - 16) / 2, 16, 16});
    m_trailing->setGeometry({end - 18, (height - 18) / 2, 18, 18});
    const double textWidth = std::max(0.0, end - 24 - textX);
    const double titleHeight = m_title->sizeHint().height;
    if (m_detail->isVisible()) {
        const double detailHeight = m_detail->sizeHint().height;
        const double top = std::round((height - titleHeight - detailHeight - 1) / 2);
        m_title->setGeometry({textX, top, textWidth, titleHeight});
        m_detail->setGeometry({textX, top + titleHeight + 1, textWidth, detailHeight});
    } else {
        m_title->setGeometry({textX, std::round((height - titleHeight) / 2), textWidth, titleHeight});
    }
}

void ListRow::paint(Painter& painter)
{
    const RectF rect = localRect();
    if (m_selected) {
        painter.drawRoundedRect(rect, {style::rowRadius, 0, Corners::Circular}, resolve(Role::SelectedFill), 1.5,
                                resolve(Role::SelectedBorder));
    } else if (isPressed() || isHovered()) {
        painter.drawRoundedRect(rect, {style::rowRadius, 0, Corners::Circular},
                                Theme::current().text.withAlpha(isPressed() ? 0.08 : 0.04));
    }
    if (hasVisibleFocus())
        painter.drawRoundedRect(rect.adjusted(1, 1, -1, -1), {style::rowRadius - 1, 0, Corners::Circular},
                                Color{0, 0, 0, 0}, 2, accent().withAlpha(0.5));
}

// --- ListCard ---------------------------------------------------------

namespace {

class RowsView : public Column
{
};

} // namespace

ListCard::ListCard()
    : Card(7)
{
    ScrollView* scroll = addChild<ScrollView>();
    scroll->setLayoutHints({.fillWidth = true, .fillHeight = true});
    Column* rows = scroll->setContent<RowsView>();
    rows->setSpacing(0);
    setLayoutHints({.fillWidth = true, .fillHeight = true});
}

ListRow* ListCard::addRow()
{
    auto* scroll = static_cast<ScrollView*>(children().front().get());
    ListRow* row = scroll->contentView()->addChild<ListRow>();
    m_rows.push_back(row);
    return row;
}

void ListCard::clearRows()
{
    auto* scroll = static_cast<ScrollView*>(children().front().get());
    View* rows = scroll->contentView();
    while (!rows->children().empty())
        rows->takeChild(rows->children().back().get());
    m_rows.clear();
    scroll->setScrollPosition({0, 0});
}

void ListCard::select(ListRow* selected)
{
    for (ListRow* row : m_rows)
        row->setSelected(row == selected);
    if (!selected)
        return;
    auto* scroll = static_cast<ScrollView*>(children().front().get());
    const RectF& rect = selected->geometry();
    if (rect.height > 0)
        scroll->ensureVisible(rect);
}

// --- IconTile ---------------------------------------------------------

IconTile::IconTile(std::string glyph, double size)
    : m_size(size)
{
    m_icon = addChild<Icon>(std::move(glyph), std::round(size * 0.5));
    m_icon->setColor(style::color(Role::SecondaryText));
}

void IconTile::setGlyph(std::string glyph)
{
    m_icon->setIconName(std::move(glyph));
}

void IconTile::setAccent(bool accent)
{
    m_accent = accent;
    m_icon->setColor(accent ? ColorRecipe(Color::rgb(0xFFFFFF)) : style::color(Role::SecondaryText));
    requestPaint();
}

void IconTile::layout()
{
    const double icon = m_icon->sizeHint().width;
    m_icon->setGeometry({(geometry().width - icon) / 2, (geometry().height - icon) / 2, icon, icon});
}

void IconTile::paint(Painter& painter)
{
    painter.drawRoundedRect(localRect(), {style::tileRadius, 0, Corners::Circular},
                            m_accent ? accent() : resolve(Role::IconTile));
}

// --- OptionCard -------------------------------------------------------

OptionCard::OptionCard(std::string glyph)
{
    m_icon = addChild<Icon>(std::move(glyph), 18);
    m_title = addText(this, {}, style::rowTitleSize, 600);
    m_title->setWrap(true);
    m_title->setMaximumLineCount(2);
    m_detail = addText(this, {}, style::rowDetailSize, 400, Role::SecondaryText);
    m_detail->setWrap(true);
    m_detail->setMaximumLineCount(2);
    m_note = addText(this, {}, style::rowDetailSize, 400, Role::SecondaryText);
    m_note->setTextAlignment(Label::TextAlignment::Right);
    setLayoutHints({.fillWidth = true});
    updateColors();
}

void OptionCard::setGlyph(std::string glyph)
{
    m_icon->setIconName(std::move(glyph));
}

void OptionCard::setTitle(std::string title)
{
    m_title->setText(std::move(title));
}

void OptionCard::setDetail(std::string detail)
{
    m_detail->setText(std::move(detail));
}

void OptionCard::setNote(std::string note, bool accentNote)
{
    m_note->setText(std::move(note));
    m_noteAccent = accentNote;
    updateColors();
    requestLayout();
}

void OptionCard::setSelected(bool selected)
{
    if (selected == m_selected)
        return;
    m_selected = selected;
    updateColors();
    requestPaint();
}

void OptionCard::enabledChanged()
{
    requestPaint();
}

void OptionCard::updateColors()
{
    m_icon->setColor(m_selected ? ColorRecipe(Color::rgb(0xFFFFFF)) : style::color(Role::SecondaryText));
    m_note->setColor(m_selected || m_noteAccent ? ColorRecipe(ColorId::Highlight) : style::color(Role::SecondaryText));
}

SizeF OptionCard::sizeHint() const
{
    return {300, style::optionCardHeight};
}

// The text's columns at a width: the title stops before the note, the
// detail before the mark.
static double optionTextX()
{
    return 14 + 36 + 14;
}

double OptionCard::heightForWidth(double width) const
{
    const double markX = width - 16 - 18;
    const double noteWidth = m_note->text().empty() ? 0 : std::min(m_note->sizeHint().width, width * 0.35);
    const double titleWidth = std::max(10.0, markX - 12 - noteWidth - 8 - optionTextX());
    const double detailWidth = std::max(10.0, markX - 12 - optionTextX());
    const double text = m_title->heightForWidth(titleWidth) + 2 + m_detail->heightForWidth(detailWidth);
    return std::max(style::optionCardHeight, std::ceil(text + 26));
}

void OptionCard::layout()
{
    const double width = geometry().width;
    const double height = geometry().height;
    m_icon->setGeometry({14 + 9, (height - 18) / 2, 18, 18});
    const double textX = optionTextX();
    const double markX = width - 16 - 18;
    const double noteWidth = m_note->text().empty() ? 0 : std::min(m_note->sizeHint().width, width * 0.35);
    const double noteX = markX - 12 - noteWidth;
    const double titleWidth = std::max(10.0, noteX - 8 - textX);
    const double detailWidth = std::max(10.0, markX - 12 - textX);
    const double titleHeight = m_title->heightForWidth(titleWidth);
    const double detailHeight = m_detail->heightForWidth(detailWidth);
    const double top = std::round((height - titleHeight - detailHeight - 2) / 2);
    m_title->setGeometry({textX, top, titleWidth, titleHeight});
    m_detail->setGeometry({textX, top + titleHeight + 2, detailWidth, detailHeight});
    m_note->setGeometry({noteX, top, noteWidth, m_note->sizeHint().height});
}

void OptionCard::paint(Painter& painter)
{
    const RectF rect = localRect();
    Color fill = resolve(Role::Card);
    if (m_selected)
        fill = resolve(Role::SelectedFill);
    painter.drawRoundedRect(rect, {style::cardRadius, 0, Corners::Circular}, resolve(Role::Card));
    painter.drawRoundedRect(rect, {style::cardRadius, 0, Corners::Circular}, fill, m_selected ? 1.5 : 1,
                            m_selected ? resolve(Role::SelectedBorder) : resolve(Role::CardBorder));
    if (!m_selected && (isHovered() || isPressed()))
        painter.drawRoundedRect(rect, {style::cardRadius, 0, Corners::Circular},
                                Theme::current().text.withAlpha(isPressed() ? 0.06 : 0.03));
    const double height = rect.height;
    painter.drawRoundedRect({14, (height - 36) / 2, 36, 36}, {style::tileRadius, 0, Corners::Circular},
                            m_selected ? accent() : resolve(Role::IconTile));
    const RectF mark{rect.width - 16 - 18, (height - 18) / 2, 18, 18};
    if (m_checkStyle)
        paintCheckBox(painter, mark, m_selected);
    else
        paintRadio(painter, mark, m_selected);
    if (hasVisibleFocus())
        paintFocusRing(painter, rect, style::cardRadius);
    if (!isEnabled())
        painter.drawRoundedRect(rect, {style::cardRadius, 0, Corners::Circular},
                                resolve(Role::Content).withAlpha(0.45));
}

// --- CheckRow ---------------------------------------------------------

CheckRow::CheckRow(std::string title, std::string detail)
{
    m_title = addText(this, std::move(title), style::rowTitleSize, 600);
    m_detail = addText(this, std::move(detail), style::rowDetailSize, 400, Role::SecondaryText);
    onClicked = [this] {
        setChecked(!m_checked);
        if (onToggled)
            onToggled(m_checked);
    };
    setLayoutHints({.fillWidth = true});
}

void CheckRow::setChecked(bool checked)
{
    m_checked = checked;
    requestPaint();
}

SizeF CheckRow::sizeHint() const
{
    return {200, 52};
}

void CheckRow::layout()
{
    const double height = geometry().height;
    const double titleHeight = m_title->sizeHint().height;
    const double detailHeight = m_detail->sizeHint().height;
    const double top = std::round((height - titleHeight - detailHeight - 1) / 2);
    const double x = 46;
    const double width = std::max(0.0, geometry().width - x - 12);
    m_title->setGeometry({x, top, width, titleHeight});
    m_detail->setGeometry({x, top + titleHeight + 1, width, detailHeight});
}

void CheckRow::paint(Painter& painter)
{
    if (isHovered() || isPressed())
        painter.drawRoundedRect(localRect(), {style::rowRadius, 0, Corners::Circular},
                                Theme::current().text.withAlpha(isPressed() ? 0.06 : 0.03));
    paintCheckBox(painter, {16, (geometry().height - 18) / 2 - 7, 18, 18}, m_checked);
    if (hasVisibleFocus())
        painter.drawRoundedRect(localRect().adjusted(1, 1, -1, -1), {style::rowRadius, 0, Corners::Circular},
                                Color{0, 0, 0, 0}, 2, accent().withAlpha(0.5));
    if (!isEnabled())
        painter.fillRect(localRect(), resolve(Role::Card).withAlpha(0.5));
}

// --- ToggleCard -------------------------------------------------------

ToggleCard::ToggleCard(std::string title, std::string detail)
{
    m_title = addText(this, std::move(title), style::rowTitleSize, 600);
    m_title->setWrap(true);
    m_detail = addText(this, std::move(detail), style::rowDetailSize, 400, Role::SecondaryText);
    m_detail->setWrap(true);
    m_switch = addChild<Switch>();
    m_switch->onToggled = [this](bool checked) {
        if (onToggled)
            onToggled(checked);
    };
    setLayoutHints({.fillWidth = true});
}

void ToggleCard::setChecked(bool checked)
{
    m_switch->setChecked(checked);
}

bool ToggleCard::isChecked() const
{
    return m_switch->isChecked();
}

void ToggleCard::setDetail(std::string detail)
{
    m_detail->setText(std::move(detail));
    requestLayout();
}

SizeF ToggleCard::sizeHint() const
{
    return {300, heightForWidth(300)};
}

double ToggleCard::heightForWidth(double width) const
{
    const double textWidth = std::max(10.0, width - 20 - m_switch->sizeHint().width - 32);
    return std::max(64.0, 28 + m_title->heightForWidth(textWidth) + 2 + m_detail->heightForWidth(textWidth));
}

void ToggleCard::layout()
{
    const double width = geometry().width;
    const double height = geometry().height;
    const SizeF toggle = m_switch->sizeHint();
    m_switch->setGeometry({width - 16 - toggle.width, (height - toggle.height) / 2, toggle.width, toggle.height});
    const double textWidth = std::max(10.0, width - 20 - toggle.width - 32);
    const double titleHeight = m_title->heightForWidth(textWidth);
    const double detailHeight = m_detail->heightForWidth(textWidth);
    const double top = std::round((height - titleHeight - detailHeight - 2) / 2);
    m_title->setGeometry({20, top, textWidth, titleHeight});
    m_detail->setGeometry({20, top + titleHeight + 2, textWidth, detailHeight});
}

void ToggleCard::paint(Painter& painter)
{
    painter.drawRoundedRect(localRect(), {style::cardRadius, 0, Corners::Circular}, resolve(Role::Card), 1,
                            resolve(Role::CardBorder));
}

// --- DetailsCard ------------------------------------------------------

namespace {

// "Key .... value" on one line.
class DetailLine : public View
{
public:
    DetailLine(std::string key, std::string value, std::optional<Role> valueColor)
    {
        m_key = addText(this, std::move(key), 13, 400, Role::SecondaryText);
        m_value = addText(this, std::move(value), 13, 400, valueColor);
        m_value->setTextAlignment(Label::TextAlignment::Right);
        setLayoutHints({.fillWidth = true});
    }

    SizeF sizeHint() const override { return {200, std::max(m_key->sizeHint().height, 22.0)}; }

protected:
    void layout() override
    {
        const double width = geometry().width;
        const double height = geometry().height;
        const double keyWidth = std::min(m_key->sizeHint().width, width * 0.5);
        m_key->setGeometry({0, 0, keyWidth, height});
        m_key->setVerticallyCentered(true);
        m_value->setGeometry({keyWidth + 12, 0, std::max(0.0, width - keyWidth - 12), height});
        m_value->setVerticallyCentered(true);
    }

private:
    Label* m_key;
    Label* m_value;
};

class Rule : public View
{
public:
    Rule() { setLayoutHints({.fillWidth = true}); }
    SizeF sizeHint() const override { return {1, 1}; }

protected:
    void paint(Painter& painter) override
    {
        painter.drawHorizontalLine(0, 0, geometry().width, resolve(Role::CardBorder));
    }
};

} // namespace

DetailsCard::DetailsCard()
    : Card(0)
{
    setMargins({20, 16, 20, 16});
    setSpacing(12);
    m_lines = addChild<Column>();
    m_lines->setSpacing(6);
    m_lines->setLayoutHints({.fillWidth = true});
}

void DetailsCard::setHeading(std::string glyph, std::string title, std::function<void()> edit, bool plain)
{
    if (m_heading)
        return;
    std::unique_ptr<View> owned = takeChild(m_lines);
    Row* heading = addChild<Row>();
    heading->setSpacing(10);
    heading->setLayoutHints({.fillWidth = true});
    Icon* icon = heading->addChild<Icon>(std::move(glyph), 16);
    icon->setColor(plain ? style::color(Role::SecondaryText) : ColorRecipe(ColorId::Highlight));
    addText(heading, std::move(title), 14, 600);
    heading->addStretch();
    if (edit) {
        ActionButton* button = heading->addChild<ActionButton>(tr("Edit"), ActionButton::Kind::Link);
        button->onClicked = std::move(edit);
    }
    if (!plain)
        addChild<Rule>();
    addChild(std::move(owned));
    m_heading = heading;
}

void DetailsCard::addLine(std::string key, std::string value, std::optional<Role> valueColor)
{
    m_lines->addChild<DetailLine>(std::move(key), std::move(value), valueColor);
}

void DetailsCard::clearLines()
{
    while (!m_lines->children().empty())
        m_lines->takeChild(m_lines->children().back().get());
}

// --- Banner -----------------------------------------------------------

Banner::Banner(Kind kind, std::string title, std::string text)
    : m_kind(kind)
{
    m_icon = addChild<Icon>(kind == Kind::Info ? "shield" : "alert", 18);
    m_title = addText(this, std::move(title), 14, 600);
    m_title->setWrap(true);
    m_text = addText(this, std::move(text), 13);
    m_text->setWrap(true);
    setLayoutHints({.fillWidth = true});
    updateColors();
}

void Banner::setText(std::string title, std::string text)
{
    m_title->setText(std::move(title));
    m_text->setText(std::move(text));
    requestLayout();
}

void Banner::setKind(Kind kind)
{
    m_kind = kind;
    m_icon->setIconName(kind == Kind::Info ? "shield" : "alert");
    updateColors();
    requestPaint();
}

void Banner::updateColors()
{
    const ColorRecipe tint = m_kind == Kind::Info      ? ColorRecipe(ColorId::Highlight)
                             : m_kind == Kind::Warning ? style::color(Role::Warning)
                                                       : style::color(Role::Danger);
    m_icon->setColor(tint);
    m_title->setColor(tint);
    m_text->setColor(m_kind == Kind::Info ? style::color(Role::SecondaryText) : std::optional<ColorRecipe>());
}

SizeF Banner::sizeHint() const
{
    return {300, heightForWidth(300)};
}

double Banner::heightForWidth(double width) const
{
    const double textWidth = std::max(10.0, width - 56 - 20);
    double height = 32;
    if (!m_title->text().empty())
        height += m_title->heightForWidth(textWidth) + 4;
    if (!m_text->text().empty())
        height += m_text->heightForWidth(textWidth);
    return height;
}

void Banner::layout()
{
    const double textWidth = std::max(10.0, geometry().width - 56 - 20);
    m_icon->setGeometry({20, 17, 18, 18});
    double y = 16;
    if (!m_title->text().empty()) {
        const double h = m_title->heightForWidth(textWidth);
        m_title->setGeometry({56, y, textWidth, h});
        y += h + 4;
    } else {
        m_title->setGeometry({});
    }
    m_text->setGeometry({56, y, textWidth, m_text->heightForWidth(textWidth)});
}

void Banner::paint(Painter& painter)
{
    Color fill = resolve(Role::InfoFill);
    Color edge = accent();
    if (m_kind == Kind::Warning) {
        fill = resolve(Role::WarningFill);
        edge = resolve(Role::Warning);
    } else if (m_kind == Kind::Error) {
        fill = resolve(Role::Danger).withAlpha(0.1);
        edge = resolve(Role::Danger);
    }
    painter.drawRoundedRect(localRect(), {style::cardRadius, 0, Corners::Circular}, fill, 1.5, edge);
}

// --- ActionButton -----------------------------------------------------

ActionButton::ActionButton(std::string text, Kind kind)
    : m_kind(kind)
{
    m_leading = addChild<Icon>(std::string(), 16);
    m_leading->setVisible(false);
    m_label = addText(this, std::move(text), 15, 500);
    m_trailing = addChild<Icon>(std::string(), 16);
    m_trailing->setVisible(false);
    m_spinner = addChild<BusyIndicator>();
    m_spinner->setVisible(false);
    updateColors();
}

void ActionButton::setText(std::string text)
{
    m_label->setText(std::move(text));
    requestLayout();
}

void ActionButton::setKind(Kind kind)
{
    m_kind = kind;
    updateColors();
    requestPaint();
}

void ActionButton::setLeadingGlyph(std::string glyph)
{
    m_leading->setIconName(glyph);
    m_leading->setVisible(!glyph.empty());
    requestLayout();
}

void ActionButton::setTrailingGlyph(std::string glyph)
{
    m_trailing->setIconName(glyph);
    m_trailing->setVisible(!glyph.empty());
    requestLayout();
}

void ActionButton::setBusy(bool busy)
{
    m_busy = busy;
    m_spinner->setVisible(busy);
    m_spinner->setRunning(busy);
    setEnabled(!busy);
    updateColors();
    requestLayout();
}

void ActionButton::enabledChanged()
{
    updateColors();
    requestPaint();
}

void ActionButton::updateColors()
{
    std::optional<ColorRecipe> text;
    if (!isEnabled())
        text = style::color(Role::TertiaryText);
    else if (m_kind == Kind::Secondary)
        text = std::nullopt;
    else if (m_kind == Kind::Link)
        text = ColorRecipe(ColorId::Highlight);
    else
        text = ColorRecipe(Color::rgb(0xFFFFFF));
    m_label->setColor(text);
    m_leading->setColor(text);
    m_trailing->setColor(text);
    m_spinner->setColor(text ? std::optional<Color>(text->resolve()) : std::nullopt);
}

SizeF ActionButton::sizeHint() const
{
    if (m_kind == Kind::Link)
        return {m_label->sizeHint().width + 4, m_label->sizeHint().height};
    double width = m_label->sizeHint().width + 2 * 22;
    if (m_leading->isVisible())
        width += 16 + 10;
    if (m_trailing->isVisible())
        width += 16 + 10;
    if (m_spinner->isVisible())
        width += 18 + 10;
    return {std::max(width, 88.0), style::buttonHeight};
}

void ActionButton::layout()
{
    const double width = geometry().width;
    const double height = geometry().height;
    const double labelWidth = m_label->sizeHint().width;
    const double labelHeight = m_label->sizeHint().height;
    if (m_kind == Kind::Link) {
        m_label->setGeometry({0, (height - labelHeight) / 2, width, labelHeight});
        return;
    }
    double content = labelWidth;
    if (m_leading->isVisible())
        content += 26;
    if (m_trailing->isVisible())
        content += 26;
    if (m_spinner->isVisible())
        content += 28;
    double x = std::round((width - content) / 2);
    if (m_leading->isVisible()) {
        m_leading->setGeometry({x, (height - 16) / 2, 16, 16});
        x += 26;
    }
    m_label->setGeometry({x, std::round((height - labelHeight) / 2), labelWidth + 1, labelHeight});
    x += labelWidth + 10;
    if (m_trailing->isVisible()) {
        m_trailing->setGeometry({x, (height - 16) / 2, 16, 16});
        x += 26;
    }
    if (m_spinner->isVisible())
        m_spinner->setGeometry({x, (height - 18) / 2, 18, 18});
}

void ActionButton::paint(Painter& painter)
{
    if (m_kind == Kind::Link)
        return;
    const RectF rect = localRect();
    const RoundedShape shape{style::buttonRadius, 0, Corners::Circular};
    if (!isEnabled()) {
        const bool frame = m_kind == Kind::Secondary && !m_busy;
        painter.drawRoundedRect(rect, shape, frame ? resolve(Role::Card) : resolve(Role::IconTile), frame ? 1 : 0,
                                resolve(Role::CardBorder));
        return;
    }
    Color fill;
    switch (m_kind) {
    case Kind::Secondary:
        fill = resolve(Role::Card);
        break;
    case Kind::Primary:
        fill = accent();
        break;
    case Kind::Success:
        fill = resolve(Role::Success);
        break;
    case Kind::Danger:
        fill = resolve(Role::Danger);
        break;
    case Kind::Link:
        break;
    }
    if (m_kind == Kind::Secondary) {
        painter.drawRoundedRect(rect, shape, fill, 1, resolve(Role::CardBorder));
        if (isHovered() || isPressed())
            painter.drawRoundedRect(rect, shape, Theme::current().text.withAlpha(isPressed() ? 0.08 : 0.04));
    } else {
        if (isPressed())
            fill = fill.darker(1.12);
        else if (isHovered())
            fill = fill.lighter(1.06);
        painter.drawRoundedRect(rect, shape, fill);
    }
    if (hasVisibleFocus())
        paintFocusRing(painter, rect, style::buttonRadius);
}

// --- Field ------------------------------------------------------------

namespace {

// The field's edit: tells the box to repaint its edge as the focus moves.
class FieldEdit : public TextField
{
protected:
    void focusIn(FocusReason reason) override
    {
        TextField::focusIn(reason);
        if (parent())
            parent()->requestPaint();
    }
    void focusOut() override
    {
        TextField::focusOut();
        if (parent())
            parent()->requestPaint();
    }
};

} // namespace

class Field::Eye : public Pressable
{
public:
    explicit Eye(Field* field)
        : m_field(field)
    {
        m_icon = addChild<Icon>("eye", 16);
        m_icon->setColor(style::color(Role::SecondaryText));
        setClickFocusOnly(true);
        onClicked = [this] {
            TextField* edit = m_field->edit();
            const bool hidden = edit->echoMode() == TextField::EchoMode::Password;
            edit->setEchoMode(hidden ? TextField::EchoMode::Normal : TextField::EchoMode::Password);
            m_icon->setIconName(hidden ? "eye-off" : "eye");
        };
    }
    bool acceptsFocus() const override { return false; }
    SizeF sizeHint() const override { return {28, 28}; }

protected:
    void layout() override { m_icon->setGeometry({6, 6, 16, 16}); }
    void paint(Painter& painter) override
    {
        if (isHovered())
            painter.fillEllipse(localRect(), Theme::current().text.withAlpha(0.06));
    }

private:
    Field* m_field;
    Icon* m_icon;
};

Field::Field(std::string text)
{
    m_edit = addChild<FieldEdit>();
    m_edit->setText(std::move(text));
    m_edit->setFrameVisible(false);
    m_edit->setTextMargins(14, 14);
    style::setFont(m_edit, 15);
    m_edit->onTextChanged = [this] {
        if (onChanged)
            onChanged();
    };
    setLayoutHints({.fillWidth = true});
}

const std::string& Field::text() const
{
    return m_edit->text();
}

void Field::setText(std::string text)
{
    m_edit->setText(std::move(text));
}

void Field::setPlaceholder(std::string placeholder)
{
    m_edit->setPlaceholder(std::move(placeholder));
}

void Field::setPassword(bool password)
{
    m_edit->setEchoMode(password ? TextField::EchoMode::Password : TextField::EchoMode::Normal);
    if (password && !m_eye)
        m_eye = addChild<Eye>(this);
    if (m_eye)
        m_eye->setVisible(password);
    requestLayout();
}

void Field::setSearch(bool search)
{
    if (search && !m_search) {
        m_search = addChild<Icon>("search", 16);
        m_search->setColor(style::color(Role::SecondaryText));
    }
    if (m_search)
        m_search->setVisible(search);
    m_edit->setTextMargins(search ? 40 : 14, 14);
    requestLayout();
}

void Field::setInvalid(bool invalid)
{
    if (invalid == m_invalid)
        return;
    m_invalid = invalid;
    requestPaint();
}

SizeF Field::sizeHint() const
{
    return {240, style::fieldHeight};
}

void Field::layout()
{
    const double width = geometry().width;
    const double height = geometry().height;
    const double eye = m_eye && m_eye->isVisible() ? 36 : 0;
    m_edit->setGeometry({0, 0, width - eye, height});
    if (m_eye)
        m_eye->setGeometry({width - 8 - 28, (height - 28) / 2, 28, 28});
    if (m_search)
        m_search->setGeometry({16, (height - 16) / 2, 16, 16});
}

void Field::paint(Painter& painter)
{
    const bool focused = m_edit->hasFocus();
    Color edge = resolve(Role::CardBorder);
    if (m_invalid)
        edge = resolve(Role::Danger);
    else if (focused)
        edge = accent();
    painter.drawRoundedRect(localRect(), {style::rowRadius, 0, Corners::Circular}, resolve(Role::Card),
                            focused || m_invalid ? 1.5 : 1, edge);
}

View* addLabeledField(View* parent, std::string label, Field** field)
{
    Column* column = parent->addChild<Column>();
    column->setSpacing(8);
    column->setLayoutHints({.fillWidth = true});
    addText(column, std::move(label), 13, 400, Role::SecondaryText);
    *field = column->addChild<Field>();
    return column;
}

// --- Dropdown ---------------------------------------------------------

Dropdown::Dropdown()
{
    m_label = addText(this, {}, 15);
    m_chevron = addChild<Icon>("chevron-down", 16);
    m_chevron->setColor(style::color(Role::SecondaryText));
    onClicked = [this] { open(); };
    setLayoutHints({.fillWidth = true});
}

Dropdown::~Dropdown() = default;

void Dropdown::setItems(std::vector<std::string> items)
{
    m_items = std::move(items);
    if (m_current >= int(m_items.size()))
        m_current = m_items.empty() ? -1 : 0;
    m_label->setText(currentText());
}

void Dropdown::setCurrentIndex(int index)
{
    m_current = index;
    m_label->setText(currentText());
}

std::string Dropdown::currentText() const
{
    return m_current >= 0 && m_current < int(m_items.size()) ? m_items[size_t(m_current)] : std::string();
}

SizeF Dropdown::sizeHint() const
{
    return {160, style::fieldHeight};
}

void Dropdown::layout()
{
    const double width = geometry().width;
    const double height = geometry().height;
    m_chevron->setGeometry({width - 16 - 16, (height - 16) / 2, 16, 16});
    const double labelHeight = m_label->sizeHint().height;
    m_label->setGeometry({16, (height - labelHeight) / 2, std::max(0.0, width - 16 - 40), labelHeight});
}

void Dropdown::paint(Painter& painter)
{
    const RectF rect = localRect();
    const bool open = m_menu && m_menu->isOpen();
    painter.drawRoundedRect(rect, {style::rowRadius, 0, Corners::Circular}, resolve(Role::Card), open ? 1.5 : 1,
                            open ? accent() : resolve(Role::CardBorder));
    if (isHovered() && !open)
        painter.drawRoundedRect(rect, {style::rowRadius, 0, Corners::Circular}, Theme::current().text.withAlpha(0.03));
    if (hasVisibleFocus())
        paintFocusRing(painter, rect, style::rowRadius);
}

void Dropdown::open()
{
    if (m_items.empty())
        return;
    m_menu = std::make_unique<Menu>();
    m_menu->setDropDown(true);
    m_menu->setMinimumWidth(geometry().width);
    for (size_t i = 0; i < m_items.size(); ++i) {
        const size_t index = m_menu->addItem(m_items[i], [this, i] {
            m_current = int(i);
            m_label->setText(currentText());
            if (onActivated)
                onActivated(int(i));
        });
        if (int(i) == m_current)
            m_menu->setItemChecked(index, true);
    }
    m_menu->onClosed = [this] { requestPaint(); };
    m_menu->popupFor(this, 4);
    requestPaint();
}

// --- LayoutBar --------------------------------------------------------

Color mountColor(const std::string& mountPoint)
{
    if (mountPoint == "/")
        return Color::rgb(0x2F7BF5);
    if (mountPoint == "/home")
        return Color::rgb(0x9EC2FF);
    if (mountPoint == "swap")
        return Color::rgb(0xC08A06);
    if (mountPoint == "/boot")
        return Color::rgb(0x0B4FC1);
    if (mountPoint == "/boot/efi")
        return Color::rgb(0x8C9AB0);
    if (mountPoint == "biosboot")
        return Color::rgb(0xA9B2C3);
    if (mountPoint.empty())
        return Color::rgb(0xC9CCD2);
    // Other mount points by their name, so each keeps its colour.
    static const Color others[] = {Color::rgb(0x7C5CF0), Color::rgb(0x17A2B8), Color::rgb(0xE0679A),
                                   Color::rgb(0x38A169)};
    size_t hash = 0;
    for (char c : mountPoint)
        hash = hash * 31 + size_t(uint8_t(c));
    return others[hash % 4];
}

namespace {

class LegendEntry : public View
{
public:
    LegendEntry(const LayoutBar::Part& part)
        : m_color(part.color)
        , m_kept(part.kept)
    {
        m_name = addText(this, part.name, 12);
        m_size = addText(this, part.size, 12, 400, Role::SecondaryText);
    }

    SizeF sizeHint() const override
    {
        return {16 + std::max(m_name->sizeHint().width, m_size->sizeHint().width),
                m_name->sizeHint().height + m_size->sizeHint().height};
    }

protected:
    void layout() override
    {
        const SizeF name = m_name->sizeHint();
        m_name->setGeometry({16, 0, name.width + 1, name.height});
        m_size->setGeometry({16, name.height, m_size->sizeHint().width + 1, m_size->sizeHint().height});
    }
    void paint(Painter& painter) override
    {
        const double y = m_name->sizeHint().height / 2 + 2;
        painter.fillEllipse({0, y - 4, 8, 8}, m_kept ? Color::rgb(0xC9CCD2) : m_color);
    }

private:
    Color m_color;
    bool m_kept;
    Label* m_name;
    Label* m_size;
};

class Legend : public View
{
public:
    SizeF sizeHint() const override { return {100, heightForWidth(400)}; }
    double heightForWidth(double width) const override
    {
        double x = 0;
        double y = 0;
        double row = 0;
        for (const auto& child : children()) {
            const SizeF size = child->sizeHint();
            if (x > 0 && x + size.width > width) {
                x = 0;
                y += row + 6;
                row = 0;
            }
            x += size.width + 22;
            row = std::max(row, size.height);
        }
        return y + row;
    }

protected:
    void layout() override
    {
        const double width = geometry().width;
        double x = 0;
        double y = 0;
        double row = 0;
        for (const auto& child : children()) {
            const SizeF size = child->sizeHint();
            if (x > 0 && x + size.width > width) {
                x = 0;
                y += row + 6;
                row = 0;
            }
            child->setGeometry({x, y, size.width, size.height});
            x += size.width + 22;
            row = std::max(row, size.height);
        }
    }
};

} // namespace

LayoutBar::LayoutBar()
{
    m_legend = addChild<Legend>();
    setLayoutHints({.fillWidth = true});
}

void LayoutBar::setParts(std::vector<Part> parts)
{
    m_parts = std::move(parts);
    while (!m_legend->children().empty())
        m_legend->takeChild(m_legend->children().back().get());
    for (const Part& part : m_parts) {
        if (!part.name.empty())
            m_legend->addChild<LegendEntry>(part);
    }
    requestLayout();
    requestPaint();
}

void LayoutBar::setLegendVisible(bool visible)
{
    m_legendVisible = visible;
    m_legend->setVisible(visible);
    requestLayout();
}

SizeF LayoutBar::sizeHint() const
{
    return {300, heightForWidth(300)};
}

double LayoutBar::heightForWidth(double width) const
{
    return 32 + (m_legendVisible ? 12 + m_legend->heightForWidth(width) : 0);
}

void LayoutBar::layout()
{
    const double width = geometry().width;
    m_legend->setGeometry({0, 44, width, m_legend->heightForWidth(width)});
}

void LayoutBar::paint(Painter& painter)
{
    const double width = geometry().width;
    const RectF bar{0, 0, width, 32};
    painter.save();
    painter.clipRoundedRect(bar, 8);
    painter.fillRect(bar, resolve(Role::IconTile));
    double total = 0;
    for (const Part& part : m_parts)
        total += part.fraction;
    double x = 0;
    const double scale = total > 0 ? width / std::max(total, 1.0) : 0;
    for (size_t i = 0; i < m_parts.size(); ++i) {
        const Part& part = m_parts[i];
        double w = part.fraction * scale;
        if (w <= 0)
            continue;
        // Even a tiny part shows as a sliver.
        w = std::max(w, 4.0);
        const RectF rect{x, 0, std::min(w, width - x), 32};
        if (part.kept) {
            painter.fillRect(rect, Color::rgb(0xC9CCD2).withAlpha(Theme::current().darkMode ? 0.4 : 0.8));
            // Hatching: space kept as it is.
            painter.save();
            painter.clip(rect);
            for (double hx = rect.x - 32; hx < rect.right(); hx += 7)
                painter.drawLine({hx, 32}, {hx + 32, 0}, Theme::current().text.withAlpha(0.12), 1);
            painter.restore();
        } else {
            painter.fillRect(rect, part.color);
        }
        x += w;
        if (i + 1 < m_parts.size() && x < width)
            painter.fillRect({x - 1, 0, 2, 32}, resolve(Role::Content));
    }
    painter.restore();
}

// --- StrengthMeter ----------------------------------------------------

StrengthMeter::StrengthMeter()
{
    m_caption = addText(this, {}, 13);
    m_length = addText(this, {}, 13, 400, Role::SecondaryText);
    m_length->setTextAlignment(Label::TextAlignment::Right);
    setLayoutHints({.fillWidth = true});
}

static Color strengthColor(int strength)
{
    switch (strength) {
    case 1:
        return accent::red;
    case 2:
        return accent::orange;
    case 3:
        return Color::rgb(0x6DBB3C);
    default:
        return style::color(Role::Success).resolve();
    }
}

void StrengthMeter::setStrength(int strength, std::string caption, std::string length)
{
    m_strength = strength;
    m_caption->setText(std::move(caption));
    m_caption->setColor(strength > 0 ? std::optional<ColorRecipe>(strengthColor(strength))
                                     : std::optional<ColorRecipe>(style::color(Role::SecondaryText)));
    m_length->setText(std::move(length));
    requestPaint();
}

SizeF StrengthMeter::sizeHint() const
{
    return {300, 4 + 12 + m_caption->sizeHint().height};
}

void StrengthMeter::layout()
{
    const double width = geometry().width;
    const double h = m_caption->sizeHint().height;
    m_caption->setGeometry({0, 16, width * 0.7, h});
    m_length->setGeometry({width * 0.5, 16, width * 0.5, h});
}

void StrengthMeter::paint(Painter& painter)
{
    const double width = geometry().width;
    const double gap = 6;
    const double segment = (width - 3 * gap) / 4;
    for (int i = 0; i < 4; ++i) {
        const RectF rect{i * (segment + gap), 0, segment, 4};
        painter.fillRoundedRect(rect, 2, i < m_strength ? strengthColor(m_strength) : resolve(Role::CardBorder));
    }
}

// --- Avatar -----------------------------------------------------------

Avatar::Avatar(double size)
    : m_size(size)
{
    m_initials = addText(this, {}, std::round(size * 0.36), 500);
    m_initials->setColor(ColorRecipe(Color::rgb(0xFFFFFF)));
    m_initials->setTextAlignment(Label::TextAlignment::Center);
    m_initials->setVerticallyCentered(true);
    m_image = addChild<ImageView>();
    m_image->setFillMode(ImageView::FillMode::PreserveAspectCrop);
    m_image->setAutoTransform(true);
    m_image->setCornerRadius(size / 2);
    m_image->setVisible(false);
}

void Avatar::setInitials(std::string initials)
{
    m_initials->setText(std::move(initials));
}

void Avatar::setPicture(std::string path)
{
    m_picture = std::move(path);
    m_initials->setVisible(m_picture.empty());
    m_image->setSource(m_picture);
    m_image->setVisible(!m_picture.empty());
    requestPaint();
}

void Avatar::layout()
{
    m_initials->setGeometry(localRect());
    m_image->setGeometry(localRect());
}

void Avatar::paint(Painter& painter)
{
    const RectF rect = localRect();
    if (!m_picture.empty()) {
        // The picture is the image view's.
    } else {
        const Color top = accent().lighter(1.25);
        Path circle;
        circle.addEllipse(rect);
        painter.fillPath(circle, LinearGradient{{0, 0}, {rect.width, rect.height}, {{0, top}, {1, accent().darker(1.1)}}});
    }
    if (hasVisibleFocus())
        painter.drawSmoothRectangle(rect.adjusted(-3, -3, 3, 3), rect.height / 2 + 3, Color{0, 0, 0, 0}, 2,
                                    accent().withAlpha(0.4));
}

} // namespace installer
