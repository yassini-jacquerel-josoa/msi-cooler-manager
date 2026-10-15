#include "ThemeManager.h"

#include <QGuiApplication>
#include <QPalette>
#include <QProcess>
#include <QProcessEnvironment>

#include <pwd.h>
#include <cstring>

namespace msi::ui {

namespace {

const struct passwd *sessionUser()
{
    const QByteArray name = qgetenv("SUDO_USER");
    if (!name.isEmpty() && name != "root") {
        if (const struct passwd *pw = getpwnam(name.constData()))
            return pw;
    }

    QByteArray uidText = qgetenv("SUDO_UID");
    if (uidText.isEmpty())
        uidText = qgetenv("PKEXEC_UID");
    if (uidText.isEmpty())
        return nullptr;

    bool ok = false;
    const uint uid = uidText.toUInt(&ok);
    if (!ok || uid == 0)
        return nullptr;
    const struct passwd *pw = getpwuid(static_cast<uid_t>(uid));
    if (!pw || std::strcmp(pw->pw_name, "root") == 0)
        return nullptr;
    return pw;
}

QProcessEnvironment sessionEnvironment()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const struct passwd *pw = sessionUser();
    if (!pw)
        return env;

    const QString runtime = QStringLiteral("/run/user/%1").arg(pw->pw_uid);
    env.insert(QStringLiteral("XDG_RUNTIME_DIR"), runtime);
    env.insert(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"),
               QStringLiteral("unix:path=%1/bus").arg(runtime));
    env.insert(QStringLiteral("HOME"), QString::fromLocal8Bit(pw->pw_dir));
    env.insert(QStringLiteral("USER"), QString::fromLocal8Bit(pw->pw_name));
    return env;
}

void startUserProcess(QProcess *proc, const QStringList &gsettingsArgs)
{
    proc->setProcessEnvironment(sessionEnvironment());
    if (const struct passwd *pw = sessionUser()) {
        QStringList args{QStringLiteral("--preserve-environment"),
                         QStringLiteral("-u"),
                         QString::fromLocal8Bit(pw->pw_name),
                         QStringLiteral("--"),
                         QStringLiteral("gsettings")};
        args += gsettingsArgs;
        proc->start(QStringLiteral("runuser"), args);
        return;
    }
    proc->start(QStringLiteral("gsettings"), gsettingsArgs);
}

QString gsettingsGet(const QString &schema, const QString &key)
{
    QProcess proc;
    startUserProcess(&proc, {QStringLiteral("get"), schema, key});
    if (!proc.waitForFinished(800)) {
        proc.kill();
        proc.waitForFinished(50);
        return {};
    }
    return QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
}

} // namespace

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
    , m_isDark(true)
    , m_isSystemDark(true)
    , m_watch(new QProcess(this))
{
    connect(m_watch, &QProcess::readyReadStandardOutput, this, [this]() {
        m_watch->readAllStandardOutput();
        detectSystemTheme();
    });
    detectSystemTheme();
    startWatching();
}

void ThemeManager::setIsDark(bool dark)
{
    if (m_isDark == dark)
        return;
    m_isDark = dark;
    emit isDarkChanged(dark);
    emit themeChanged();
}

QColor ThemeManager::bgColor() const
{
    return m_isDark ? QColor(0, 0, 0, 158) : QColor(255, 255, 255, 210);
}

QColor ThemeManager::cardColor() const
{
    return m_isDark ? QColor(255, 255, 255, 26) : QColor(0, 0, 0, 13);
}

QColor ThemeManager::textColor() const
{
    return m_isDark ? QColor(QStringLiteral("#FFFFFF"))
                    : QColor(QStringLiteral("#1C1C1E"));
}

QColor ThemeManager::mutedTextColor() const
{
    return m_isDark ? QColor(255, 255, 255, 140) : QColor(28, 28, 30, 140);
}

void ThemeManager::detectSystemTheme()
{
    const QString scheme = gsettingsGet(QStringLiteral("org.gnome.desktop.interface"),
                                        QStringLiteral("color-scheme"));
    bool dark = false;
    bool known = false;
    if (scheme.contains(QStringLiteral("prefer-dark"))) {
        dark = true;
        known = true;
    } else if (scheme.contains(QStringLiteral("prefer-light"))
               || scheme.contains(QStringLiteral("default"))) {
        dark = false;
        known = true;
    }

    if (!known) {
        const QString gtk = gsettingsGet(QStringLiteral("org.gnome.desktop.interface"),
                                         QStringLiteral("gtk-theme"));
        if (gtk.contains(QStringLiteral("dark"), Qt::CaseInsensitive)) {
            dark = true;
            known = true;
        }
    }

    if (!known) {
        const QColor window = QGuiApplication::palette().color(QPalette::Window);
        dark = window.lightness() < 128;
    }

    if (m_isSystemDark != dark) {
        m_isSystemDark = dark;
        emit isSystemDarkChanged(dark);
    }
    setIsDark(dark);
}

void ThemeManager::startWatching()
{
    if (m_watch->state() != QProcess::NotRunning)
        return;
    startUserProcess(m_watch, {QStringLiteral("monitor"),
                               QStringLiteral("org.gnome.desktop.interface"),
                               QStringLiteral("color-scheme")});
}

} // namespace msi::ui
