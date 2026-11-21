#include <QApplication>
#include <QFont>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQml>
#include <QLocalServer>
#include <QLocalSocket>

#include <cstdio>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "core/CoolerBoostController.h"
#include "core/TemperatureMonitor.h"
#include "core/SettingsManager.h"
#include "core/HardwareAbstraction.h"
#include "db/DatabaseManager.h"
#include "db/MetricsRepository.h"
#include "ui/ThemeManager.h"
#include "ui/TrayManager.h"
#include "ui/DesktopShortcuts.h"
#include "ui/InputMask.h"
#include "ui/GlobalHotkey.h"
#include "ui/BackdropBlur.h"

using namespace msi;

namespace {

// L'instance root est lancée via sudo ou pkexec (README et lanceur .desktop) :
// ces outils exposent l'UID de la session dans l'environnement. Sans valeur
// exploitable, seul root est autorisé.
uid_t sessionUid()
{
    bool ok = false;
    const int sudoUid = qEnvironmentVariableIntValue("SUDO_UID", &ok);
    if (ok && sudoUid > 0)
        return static_cast<uid_t>(sudoUid);
    const int pkexecUid = qEnvironmentVariableIntValue("PKEXEC_UID", &ok);
    if (ok && pkexecUid > 0)
        return static_cast<uid_t>(pkexecUid);
    return static_cast<uid_t>(-1);
}

// Qt 6.4 n'expose pas QLocalSocket::peerCredentials() : les identifiants du
// pair sont lus directement via SO_PEERCRED sur le descripteur Unix.
uid_t peerUid(QLocalSocket *socket, bool &ok)
{
    ok = false;
    if (socket->socketDescriptor() < 0)
        return static_cast<uid_t>(-1);

    struct ucred credentials{};
    socklen_t length = sizeof(credentials);
    if (::getsockopt(static_cast<int>(socket->socketDescriptor()), SOL_SOCKET, SO_PEERCRED,
                     &credentials, &length) != 0
        || length < sizeof(credentials)) {
        return static_cast<uid_t>(-1);
    }
    ok = true;
    return credentials.uid;
}

} // namespace

int main(int argc, char *argv[])
{
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setAlphaBufferSize(8);
    QSurfaceFormat::setDefaultFormat(format);
    QQuickWindow::setDefaultAlphaBuffer(true);

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("MSI Cooler Manager"));
    app.setOrganizationName(QStringLiteral("msi"));
    QFont appFont(QStringLiteral("Ubuntu"));
    appFont.setWeight(QFont::Normal);
    app.setFont(appFont);

    // ── Une seule instance ─────────────────────────────────────────────────
    const QString serverName = QStringLiteral("msi_cooler_manager_ipc");
    QString pendingCommand;

    const auto commandFromArg = [](const QString &arg) -> QString {
        if (arg == QLatin1String("--toggle") || arg == QLatin1String("-t"))
            return QStringLiteral("toggle");
        if (arg == QLatin1String("--auto"))
            return QStringLiteral("auto");
        if (arg == QLatin1String("--manual"))
            return QStringLiteral("manual");
        if (arg == QLatin1String("--expand"))
            return QStringLiteral("expand");
        if (arg == QLatin1String("--quit"))
            return QStringLiteral("quit");
        return {};
    };

    for (int i = 1; i < argc; ++i) {
        const QString command = commandFromArg(QString::fromLocal8Bit(argv[i]));
        if (!command.isEmpty())
            pendingCommand = command;
    }

    const auto sendToRunning = [&](QLocalSocket &socket) {
        if (pendingCommand.isEmpty()) {
            std::fprintf(stderr, "MSI Cooler Manager est déjà lancé.\n");
            return 1;
        }
        socket.write(pendingCommand.toUtf8());
        socket.waitForBytesWritten(200);
        return 0;
    };

    {
        QLocalSocket socket;
        socket.connectToServer(serverName);
        if (socket.waitForConnected(300))
            return sendToRunning(socket);
    }

    // Aucune instance n'écoute : seules les commandes CLI sont traitées avant
    // ce point. Le contrôle matériel et le GUI ne démarrent qu'en root.
    if (geteuid() != 0) {
        std::fprintf(stderr, "MSI Cooler Manager doit être lancé avec sudo.\n");
        return 1;
    }

    const uid_t allowedSessionUid = sessionUid();

    // Contrôle juste avant removeServer() : si une instance est apparue entre
    // temps, on lui transmet la commande plutôt que de lui prendre le socket.
    {
        QLocalSocket socket;
        socket.connectToServer(serverName);
        if (socket.waitForConnected(300))
            return sendToRunning(socket);
    }

    QLocalServer::removeServer(serverName);
    QLocalServer *server = new QLocalServer(&app);
    // umask 0177 : le socket Unix naît directement en 0600 (0777 & ~0177),
    // sans intervalle où il serait accessible à d'autres utilisateurs.
    const mode_t previousUmask = umask(0177);
    const bool listening = server->listen(serverName);
    umask(previousUmask);
    if (!listening) {
        QLocalSocket socket;
        socket.connectToServer(serverName);
        if (socket.waitForConnected(300))
            return sendToRunning(socket);
        std::fprintf(stderr, "Impossible de démarrer MSI Cooler Manager.\n");
        return 1;
    }

    // Root peut toujours se connecter ; l'utilisateur de session (sudo/pkexec)
    // devient propriétaire du socket 0600 pour envoyer ses propres commandes.
    if (allowedSessionUid != static_cast<uid_t>(-1)) {
        const QByteArray socketPath = server->fullServerName().toUtf8();
        if (::chown(socketPath.constData(), allowedSessionUid, static_cast<gid_t>(-1)) != 0) {
            qWarning().noquote() << "IPC: impossible de donner le socket à l'utilisateur de session.";
        }
    }

    // F7 ne doit plus être pris par « Déplacer la fenêtre » ni par un raccourci GNOME.
    ui::DesktopShortcuts::claimAltF7(QCoreApplication::applicationFilePath());
    QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
        ui::DesktopShortcuts::restore();
    });
    ui::GlobalHotkey hotkey;
    const bool globalHotkey = hotkey.install();
    if (!globalHotkey)
        ui::DesktopShortcuts::installToggleCommand(QCoreApplication::applicationFilePath());

    // ── Composants ────────────────────────────────────────────────────────
    auto hw = std::make_unique<core::SysfsHardware>();
    core::CoolerBoostController controller(std::move(hw));
    core::TemperatureMonitor tempMonitor;
    core::SettingsManager settings;
    db::DatabaseManager dbManager;
    ui::ThemeManager themeManager;
    ui::TrayManager trayManager;
    ui::InputMask inputMask;

    // Charger les paramètres sauvegardés
    controller.setCpuThreshold(settings.cpuThreshold());
    controller.setGpuThreshold(settings.gpuThreshold());
    QObject::connect(&controller, &core::CoolerBoostController::modeChanged,
                     &settings, [&settings](const QString &mode) {
                         settings.setDefaultMode(mode);
                         settings.sync();
                     });
    QObject::connect(&controller, &core::CoolerBoostController::thresholdsChanged,
                     &settings, [&settings](int cpu, int gpu) {
                         settings.setCpuThreshold(cpu);
                         settings.setGpuThreshold(gpu);
                         settings.sync();
                     });
    if (settings.defaultMode() == QLatin1String("MANUEL"))
        controller.restoreMode(core::CoolerBoostController::Mode::Manual);
    if (pendingCommand == QLatin1String("toggle"))
        controller.toggleMode();
    else if (pendingCommand == QLatin1String("auto"))
        controller.setMode(QStringLiteral("AUTO"));
    else if (pendingCommand == QLatin1String("manual"))
        controller.setMode(QStringLiteral("MANUEL"));

    // Historique : recharger la base, puis enregistrer chaque nouvelle mesure.
    QObject::connect(&dbManager, &db::DatabaseManager::errorOccurred,
                     [](const QString &msg) { qWarning() << "Database error:" << msg; });
    if (dbManager.initialize()) {
        tempMonitor.setHistorySize(settings.historySize());
        db::MetricsRepository history(QSqlDatabase::database());
        const auto saved = history.getHistory(tempMonitor.historySize());
        for (const auto &entry : saved)
            tempMonitor.appendRestored(entry.timestamp, entry.cpuTemp, entry.gpuTemp);
    } else {
        qWarning() << "Database init failed:" << dbManager.databasePath()
                   << "(l'appli continue sans historique)";
    }

    QObject::connect(&controller, &core::CoolerBoostController::temperaturesChanged,
                     &tempMonitor, &core::TemperatureMonitor::addReading);
    QObject::connect(&controller, &core::CoolerBoostController::temperaturesChanged,
                     &dbManager, [&dbManager, &controller](int cpu, int gpu) {
                         dbManager.insertTemperature(cpu, gpu, controller.mode());
                     });

    // Le monitoring démarre immédiatement dans tous les modes, y compris en
    // MANUEL restauré : la fenêtre peut ne jamais être révélée par l'utilisateur.
    controller.start();

    QObject *mainWindow = nullptr;
    QObject::connect(server, &QLocalServer::newConnection, [&]() {
        QLocalSocket *client = server->nextPendingConnection();
        if (!client) return;
        QObject::connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);

        bool peerKnown = false;
        const uid_t peer = peerUid(client, peerKnown);
        const bool authorized = peerKnown
                                && (peer == 0
                                    || (allowedSessionUid != static_cast<uid_t>(-1)
                                        && peer == allowedSessionUid));
        if (!authorized) {
            qWarning().noquote() << "IPC: connexion refusée (pair non autorisé).";
            client->disconnectFromServer();
            return;
        }

        QObject::connect(client, &QLocalSocket::readyRead, [client, &controller, &mainWindow]() {
            const QString cmd = QString::fromUtf8(client->readAll()).trimmed();
            if (cmd == QLatin1String("toggle")) {
                if (mainWindow)
                    QMetaObject::invokeMethod(mainWindow, "handleShortF7");
                else
                    controller.toggleMode();
            } else if (cmd == QLatin1String("auto"))
                controller.setMode(QStringLiteral("AUTO"));
            else if (cmd == QLatin1String("manual"))
                controller.setMode(QStringLiteral("MANUEL"));
            else if (cmd == QLatin1String("expand")) {
                if (mainWindow)
                    QMetaObject::invokeMethod(mainWindow, "handleLongF7");
            } else if (cmd == QLatin1String("quit")) {
                bool quitPeerKnown = false;
                if (peerUid(client, quitPeerKnown) == 0 && quitPeerKnown)
                    QCoreApplication::quit();
                else
                    qWarning().noquote() << "IPC: commande 'quit' refusée (pair non root).";
            }
            client->disconnectFromServer();
        });
    });

    // ── Moteur QML ────────────────────────────────────────────────────────
    qmlRegisterType<ui::BackdropBlur>("MSI.Effects", 1, 0, "BackdropBlur");

    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty(QStringLiteral("controller"), &controller);
    engine.rootContext()->setContextProperty(QStringLiteral("tempMonitor"), &tempMonitor);
    engine.rootContext()->setContextProperty(QStringLiteral("settings"), &settings);
    engine.rootContext()->setContextProperty(QStringLiteral("themeHelper"), &themeManager);
    engine.rootContext()->setContextProperty(QStringLiteral("trayManager"), &trayManager);
    engine.rootContext()->setContextProperty(QStringLiteral("inputMask"), &inputMask);
    engine.rootContext()->setContextProperty(QStringLiteral("globalHotkey"), globalHotkey);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(-1); },
                     Qt::QueuedConnection);

    engine.load(QUrl(QStringLiteral("qrc:/MSI/CoolerManager/src/ui/Main.qml")));

    if (engine.rootObjects().isEmpty()) {
        qCritical() << "QML load failed: no root objects";
        ui::DesktopShortcuts::restore();
        return -1;
    }

    trayManager.show();

    mainWindow = engine.rootObjects().constFirst();
    QObject::connect(&trayManager, &ui::TrayManager::quitRequested, &app, &QApplication::quit);
    QObject::connect(&trayManager, &ui::TrayManager::showWindowRequested, mainWindow, [mainWindow]() {
        QMetaObject::invokeMethod(mainWindow, "showMini");
    });
    QObject::connect(&trayManager, &ui::TrayManager::toggleModeRequested,
                     &controller, &core::CoolerBoostController::toggleMode);

    if (globalHotkey) {
        QObject::connect(&hotkey, &ui::GlobalHotkey::holdStarted, mainWindow, [mainWindow]() {
            QMetaObject::invokeMethod(mainWindow, "handleF7Down");
        });
        QObject::connect(&hotkey, &ui::GlobalHotkey::toggleRequested, mainWindow, [mainWindow]() {
            QMetaObject::invokeMethod(mainWindow, "handleShortF7");
        });
        QObject::connect(&hotkey, &ui::GlobalHotkey::expandRequested, mainWindow, [mainWindow]() {
            QMetaObject::invokeMethod(mainWindow, "handleLongF7");
        });
    }

    return app.exec();
}
