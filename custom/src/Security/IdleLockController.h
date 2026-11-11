/****************************************************************************
 *
 * Daeyoung Ground Control - idle session lock (MIL edition)
 *
 * After a period without keyboard, mouse or touch input the session is
 * locked and a full screen, application modal lock window asks for the
 * current user's password. The lock is not applied while any vehicle is
 * armed (decided 2026-11-09: no lock while flying).
 *
 ****************************************************************************/

#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>
#include <QtCore/QTimer>

#include <functional>
#include <memory>

class QQmlApplicationEngine;
class QWindow;

namespace KevaSecurity {

class AuthManager;

class IdleLockController : public QObject
{
    Q_OBJECT

public:
    struct Hooks
    {
        std::function<bool()>     lockSuppressed;   // true while a vehicle is armed / flying
        std::function<QWindow*()> referenceWindow;  // main window, used to pick the screen
    };

    IdleLockController(AuthManager *auth, qint64 idleMilliseconds, const Hooks &hooks, QObject *parent = nullptr);
    ~IdleLockController() override;

    void start();
    qint64 idleMilliseconds() const { return _idle.elapsed(); }

    /// Locks immediately (also used by tests and a future "Lock now" button).
    void lockNow(const QString &reason);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void _check();
    void _onLockChanged();
    void _showLockWindow();
    void _hideLockWindow();

    AuthManager *_auth;
    qint64 _idleLimitMs;
    Hooks _hooks;
    QElapsedTimer _idle;
    QTimer _checkTimer;
    std::unique_ptr<QQmlApplicationEngine> _engine;
};

} // namespace KevaSecurity
