#pragma once

#include <QObject>
#include <QKeySequence>
#include <QHash>
#include <functional>

namespace msi::ui {

/**
 * @brief Gestionnaire de raccourcis clavier globaux.
 *
 * Gère les raccourcis clavier applicatif (F7, etc.)
 * avec support du maintien long (long press).
 */
class ShortcutManager : public QObject
{
    Q_OBJECT

public:
    explicit ShortcutManager(QObject *parent = nullptr);
    ~ShortcutManager() override;

    void registerShortcut(const QString &id, const QKeySequence &sequence,
                          std::function<void()> callback);
    void unregisterShortcut(const QString &id);

    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    // Raccourcis par défaut
    void setupDefaultShortcuts();

signals:
    void shortcutTriggered(const QString &id);
    void toggleModeRequested();
    void expandRequested();
    void escapeRequested();

private:
    QHash<QString, QKeySequence> m_shortcuts;
    QHash<QString, std::function<void()>> m_callbacks;
    bool m_enabled;
};

} // namespace msi::ui
