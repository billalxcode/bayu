#ifndef BAYUSTYLE_H
#define BAYUSTYLE_H

#include <QProxyStyle>

// Draws Qt Widgets controls the way bayu-toolkit draws its own: the same
// metrics, corner radii and colours (bayu-toolkit/src/ui/theme.cpp and
// toolkit_colors.cpp), over Fusion for everything else.
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
};

#endif // BAYUSTYLE_H
