#include "ShortcutManager.h"

#include <QShortcut>
#include <QVector>

namespace msi::ui {

struct ShortcutData {
    QKeySequence sequence;
    std::function<void()> callback;
};

ShortcutManager::ShortcutManager(QObject *parent)
    : QObject(parent)
    , m_enabled(true)
{}

ShortcutManager::~ShortcutManager() = default;

void ShortcutManager::registerShortcut(const QString &id,
                                       const QKeySequence &sequence,
                                       std::function<void()> callback)
{
    m_shortcuts[id] = sequence;
    m_callbacks[id] = std::move(callback);
}

void ShortcutManager::unregisterShortcut(const QString &id)
{
    m_shortcuts.remove(id);
    m_callbacks.remove(id);
}

void ShortcutManager::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

void ShortcutManager::setupDefaultShortcuts()
{
    // F7 court = toggle mode
    registerShortcut(QStringLiteral("toggleMode"), QKeySequence(Qt::Key_F7), [this]() {
        emit toggleModeRequested();
    });

    // Échap = réduire / quitter
    registerShortcut(QStringLiteral("escape"), QKeySequence(Qt::Key_Escape), [this]() {
        emit escapeRequested();
    });
}

} // namespace msi::ui
