#pragma once

#include <QObject>
#include <QColor>

class QProcess;

namespace msi::ui {

/**
 * @brief Gestionnaire de thème clair/sombre.
 *
 * Détecte le thème système via gsettings et freedesktop portal,
 * expose les couleurs aux propriétés QML.
 */
class ThemeManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool isDark READ isDark WRITE setIsDark NOTIFY isDarkChanged)
    Q_PROPERTY(bool isSystemDark READ isSystemDark NOTIFY isSystemDarkChanged)
    Q_PROPERTY(QColor accentColor READ accentColor CONSTANT)
    Q_PROPERTY(QColor bgColor READ bgColor NOTIFY themeChanged)
    Q_PROPERTY(QColor cardColor READ cardColor NOTIFY themeChanged)
    Q_PROPERTY(QColor textColor READ textColor NOTIFY themeChanged)
    Q_PROPERTY(QColor mutedTextColor READ mutedTextColor NOTIFY themeChanged)
    Q_PROPERTY(QColor dangerColor READ dangerColor CONSTANT)

public:
    explicit ThemeManager(QObject *parent = nullptr);

    bool isDark() const { return m_isDark; }
    bool isSystemDark() const { return m_isSystemDark; }

    QColor accentColor() const { return QColor(QStringLiteral("#0A84FF")); }
    QColor bgColor() const;
    QColor cardColor() const;
    QColor textColor() const;
    QColor mutedTextColor() const;
    QColor dangerColor() const { return QColor(QStringLiteral("#FF453A")); }

    Q_INVOKABLE void detectSystemTheme();

public slots:
    void setIsDark(bool dark);

signals:
    void isDarkChanged(bool dark);
    void isSystemDarkChanged(bool dark);
    void themeChanged();

private:
    void startWatching();

    bool m_isDark;
    bool m_isSystemDark;
    QProcess *m_watch = nullptr;
};

} // namespace msi::ui
