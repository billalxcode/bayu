#ifndef BAYUSTYLE_H
#define BAYUSTYLE_H

#include <QProxyStyle>

// Draws Qt Widgets with the Bayu design system: its colour tokens, radii
// (6 tile, 8 control, 10 menu, 12 card), 4 px spacing rhythm and interaction
// states, over Fusion for anything it does not cover. The GTK theme in
// components/gtk-theme follows the same tokens.
//
// Applications can opt into a few variants with dynamic properties:
// QPushButton "primary" and "danger", QCheckBox "switch", QFrame named
// "Card", QWidget named "Row", and QLabel "badge", "keycap" and "banner".
class BayuStyle : public QProxyStyle
{
    Q_OBJECT

public:
    BayuStyle();

    void polish(QWidget *widget) override;
    void unpolish(QWidget *widget) override;
    using QProxyStyle::polish;
    using QProxyStyle::unpolish;

    int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr,
                    const QWidget *widget = nullptr) const override;
    int styleHint(StyleHint hint, const QStyleOption *option = nullptr, const QWidget *widget = nullptr,
                  QStyleHintReturn *returnData = nullptr) const override;
    QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &size,
                           const QWidget *widget) const override;
    QRect subControlRect(ComplexControl control, const QStyleOptionComplex *option, SubControl subControl,
                         const QWidget *widget) const override;
    QRect subElementRect(SubElement element, const QStyleOption *option, const QWidget *widget) const override;

    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
                       const QWidget *widget = nullptr) const override;
    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                     const QWidget *widget = nullptr) const override;
    void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter,
                            const QWidget *widget = nullptr) const override;

protected:
    bool eventFilter(QObject *object, QEvent *event) override;
};

#endif // BAYUSTYLE_H
