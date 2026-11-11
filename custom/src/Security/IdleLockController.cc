/****************************************************************************
 *
 * Daeyoung Ground Control - idle session lock (MIL edition)
 *
 ****************************************************************************/

#include "IdleLockController.h"
#include "AuthManager.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QEvent>
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtGui/QWindow>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>

namespace KevaSecurity {

IdleLockController::IdleLockController(AuthManager *auth, qint64 idleMilliseconds, const Hooks &hooks, QObject *parent)
    : QObject(parent)
    , _auth(auth)
    , _idleLimitMs(idleMilliseconds)
    , _hooks(hooks)
{
    _checkTimer.setInterval(1000);
    connect(&_checkTimer, &QTimer::timeout, this, &IdleLockController::_check);
    connect(_auth, &AuthManager::sessionLockedChanged, this, &IdleLockController::_onLockChanged);
}

IdleLockController::~IdleLockController()
{
    if (qApp) {
        qApp->removeEventFilter(this);
    }
}

void IdleLockController::start()
{
    _idle.start();
    qApp->installEventFilter(this);
    _checkTimer.start();
}

bool IdleLockController::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::KeyPress:
    case QEvent::MouseButtonPress:
    case QEvent::MouseMove:
    case QEvent::Wheel:
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TabletPress:
        if (!_auth->sessionLocked()) {
            _idle.restart();
        }
        break;
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

void IdleLockController::_check()
{
    if (_auth->sessionLocked() || !_auth->authenticated()) {
        return;
    }
    if (_hooks.lockSuppressed && _hooks.lockSuppressed()) {
        // Never lock the operator out while a vehicle is armed.
        _idle.restart();
        return;
    }
    if (_idle.elapsed() >= _idleLimitMs) {
        lockNow(QStringLiteral("Session locked after %1 minutes of inactivity.").arg(qMax<qint64>(1, _idleLimitMs / 60000)));
    }
}

void IdleLockController::lockNow(const QString &reason)
{
    _auth->lockSession(reason);
}

void IdleLockController::_onLockChanged()
{
    if (_auth->sessionLocked()) {
        _showLockWindow();
    } else {
        _hideLockWindow();
        _idle.restart();
    }
}

void IdleLockController::_showLockWindow()
{
    if (_engine) {
        return;
    }
    _engine = std::make_unique<QQmlApplicationEngine>();
    _engine->rootContext()->setContextProperty(QStringLiteral("auth"), _auth);
    _engine->load(QUrl(QStringLiteral("qrc:/KevaSecurity/LockWindow.qml")));
    if (_engine->rootObjects().isEmpty()) {
        qCritical() << "KevaSecurity: lock window failed to load";
        return;
    }
    auto *window = qobject_cast<QWindow *>(_engine->rootObjects().first());
    QWindow *reference = _hooks.referenceWindow ? _hooks.referenceWindow() : nullptr;
    if (window) {
        if (reference && reference->screen()) {
            window->setScreen(reference->screen());
            window->setGeometry(reference->screen()->geometry());
        }
        window->showFullScreen();
        window->raise();
        window->requestActivate();
    }
}

void IdleLockController::_hideLockWindow()
{
    _engine.reset();
}

} // namespace KevaSecurity
