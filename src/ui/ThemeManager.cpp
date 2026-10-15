#include "ThemeManager.h"

#include <QProcess>

namespace msi::ui {

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
    , m_isDark(true)
    , m_isSystemDark(true)
{
}

void ThemeManager::setIsDark(bool dark)
{
    if (m_isDark == dark)
        return;
    m_isDark = dark;
    emit isDarkChanged(dark);
    emit themeChanged();
}

void ThemeManager::toggle()
{
    setIsDark(!m_isDark);
}

QColor ThemeManager::bgColor() const
{
    return QColor(0, 0, 0, 158);
}

QColor ThemeManager::cardColor() const
{
    return QColor(255, 255, 255, 26);
}

QColor ThemeManager::textColor() const
{
    return QColor(QStringLiteral("#FFFFFF"));
}

QColor ThemeManager::mutedTextColor() const
{
    return QColor(QStringLiteral("#8E8E93"));
}

void ThemeManager::detectSystemTheme()
{
    bool dark = false;

    // Méthode 1 : Palette Qt
    QColor winColor = QGuiApplication::palette().color(QPalette::Window);
    if (winColor.lightness() < 128)
        dark = true;

    // Méthode 2 : gsettings (GNOME/Ubuntu)
    QProcess proc;
    proc.start(QStringLiteral("gsettings"),
               QStringList{QStringLiteral("get"),
                           QStringLiteral("org.gnome.shell.ubuntu"),
                           QStringLiteral("color-scheme")});
    if (proc.waitForFinished(100)) {
        QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        if (out.contains(QStringLiteral("prefer-dark")))
            dark = true;
        else if (out.contains(QStringLiteral("prefer-light")) ||
                 out.contains(QStringLiteral("default")))
            dark = false;
    } else {
        proc.kill();
        proc.waitForFinished(30);
    }

    // Méthode 3 : XDG Desktop Portal
    if (!dark) {
        QProcess portalProc;
        portalProc.start(QStringLiteral("busctl"),
                        QStringList{QStringLiteral("--user"), QStringLiteral("call"),
                                    QStringLiteral("org.freedesktop.portal.Desktop"),
                                    QStringLiteral("/org/freedesktop/portal/desktop"),
                                    QStringLiteral("org.freedesktop.portal.Settings"),
                                    QStringLiteral("Read"), QStringLiteral("ss"),
                                    QStringLiteral("org.freedesktop.appearance"),
                                    QStringLiteral("color-scheme")});
        if (portalProc.waitForFinished(100)) {
            QString out = QString::fromUtf8(portalProc.readAllStandardOutput());
            if (out.contains(QLatin1String(" 1")))
                dark = true;
        } else {
            portalProc.kill();
            portalProc.waitForFinished(30);
        }
    }

    m_isSystemDark = dark;
    emit isSystemDarkChanged(dark);
    setIsDark(dark);
}

} // namespace msi::ui
