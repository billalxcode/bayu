#include "platformtheme.h"
#include "filedialoghelper.h"
#include "touchpadscrolling.h"

#include <QApplication>
#include <QFont>
#include <QPalette>
#include <QString>
#include <QVariant>
#include <QWindow>

// Qt Private
#include <private/qiconloader_p.h>
#include <qpa/qwindowsysteminterface.h>

// The Bayu design system's light and dark tokens (gtk-theme/Bayu/colors-*.css,
// style/bayustyle.cpp): Window is $bg-window, Base $bg-card (views, menus,
// secondary buttons), Button $bg-card, Highlight $accent.
static QPalette bayuPalette(bool dark)
{
    const QColor window = dark ? QColor(0x1B1B1D) : QColor(0xF1F3F5);
    const QColor sidebar = dark ? QColor(0x222224) : QColor(0xF7F8FA);
    const QColor card = dark ? QColor(0x2C2C2E) : QColor(0xFFFFFF);
    const QColor field = dark ? QColor(0x3A3A3C) : QColor(0xEFF0F3);
    const QColor selected = dark ? QColor(0x3F3F43) : QColor(0xE2E4E9);
    const QColor text = dark ? QColor(0xF2F2F5) : QColor(0x17191D);
    const QColor tertiary = dark ? QColor(0x76767C) : QColor(0x9BA1AC);
    const QColor border = dark ? QColor(0x3A3A3C) : QColor(0xE1E4E9);
    const QColor accent = dark ? QColor(0x0A84FF) : QColor(0x0A7AFF);
    const QColor accentHover = dark ? QColor(0x2B95FF) : QColor(0x0B6BE0);

    QPalette palette(window);
    palette.setColor(QPalette::Window, window);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, card);
    palette.setColor(QPalette::AlternateBase, sidebar);
    palette.setColor(QPalette::ToolTipBase, dark ? field : text);
    palette.setColor(QPalette::ToolTipText, dark ? text : card);
    palette.setColor(QPalette::PlaceholderText, tertiary);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, card);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::BrightText, dark ? QColor(0xFF453A) : QColor(0xE8453C));
    palette.setColor(QPalette::Link, accent);
    palette.setColor(QPalette::LinkVisited, accentHover);
    palette.setColor(QPalette::Highlight, accent);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Accent, accent);
    palette.setColor(QPalette::Light, card);
    palette.setColor(QPalette::Midlight, field);
    palette.setColor(QPalette::Mid, border);
    palette.setColor(QPalette::Dark, tertiary);
    palette.setColor(QPalette::Shadow, QColor(0, 0, 0, dark ? 128 : 40));
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText, QPalette::HighlightedText,
                      QPalette::PlaceholderText})
        palette.setColor(QPalette::Disabled, role, tertiary);
    palette.setColor(QPalette::Disabled, QPalette::Button, field);
    palette.setColor(QPalette::Disabled, QPalette::Highlight, selected);
    return palette;
}

PlatformTheme::PlatformTheme()
    : m_hints(new HintsSettings)
{
    connect(m_hints, &HintsSettings::systemFontChanged, this, &PlatformTheme::onFontChanged);
    connect(m_hints, &HintsSettings::systemFontPointSizeChanged, this,
            &PlatformTheme::onFontChanged);
    connect(m_hints, &HintsSettings::iconThemeChanged, this, &PlatformTheme::onIconThemeChanged);
    connect(m_hints, &HintsSettings::cursorThemeChanged, this, [] {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        // Qt 6.10 has no the per-window overload; one call reaches every window.
        QWindowSystemInterface::handleThemeChange();
#else
        for (auto *window : qGuiApp->allWindows()) {
            QWindowSystemInterface::handleThemeChange(window);
        }
#endif
    });
    connect(m_hints, &HintsSettings::darkModeChanged, this, [] {
        QWindowSystemInterface::handleThemeChange();
    });

    QCoreApplication::setAttribute(Qt::AA_DontUseNativeMenuBar, false);
    // The application object is still being constructed.
    QMetaObject::invokeMethod(this, &TouchpadScrolling::install, Qt::QueuedConnection);
}

PlatformTheme::~PlatformTheme()
{
    delete m_hints;
}

QVariant PlatformTheme::themeHint(QPlatformTheme::ThemeHint hintType) const
{
    QVariant hint = m_hints->hint(hintType);
    if (hint.isValid()) {
        return hint;
    } else {
        return QGenericUnixTheme::themeHint(hintType);
    }
}

Qt::ColorScheme PlatformTheme::colorScheme() const
{
    return m_hints->darkMode() ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light;
}

const QPalette *PlatformTheme::palette(Palette type) const
{
    if (type == SystemPalette) {
        static const QPalette light = bayuPalette(false);
        static const QPalette dark = bayuPalette(true);
        return m_hints->darkMode() ? &dark : &light;
    }

    return QGenericUnixTheme::palette(type);
}

const QFont *PlatformTheme::font(Font type) const
{
    if (type == FixedFont) {
        const QString &fontName = m_hints->systemFixedFont();
        qreal fontSize = m_hints->systemFontPointSize();
        static QFont font = QFont(QString());
        font.setFamily(fontName);
        font.setPointSizeF(fontSize);
        return &font;
    }

    const QString &fontName = m_hints->systemFont();
    qreal fontSize = m_hints->systemFontPointSize();
    static QFont font = QFont(QString());
    font.setFamily(fontName);
    font.setPointSizeF(fontSize);
    return &font;
}

bool PlatformTheme::usePlatformNativeDialog(DialogType type) const
{
    return type == FileDialog;
}

QPlatformDialogHelper *PlatformTheme::createPlatformDialogHelper(DialogType type) const
{
    if (type == FileDialog)
        return new FileDialogHelper;
    return QGenericUnixTheme::createPlatformDialogHelper(type);
}

void PlatformTheme::onFontChanged()
{
    QFont font;
    font.setFamily(m_hints->systemFont());
    font.setPointSizeF(m_hints->systemFontPointSize());

    // Change font
    if (qobject_cast<QApplication *>(QCoreApplication::instance()))
        QApplication::setFont(font);
    else if (qobject_cast<QGuiApplication *>(QCoreApplication::instance()))
        QGuiApplication::setFont(font);
}

void PlatformTheme::onIconThemeChanged()
{
    QIconLoader::instance()->updateSystemTheme();

    QEvent update(QEvent::UpdateRequest);
    for (QWindow *window : qGuiApp->allWindows()) {
        if (window->type() == Qt::Desktop)
            continue;

        qApp->sendEvent(window, &update);
    }
}
