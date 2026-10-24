#include "bayustyle.h"

#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QEvent>
#include <QHeaderView>
#include <QLabel>
#include <QLayout>
#include <QListView>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSlider>
#include <QStyleFactory>
#include <QStyleOption>
#include <QTabBar>
#include <QTextEdit>
#include <QToolButton>

#include <tuple>
#include <utility>

namespace {

// The design system's colour tokens, light and dark (gtk-theme/Bayu/colors-*.css).
struct Tokens {
    QColor window, sidebar, card, field, hover, selected;
    QColor text, text2, text3, onAccent, border, divider;
    QColor accent, accentHover, accentSoft, success, warning, danger;
    QColor sliderTrack, tooltipBg, tooltipFg;
    int cardShadow; // alpha of the elevation-0 shadow
};

const Tokens &lightTokens()
{
    static const Tokens t{QColor(0xF1F3F5), QColor(0xF7F8FA), QColor(0xFFFFFF), QColor(0xEFF0F3), QColor(0xE9EAEE),
                          QColor(0xE2E4E9), QColor(0x17191D), QColor(0x6B7280), QColor(0x9BA1AC), QColor(0xFFFFFF),
                          QColor(0xE1E4E9), QColor(0xECEEF1), QColor(0x0A7AFF), QColor(0x0B6BE0), QColor(0xE6F0FF),
                          QColor(0x2FB457), QColor(0xF0A030), QColor(0xE8453C), QColor(0xD6D8DE), QColor(0x17191D),
                          QColor(0xFFFFFF), 15};
    return t;
}

const Tokens &darkTokens()
{
    static const Tokens t{QColor(0x1B1B1D), QColor(0x222224), QColor(0x2C2C2E), QColor(0x3A3A3C), QColor(0x3A3A3C),
                          QColor(0x3F3F43), QColor(0xF2F2F5), QColor(0x9D9DA5), QColor(0x76767C), QColor(0xFFFFFF),
                          QColor(0x3A3A3C), QColor(0x343436), QColor(0x0A84FF), QColor(0x2B95FF), QColor(0x15345C),
                          QColor(0x2FB457), QColor(0xF0A030), QColor(0xFF453A), QColor(0x48484C), QColor(0x3A3A3C),
                          QColor(0xF2F2F5), 50};
    return t;
}

// Radii, spacing and sizes of the design system.
constexpr qreal kTileRadius = 6;
constexpr qreal kControlRadius = 8;
constexpr qreal kMenuRadius = 10;
constexpr qreal kCardRadius = 12;
constexpr int kControlHeight = 36;   // buttons, fields, selects
constexpr int kButtonPaddingX = 18;
constexpr int kButtonMinWidth = 64;
constexpr int kFieldPaddingX = 12;
constexpr int kBox = 18;             // check box and radio button
constexpr int kIndicator = kBox + 8; // with room for the focus ring
constexpr int kSwitchWidth = 44;
constexpr int kSwitchHeight = 26;
constexpr int kKnob = 22;
constexpr int kGroove = 4;           // slider and progress bar
constexpr int kHandle = 18;          // slider handle
constexpr int kSliderExtent = kHandle + 8;
constexpr int kScrollBarExtent = 10;
constexpr int kScrollBarMinLength = 32;
constexpr int kMenuRowHeight = 32;
constexpr int kMenuPadding = 6;
constexpr int kTabHeight = 32;
constexpr int kCaptionSpacing = 8;   // caption above a card
constexpr int kRing = 2;             // focus ring width and offset

bool isDark(const QPalette &palette)
{
    return palette.color(QPalette::Window).lightness() < 128;
}

const Tokens &tokens(const QPalette &palette)
{
    return isDark(palette) ? darkTokens() : lightTokens();
}

// The accent follows the palette's highlight, so applications can recolour it.
QColor accent(const QPalette &palette)
{
    return palette.color(QPalette::Highlight);
}

QColor accentHover(const QPalette &palette)
{
    const Tokens &t = tokens(palette);
    const QColor a = accent(palette);
    return a == t.accent ? t.accentHover : (isDark(palette) ? a.lighter(115) : a.darker(112));
}

QColor withAlpha(QColor color, qreal alpha)
{
    color.setAlphaF(color.alphaF() * alpha);
    return color;
}

bool enabled(const QStyleOption *option)
{
    return option->state & QStyle::State_Enabled;
}

bool keyboardFocus(const QStyleOption *option)
{
    return (option->state & QStyle::State_HasFocus) && (option->state & QStyle::State_KeyboardFocusChange)
        && enabled(option);
}

void fillRounded(QPainter *painter, const QRectF &rect, qreal radius, const QColor &fill,
                 const QColor &border = QColor(), qreal borderWidth = 1)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    if (border.isValid() && border.alpha() > 0) {
        const qreal inset = borderWidth / 2;
        painter->setPen(QPen(border, borderWidth));
        painter->setBrush(fill.isValid() ? QBrush(fill) : QBrush(Qt::NoBrush));
        painter->drawRoundedRect(rect.adjusted(inset, inset, -inset, -inset), qMax<qreal>(0, radius - inset),
                                 qMax<qreal>(0, radius - inset));
    } else if (fill.isValid()) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(fill);
        painter->drawRoundedRect(rect, radius, radius);
    }
    painter->restore();
}

// Elevation 0 (cards): 0 1 2 6%.
void fillCard(QPainter *painter, const QRectF &rect, const QPalette &palette)
{
    const Tokens &t = tokens(palette);
    fillRounded(painter, rect.translated(0, 1), kCardRadius, QColor(0, 0, 0, t.cardShadow));
    fillRounded(painter, rect, kCardRadius, palette.color(QPalette::Base));
}

// Keyboard focus: a 2 px accent ring, 2 px out from the control. Controls that
// fill their whole rect draw their body inset by ringInset() while focused.
void drawFocusRing(QPainter *painter, const QRectF &rect, qreal radius, const QPalette &palette)
{
    fillRounded(painter, rect, radius, QColor(), accent(palette), kRing);
}

QRectF ringInset(const QRectF &rect)
{
    return rect.adjusted(2 * kRing, 2 * kRing, -2 * kRing, -2 * kRing);
}

enum class Direction { Down, Up, Left, Right };

void drawChevron(QPainter *painter, const QRectF &rect, const QColor &color, Direction direction = Direction::Down,
                 qreal size = 4)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(Qt::NoBrush);
    painter->translate(rect.center());
    switch (direction) {
    case Direction::Up:
        painter->rotate(180);
        break;
    case Direction::Left:
        painter->rotate(90);
        break;
    case Direction::Right:
        painter->rotate(-90);
        break;
    case Direction::Down:
        break;
    }
    QPainterPath path;
    path.moveTo(-size, -size / 2);
    path.lineTo(0, size / 2);
    path.lineTo(size, -size / 2);
    painter->drawPath(path);
    painter->restore();
}

void drawCheckMark(QPainter *painter, const QRectF &box, const QColor &color, qreal width = 1.8)
{
    const qreal s = box.width() / 16;
    QPainterPath mark;
    mark.moveTo(box.left() + 3.5 * s, box.top() + 8.5 * s);
    mark.lineTo(box.left() + 6.5 * s, box.top() + 11.5 * s);
    mark.lineTo(box.left() + 12.5 * s, box.top() + 4.5 * s);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(mark);
    painter->restore();
}

bool boolProperty(const QWidget *widget, const char *name)
{
    return widget && widget->property(name).toBool();
}

bool isSwitch(const QWidget *widget)
{
    return qobject_cast<const QCheckBox *>(widget) && boolProperty(widget, "switch");
}

// A field nested in a spin box or combo box, which draws the frame.
bool isNestedField(const QWidget *widget)
{
    return widget && (qobject_cast<const QAbstractSpinBox *>(widget->parentWidget())
                      || qobject_cast<const QComboBox *>(widget->parentWidget()));
}

bool isTextEditor(const QWidget *widget)
{
    if (const auto *edit = qobject_cast<const QTextEdit *>(widget))
        return !edit->isReadOnly();
    if (const auto *edit = qobject_cast<const QPlainTextEdit *>(widget))
        return !edit->isReadOnly();
    return false;
}

enum class ButtonKind { Secondary, Primary, Danger, Ghost };

ButtonKind buttonKind(const QStyleOption *option, const QWidget *widget)
{
    const auto *button = qstyleoption_cast<const QStyleOptionButton *>(option);
    if (boolProperty(widget, "danger"))
        return ButtonKind::Danger;
    if (boolProperty(widget, "primary") || (button && (button->features & QStyleOptionButton::DefaultButton)))
        return ButtonKind::Primary;
    if (button && (button->features & QStyleOptionButton::Flat))
        return ButtonKind::Ghost;
    return ButtonKind::Secondary;
}

// A button plate: primary in the accent, danger in red, secondary on the card
// colour with a border, ghost clear until hovered. Hover and press shift the
// background only; disabled buttons sit on the field colour.
void drawButton(QPainter *painter, const QStyleOption *option, ButtonKind kind)
{
    const QPalette &palette = option->palette;
    const Tokens &t = tokens(palette);
    const bool hover = option->state & QStyle::State_MouseOver;
    const bool down = option->state & (QStyle::State_Sunken | QStyle::State_On);
    QRectF rect = option->rect;
    if (keyboardFocus(option)) {
        drawFocusRing(painter, rect, kControlRadius + 2 * kRing, palette);
        rect = ringInset(rect);
    }
    QColor fill;
    QColor border;
    if (!enabled(option)) {
        fill = kind == ButtonKind::Ghost ? QColor() : t.field;
    } else {
        switch (kind) {
        case ButtonKind::Primary:
            fill = down ? withAlpha(accentHover(palette), 0.86) : hover ? accentHover(palette) : accent(palette);
            break;
        case ButtonKind::Danger:
            fill = down ? t.danger.darker(116) : hover ? t.danger.darker(108) : t.danger;
            break;
        case ButtonKind::Ghost:
            fill = down ? t.selected : hover ? t.hover : QColor();
            break;
        case ButtonKind::Secondary:
            fill = down ? t.selected : hover ? t.hover : palette.color(QPalette::Button);
            border = t.border;
            break;
        }
    }
    fillRounded(painter, rect, kControlRadius, fill, border);
}

QColor buttonTextColor(const QStyleOption *option, ButtonKind kind)
{
    const Tokens &t = tokens(option->palette);
    if (!enabled(option))
        return t.text3;
    switch (kind) {
    case ButtonKind::Primary:
    case ButtonKind::Danger:
        return t.onAccent;
    case ButtonKind::Ghost:
        return accent(option->palette);
    case ButtonKind::Secondary:
        break;
    }
    return option->palette.color(QPalette::ButtonText);
}

// A text field, spin box or editable combo box: the field colour, the card
// colour with a 2 px accent border while focused.
void drawField(QPainter *painter, const QStyleOption *option, const QRectF &rect, bool focused)
{
    const Tokens &t = tokens(option->palette);
    if (!enabled(option)) {
        fillRounded(painter, rect, kControlRadius, withAlpha(t.field, 0.6));
        return;
    }
    if (focused)
        fillRounded(painter, rect, kControlRadius, option->palette.color(QPalette::Base), accent(option->palette), kRing);
    else
        fillRounded(painter, rect, kControlRadius, t.field);
}

// A select (non-editable combo box): the field colour, shifting on hover and
// press, ringed while it has keyboard focus.
void drawSelect(QPainter *painter, const QStyleOption *option, const QRectF &rect)
{
    const Tokens &t = tokens(option->palette);
    QColor fill = t.field;
    if (option->state & (QStyle::State_Sunken | QStyle::State_On))
        fill = t.selected;
    else if (option->state & QStyle::State_MouseOver)
        fill = t.hover;
    if (!enabled(option))
        fill = withAlpha(t.field, 0.6);
    QRectF body = rect;
    if (keyboardFocus(option)) {
        drawFocusRing(painter, rect, kControlRadius + 2 * kRing, option->palette);
        body = ringInset(rect);
    }
    fillRounded(painter, body, kControlRadius, fill);
}

void drawCheckBox(QPainter *painter, const QStyleOption *option, bool round)
{
    const QPalette &palette = option->palette;
    const Tokens &t = tokens(palette);
    const bool on = option->state & (QStyle::State_On | QStyle::State_NoChange);
    const bool hover = option->state & QStyle::State_MouseOver;
    QRectF box(0, 0, kBox, kBox);
    box.moveCenter(QRectF(option->rect).center());
    const qreal radius = round ? kBox / 2.0 : kTileRadius;

    painter->save();
    if (!enabled(option))
        painter->setOpacity(0.45);
    if (on) {
        const QColor fill = hover ? accentHover(palette) : accent(palette);
        fillRounded(painter, box, radius, fill);
    } else {
        fillRounded(painter, box, radius, hover ? t.hover : palette.color(QPalette::Base),
                    hover ? t.text3 : withAlpha(t.text3, 0.7));
    }
    painter->setRenderHint(QPainter::Antialiasing);
    if (round && (option->state & QStyle::State_On)) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(t.onAccent);
        painter->drawEllipse(box.center(), 3.5, 3.5);
    } else if (option->state & QStyle::State_NoChange) {
        painter->setPen(QPen(t.onAccent, 2, Qt::SolidLine, Qt::RoundCap));
        painter->drawLine(QPointF(box.left() + 5, box.center().y()), QPointF(box.right() - 5, box.center().y()));
    } else if (option->state & QStyle::State_On) {
        drawCheckMark(painter, box.adjusted(1, 1, -1, -1), t.onAccent, 2);
    }
    painter->restore();
    if (keyboardFocus(option))
        drawFocusRing(painter, box.adjusted(-2 * kRing, -2 * kRing, 2 * kRing, 2 * kRing), radius + 2 * kRing, palette);
}

// A QCheckBox with the "switch" property: 44 x 26 with a 22 px knob.
void drawSwitch(QPainter *painter, const QStyleOption *option)
{
    const QPalette &palette = option->palette;
    const Tokens &t = tokens(palette);
    const bool on = option->state & QStyle::State_On;
    const bool hover = option->state & QStyle::State_MouseOver;
    QRectF track(0, 0, kSwitchWidth, kSwitchHeight);
    track.moveCenter(QRectF(option->rect).center());
    painter->save();
    if (!enabled(option))
        painter->setOpacity(0.45);
    QColor fill = on ? (hover ? accentHover(palette) : accent(palette)) : (hover ? t.selected.darker(104) : t.selected);
    fillRounded(painter, track, kSwitchHeight / 2.0, fill);
    QRectF knob(0, 0, kKnob, kKnob);
    const qreal inset = (kSwitchHeight - kKnob) / 2.0;
    knob.moveTopLeft(QPointF(on ? track.right() - inset - kKnob : track.left() + inset, track.top() + inset));
    fillRounded(painter, knob.translated(0, 1), kKnob / 2.0, QColor(0, 0, 0, 40));
    fillRounded(painter, knob, kKnob / 2.0, Qt::white);
    painter->restore();
    if (keyboardFocus(option))
        drawFocusRing(painter, track.adjusted(-2 * kRing, -2 * kRing, 2 * kRing, 2 * kRing),
                      kSwitchHeight / 2.0 + 2 * kRing, palette);
}

} // namespace

BayuStyle::BayuStyle()
    : QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion")))
{
    setObjectName(QStringLiteral("Bayu"));
}

void BayuStyle::polish(QWidget *widget)
{
    QProxyStyle::polish(widget);
    // Menus, combo box popups and tool tips draw their own rounded panel;
    // the corners around it stay clear.
    if (qobject_cast<QMenu *>(widget) || widget->inherits("QComboBoxPrivateContainer") || widget->inherits("QTipLabel"))
        widget->setAttribute(Qt::WA_TranslucentBackground);
    // Hover is a background shift on every interactive element.
    if (qobject_cast<QAbstractButton *>(widget) || qobject_cast<QComboBox *>(widget) || qobject_cast<QTabBar *>(widget)
        || qobject_cast<QHeaderView *>(widget) || qobject_cast<QAbstractSpinBox *>(widget))
        widget->setAttribute(Qt::WA_Hover);
    if (auto *view = qobject_cast<QAbstractItemView *>(widget)) {
        view->viewport()->setAttribute(Qt::WA_Hover);
        // A combo box popup's list sits on the menu panel.
        if (view->parentWidget() && view->parentWidget()->inherits("QComboBoxPrivateContainer")) {
            view->setAutoFillBackground(false);
            view->viewport()->setAutoFillBackground(false);
        }
    }
    // Dialog actions sit 10 px apart.
    if (auto *box = qobject_cast<QDialogButtonBox *>(widget); box && box->layout())
        box->layout()->setSpacing(10);
    if (widget->objectName() == QLatin1String("Row")) {
        widget->setAttribute(Qt::WA_Hover);
        widget->installEventFilter(this);
    }
    if (auto *label = qobject_cast<QLabel *>(widget)) {
        QFont font = label->font();
        const bool badge = boolProperty(label, "badge");
        if (badge || boolProperty(label, "keycap") || boolProperty(label, "banner")) {
            // A zero-width box frame, so that CE_ShapedFrame draws the plate.
            label->setFrameShape(QFrame::Box);
            label->setLineWidth(0);
        }
        if (badge) {
            QPalette palette = label->palette();
            palette.setColor(QPalette::WindowText, accent(palette));
            label->setPalette(palette);
            label->setContentsMargins(8, 2, 8, 2);
            font.setPixelSize(12);
            font.setWeight(QFont::Medium);
            label->setFont(font);
        } else if (boolProperty(label, "keycap")) {
            label->setContentsMargins(6, 2, 6, 2);
            font.setFamilies({QStringLiteral("JetBrains Mono"), QStringLiteral("monospace")});
            font.setStyleHint(QFont::Monospace);
            font.setPixelSize(12);
            label->setFont(font);
        } else if (boolProperty(label, "banner")) {
            label->setContentsMargins(16, 10, 16, 10);
            label->setWordWrap(true);
        }
    }
}

void BayuStyle::unpolish(QWidget *widget)
{
    if (qobject_cast<QMenu *>(widget) || widget->inherits("QComboBoxPrivateContainer") || widget->inherits("QTipLabel"))
        widget->setAttribute(Qt::WA_TranslucentBackground, false);
    if (widget->objectName() == QLatin1String("Row"))
        widget->removeEventFilter(this);
    QProxyStyle::unpolish(widget);
}

// Rows (QWidget named "Row") shift to the hover colour under the pointer.
bool BayuStyle::eventFilter(QObject *object, QEvent *event)
{
    auto *widget = qobject_cast<QWidget *>(object);
    if (!widget)
        return QProxyStyle::eventFilter(object, event);
    switch (event->type()) {
    case QEvent::HoverEnter:
    case QEvent::HoverLeave:
        widget->update();
        break;
    case QEvent::Paint:
        if (widget->underMouse() && widget->isEnabled()) {
            QPainter painter(widget);
            painter.fillRect(widget->rect(), tokens(widget->palette()).hover);
        }
        break;
    default:
        break;
    }
    return QProxyStyle::eventFilter(object, event);
}

int BayuStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const
{
    switch (metric) {
    case PM_ButtonMargin:
        return 2 * kButtonPaddingX;
    case PM_ButtonDefaultIndicator:
    case PM_ButtonShiftHorizontal:
    case PM_ButtonShiftVertical:
    case PM_TabBarTabShiftHorizontal:
    case PM_TabBarTabShiftVertical:
    case PM_TabBarBaseOverlap:
    case PM_ToolBarFrameWidth:
    case PM_MenuBarPanelWidth:
        return 0;
    case PM_DefaultFrameWidth:
        // Room inside a scroll area's rounded frame for the square viewport.
        if (qobject_cast<const QAbstractScrollArea *>(widget))
            return 3;
        break;
    case PM_IndicatorWidth:
        return isSwitch(widget) ? kSwitchWidth + 4 * kRing : kIndicator;
    case PM_IndicatorHeight:
        return isSwitch(widget) ? kSwitchHeight + 4 * kRing : kIndicator;
    case PM_ExclusiveIndicatorWidth:
    case PM_ExclusiveIndicatorHeight:
        return kIndicator;
    case PM_CheckBoxLabelSpacing:
    case PM_RadioButtonLabelSpacing:
        return 4;
    case PM_ScrollBarExtent:
        return kScrollBarExtent;
    case PM_ScrollBarSliderMin:
        return kScrollBarMinLength;
    case PM_SliderThickness:
    case PM_SliderControlThickness:
    case PM_SliderLength:
        return kSliderExtent;
    case PM_MenuHMargin:
    case PM_MenuVMargin:
        return kMenuPadding;
    case PM_MenuPanelWidth:
        return 1;
    case PM_MenuBarItemSpacing:
        return 2;
    case PM_MenuBarHMargin:
        return 4;
    case PM_MenuBarVMargin:
        return 2;
    case PM_ToolTipLabelFrameWidth:
        return 6;
    case PM_TabBarTabHSpace:
        return 24;
    case PM_TabBarBaseHeight:
        return 1;
    case PM_LayoutLeftMargin:
    case PM_LayoutTopMargin:
    case PM_LayoutRightMargin:
    case PM_LayoutBottomMargin:
        return (widget && widget->isWindow()) ? 16 : 12;
    case PM_LayoutHorizontalSpacing:
    case PM_LayoutVerticalSpacing:
        return 8;
    case PM_ToolBarItemSpacing:
    case PM_ToolBarItemMargin:
        return 4;
    case PM_ToolBarSeparatorExtent:
        return 9;
    case PM_HeaderMargin:
        return 6;
    default:
        break;
    }
    return QProxyStyle::pixelMetric(metric, option, widget);
}

int BayuStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget,
                         QStyleHintReturn *returnData) const
{
    switch (hint) {
    case SH_DialogButtonBox_ButtonsHaveIcons:
    case SH_EtchDisabledText:
    case SH_DitherDisabledText:
        return 0;
    case SH_DialogButtonLayout:
        return QDialogButtonBox::GnomeLayout; // actions right aligned, confirm last
    case SH_LineEdit_PasswordCharacter:
        return 0x2022;
    case SH_TabBar_Alignment:
        return Qt::AlignLeft;
    case SH_ComboBox_Popup:
        return 1; // a menu with a check mark on the current item
    case SH_ItemView_ShowDecorationSelected:
        return 1;
    case SH_Table_GridLineColor:
        return int(tokens(option ? option->palette : (widget ? widget->palette() : QPalette())).divider.rgba());
    case SH_ToolTipLabel_Opacity:
        return 255;
    case SH_Menu_Scrollable:
        return 1;
    default:
        break;
    }
    return QProxyStyle::styleHint(hint, option, widget, returnData);
}

QSize BayuStyle::sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &size,
                                  const QWidget *widget) const
{
    switch (type) {
    case CT_PushButton:
        if (const auto *button = qstyleoption_cast<const QStyleOptionButton *>(option)) {
            int width = size.width() + 2 * kButtonPaddingX;
            if (button->text.isEmpty())
                width = size.width() + 2 * 10; // icon only
            else
                width = qMax(width, kButtonMinWidth);
            if (button->features & QStyleOptionButton::HasMenu)
                width += 20;
            return QSize(width, qMax(kControlHeight, size.height() + 16));
        }
        break;
    case CT_ToolButton:
        return QSize(size.width() + 16, size.height() + 16).expandedTo(QSize(32, 32));
    case CT_LineEdit:
        if (isNestedField(widget))
            break;
        return QSize(size.width() + 2 * (kFieldPaddingX - 2), qMax(kControlHeight, size.height()));
    case CT_SpinBox:
        return QSize(size.width() + kFieldPaddingX + 32, qMax(kControlHeight, size.height()));
    case CT_ComboBox:
        if (const auto *combo = qstyleoption_cast<const QStyleOptionComboBox *>(option)) {
            // Text, the chevron and, for selects, the popup's check column.
            int width = size.width() + kFieldPaddingX + 32;
            width += combo->editable ? 8 : 24;
            return QSize(width, kControlHeight);
        }
        break;
    case CT_CheckBox:
    case CT_RadioButton: {
        QSize result = QProxyStyle::sizeFromContents(type, option, size, widget);
        return result.expandedTo(QSize(0, isSwitch(widget) ? kSwitchHeight + 4 * kRing : kIndicator));
    }
    case CT_ItemViewItem: {
        QSize result = QProxyStyle::sizeFromContents(type, option, size, widget);
        const int minimum = qobject_cast<const QListView *>(widget) ? 32 : 28;
        if (const auto *item = qstyleoption_cast<const QStyleOptionViewItem *>(option);
            item && item->features & QStyleOptionViewItem::HasDisplay)
            result.setHeight(qMax(result.height(), minimum));
        return result;
    }
    case CT_TabBarTab:
        if (const auto *tab = qstyleoption_cast<const QStyleOptionTab *>(option)) {
            QSize result = QProxyStyle::sizeFromContents(type, option, size, widget);
            const bool horizontal = tab->shape == QTabBar::RoundedNorth || tab->shape == QTabBar::RoundedSouth
                || tab->shape == QTabBar::TriangularNorth || tab->shape == QTabBar::TriangularSouth;
            // The current tab's label is medium weight; leave it room.
            if (horizontal)
                return QSize(result.width() + 8, kTabHeight);
            return QSize(kTabHeight, result.height() + 8);
        }
        break;
    case CT_MenuItem:
        if (const auto *item = qstyleoption_cast<const QStyleOptionMenuItem *>(option)) {
            if (item->menuItemType == QStyleOptionMenuItem::Separator)
                return QSize(size.width(), item->text.isEmpty() ? 9 : kMenuRowHeight);
            QSize result = QProxyStyle::sizeFromContents(type, option, size, widget);
            return QSize(result.width() + 40, qMax(result.height(), kMenuRowHeight));
        }
        break;
    case CT_MenuBarItem:
        return QSize(size.width() + 20, qMax(size.height() + 8, 28));
    case CT_HeaderSection: {
        QSize result = QProxyStyle::sizeFromContents(type, option, size, widget);
        return QSize(result.width() + 8, qMax(result.height(), 32));
    }
    default:
        break;
    }
    return QProxyStyle::sizeFromContents(type, option, size, widget);
}

QRect BayuStyle::subControlRect(ComplexControl control, const QStyleOptionComplex *option,
                                SubControl subControl, const QWidget *widget) const
{
    switch (control) {
    case CC_ScrollBar:
        // No arrow buttons: the handle runs the whole length.
        if (const auto *bar = qstyleoption_cast<const QStyleOptionSlider *>(option)) {
            const bool horizontal = bar->orientation == Qt::Horizontal;
            const QRect groove = bar->rect;
            const int length = horizontal ? groove.width() : groove.height();
            const int range = bar->maximum - bar->minimum;
            int handle = length;
            if (range > 0) {
                handle = int(qint64(length) * bar->pageStep / (range + bar->pageStep));
                handle = qBound(qMin(kScrollBarMinLength, length), handle, length);
            }
            const int position = sliderPositionFromValue(bar->minimum, bar->maximum, bar->sliderPosition,
                                                         length - handle, bar->upsideDown);
            auto along = [&](int start, int extent) {
                return horizontal ? QRect(groove.left() + start, groove.top(), extent, groove.height())
                                  : QRect(groove.left(), groove.top() + start, groove.width(), extent);
            };
            QRect rect;
            switch (subControl) {
            case SC_ScrollBarSlider:
                rect = along(position, handle);
                break;
            case SC_ScrollBarSubPage:
                rect = along(0, position);
                break;
            case SC_ScrollBarAddPage:
                rect = along(position + handle, length - position - handle);
                break;
            case SC_ScrollBarGroove:
                rect = groove;
                break;
            default: // SC_ScrollBarAddLine, SC_ScrollBarSubLine, SC_ScrollBarFirst, SC_ScrollBarLast
                break;
            }
            return visualRect(bar->direction, groove, rect);
        }
        break;
    case CC_GroupBox:
        // The caption sits above the card, the check box (if any) before it.
        if (const auto *box = qstyleoption_cast<const QStyleOptionGroupBox *>(option)) {
            const QRect rect = option->rect;
            const bool checkable = box->subControls & SC_GroupBoxCheckBox;
            const bool titled = !box->text.isEmpty() || checkable;
            const int lineHeight = qMax(box->fontMetrics.height(), checkable ? kIndicator : 0);
            QRect frame = rect;
            if (titled)
                frame.setTop(rect.top() + lineHeight + kCaptionSpacing);
            switch (subControl) {
            case SC_GroupBoxCheckBox:
                return checkable ? QRect(rect.left(), rect.top() + (lineHeight - kIndicator) / 2, kIndicator, kIndicator)
                                 : QRect();
            case SC_GroupBoxLabel: {
                const int left = checkable ? rect.left() + kIndicator + 4 : rect.left() + 4;
                const int width = box->fontMetrics.horizontalAdvance(box->text) + 4;
                return QRect(left, rect.top(), qMin(width, rect.right() - left), lineHeight);
            }
            case SC_GroupBoxFrame:
                return frame;
            case SC_GroupBoxContents:
                return frame.adjusted(4, 4, -4, -4);
            default:
                break;
            }
        }
        break;
    case CC_ComboBox:
        if (const auto *combo = qstyleoption_cast<const QStyleOptionComboBox *>(option)) {
            const QRect rect = option->rect;
            const QRect arrow(rect.right() - 32 + 1, rect.top(), 32, rect.height());
            switch (subControl) {
            case SC_ComboBoxArrow:
                return visualRect(combo->direction, rect, arrow);
            case SC_ComboBoxEditField:
                return visualRect(combo->direction, rect,
                                  QRect(rect.left() + kFieldPaddingX, rect.top() + kRing, rect.width() - kFieldPaddingX - 32,
                                        rect.height() - 2 * kRing));
            case SC_ComboBoxFrame:
                return rect;
            case SC_ComboBoxListBoxPopup:
                return rect;
            default:
                break;
            }
        }
        break;
    case CC_SpinBox:
        if (const auto *spin = qstyleoption_cast<const QStyleOptionSpinBox *>(option)) {
            const QRect rect = option->rect;
            const bool buttons = spin->buttonSymbols != QAbstractSpinBox::NoButtons;
            const int buttonWidth = buttons ? 28 : 0;
            const int half = rect.height() / 2;
            switch (subControl) {
            case SC_SpinBoxUp:
                return buttons ? visualRect(spin->direction, rect,
                                            QRect(rect.right() - buttonWidth - 2, rect.top() + 3, buttonWidth, half - 3))
                               : QRect();
            case SC_SpinBoxDown:
                return buttons ? visualRect(spin->direction, rect,
                                            QRect(rect.right() - buttonWidth - 2, rect.top() + half, buttonWidth,
                                                  rect.height() - half - 3))
                               : QRect();
            case SC_SpinBoxEditField:
                return visualRect(spin->direction, rect,
                                  QRect(rect.left() + kFieldPaddingX, rect.top() + kRing,
                                        rect.width() - kFieldPaddingX - buttonWidth - 4, rect.height() - 2 * kRing));
            case SC_SpinBoxFrame:
                return rect;
            default:
                break;
            }
        }
        break;
    default:
        break;
    }
    return QProxyStyle::subControlRect(control, option, subControl, widget);
}

QRect BayuStyle::subElementRect(SubElement element, const QStyleOption *option, const QWidget *widget) const
{
    switch (element) {
    case SE_LineEditContents:
        if (!isNestedField(widget)) {
            if (const auto *frame = qstyleoption_cast<const QStyleOptionFrame *>(option); frame && frame->lineWidth > 0)
                return option->rect.adjusted(kFieldPaddingX, kRing, -kFieldPaddingX, -kRing);
        }
        break;
    case SE_ProgressBarGroove:
    case SE_ProgressBarContents:
    case SE_ProgressBarLabel:
        // A thin groove with the percentage beside it, not on top.
        if (const auto *bar = qstyleoption_cast<const QStyleOptionProgressBar *>(option)) {
            const bool horizontal = bar->state & State_Horizontal;
            QRect rect = option->rect;
            QRect label;
            if (bar->textVisible && horizontal) {
                const int width = option->fontMetrics.horizontalAdvance(QStringLiteral("100%")) + 8;
                label = QRect(rect.right() - width + 1, rect.top(), width, rect.height());
                rect.setRight(label.left() - 1);
                label = visualRect(option->direction, option->rect, label);
                rect = visualRect(option->direction, option->rect, rect);
            }
            if (element == SE_ProgressBarLabel)
                return label;
            if (horizontal)
                return QRect(rect.left(), rect.center().y() - kGroove / 2 + 1, rect.width(), kGroove);
            return QRect(rect.center().x() - kGroove / 2 + 1, rect.top(), kGroove, rect.height());
        }
        break;
    default:
        break;
    }
    return QProxyStyle::subElementRect(element, option, widget);
}

void BayuStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
                              const QWidget *widget) const
{
    const QPalette &palette = option->palette;
    const Tokens &t = tokens(palette);
    switch (element) {
    case PE_PanelButtonCommand:
    case PE_PanelButtonBevel:
        drawButton(painter, option, buttonKind(option, widget));
        return;
    case PE_PanelButtonTool: {
        // Tool buttons: ghost when auto-raised, otherwise secondary buttons.
        const bool raised = option->state & State_AutoRaise;
        if (!raised) {
            drawButton(painter, option, ButtonKind::Secondary);
            return;
        }
        QRectF rect = option->rect;
        if (keyboardFocus(option)) {
            drawFocusRing(painter, rect, kControlRadius + 2 * kRing, palette);
            rect = ringInset(rect);
        }
        if (!enabled(option))
            return;
        if (option->state & (State_Sunken | State_On))
            fillRounded(painter, rect, kControlRadius, t.selected);
        else if (option->state & State_MouseOver)
            fillRounded(painter, rect, kControlRadius, t.hover);
        return;
    }
    case PE_FrameButtonTool:
    case PE_FrameDefaultButton:
    case PE_FrameLineEdit:
    case PE_FrameStatusBarItem:
    case PE_FrameTabBarBase:
    case PE_IndicatorButtonDropDown:
    case PE_PanelMenuBar:
        return;
    case PE_FrameFocusRect:
        // Item views: a 2 px ring inside the focused row. Other controls ring
        // themselves.
        if (qobject_cast<const QAbstractItemView *>(widget) && (option->state & State_KeyboardFocusChange))
            fillRounded(painter, QRectF(option->rect).adjusted(1, 1, -1, -1), kControlRadius, QColor(), accent(palette),
                        kRing);
        return;
    case PE_PanelLineEdit:
        if (isNestedField(widget))
            return;
        if (const auto *frame = qstyleoption_cast<const QStyleOptionFrame *>(option); frame && frame->lineWidth > 0) {
            drawField(painter, option, option->rect, option->state & State_HasFocus);
            return;
        }
        break; // frameless fields (item editors) keep Fusion's
    case PE_IndicatorCheckBox:
        if (isSwitch(widget))
            drawSwitch(painter, option);
        else
            drawCheckBox(painter, option, false);
        return;
    case PE_IndicatorItemViewItemCheck:
        drawCheckBox(painter, option, false);
        return;
    case PE_IndicatorRadioButton:
        drawCheckBox(painter, option, true);
        return;
    case PE_PanelMenu:
    case PE_FrameMenu:
        // Menus: radius 10 on the card colour, edged so they stand off the window.
        fillRounded(painter, option->rect, kMenuRadius, palette.color(QPalette::Base), t.border);
        return;
    case PE_PanelTipLabel:
        fillRounded(painter, option->rect, kTileRadius, t.tooltipBg);
        return;
    case PE_FrameTabWidget:
        fillCard(painter, QRectF(option->rect).adjusted(0, 0, 0, -1), palette);
        return;
    case PE_FrameGroupBox:
        fillCard(painter, QRectF(option->rect).adjusted(0, 0, 0, -1), palette);
        return;
    case PE_Frame:
        // Scroll areas: a rounded field of the view colour with a hairline
        // border; editable text areas take the accent border while focused.
        if (option->state & (State_Sunken | State_Raised)) {
            const bool editing = isTextEditor(widget) && (option->state & State_HasFocus) && enabled(option);
            const bool view = qobject_cast<const QAbstractItemView *>(widget) || isTextEditor(widget)
                || qobject_cast<const QTextEdit *>(widget) || qobject_cast<const QPlainTextEdit *>(widget);
            fillRounded(painter, option->rect, kControlRadius, view ? palette.color(QPalette::Base) : QColor(),
                        editing ? accent(palette) : t.border, editing ? kRing : 1);
        }
        return;
    case PE_PanelItemViewRow:
        if (option->state & State_Selected) {
            painter->fillRect(option->rect, t.selected);
            return;
        }
        if ((option->state & State_MouseOver) && enabled(option)) {
            painter->fillRect(option->rect, t.hover);
            return;
        }
        break; // alternate rows
    case PE_PanelItemViewItem:
        return; // CE_ItemViewItem draws the row plate
    case PE_IndicatorBranch:
        if (option->state & State_Children) {
            const bool open = option->state & State_Open;
            QRectF rect(0, 0, 16, 16);
            rect.moveCenter(QRectF(option->rect).center());
            drawChevron(painter, rect, (option->state & State_MouseOver) ? t.text : t.text2,
                        open ? Direction::Down : (option->direction == Qt::RightToLeft ? Direction::Left : Direction::Right));
        }
        return;
    case PE_IndicatorArrowDown:
    case PE_IndicatorArrowUp:
    case PE_IndicatorArrowLeft:
    case PE_IndicatorArrowRight:
    case PE_IndicatorSpinDown:
    case PE_IndicatorSpinUp: {
        Direction direction = Direction::Down;
        if (element == PE_IndicatorArrowUp || element == PE_IndicatorSpinUp)
            direction = Direction::Up;
        else if (element == PE_IndicatorArrowLeft)
            direction = Direction::Left;
        else if (element == PE_IndicatorArrowRight)
            direction = Direction::Right;
        drawChevron(painter, option->rect, enabled(option) ? t.text2 : t.text3, direction, 3.5);
        return;
    }
    case PE_IndicatorHeaderArrow:
        if (const auto *header = qstyleoption_cast<const QStyleOptionHeader *>(option)) {
            drawChevron(painter, option->rect, t.text2,
                        header->sortIndicator == QStyleOptionHeader::SortUp ? Direction::Up : Direction::Down, 3.5);
            return;
        }
        break;
    case PE_IndicatorToolBarSeparator: {
        const QRectF rect = option->rect;
        if (option->state & State_Horizontal)
            painter->fillRect(QRectF(rect.center().x(), rect.top() + 4, 1, rect.height() - 8), t.divider);
        else
            painter->fillRect(QRectF(rect.left() + 4, rect.center().y(), rect.width() - 8, 1), t.divider);
        return;
    }
    default:
        break;
    }
    QProxyStyle::drawPrimitive(element, option, painter, widget);
}

void BayuStyle::drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                            const QWidget *widget) const
{
    const QPalette &palette = option->palette;
    const Tokens &t = tokens(palette);
    switch (element) {
    case CE_PushButtonBevel:
        // Flat buttons get their hover plate too.
        if (const auto *button = qstyleoption_cast<const QStyleOptionButton *>(option);
            button && (button->features & QStyleOptionButton::Flat)) {
            drawButton(painter, option, buttonKind(option, widget));
            if (button->features & QStyleOptionButton::HasMenu) {
                const QRect arrow(option->rect.right() - 24, option->rect.top(), 20, option->rect.height());
                drawChevron(painter, arrow, buttonTextColor(option, buttonKind(option, widget)));
            }
            return;
        }
        break;
    case CE_PushButtonLabel:
        if (const auto *button = qstyleoption_cast<const QStyleOptionButton *>(option)) {
            const ButtonKind kind = buttonKind(option, widget);
            QStyleOptionButton copy(*button);
            copy.palette.setColor(QPalette::ButtonText, buttonTextColor(option, kind));
            painter->save();
            if (kind == ButtonKind::Primary || kind == ButtonKind::Danger) {
                QFont font = painter->font();
                font.setWeight(QFont::Medium);
                painter->setFont(font);
            }
            QProxyStyle::drawControl(element, &copy, painter, widget);
            painter->restore();
            return;
        }
        break;
    case CE_TabBarTabShape:
        if (const auto *tab = qstyleoption_cast<const QStyleOptionTab *>(option)) {
            const QRectF rect = option->rect;
            if (tab->documentMode) {
                // Segmented control: a field-coloured track rounded at the ends,
                // the current segment a raised card-coloured plate.
                const bool first = tab->position == QStyleOptionTab::Beginning || tab->position == QStyleOptionTab::OnlyOneTab;
                const bool last = tab->position == QStyleOptionTab::End || tab->position == QStyleOptionTab::OnlyOneTab;
                QPainterPath track;
                track.addRoundedRect(rect, kControlRadius, kControlRadius);
                if (!first)
                    track.addRect(QRectF(rect.left(), rect.top(), kControlRadius, rect.height()));
                if (!last)
                    track.addRect(QRectF(rect.right() - kControlRadius + 1, rect.top(), kControlRadius, rect.height()));
                track.setFillRule(Qt::WindingFill);
                painter->save();
                painter->setRenderHint(QPainter::Antialiasing);
                painter->fillPath(track.simplified(), t.field);
                painter->restore();
                const QRectF plate = rect.adjusted(2, 2, -2, -2);
                if (option->state & State_Selected) {
                    fillRounded(painter, plate.translated(0, 1), kTileRadius, QColor(0, 0, 0, t.cardShadow + 10));
                    fillRounded(painter, plate, kTileRadius, palette.color(QPalette::Base));
                }
                if (keyboardFocus(option))
                    fillRounded(painter, plate, kTileRadius, QColor(), accent(palette), kRing);
                return;
            }
            // Tab bar: the current tab is underlined in the accent.
            const bool selected = option->state & State_Selected;
            QRectF line;
            switch (tab->shape) {
            case QTabBar::RoundedSouth:
            case QTabBar::TriangularSouth:
                line = QRectF(rect.left() + 4, rect.top(), rect.width() - 8, 2);
                break;
            case QTabBar::RoundedWest:
            case QTabBar::TriangularWest:
                line = QRectF(rect.right() - 1, rect.top() + 4, 2, rect.height() - 8);
                break;
            case QTabBar::RoundedEast:
            case QTabBar::TriangularEast:
                line = QRectF(rect.left(), rect.top() + 4, 2, rect.height() - 8);
                break;
            default:
                line = QRectF(rect.left() + 4, rect.bottom() - 1, rect.width() - 8, 2);
                break;
            }
            if (selected)
                painter->fillRect(line, accent(palette));
            if (keyboardFocus(option))
                fillRounded(painter, rect.adjusted(2, 2, -2, -2), kTileRadius, QColor(), accent(palette), kRing);
            return;
        }
        break;
    case CE_TabBarTabLabel:
        if (const auto *tab = qstyleoption_cast<const QStyleOptionTab *>(option)) {
            QStyleOptionTab copy(*tab);
            const bool selected = option->state & State_Selected;
            QColor text = t.text2;
            if (!enabled(option))
                text = t.text3;
            else if (selected)
                text = tab->documentMode ? t.text : accent(palette);
            else if (option->state & State_MouseOver)
                text = t.text;
            copy.palette.setColor(QPalette::WindowText, text);
            copy.palette.setColor(QPalette::ButtonText, text);
            painter->save();
            if (selected) {
                QFont font = painter->font();
                font.setWeight(QFont::Medium);
                painter->setFont(font);
            }
            QProxyStyle::drawControl(element, &copy, painter, widget);
            painter->restore();
            return;
        }
        break;
    case CE_ItemViewItem:
        // Rows: hover and selection are background shifts; selected rows are
        // medium weight. List views round their rows like sidebar items.
        if (const auto *item = qstyleoption_cast<const QStyleOptionViewItem *>(option)) {
            QStyleOptionViewItem copy(*item);
            const bool selected = option->state & State_Selected;
            const bool hover = (option->state & State_MouseOver) && enabled(option);
            QColor plate;
            if (selected)
                plate = t.selected;
            else if (hover)
                plate = t.hover;
            const bool list = qobject_cast<const QListView *>(widget) && !widget->inherits("QComboBoxListView");
            if (plate.isValid()) {
                if (list)
                    fillRounded(painter, QRectF(option->rect).adjusted(2, 1, -2, -1), kControlRadius, plate);
                else
                    painter->fillRect(option->rect, plate);
            } else if (item->backgroundBrush.style() != Qt::NoBrush) {
                painter->fillRect(option->rect, item->backgroundBrush);
            }
            if (selected) {
                copy.state &= ~State_Selected;
                copy.font.setWeight(QFont::Medium);
                copy.palette.setColor(QPalette::Text, palette.color(QPalette::Text));
            }
            copy.backgroundBrush = Qt::NoBrush;
            QProxyStyle::drawControl(element, &copy, painter, widget);
            return;
        }
        break;
    case CE_ProgressBar:
        // Fusion would lay the parts out with its own rects.
        if (const auto *bar = qstyleoption_cast<const QStyleOptionProgressBar *>(option)) {
            QStyleOptionProgressBar part(*bar);
            for (auto [sub, ce] : {std::pair{SE_ProgressBarGroove, CE_ProgressBarGroove},
                                   std::pair{SE_ProgressBarContents, CE_ProgressBarContents},
                                   std::pair{SE_ProgressBarLabel, CE_ProgressBarLabel}}) {
                if (ce == CE_ProgressBarLabel && !bar->textVisible)
                    continue;
                part.rect = subElementRect(sub, bar, widget);
                proxy()->drawControl(ce, &part, painter, widget);
            }
            return;
        }
        break;
    case CE_ProgressBarGroove:
        fillRounded(painter, option->rect, kGroove / 2.0, t.selected);
        return;
    case CE_ProgressBarContents:
        if (const auto *bar = qstyleoption_cast<const QStyleOptionProgressBar *>(option)) {
            const qint64 range = qint64(bar->maximum) - bar->minimum;
            const bool horizontal = option->state & State_Horizontal;
            QRectF rect = option->rect;
            if (range <= 0) {
                // Busy: a third of the groove, Fusion moves it.
                break;
            }
            const qreal fraction = qBound<qreal>(0, qreal(bar->progress - bar->minimum) / range, 1);
            if (horizontal) {
                const qreal width = rect.width() * fraction;
                const bool fromRight = bar->invertedAppearance != (option->direction == Qt::RightToLeft);
                rect = fromRight ? QRectF(rect.right() - width + 1, rect.top(), width, rect.height())
                                 : QRectF(rect.left(), rect.top(), width, rect.height());
            } else {
                const qreal height = rect.height() * fraction;
                rect = bar->invertedAppearance ? QRectF(rect.left(), rect.top(), rect.width(), height)
                                               : QRectF(rect.left(), rect.bottom() - height + 1, rect.width(), height);
            }
            if (!rect.isEmpty())
                fillRounded(painter, rect, kGroove / 2.0, enabled(option) ? accent(palette) : t.text3);
            return;
        }
        break;
    case CE_ProgressBarLabel:
        if (const auto *bar = qstyleoption_cast<const QStyleOptionProgressBar *>(option); bar && !option->rect.isEmpty()) {
            painter->save();
            painter->setPen(enabled(option) ? t.text2 : t.text3);
            painter->drawText(option->rect, Qt::AlignRight | Qt::AlignVCenter, bar->text);
            painter->restore();
            return;
        }
        break;
    case CE_MenuItem:
        if (const auto *item = qstyleoption_cast<const QStyleOptionMenuItem *>(option)) {
            const QRect rect = option->rect;
            if (item->menuItemType == QStyleOptionMenuItem::Separator) {
                if (item->text.isEmpty()) {
                    painter->fillRect(QRectF(rect.left() + 6, rect.center().y(), rect.width() - 12, 1), t.divider);
                } else {
                    painter->save();
                    painter->setPen(t.text2);
                    painter->drawText(rect.adjusted(12, 0, -12, 0), Qt::AlignLeft | Qt::AlignVCenter, item->text);
                    painter->restore();
                }
                return;
            }
            const bool on = enabled(option);
            if ((option->state & State_Selected) && on)
                fillRounded(painter, rect, kTileRadius, (option->state & State_Sunken) ? t.selected : t.hover);
            painter->save();
            painter->setFont(item->font);
            int left = rect.left() + 12;
            int right = rect.right() - 12;
            const int iconSize = pixelMetric(PM_SmallIconSize, option, widget);
            // Combo box popups pass an icon width even without icons.
            const bool iconColumn = qobject_cast<const QComboBox *>(widget) ? !item->icon.isNull()
                                                                            : item->maxIconWidth > 0 || !item->icon.isNull();
            if (iconColumn) {
                const QRect iconRect(left, rect.center().y() - iconSize / 2, iconSize, iconSize);
                if (!item->icon.isNull())
                    item->icon.paint(painter, iconRect, Qt::AlignCenter, on ? QIcon::Normal : QIcon::Disabled);
                left += qMax(item->maxIconWidth, iconSize) + 8;
            }
            if (item->menuItemType == QStyleOptionMenuItem::SubMenu) {
                drawChevron(painter, QRectF(right - 12, rect.top(), 12, rect.height()), on ? t.text2 : t.text3,
                            option->direction == Qt::RightToLeft ? Direction::Left : Direction::Right, 3.5);
                right -= 20;
            }
            if (item->checkType != QStyleOptionMenuItem::NotCheckable) {
                // The chosen item carries an accent check mark on the right.
                if (item->checked)
                    drawCheckMark(painter, QRectF(right - 15, rect.center().y() - 8, 16, 16),
                                  on ? accent(palette) : t.text3);
                right -= 24;
            }
            QString text = item->text;
            const int tab = text.indexOf(QLatin1Char('\t'));
            const int mnemonic = styleHint(SH_UnderlineShortcut, option, widget) ? Qt::TextShowMnemonic : Qt::TextHideMnemonic;
            if (tab >= 0) {
                painter->setPen(t.text3);
                painter->drawText(QRect(left, rect.top(), right - left, rect.height()),
                                  Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine, text.mid(tab + 1));
                text = text.left(tab);
            }
            painter->setPen(on ? palette.color(QPalette::Text) : t.text3);
            painter->drawText(QRect(left, rect.top(), right - left, rect.height()),
                              Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine | mnemonic, text);
            painter->restore();
            return;
        }
        break;
    case CE_MenuBarItem:
        if (const auto *item = qstyleoption_cast<const QStyleOptionMenuItem *>(option)) {
            const bool active = (option->state & State_Selected) && enabled(option);
            const bool down = option->state & State_Sunken;
            if (active || down)
                fillRounded(painter, QRectF(option->rect).adjusted(0, 2, 0, -2), kTileRadius, down ? t.selected : t.hover);
            painter->save();
            painter->setPen(enabled(option) ? palette.color(QPalette::WindowText) : t.text3);
            const int mnemonic = styleHint(SH_UnderlineShortcut, option, widget) ? Qt::TextShowMnemonic : Qt::TextHideMnemonic;
            painter->drawText(option->rect, Qt::AlignCenter | Qt::TextSingleLine | mnemonic, item->text);
            painter->restore();
            return;
        }
        break;
    case CE_MenuBarEmptyArea:
    case CE_ToolBar:
        return; // the window shows through
    case CE_HeaderSection:
        if (const auto *header = qstyleoption_cast<const QStyleOptionHeader *>(option)) {
            painter->fillRect(option->rect, (option->state & State_MouseOver) && enabled(option) ? t.hover
                                                                                                 : palette.color(QPalette::Base));
            const QRectF rect = option->rect;
            painter->fillRect(QRectF(rect.left(), rect.bottom(), rect.width(), 1), t.divider);
            const bool last = header->position == QStyleOptionHeader::End || header->position == QStyleOptionHeader::OnlyOneSection;
            if (!last && header->orientation == Qt::Horizontal)
                painter->fillRect(QRectF(rect.right(), rect.top() + 6, 1, rect.height() - 12), t.divider);
            return;
        }
        break;
    case CE_HeaderLabel:
        if (const auto *header = qstyleoption_cast<const QStyleOptionHeader *>(option)) {
            QStyleOptionHeader copy(*header);
            copy.palette.setColor(QPalette::ButtonText, enabled(option) ? t.text2 : t.text3);
            QProxyStyle::drawControl(element, &copy, painter, widget);
            return;
        }
        break;
    case CE_HeaderEmptyArea:
        painter->fillRect(option->rect, palette.color(QPalette::Base));
        painter->fillRect(QRectF(option->rect.left(), option->rect.bottom(), option->rect.width(), 1), t.divider);
        return;
    case CE_Splitter: {
        const QRectF rect = option->rect;
        if (option->state & State_Horizontal)
            painter->fillRect(QRectF(rect.center().x(), rect.top(), 1, rect.height()), t.border);
        else
            painter->fillRect(QRectF(rect.left(), rect.center().y(), rect.width(), 1), t.border);
        return;
    }
    case CE_RubberBand:
        fillRounded(painter, option->rect, 2, withAlpha(accent(palette), 0.15), accent(palette));
        return;
    case CE_ToolBoxTabShape:
        if (option->state & State_MouseOver)
            fillRounded(painter, option->rect, kControlRadius, t.hover);
        return;
    case CE_ShapedFrame:
        if (const auto *frame = qstyleoption_cast<const QStyleOptionFrame *>(option)) {
            // Opt-in surfaces fill the whole widget: a frame of no width is
            // only as large as the contents rect, inside the margins.
            if (widget && widget->objectName() == QLatin1String("Card")) {
                fillCard(painter, QRectF(widget->rect()).adjusted(0, 0, 0, -1), palette);
                return;
            }
            if (boolProperty(widget, "badge")) {
                fillRounded(painter, widget->rect(), widget->height() / 2.0, t.accentSoft);
                return;
            }
            if (boolProperty(widget, "keycap")) {
                fillRounded(painter, widget->rect(), kTileRadius, palette.color(QPalette::Base), t.border);
                return;
            }
            if (boolProperty(widget, "banner")) {
                fillRounded(painter, widget->rect(), kControlRadius, t.accentSoft);
                return;
            }
            switch (frame->frameShape) {
            case QFrame::HLine:
                painter->fillRect(QRectF(option->rect.left(), option->rect.center().y(), option->rect.width(), 1), t.divider);
                return;
            case QFrame::VLine:
                painter->fillRect(QRectF(option->rect.center().x(), option->rect.top(), 1, option->rect.height()), t.divider);
                return;
            case QFrame::Box:
            case QFrame::Panel:
            case QFrame::WinPanel:
                fillRounded(painter, option->rect, kControlRadius, QColor(), t.border);
                return;
            default:
                break;
            }
        }
        break;
    default:
        break;
    }
    QProxyStyle::drawControl(element, option, painter, widget);
}

void BayuStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter,
                                   const QWidget *widget) const
{
    const QPalette &palette = option->palette;
    const Tokens &t = tokens(palette);
    switch (control) {
    case CC_ComboBox:
        if (const auto *combo = qstyleoption_cast<const QStyleOptionComboBox *>(option)) {
            if (combo->frame || !combo->editable) {
                if (combo->editable)
                    drawField(painter, option, option->rect, option->state & State_HasFocus);
                else
                    drawSelect(painter, option, option->rect);
            }
            if (combo->subControls & SC_ComboBoxArrow) {
                const QRect arrow = subControlRect(CC_ComboBox, combo, SC_ComboBoxArrow, widget);
                drawChevron(painter, QRectF(arrow).adjusted(0, 0, -6, 0), enabled(option) ? t.text2 : t.text3);
            }
            return;
        }
        break;
    case CC_SpinBox:
        if (const auto *spin = qstyleoption_cast<const QStyleOptionSpinBox *>(option)) {
            if (spin->frame)
                drawField(painter, option, option->rect, option->state & State_HasFocus);
            for (auto [sub, step, direction] :
                 {std::tuple{SC_SpinBoxUp, QAbstractSpinBox::StepUpEnabled, Direction::Up},
                  std::tuple{SC_SpinBoxDown, QAbstractSpinBox::StepDownEnabled, Direction::Down}}) {
                if (!(spin->subControls & sub) || spin->buttonSymbols == QAbstractSpinBox::NoButtons)
                    continue;
                const QRectF rect = subControlRect(CC_SpinBox, spin, sub, widget);
                const bool active = enabled(option) && (spin->stepEnabled & step);
                const bool hovered = active && (spin->activeSubControls & sub);
                if (hovered)
                    fillRounded(painter, rect, kTileRadius, (option->state & State_Sunken) ? t.selected : t.hover);
                drawChevron(painter, rect, active ? (hovered ? t.text : t.text2) : t.text3, direction, 3.5);
            }
            return;
        }
        break;
    case CC_Slider:
        if (const auto *slider = qstyleoption_cast<const QStyleOptionSlider *>(option)) {
            const bool horizontal = slider->orientation == Qt::Horizontal;
            const QRect groove = subControlRect(CC_Slider, slider, SC_SliderGroove, widget);
            const QRect handle = subControlRect(CC_Slider, slider, SC_SliderHandle, widget);
            const qreal inset = kSliderExtent / 2.0;
            const QRectF track = horizontal
                ? QRectF(groove.left() + inset, groove.center().y() - kGroove / 2.0 + 1, groove.width() - 2 * inset, kGroove)
                : QRectF(groove.center().x() - kGroove / 2.0 + 1, groove.top() + inset, kGroove, groove.height() - 2 * inset);
            painter->save();
            if (!enabled(option))
                painter->setOpacity(0.45);
            fillRounded(painter, track, kGroove / 2.0, t.sliderTrack);
            QRectF filled = track;
            const QPointF centre = QRectF(handle).center();
            const bool fromEnd = horizontal ? (slider->upsideDown != (option->direction == Qt::RightToLeft))
                                            : !slider->upsideDown;
            if (horizontal)
                fromEnd ? filled.setLeft(centre.x()) : filled.setRight(centre.x());
            else
                fromEnd ? filled.setTop(centre.y()) : filled.setBottom(centre.y());
            fillRounded(painter, filled, kGroove / 2.0, accent(palette));

            // Tick marks in the tertiary text colour.
            if ((slider->subControls & SC_SliderTickmarks) && slider->tickPosition != QSlider::NoTicks
                && slider->maximum > slider->minimum) {
                const int interval = slider->tickInterval > 0 ? slider->tickInterval : qMax(1, slider->pageStep);
                for (int value = slider->minimum; value <= slider->maximum; value += interval) {
                    const int position = sliderPositionFromValue(slider->minimum, slider->maximum, value,
                                                                 int(horizontal ? track.width() : track.height()),
                                                                 slider->upsideDown);
                    if (horizontal) {
                        const qreal x = track.left() + position;
                        if (slider->tickPosition & QSlider::TicksAbove)
                            painter->fillRect(QRectF(x, groove.top(), 1, 4), t.text3);
                        if (slider->tickPosition & QSlider::TicksBelow)
                            painter->fillRect(QRectF(x, groove.bottom() - 3, 1, 4), t.text3);
                    } else {
                        const qreal y = track.top() + position;
                        if (slider->tickPosition & QSlider::TicksLeft)
                            painter->fillRect(QRectF(groove.left(), y, 4, 1), t.text3);
                        if (slider->tickPosition & QSlider::TicksRight)
                            painter->fillRect(QRectF(groove.right() - 3, y, 4, 1), t.text3);
                    }
                }
            }

            // The handle: 18 px white with a hairline border and a soft shadow.
            QRectF knob(0, 0, kHandle, kHandle);
            knob.moveCenter(centre);
            const bool pressed = (slider->activeSubControls & SC_SliderHandle) && (option->state & State_Sunken);
            const bool hover = (slider->activeSubControls & SC_SliderHandle) && (option->state & State_MouseOver);
            fillRounded(painter, knob.translated(0, 1), kHandle / 2.0, QColor(0, 0, 0, hover ? 46 : 30));
            fillRounded(painter, knob, kHandle / 2.0, pressed ? t.field : QColor(Qt::white), t.border);
            painter->restore();
            if (keyboardFocus(option))
                drawFocusRing(painter, knob.adjusted(-2 * kRing, -2 * kRing, 2 * kRing, 2 * kRing),
                              kHandle / 2.0 + 2 * kRing, palette);
            return;
        }
        break;
    case CC_ScrollBar:
        if (const auto *bar = qstyleoption_cast<const QStyleOptionSlider *>(option)) {
            if (bar->maximum <= bar->minimum)
                return;
            const bool horizontal = bar->orientation == Qt::Horizontal;
            const bool hover = option->state & State_MouseOver;
            const bool dragging = (bar->activeSubControls & SC_ScrollBarSlider) && (option->state & State_Sunken);
            QRectF handle = subControlRect(CC_ScrollBar, bar, SC_ScrollBarSlider, widget);
            // A thin indicator that widens under the pointer.
            const qreal thickness = (hover || dragging) ? 8 : 6;
            if (horizontal)
                handle = QRectF(handle.left() + 2, handle.center().y() - thickness / 2, handle.width() - 4, thickness);
            else
                handle = QRectF(handle.center().x() - thickness / 2, handle.top() + 2, thickness, handle.height() - 4);
            fillRounded(painter, handle, thickness / 2, (hover || dragging) ? t.text2 : withAlpha(t.text3, 0.7));
            return;
        }
        break;
    case CC_GroupBox:
        if (const auto *box = qstyleoption_cast<const QStyleOptionGroupBox *>(option)) {
            if ((box->subControls & SC_GroupBoxFrame) && !(box->features & QStyleOptionFrame::Flat)) {
                QStyleOptionFrame frame;
                frame.QStyleOption::operator=(*box);
                frame.rect = subControlRect(CC_GroupBox, box, SC_GroupBoxFrame, widget);
                proxy()->drawPrimitive(PE_FrameGroupBox, &frame, painter, widget);
            }
            if ((box->subControls & SC_GroupBoxLabel) && !box->text.isEmpty()) {
                const QRect label = subControlRect(CC_GroupBox, box, SC_GroupBoxLabel, widget);
                painter->save();
                // The caption above a card: secondary text.
                painter->setPen(enabled(option) ? t.text2 : t.text3);
                painter->drawText(label, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextShowMnemonic, box->text);
                painter->restore();
            }
            if (box->subControls & SC_GroupBoxCheckBox) {
                QStyleOptionButton check;
                check.QStyleOption::operator=(*box);
                check.rect = subControlRect(CC_GroupBox, box, SC_GroupBoxCheckBox, widget);
                proxy()->drawPrimitive(PE_IndicatorCheckBox, &check, painter, nullptr);
            }
            return;
        }
        break;
    case CC_ToolButton:
        // Text-only auto-raised tool buttons are ghost buttons: accent labels.
        if (const auto *button = qstyleoption_cast<const QStyleOptionToolButton *>(option);
            button && (option->state & State_AutoRaise) && enabled(option)
            && (button->toolButtonStyle == Qt::ToolButtonTextOnly || button->icon.isNull()) && !button->text.isEmpty()) {
            QStyleOptionToolButton copy(*button);
            copy.palette.setColor(QPalette::ButtonText, accent(palette));
            QProxyStyle::drawComplexControl(control, &copy, painter, widget);
            return;
        }
        break;
    default:
        break;
    }
    QProxyStyle::drawComplexControl(control, option, painter, widget);
}
