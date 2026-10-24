#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QStyleOptionSlider>
#include <QTest>

#include "bayustyle.h"

class StyleTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QApplication::setStyle(new BayuStyle);
    }

    // Controls keep the design system's sizes: 36 px buttons and fields, an
    // 18 px check box with room for its focus ring, 44 x 26 switches.
    void metrics()
    {
        QStyle *style = QApplication::style();
        QCOMPARE(style->pixelMetric(QStyle::PM_IndicatorWidth), 26);
        QCOMPARE(QPushButton(QStringLiteral("OK")).sizeHint().height(), 36);
        QVERIFY(QPushButton(QStringLiteral("OK")).sizeHint().width() >= 64);
        QCOMPARE(QLineEdit().sizeHint().height(), 36);
        QCOMPARE(QComboBox().sizeHint().height(), 36);
        QCheckBox toggle;
        toggle.setProperty("switch", true);
        QCOMPARE(style->pixelMetric(QStyle::PM_IndicatorWidth, nullptr, &toggle), 52);
        QCOMPARE(style->pixelMetric(QStyle::PM_IndicatorHeight, nullptr, &toggle), 34);
    }

    // Scroll bars have no arrow buttons and their handle tracks the value.
    void scrollBar()
    {
        QScrollBar bar(Qt::Vertical);
        bar.setRange(0, 100);
        bar.setPageStep(100);
        bar.resize(10, 200);
        QStyleOptionSlider option;
        option.initFrom(&bar);
        option.orientation = Qt::Vertical;
        option.minimum = 0;
        option.maximum = 100;
        option.pageStep = 100;
        option.rect = bar.rect();
        QStyle *style = QApplication::style();
        QVERIFY(style->subControlRect(QStyle::CC_ScrollBar, &option, QStyle::SC_ScrollBarAddLine).isEmpty());
        QVERIFY(style->subControlRect(QStyle::CC_ScrollBar, &option, QStyle::SC_ScrollBarSubLine).isEmpty());

        option.sliderPosition = 0;
        const QRect top = style->subControlRect(QStyle::CC_ScrollBar, &option, QStyle::SC_ScrollBarSlider);
        QCOMPARE(top.top(), 0);
        QCOMPARE(top.height(), 100); // half the length: a page of two
        option.sliderPosition = 100;
        const QRect bottom = style->subControlRect(QStyle::CC_ScrollBar, &option, QStyle::SC_ScrollBarSlider);
        QCOMPARE(bottom.bottom(), 199);
    }
};

QTEST_MAIN(StyleTest)
#include "style_test.moc"
