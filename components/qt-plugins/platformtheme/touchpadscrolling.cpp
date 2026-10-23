// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "touchpadscrolling.h"

#include <scrollhold.h>
#include <widgetscrolling.h>

#include <QCoreApplication>
#include <QEvent>
#include <QLibrary>

void TouchpadScrolling::install()
{
    Bayu::ScrollHold::watch();
    Bayu::WidgetScrolling::install();
    new TouchpadScrolling;
}

TouchpadScrolling::TouchpadScrolling()
    : QObject(QCoreApplication::instance())
{
    QCoreApplication::instance()->installEventFilter(this);
}

bool TouchpadScrolling::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Wheel && watched->inherits("QQuickWindow")) {
        QCoreApplication::instance()->removeEventFilter(this);
        // Not while the application's event filters are being run.
        QMetaObject::invokeMethod(this, &TouchpadScrolling::loadQuickScrolling, Qt::QueuedConnection);
    }
    return false;
}

void TouchpadScrolling::loadQuickScrolling()
{
    QLibrary library(QStringLiteral("bayu-qt-scrolling-quick"));
    if (auto setAutomatic = reinterpret_cast<void (*)()>(
                library.resolve("bayu_scrolling_quick_set_automatic")))
        setAutomatic();
    deleteLater();
}
