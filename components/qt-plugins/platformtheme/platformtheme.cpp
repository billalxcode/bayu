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

// Bayu's light and dark themes (bayu-toolkit/src/ui/theme.cpp): Window is the
// background, Base the second background (views, menus), Button the
// alternate background that buttons and fields are filled with.
static QPalette bayuPalette(bool dark)
{
    const QColor window = dark ? QColor(0x1C1C1D) : QColor(0xF5F6FA);
    const QColor base = dark ? QColor(0x2C2C2D) : QColor(0xFFFFFF);
    const QColor alternate = dark ? QColor(0x3C3C3D) : QColor(0xF2F4F5);
    const QColor text = dark ? QColor(0xFFFFFF) : QColor(0x323238);
    const QColor disabledText = dark ? QColor(0x888888) : QColor(0x64646E);
    const QColor highlight(0x3385FF);
    QColor placeholder = text;
    placeholder.setAlphaF(0.4);

    QPalette palette(window);
    palette.setColor(QPalette::Window, window);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, base);
    palette.setColor(QPalette::AlternateBase, alternate);
    palette.setColor(QPalette::ToolTipBase, base);
    palette.setColor(QPalette::ToolTipText, text);
    palette.setColor(QPalette::PlaceholderText, placeholder);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, alternate);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::BrightText, dark ? QColor(0xFF453A) : QColor(0xD92D20));
    palette.setColor(QPalette::Link, highlight);
    palette.setColor(QPalette::LinkVisited, highlight.darker(120));
    palette.setColor(QPalette::Highlight, highlight);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Accent, highlight);
    palette.setColor(QPalette::Light, alternate.lighter(150));
    palette.setColor(QPalette::Midlight, alternate.lighter(125));
    palette.setColor(QPalette::Mid, alternate.darker(125));
    palette.setColor(QPalette::Dark, alternate.darker(150));
    palette.setColor(QPalette::Shadow, Qt::black);
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText, QPalette::HighlightedText})
        palette.setColor(QPalette::Disabled, role, disabledText);
    palette.setColor(QPalette::Disabled, QPalette::Highlight, alternate);
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
