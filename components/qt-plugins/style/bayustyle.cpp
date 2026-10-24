#include "bayustyle.h"

#include <QAbstractSpinBox>
#include <QComboBox>
#include <QListView>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QStyleFactory>
#include <QStyleOption>
#include <QTabBar>

#include <tuple>

namespace {

// bayu-toolkit metrics (theme.h and the controls' constants).
constexpr qreal kSmallRadius = 8;     // fields, combo boxes, scroll bar handles
constexpr qreal kMediumRadius = 10;   // push buttons
constexpr qreal kBigRadius = 12;      // menus
constexpr int kButtonHeight = 32;     // button.cpp kMinimumHeight
constexpr int kButtonMinWidth = 64;   // button.cpp kMinimumWidth
constexpr int kFieldHeight = 35;      // text_field.cpp, combo_box.cpp kHeight
constexpr int kIndicator = 20;        // check_box.cpp kIndicator
constexpr qreal kIndicatorRadius = 6; // check_box.cpp kRadius
constexpr int kSliderTrack = 4;       // slider.cpp kTrack
constexpr int kSliderHandle = 22;     // slider.cpp kHandle
constexpr int kScrollBarExtent = 10;
constexpr int kScrollBarMinLength = 32; // scroll_bar.cpp kMinimumHandleLength
constexpr int kMenuRowHeight = 30;    // menu kRowHeight
constexpr int kFieldPadding = 8;      // text inset in fields
constexpr int kSegmentedHeight = 32;  // segmented_control.cpp kSegmentedHeight
constexpr int kSectionLabelSpacing = 8; // settings metrics sectionLabelSpacing

bool isDark(const QPalette &palette)
{
    return palette.color(QPalette::Window).lightness() < 128;
}

QColor withAlpha(QColor color, qreal alpha)
{
    color.setAlphaF(alpha);
    return color;
}

// ButtonBackground{,Hovered,Pressed} in toolkit_colors.cpp. QColor::lighter
// and darker scale the HSV value, as bayu's Color does.
QColor buttonColor(const QStyleOption *option)
{
    const bool dark = isDark(option->palette);
    const QColor base = option->palette.color(QPalette::Button);
    if (option->state & (QStyle::State_Sunken | QStyle::State_On))
        return dark ? base.lighter(110) : base.darker(120);
    if (option->state & QStyle::State_MouseOver)
        return dark ? base.lighter(120) : base.darker(110);
    return base;
}

bool hasFocusRing(const QStyleOption *option)
{
    return (option->state & QStyle::State_HasFocus) && (option->state & QStyle::State_KeyboardFocusChange)
        && (option->state & QStyle::State_Enabled);
}

void fillRounded(QPainter *painter, const QRectF &rect, qreal radius, const QColor &fill,
                 const QColor &border = QColor())
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    if (border.isValid() && border.alpha() > 0) {
        painter->setPen(QPen(border, 1));
        painter->setBrush(fill);
        painter->drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    } else {
        painter->setPen(Qt::NoPen);
        painter->setBrush(fill);
        painter->drawRoundedRect(rect, radius, radius);
    }
    painter->restore();
}

void strokeRounded(QPainter *painter, const QRectF &rect, qreal radius, const QColor &color, qreal width)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(color, width));
    painter->setBrush(Qt::NoBrush);
    const qreal inset = width / 2;
    painter->drawRoundedRect(rect.adjusted(inset, inset, -inset, -inset), radius, radius);
    painter->restore();
}

void drawChevron(QPainter *painter, const QRectF &rect, const QColor &color)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    const QPointF c = rect.center();
    QPainterPath path;
    path.moveTo(c.x() - 4, c.y() - 2);
    path.lineTo(c.x(), c.y() + 2);
    path.lineTo(c.x() + 4, c.y() - 2);
    painter->drawPath(path);
    painter->restore();
}

// The check box and radio button plate: ToggleBackground{,Checked} and
// ToggleBorder{,Checked} in toolkit_colors.cpp.
void drawTogglePlate(QPainter *painter, const QStyleOption *option, bool round)
{
    const QPalette &palette = option->palette;
    const bool dark = isDark(palette);
    const bool on = option->state & (QStyle::State_On | QStyle::State_NoChange);
    const QRectF rect = QRectF(option->rect).adjusted(0, 0, 0, 0);
    const QSizeF size(kIndicator, kIndicator);
    QRectF plate(QPointF(0, 0), size.boundedTo(rect.size()));
    plate.moveCenter(rect.center());
    const qreal radius = round ? plate.height() / 2 : kIndicatorRadius;

    QColor fill = on ? palette.color(QPalette::Highlight) : palette.color(QPalette::Base);
    QColor border = on ? palette.color(QPalette::Highlight).darker(112)
                       : withAlpha(palette.color(QPalette::Text), dark ? 0.28 : 0.2);
    if (!(option->state & QStyle::State_Enabled)) {
        fill = withAlpha(fill, 0.5);
        border = withAlpha(border, 0.5);
    }
    if (hasFocusRing(option))
        strokeRounded(painter, plate.adjusted(-3, -3, 3, 3), radius + 3,
                      withAlpha(palette.color(QPalette::Highlight), 0.35), 2);
    fillRounded(painter, plate, radius, fill, border);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const QColor mark = palette.color(QPalette::HighlightedText);
    if (round && (option->state & QStyle::State_On)) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(mark);
        painter->drawEllipse(plate.center(), plate.width() * 0.2, plate.width() * 0.2);
    } else if (option->state & QStyle::State_NoChange) {
        painter->setPen(QPen(mark, 2, Qt::SolidLine, Qt::RoundCap));
        painter->drawLine(QPointF(plate.left() + 5.5, plate.center().y()),
                          QPointF(plate.right() - 5.5, plate.center().y()));
    } else if (option->state & QStyle::State_On) {
        const qreal s = plate.width() / kIndicator;
        QPainterPath check;
        check.moveTo(plate.left() + 5.5 * s, plate.top() + 10.5 * s);
        check.lineTo(plate.left() + 8.5 * s, plate.top() + 13.5 * s);
        check.lineTo(plate.left() + 14.5 * s, plate.top() + 6.5 * s);
        painter->setPen(QPen(mark, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(check);
    }
    painter->restore();
}

// A text field, spin box or combo box (text_field.cpp, combo_box.cpp): the
// alternate background, edged with the highlight while focused or open.
void drawField(QPainter *painter, const QStyleOption *option, const QRectF &rect, bool active)
{
    const QPalette &palette = option->palette;
    QColor fill = palette.color(QPalette::Button);
    if (!(option->state & QStyle::State_Enabled))
        fill = withAlpha(fill, 0.6);
    const bool edged = active && (option->state & QStyle::State_Enabled);
    fillRounded(painter, rect, kSmallRadius, fill, edged ? palette.color(QPalette::Highlight) : QColor());
}

// A field nested in a spin box or combo box, which draws the frame.
bool isNestedField(const QWidget *widget)
{
    return widget && (qobject_cast<const QAbstractSpinBox *>(widget->parentWidget())
                      || qobject_cast<const QComboBox *>(widget->parentWidget()));
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
    // Menus draw their own rounded panel; the corners around it stay clear.
    if (qobject_cast<QMenu *>(widget) || widget->inherits("QComboBoxPrivateContainer"))
        widget->setAttribute(Qt::WA_TranslucentBackground);
}

void BayuStyle::unpolish(QWidget *widget)
{
    if (qobject_cast<QMenu *>(widget) || widget->inherits("QComboBoxPrivateContainer"))
        widget->setAttribute(Qt::WA_TranslucentBackground, false);
    QProxyStyle::unpolish(widget);
}

int BayuStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const
{
    switch (metric) {
    case PM_ButtonMargin:
        return 24;
    case PM_ButtonDefaultIndicator:
    case PM_ButtonShiftHorizontal:
    case PM_ButtonShiftVertical:
        return 0;
    case PM_IndicatorWidth:
    case PM_IndicatorHeight:
    case PM_ExclusiveIndicatorWidth:
    case PM_ExclusiveIndicatorHeight:
        return kIndicator;
    case PM_CheckBoxLabelSpacing:
    case PM_RadioButtonLabelSpacing:
        return 8;
    case PM_ScrollBarExtent:
        return kScrollBarExtent;
    case PM_ScrollBarSliderMin:
        return kScrollBarMinLength;
    case PM_SliderThickness:
    case PM_SliderLength:
    case PM_SliderControlThickness:
        return kSliderHandle;
    case PM_MenuHMargin:
    case PM_MenuVMargin:
        return 4;
    case PM_MenuPanelWidth:
        return 1;
    case PM_ToolTipLabelFrameWidth:
        return 6;
    default:
        return QProxyStyle::pixelMetric(metric, option, widget);
    }
}

int BayuStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget,
                         QStyleHintReturn *returnData) const
{
    switch (hint) {
    case SH_DialogButtonBox_ButtonsHaveIcons:
    case SH_EtchDisabledText:
        return 0;
    case SH_LineEdit_PasswordCharacter:
        return 0x2022; // text_field.cpp kPasswordDot
    case SH_TabBar_Alignment:
        return Qt::AlignCenter;
    case SH_ComboBox_Popup:
        return 1; // a menu with a check mark on the current item
    case SH_ItemView_ShowDecorationSelected:
        return 1;
    default:
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }
}

QSize BayuStyle::sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &size,
                                  const QWidget *widget) const
{
    QSize result = QProxyStyle::sizeFromContents(type, option, size, widget);
    switch (type) {
    case CT_PushButton:
        result = result.expandedTo(QSize(kButtonMinWidth, kButtonHeight));
        break;
    case CT_LineEdit:
        // Fields inside spin boxes and combo boxes take their parent's size.
        if (!widget || !(qobject_cast<QAbstractSpinBox *>(widget->parentWidget())
                         || qobject_cast<QComboBox *>(widget->parentWidget())))
            result.setHeight(qMax(result.height(), kFieldHeight));
        break;
    case CT_ComboBox:
        result.setHeight(qMax(result.height(), kFieldHeight));
        if (const auto *combo = qstyleoption_cast<const QStyleOptionComboBox *>(option); combo && !combo->editable)
            result.setWidth(result.width() + 20); // the popup's check column
        break;
    case CT_ItemViewItem:
        // List rows as tall as Bayu's sidebar and list rows.
        if (qobject_cast<const QListView *>(widget))
            result.setHeight(qMax(result.height(), 32));
        break;
    case CT_TabBarTab:
        if (const auto *tab = qstyleoption_cast<const QStyleOptionTab *>(option);
            tab && (tab->shape == QTabBar::RoundedNorth || tab->shape == QTabBar::RoundedSouth
                    || tab->shape == QTabBar::TriangularNorth || tab->shape == QTabBar::TriangularSouth))
            result = QSize(result.width() + 16, qMax(result.height(), kSegmentedHeight));
        break;
    case CT_MenuItem:
        if (const auto *item = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
            item && item->menuItemType != QStyleOptionMenuItem::Separator)
            result.setHeight(qMax(result.height(), kMenuRowHeight));
        break;
    default:
        break;
    }
    return result;
}

QRect BayuStyle::subControlRect(ComplexControl control, const QStyleOptionComplex *option,
                                SubControl subControl, const QWidget *widget) const
{
    // Scroll bars have no arrow buttons: the handle runs the whole length.
    if (control == CC_ScrollBar) {
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
    }
    if (control == CC_GroupBox) {
        if (const auto *box = qstyleoption_cast<const QStyleOptionGroupBox *>(option)) {
            const QRect label = QProxyStyle::subControlRect(control, option, SC_GroupBoxLabel, widget);
            const QRect check = QProxyStyle::subControlRect(control, option, SC_GroupBoxCheckBox, widget);
            const bool titled = !box->text.isEmpty() || (box->subControls & SC_GroupBoxCheckBox);
            const int labelBottom = qMax(box->text.isEmpty() ? option->rect.top() : label.bottom() + 1,
                                         (box->subControls & SC_GroupBoxCheckBox) ? check.bottom() + 1 : option->rect.top());
            QRect frame = option->rect;
            if (titled)
                frame.setTop(labelBottom + kSectionLabelSpacing);
            switch (subControl) {
            case SC_GroupBoxFrame:
                return frame;
            case SC_GroupBoxContents:
                return frame.adjusted(6, 6, -6, -6);
            default:
                break;
            }
        }
    }
    QRect rect = QProxyStyle::subControlRect(control, option, subControl, widget);
    if (control == CC_SpinBox && subControl == SC_SpinBoxEditField)
        rect.adjust(kFieldPadding - 2, 0, 0, 0);
    return rect;
}

QRect BayuStyle::subElementRect(SubElement element, const QStyleOption *option, const QWidget *widget) const
{
    QRect rect = QProxyStyle::subElementRect(element, option, widget);
    if (element == SE_LineEditContents && !isNestedField(widget)) {
        if (const auto *frame = qstyleoption_cast<const QStyleOptionFrame *>(option); frame && frame->lineWidth > 0)
            rect.adjust(kFieldPadding - 2, 0, -(kFieldPadding - 2), 0);
    }
    return rect;
}

void BayuStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
                              const QWidget *widget) const
{
    const QPalette &palette = option->palette;
    switch (element) {
    case PE_PanelButtonCommand:
    case PE_PanelButtonBevel: {
        // DialogButton (dialog_window.cpp): a capsule, the default one in the
        // accent colour, the others edged with DialogButtonBorder.
        const auto *button = qstyleoption_cast<const QStyleOptionButton *>(option);
        const bool primary = button && (button->features & QStyleOptionButton::DefaultButton);
        const bool dark = isDark(palette);
        const QRectF rect = option->rect;
        const qreal radius = rect.height() / 2;
        QColor fill;
        QColor border;
        if (primary) {
            const QColor accent = palette.color(QPalette::Highlight);
            fill = accent;
            if (option->state & State_Sunken)
                fill = accent.darker(115);
            else if (option->state & State_MouseOver)
                fill = accent.lighter(105);
            border = accent.darker(125);
        } else {
            const QColor base = palette.color(QPalette::Button);
            fill = base;
            if (option->state & (State_Sunken | State_On))
                fill = dark ? base.lighter(112) : base.darker(110);
            else if (option->state & State_MouseOver)
                fill = dark ? base.lighter(105) : base.darker(104);
            border = dark ? QColor(0x545455) : QColor(0xD9DADD);
        }
        if (!(option->state & State_Enabled)) {
            fill = withAlpha(fill, 0.5);
            border = withAlpha(border, 0.5);
        }
        fillRounded(painter, rect, radius, fill, border);
        if (!primary && !dark)
            strokeRounded(painter, rect.adjusted(1, 1, -1, -1), radius - 1, QColor(255, 255, 255, 20), 1);
        if (hasFocusRing(option))
            strokeRounded(painter, rect, radius, withAlpha(palette.color(QPalette::Highlight), 0.3), 2);
        return;
    }
    case PE_FrameDefaultButton:
    case PE_FrameFocusRect:
        // Focus shows as a ring on the control itself.
        return;
    case PE_PanelLineEdit:
        if (isNestedField(widget))
            return;
        if (const auto *frame = qstyleoption_cast<const QStyleOptionFrame *>(option);
            frame && frame->lineWidth > 0) {
            drawField(painter, option, option->rect, option->state & State_HasFocus);
            return;
        }
        // Frameless fields (item editors) keep Fusion's.
        break;
    case PE_FrameLineEdit:
        return;
    case PE_IndicatorCheckBox:
        drawTogglePlate(painter, option, false);
        return;
    case PE_IndicatorRadioButton:
        drawTogglePlate(painter, option, true);
        return;
    case PE_PanelMenu:
    case PE_FrameMenu: {
        const QRectF rect = QRectF(option->rect).adjusted(0.5, 0.5, -0.5, -0.5);
        fillRounded(painter, rect, kBigRadius, palette.color(QPalette::Base),
                    withAlpha(palette.color(QPalette::Text), isDark(palette) ? 0.14 : 0.1));
        return;
    }
    case PE_FrameTabWidget:
        // A Settings card (settings/src/widgets/card.cpp).
        fillRounded(painter, option->rect, kBigRadius, palette.color(QPalette::Base));
        return;
    case PE_FrameTabBarBase:
        return;
    case PE_Frame:
        // Scroll areas (tree views, text edits): a faint rounded edge, no dark box.
        if (option->state & (State_Sunken | State_Raised)) {
            fillRounded(painter, option->rect, kSmallRadius, Qt::transparent,
                        withAlpha(palette.color(QPalette::Text), isDark(palette) ? 0.14 : 0.1));
        }
        return;
    case PE_FrameGroupBox:
        fillRounded(painter, option->rect, kBigRadius, palette.color(QPalette::Base));
        return;
    case PE_PanelItemViewRow:
        if (option->state & State_Selected) {
            fillRounded(painter, option->rect, 0, withAlpha(palette.color(QPalette::Text), isDark(palette) ? 0.1 : 0.08));
            return;
        }
        break;
    case PE_PanelTipLabel:
        painter->fillRect(option->rect, palette.color(QPalette::ToolTipBase));
        painter->save();
        painter->setPen(withAlpha(palette.color(QPalette::ToolTipText), 0.15));
        painter->drawRect(option->rect.adjusted(0, 0, -1, -1));
        painter->restore();
        return;
    default:
        break;
    }
    QProxyStyle::drawPrimitive(element, option, painter, widget);
}

void BayuStyle::drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                            const QWidget *widget) const
{
    switch (element) {
    case CE_ToolBoxTabShape:
        // QToolBox pages: flat rows, highlighted like sidebar items on hover.
        if (option->state & State_MouseOver)
            fillRounded(painter, option->rect, kSmallRadius, withAlpha(option->palette.color(QPalette::Text), isDark(option->palette) ? 0.1 : 0.05));
        return;
    case CE_PushButtonLabel:
        // The default button is filled with the highlight, so its label is light.
        if (const auto *button = qstyleoption_cast<const QStyleOptionButton *>(option);
            button && (button->features & QStyleOptionButton::DefaultButton)) {
            QStyleOptionButton copy(*button);
            QColor text = option->palette.color(QPalette::HighlightedText);
            if (!(option->state & State_Enabled))
                text = withAlpha(text, 0.6);
            copy.palette.setColor(QPalette::ButtonText, text);
            copy.palette.setColor(QPalette::Disabled, QPalette::ButtonText, text);
            QProxyStyle::drawControl(element, &copy, painter, widget);
            return;
        }
        break;
    case CE_TabBarTabShape:
        if (const auto *tab = qstyleoption_cast<const QStyleOptionTab *>(option)) {
            // segmented_control.cpp: a pill track (SegmentedControlTrack) behind
            // the tabs, rounded at the ends of the row, and a pill plate with a
            // soft shadow under the current tab.
            const bool dark = isDark(option->palette);
            const QRectF rect = option->rect;
            const qreal radius = rect.height() / 2;
            const bool first = tab->position == QStyleOptionTab::Beginning || tab->position == QStyleOptionTab::OnlyOneTab;
            const bool last = tab->position == QStyleOptionTab::End || tab->position == QStyleOptionTab::OnlyOneTab;
            QPainterPath track;
            track.addRoundedRect(rect, radius, radius);
            if (!first)
                track.addRect(QRectF(rect.left(), rect.top(), radius, rect.height()));
            if (!last)
                track.addRect(QRectF(rect.right() - radius, rect.top(), radius, rect.height()));
            track.setFillRule(Qt::WindingFill);
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            painter->fillPath(track.simplified(), dark ? QColor(255, 255, 255, 15) : QColor(0, 0, 0, 10));
            painter->restore();
            const QRectF plate = rect.adjusted(2, 2, -2, -2);
            if (option->state & State_Selected) {
                QColor fill = option->palette.color(QPalette::Base);
                if (dark)
                    fill = fill.lighter(200);
                fillRounded(painter, plate.translated(0, 1), plate.height() / 2, QColor(0, 0, 0, dark ? 46 : 31));
                fillRounded(painter, plate, plate.height() / 2, fill);
            } else if (option->state & State_MouseOver) {
                fillRounded(painter, plate, plate.height() / 2, withAlpha(option->palette.color(QPalette::Text), 0.05));
            }
            return;
        }
        break;
    case CE_ItemViewItem:
        // Selected rows are a soft grey plate (SidebarItemCurrent), not the accent.
        if (const auto *item = qstyleoption_cast<const QStyleOptionViewItem *>(option);
            item && (option->state & State_Selected)) {
            QStyleOptionViewItem copy(*item);
            const QColor plate = withAlpha(option->palette.color(QPalette::Text), isDark(option->palette) ? 0.1 : 0.08);
            const bool whole = item->viewItemPosition == QStyleOptionViewItem::OnlyOne
                || item->viewItemPosition == QStyleOptionViewItem::Invalid;
            if (whole)
                fillRounded(painter, QRectF(option->rect).adjusted(2, 1, -2, -1), kSmallRadius, plate);
            else
                painter->fillRect(option->rect, plate);
            copy.state &= ~State_Selected;
            QProxyStyle::drawControl(element, &copy, painter, widget);
            return;
        }
        break;
    case CE_ProgressBarGroove: {
        const QRectF rect = option->rect;
        fillRounded(painter, rect, qMin(rect.width(), rect.height()) / 2, option->palette.color(QPalette::Button));
        return;
    }
    case CE_ProgressBarContents:
        if (const auto *bar = qstyleoption_cast<const QStyleOptionProgressBar *>(option)) {
            const qint64 range = qint64(bar->maximum) - bar->minimum;
            if (range <= 0)
                break; // busy: Fusion's moving bar
            const qreal fraction = qBound<qreal>(0, qreal(bar->progress - bar->minimum) / range, 1);
            const bool horizontal = option->state & State_Horizontal;
            QRectF rect = option->rect;
            if (horizontal) {
                const qreal width = rect.width() * fraction;
                const bool fromRight = bar->invertedAppearance != (option->direction == Qt::RightToLeft);
                rect = fromRight ? QRectF(rect.right() - width, rect.top(), width, rect.height())
                                 : QRectF(rect.left(), rect.top(), width, rect.height());
            } else {
                const qreal height = rect.height() * fraction;
                rect = bar->invertedAppearance ? QRectF(rect.left(), rect.top(), rect.width(), height)
                                               : QRectF(rect.left(), rect.bottom() - height, rect.width(), height);
            }
            if (!rect.isEmpty())
                fillRounded(painter, rect, qMin(option->rect.width(), option->rect.height()) / 2.0,
                            option->palette.color(QPalette::Highlight));
            return;
        }
        break;
    case CE_MenuItem:
        if (const auto *item = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
            item && item->menuItemType != QStyleOptionMenuItem::Separator) {
            QStyleOptionMenuItem copy(*item);
            const bool highlighted = (option->state & State_Selected) && (option->state & State_Enabled);
            QColor markColor = option->palette.color(QPalette::Highlight);
            if (highlighted) {
                // MenuHighlight: a rounded plate inset from the menu's edges.
                const bool dark = isDark(option->palette);
                fillRounded(painter, QRectF(option->rect).adjusted(3, 1, -3, -1), 6,
                            withAlpha(option->palette.color(QPalette::Highlight), dark ? 0.82 : 0.9));
                copy.state &= ~State_Selected;
                markColor = option->palette.color(QPalette::HighlightedText);
                for (auto role : {QPalette::Text, QPalette::ButtonText, QPalette::WindowText})
                    copy.palette.setColor(role, markColor);
                copy.palette.setColor(QPalette::Window, Qt::transparent);
            }
            const bool checked = item->checkType != QStyleOptionMenuItem::NotCheckable && item->checked;
            copy.checked = false;
            // Combo box popups list their items without a check column; make one.
            const bool comboPopup = item->checkType != QStyleOptionMenuItem::NotCheckable && !qobject_cast<const QMenu *>(widget);
            if (comboPopup)
                copy.rect.setLeft(copy.rect.left() + 20);
            QProxyStyle::drawControl(element, &copy, painter, widget);
            if (checked) {
                // The selected choice gets a check mark, as Bayu's menus do.
                const QRectF box(option->rect.left() + (comboPopup ? 6 : 7), option->rect.center().y() - 7, 14, 14);
                QPainterPath mark;
                mark.moveTo(box.left() + 2, box.top() + 7.5);
                mark.lineTo(box.left() + 5.5, box.top() + 11);
                mark.lineTo(box.right() - 2, box.top() + 3);
                painter->save();
                painter->setRenderHint(QPainter::Antialiasing);
                painter->setPen(QPen(markColor, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                painter->drawPath(mark);
                painter->restore();
            }
            return;
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
    switch (control) {
    case CC_ComboBox:
        if (const auto *combo = qstyleoption_cast<const QStyleOptionComboBox *>(option)) {
            // ComboBoxBackground{,Pressed} and ComboBoxFocusBorder.
            QStyleOptionComboBox copy(*combo);
            if (!combo->editable && (option->state & State_Sunken))
                copy.palette.setColor(QPalette::Button, isDark(palette) ? palette.color(QPalette::Button).lighter(110)
                                                                       : palette.color(QPalette::Button).darker(120));
            drawField(painter, &copy, option->rect,
                      (option->state & State_On) || (option->state & State_HasFocus));
            if (combo->subControls & SC_ComboBoxArrow) {
                const QRect arrow = subControlRect(CC_ComboBox, combo, SC_ComboBoxArrow, widget);
                drawChevron(painter, arrow, withAlpha(palette.color(QPalette::ButtonText),
                                                      (option->state & State_Enabled) ? 0.7 : 0.35));
            }
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
                // Section labels: DisabledText (settings/src/widgets/sections.cpp).
                painter->setPen(palette.color(QPalette::Disabled, QPalette::WindowText));
                painter->drawText(label, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextShowMnemonic, box->text);
                painter->restore();
            }
            if (box->subControls & SC_GroupBoxCheckBox) {
                QStyleOptionButton check;
                check.QStyleOption::operator=(*box);
                check.rect = subControlRect(CC_GroupBox, box, SC_GroupBoxCheckBox, widget);
                proxy()->drawPrimitive(PE_IndicatorCheckBox, &check, painter, widget);
            }
            return;
        }
        break;
    case CC_SpinBox:
        if (const auto *spin = qstyleoption_cast<const QStyleOptionSpinBox *>(option)) {
            if (spin->frame)
                drawField(painter, option, option->rect, option->state & State_HasFocus);
            const bool enabled = option->state & State_Enabled;
            for (auto [sub, step, up] : {std::tuple{SC_SpinBoxUp, QAbstractSpinBox::StepUpEnabled, true},
                                         std::tuple{SC_SpinBoxDown, QAbstractSpinBox::StepDownEnabled, false}}) {
                if (!(spin->subControls & sub) || spin->buttonSymbols == QAbstractSpinBox::NoButtons)
                    continue;
                const QRectF rect = subControlRect(CC_SpinBox, spin, sub, widget);
                const bool active = enabled && (spin->stepEnabled & step);
                QColor color = withAlpha(palette.color(QPalette::ButtonText), active ? 0.7 : 0.3);
                if (active && (spin->activeSubControls & sub) && (option->state & State_Sunken))
                    color = palette.color(QPalette::Highlight);
                painter->save();
                if (up) {
                    // Mirror the chevron to point up.
                    painter->translate(rect.center());
                    painter->scale(1, -1);
                    painter->translate(-rect.center());
                }
                drawChevron(painter, rect, color);
                painter->restore();
            }
            return;
        }
        break;
    case CC_Slider:
        if (const auto *slider = qstyleoption_cast<const QStyleOptionSlider *>(option)) {
            const bool dark = isDark(palette);
            const bool horizontal = slider->orientation == Qt::Horizontal;
            const QRect groove = subControlRect(CC_Slider, slider, SC_SliderGroove, widget);
            const QRect handle = subControlRect(CC_Slider, slider, SC_SliderHandle, widget);
            const qreal inset = kSliderHandle / 2.0;
            QRectF track = horizontal
                ? QRectF(groove.left() + inset, groove.center().y() - kSliderTrack / 2.0 + 0.5,
                         groove.width() - 2 * inset, kSliderTrack)
                : QRectF(groove.center().x() - kSliderTrack / 2.0 + 0.5, groove.top() + inset,
                         kSliderTrack, groove.height() - 2 * inset);
            // SliderTrack and SliderTrackFilled.
            fillRounded(painter, track, kSliderTrack / 2.0,
                        dark ? QColor(255, 255, 255, 77) : QColor(0, 0, 0, 26));
            QRectF filled = track;
            const QPointF centre = QRectF(handle).center();
            const bool fromEnd = horizontal ? (slider->upsideDown != (option->direction == Qt::RightToLeft))
                                            : !slider->upsideDown;
            if (horizontal) {
                if (fromEnd)
                    filled.setLeft(centre.x());
                else
                    filled.setRight(centre.x());
            } else {
                if (fromEnd)
                    filled.setTop(centre.y());
                else
                    filled.setBottom(centre.y());
            }
            QColor highlight = palette.color(QPalette::Highlight);
            if (!(option->state & State_Enabled))
                highlight = withAlpha(highlight, 0.5);
            fillRounded(painter, filled, kSliderTrack / 2.0, highlight);

            // SliderHandle: white with a hairline border and a soft shadow.
            QRectF knob(0, 0, kSliderHandle, kSliderHandle);
            knob.moveCenter(centre);
            const bool pressed = (slider->activeSubControls & SC_SliderHandle) && (option->state & State_Sunken);
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setPen(Qt::NoPen);
            for (int i = 3; i >= 1; --i) {
                painter->setBrush(QColor(0, 0, 0, pressed ? 14 : 10));
                painter->drawEllipse(knob.translated(0, 1).adjusted(-i * 0.7, -i * 0.7, i * 0.7, i * 0.7));
            }
            painter->setBrush(pressed ? QColor(0xFA, 0xFA, 0xFA) : QColor(Qt::white));
            painter->setPen(QPen(QColor(0, 0, 0, 10), 1));
            painter->drawEllipse(knob.adjusted(0.5, 0.5, -0.5, -0.5));
            painter->restore();
            if (hasFocusRing(option))
                strokeRounded(painter, knob.adjusted(-3, -3, 3, 3), kSliderHandle / 2.0 + 3,
                              withAlpha(palette.color(QPalette::Highlight), 0.35), 2);
            return;
        }
        break;
    case CC_ScrollBar:
        if (const auto *bar = qstyleoption_cast<const QStyleOptionSlider *>(option)) {
            const bool dark = isDark(palette);
            QRectF handle = subControlRect(CC_ScrollBar, bar, SC_ScrollBarSlider, widget);
            handle.adjust(1, 1, -1, -1); // scroll_bar.cpp kPadding
            if (handle.isEmpty() || bar->maximum <= bar->minimum)
                return;
            const bool dragging = (bar->activeSubControls & SC_ScrollBarSlider) && (option->state & State_Sunken);
            QColor color = dark ? QColor(255, 255, 255, dragging ? 102 : 128)
                                : QColor(0, 0, 0, dragging ? 128 : 102);
            if (!(option->state & State_MouseOver) && !dragging)
                color = withAlpha(color, color.alphaF() * 0.75);
            const qreal radius = qMin<qreal>(kSmallRadius, qMin(handle.width(), handle.height()) / 2);
            fillRounded(painter, handle, radius, color);
            return;
        }
        break;
    default:
        break;
    }
    QProxyStyle::drawComplexControl(control, option, painter, widget);
}
