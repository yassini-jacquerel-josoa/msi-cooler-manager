#include "DesktopShortcuts.h"

#include <QProcess>
#include <QRegularExpression>

namespace msi::ui {

namespace {

constexpr auto kWmSchema = "org.gnome.desktop.wm.keybindings";
constexpr auto kMoveKey = "begin-move";
constexpr auto kLegacyMoveKey = "move-window";
constexpr auto kMediaSchema = "org.gnome.settings-daemon.plugins.media-keys";
constexpr auto kCustomListKey = "custom-keybindings";
constexpr auto kCustomSchema = "org.gnome.settings-daemon.plugins.media-keys.custom-keybinding";
constexpr auto kCustomPath =
    "/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/msi-cooler-manager/";
constexpr auto kAppBinding = "<Alt>F7";
constexpr auto kMoveReplacement = "<Control><Alt>m";
constexpr auto kShortcutName = "MSI Cooler Manager";

struct GsettingsResult {
    bool ok = false;
    QString text;
};

struct MoveRebind {
    bool ok = false;
    bool changed = false;
    QString key;
    QString previous;
};

bool usesF7(const QString &binding)
{
    static const QRegularExpression pattern(
        QStringLiteral("(?:^|>)F7$"),
        QRegularExpression::CaseInsensitiveOption);
    return pattern.match(binding.trimmed()).hasMatch();
}

QStringList parseStringArray(const QString &raw)
{
    const QString text = raw.trimmed();
    if (text.isEmpty() || text.startsWith(QLatin1Char('@')))
        return {};

    QStringList values;
    static const QRegularExpression pattern(QStringLiteral("'([^']*)'|\"([^\"]*)\""));
    auto match = pattern.globalMatch(text);
    while (match.hasNext()) {
        const QRegularExpressionMatch found = match.next();
        const QString singleQuoted = found.captured(1);
        const QString doubleQuoted = found.captured(2);
        values.append(found.capturedStart(1) >= 0 ? singleQuoted : doubleQuoted);
    }
    return values;
}

QString formatString(const QString &value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escaped.replace(QLatin1Char('\''), QStringLiteral("\\'"));
    return QStringLiteral("'") + escaped + QStringLiteral("'");
}

QString formatStringArray(const QStringList &values)
{
    QStringList quoted;
    quoted.reserve(values.size());
    for (const QString &value : values)
        quoted.append(formatString(value));
    return QStringLiteral("[") + quoted.join(QStringLiteral(", ")) + QStringLiteral("]");
}

bool gnomeSession()
{
    const QString desktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    const QString session = qEnvironmentVariable("XDG_SESSION_DESKTOP");
    const auto supported = [](const QString &value) {
        return value.contains(QLatin1String("GNOME"), Qt::CaseInsensitive)
               || value.contains(QLatin1String("ubuntu"), Qt::CaseInsensitive);
    };
    if (desktop.isEmpty() && session.isEmpty())
        return true;
    return supported(desktop) || supported(session);
}

GsettingsResult runGsettings(const QStringList &args)
{
    QProcess process;
    process.start(QStringLiteral("gsettings"), args);
    if (!process.waitForStarted(1000) || !process.waitForFinished(2000))
        return {};

    const QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    const QString error = QString::fromUtf8(process.readAllStandardError()).trimmed();
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        return {false, error.isEmpty() ? output : error};
    return {true, output};
}

GsettingsResult readKey(const QString &schema, const QString &key, const QString &path = {})
{
    const QString target = path.isEmpty() ? schema : schema + QLatin1Char(':') + path;
    return runGsettings({QStringLiteral("get"), target, key});
}

bool writeKey(const QString &schema, const QString &key, const QString &value, const QString &path = {})
{
    const QString target = path.isEmpty() ? schema : schema + QLatin1Char(':') + path;
    return runGsettings({QStringLiteral("set"), target, key, value}).ok;
}

QString toggleCommand(const QString &executablePath)
{
    QString escaped = executablePath;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QStringLiteral("\"") + escaped + QStringLiteral("\" --toggle");
}

MoveRebind rebindMoveShortcut()
{
    GsettingsResult current = readKey(QLatin1String(kWmSchema), QLatin1String(kMoveKey));
    QString key = QLatin1String(kMoveKey);
    if (!current.ok) {
        current = readKey(QLatin1String(kWmSchema), QLatin1String(kLegacyMoveKey));
        key = QLatin1String(kLegacyMoveKey);
    }
    if (!current.ok)
        return {};

    const QStringList bindings = parseStringArray(current.text);
    QStringList kept;
    bool removed = false;
    for (const QString &binding : bindings) {
        if (usesF7(binding))
            removed = true;
        else
            kept.append(binding);
    }
    if (!removed)
        return {true, false, key, {}};

    if (kept.isEmpty())
        kept.append(QLatin1String(kMoveReplacement));

    if (!writeKey(QLatin1String(kWmSchema), key, formatStringArray(kept)))
        return {false, false, key, {}};

    return {true, true, key, current.text};
}

bool installCustomShortcut(const QString &command)
{
    const QString path = QLatin1String(kCustomPath);
    const GsettingsResult list = readKey(QLatin1String(kMediaSchema), QLatin1String(kCustomListKey));
    if (!list.ok)
        return false;

    QStringList paths = parseStringArray(list.text);
    if (!paths.contains(path)) {
        paths.append(path);
        if (!writeKey(QLatin1String(kMediaSchema), QLatin1String(kCustomListKey), formatStringArray(paths)))
            return false;
    }

    return writeKey(QLatin1String(kCustomSchema), QStringLiteral("name"),
                    formatString(QLatin1String(kShortcutName)), path)
           && writeKey(QLatin1String(kCustomSchema), QStringLiteral("command"),
                       formatString(command), path)
           && writeKey(QLatin1String(kCustomSchema), QStringLiteral("binding"),
                       formatString(QLatin1String(kAppBinding)), path);
}

bool removeCustomShortcut()
{
    const QString path = QLatin1String(kCustomPath);
    const GsettingsResult list = readKey(QLatin1String(kMediaSchema), QLatin1String(kCustomListKey));
    if (!list.ok)
        return false;

    QStringList paths = parseStringArray(list.text);
    if (!paths.contains(path))
        return true;

    paths.removeAll(path);
    const QString formatted = paths.isEmpty() ? QStringLiteral("[]") : formatStringArray(paths);
    return writeKey(QLatin1String(kMediaSchema), QLatin1String(kCustomListKey), formatted);
}

} // namespace

QString DesktopShortcuts::claimAltF7(const QString &executablePath)
{
    if (executablePath.isEmpty() || !gnomeSession())
        return {};

    rebindMoveShortcut();
    removeCustomShortcut();
    return {};
}

bool DesktopShortcuts::installToggleCommand(const QString &executablePath)
{
    if (executablePath.isEmpty() || !gnomeSession())
        return false;
    return installCustomShortcut(toggleCommand(executablePath));
}

} // namespace msi::ui
