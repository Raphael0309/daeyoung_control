/****************************************************************************
 *
 * Renders every stage of the login window to PNG files, or runs it
 * interactively, without building QGC.
 *
 *   login_preview <output-folder>   : walk all stages and save screenshots
 *   login_preview --interactive     : open the login window with a temporary account store
 *
 ****************************************************************************/

#include "AuthManager.h"

#include <QtCore/QDir>
#include <QtCore/QTemporaryDir>
#include <QtCore/QTimer>
#include <QtGui/QGuiApplication>
#include <QtGui/QImage>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickWindow>
#include <QtQuickControls2/QQuickStyle>
#include <QtTest/QTest>

using namespace KevaSecurity;

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("DaeyoungMnS-Preview"));
    QCoreApplication::setApplicationName(QStringLiteral("LoginPreview"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    const bool interactive = app.arguments().contains(QStringLiteral("--interactive"));
    const QString outDir = (!interactive && app.arguments().size() > 1) ? app.arguments().at(1) : QStringLiteral("login_preview_out");
    QDir().mkpath(outDir);

    QTemporaryDir dataDir;
    AuthManager::Options o;
    o.dataDirectory = dataDir.path();
    o.terminal = QStringLiteral("GCS-LAPTOP-01");
    o.productName = QStringLiteral("Daeyoung Ground Control");
    o.productVersion = QStringLiteral("1.0.1");
    o.companyName = QStringLiteral("Daeyoung M&S Co.");
    o.dictionaryPath = QStringLiteral(KEVA_TEST_DICTIONARY);
    o.commonPasswordsPath = QStringLiteral(KEVA_TEST_COMMON_PASSWORDS);
    o.iterations = 1000;
    o.useInstallSentinel = false;
    AuthManager auth(o);
    auth.start();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("auth"), &auth);
    engine.load(QUrl(QStringLiteral("qrc:/KevaSecurity/LoginWindow.qml")));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (interactive) {
        QObject::connect(&auth, &AuthManager::finished, &app, &QCoreApplication::quit);
        return app.exec();
    }

    window->showNormal();
    window->resize(1600, 900);
    int index = 0;
    auto snap = [&](const QString &name) {
        QTest::qWait(250);
        const QImage img = window->grabWindow();
        img.save(QDir(outDir).filePath(QStringLiteral("%1_%2.png").arg(++index, 2, 10, QLatin1Char('0')).arg(name)));
    };

    const QString a1 = QStringLiteral("secoff1"), a2 = QStringLiteral("syseng2"), u = QStringLiteral("pilotKim");
    const QString p1 = QStringLiteral("Kv7#mQ2!pZx"), p2 = QStringLiteral("Tn4$wR8@jLb"), pu = QStringLiteral("Hq5&zM3^vYc");

    snap(QStringLiteral("setup_admin"));
    auth.createAccount(QStringLiteral("admin"), QStringLiteral("Password1"), QStringLiteral("Password1"));
    snap(QStringLiteral("setup_admin_bad_id"));
    auth.createAccount(a1, QStringLiteral("drone123"), QStringLiteral("drone123"));
    snap(QStringLiteral("setup_admin_bad_pw"));
    auth.createAccount(a1, p1, p1);
    snap(QStringLiteral("setup_admin_2"));
    auth.createAccount(a2, p2, p2);
    snap(QStringLiteral("recovery_code"));
    auth.acknowledgeRecoveryCode();
    snap(QStringLiteral("setup_user"));
    auth.createAccount(u, pu, pu);
    snap(QStringLiteral("login"));
    auth.login(u, QStringLiteral("wrong"));
    snap(QStringLiteral("login_failed"));
    auth.login(u, QStringLiteral("wrong"));
    auth.login(u, QStringLiteral("wrong"));
    snap(QStringLiteral("login_locked"));
    auth.showRecovery();
    snap(QStringLiteral("recovery"));
    auth.backToLogin();
    auth.login(a1, p1);
    snap(QStringLiteral("notice_admin"));
    auth.showAdminTools();
    snap(QStringLiteral("admin_tools"));
    auth.closeAdminTools();
    auth.requestPasswordChange();
    snap(QStringLiteral("change_password"));
    return 0;
}
