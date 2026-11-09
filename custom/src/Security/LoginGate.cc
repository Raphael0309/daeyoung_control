/****************************************************************************
 *
 * Daeyoung Ground Control - login gate (MIL edition)
 *
 ****************************************************************************/

#include "LoginGate.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QEventLoop>
#include <QtCore/QStandardPaths>
#include <QtCore/QSysInfo>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>

#ifndef KEVA_DISPLAY_NAME
#define KEVA_DISPLAY_NAME "Daeyoung Ground Control"
#endif
#ifndef KEVA_PRODUCT_VERSION
#define KEVA_PRODUCT_VERSION "1.0.1"
#endif
#ifndef KEVA_COMPANY_NAME
#define KEVA_COMPANY_NAME "Daeyoung M&S Co."
#endif

namespace KevaSecurity {

QString LoginGate::securityDirectory()
{
#ifdef Q_OS_WIN
    // Shared by every Windows user of this PC (one GCS = one account set).
    // The installer grants standard users write access to this folder.
    const QString base = qEnvironmentVariable("ProgramData", QStringLiteral("C:/ProgramData"));
    return QDir(base).filePath(QStringLiteral("DaeyoungMnS/%1/Security").arg(QCoreApplication::applicationName()));
#else
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(QStringLiteral("Security"));
#endif
}

AuthManager::Options LoginGate::defaultOptions()
{
    AuthManager::Options o;
    o.dataDirectory = securityDirectory();
    o.terminal = QSysInfo::machineHostName();
    o.productName = QStringLiteral(KEVA_DISPLAY_NAME);
    o.productVersion = QStringLiteral(KEVA_PRODUCT_VERSION);
    o.companyName = QStringLiteral(KEVA_COMPANY_NAME);
    o.eventSourceName = QStringLiteral(KEVA_DISPLAY_NAME);
    o.dictionaryPath = QStringLiteral(":/KevaSecurity/dictionary_words.txt");
    o.commonPasswordsPath = QStringLiteral(":/KevaSecurity/common_passwords.txt");
    return o;
}

bool LoginGate::run(AuthManager *auth)
{
    auth->start();

    // Closing the login window must not count as "last window closed" for the application.
    const bool quitOnLastWindowClosed = QGuiApplication::quitOnLastWindowClosed();
    QGuiApplication::setQuitOnLastWindowClosed(false);

    bool authenticated = false;
    QEventLoop loop;
    QObject::connect(auth, &AuthManager::finished, &loop, [&](bool ok) {
        authenticated = ok;
        loop.quit();
    });

    {
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty(QStringLiteral("auth"), auth);
        engine.load(QUrl(QStringLiteral("qrc:/KevaSecurity/LoginWindow.qml")));
        if (engine.rootObjects().isEmpty()) {
            qCritical() << "KevaSecurity: login window failed to load";
            QGuiApplication::setQuitOnLastWindowClosed(quitOnLastWindowClosed);
            return false;
        }
        loop.exec();
        // Leaving this scope destroys the engine and the login window.
    }

    QGuiApplication::setQuitOnLastWindowClosed(quitOnLastWindowClosed);
    return authenticated && auth->authenticated();
}

} // namespace KevaSecurity
