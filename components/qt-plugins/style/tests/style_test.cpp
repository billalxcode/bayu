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

    // Controls keep bayu-toolkit's sizes.
    void metrics()
    {
        QCOMPARE(QApplication::style()->pixelMetric(QStyle::PM_IndicatorWidth), 20);
        QVERIFY(QPushButton(QStringLiteral("OK")).sizeHint().height() >= 32);
        QVERIFY(QPushButton(QStringLiteral("OK")).sizeHint().width() >= 64);
        QVERIFY(QLineEdit().sizeHint().height() >= 35);
        QVERIFY(QComboBox().sizeHint().height() >= 35);
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
