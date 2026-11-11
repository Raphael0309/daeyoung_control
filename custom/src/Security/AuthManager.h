/****************************************************************************
 *
 * Daeyoung Ground Control - authentication and account management (MIL edition)
 *
 * Drives the login window (LoginWindow.qml) through a small set of stages:
 *
 *   setupAdmin -> recoveryCode -> setupUser -> login -> [changePassword] -> notice -> (application)
 *                                              login <-> recovery
 *                                              notice <-> adminTools
 *   fatal: security data missing, damaged or crypto self test failed
 *
 ****************************************************************************/

#pragma once

#include "AccountStore.h"
#include "AuditLog.h"
#include "PasswordPolicy.h"

#include <QtCore/QDateTime>
#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QVariantList>

#include <functional>
#include <memory>

namespace KevaSecurity {

class AuthManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString      stage           READ stage          NOTIFY stageChanged)
    Q_PROPERTY(QString      title           READ title          NOTIFY stageChanged)
    Q_PROPERTY(QString      infoText        READ infoText       NOTIFY stageChanged)
    Q_PROPERTY(QString      recoveryCode    READ recoveryCode   NOTIFY stageChanged)
    Q_PROPERTY(QString      auditWarning    READ auditWarning   NOTIFY stageChanged)
    Q_PROPERTY(QString      message         READ message        NOTIFY messageChanged)
    Q_PROPERTY(QStringList  messages        READ messages       NOTIFY messageChanged)
    Q_PROPERTY(QString      currentUser     READ currentUser    NOTIFY sessionChanged)
    Q_PROPERTY(bool         isAdmin         READ isAdmin        NOTIFY sessionChanged)
    Q_PROPERTY(bool         authenticated   READ authenticated  NOTIFY sessionChanged)
    Q_PROPERTY(QVariantList accounts        READ accounts       NOTIFY accountsChanged)
    Q_PROPERTY(QString      productName     READ productName    CONSTANT)
    Q_PROPERTY(QString      productVersion  READ productVersion CONSTANT)
    Q_PROPERTY(QString      companyName     READ companyName    CONSTANT)
    Q_PROPERTY(QString      passwordRules   READ passwordRules  CONSTANT)
    Q_PROPERTY(bool         sessionLocked   READ sessionLocked  NOTIFY sessionLockedChanged)
    Q_PROPERTY(bool         sessionTerminated READ sessionTerminated NOTIFY sessionLockedChanged)
    Q_PROPERTY(QString      lockText        READ lockText       NOTIFY sessionLockedChanged)

public:
    struct Options
    {
        QString dataDirectory;
        QString terminal;
        QString productName;
        QString productVersion;
        QString companyName;
        QString eventSourceName;
        QString dictionaryPath;
        QString commonPasswordsPath;
        int     iterations = 0;                     // 0 = SecurityConfig default
        std::function<QDateTime()> clock;           // null = system clock (UTC)
        bool    useInstallSentinel = true;          // registry marker of a completed setup
    };

    explicit AuthManager(const Options &options, QObject *parent = nullptr);
    ~AuthManager() override;

    /// Decides the first stage. Call once before showing the login window.
    void start();

    QString stage() const { return _stage; }
    QString title() const;
    QString infoText() const { return _infoText; }
    QString recoveryCode() const { return _recoveryCodePlain; }
    QString auditWarning() const { return _auditWarning; }
    QString message() const { return _message; }
    QStringList messages() const { return _messages; }
    QString currentUser() const { return _currentId; }
    bool isAdmin() const { return _authenticated && _currentRole == Role::Admin; }
    bool authenticated() const { return _authenticated; }
    QVariantList accounts() const;
    QString productName() const { return _options.productName; }
    QString productVersion() const { return _options.productVersion; }
    QString companyName() const { return _options.companyName; }
    QString passwordRules() const;

    // Setup
    Q_INVOKABLE bool createAccount(const QString &id, const QString &password, const QString &confirm);
    Q_INVOKABLE void acknowledgeRecoveryCode();

    // Login
    Q_INVOKABLE bool login(const QString &id, const QString &password);
    Q_INVOKABLE void showRecovery();
    Q_INVOKABLE void backToLogin();
    Q_INVOKABLE bool recoverAdmin(const QString &code, const QString &adminId, const QString &password, const QString &confirm);

    // After login
    Q_INVOKABLE bool changePassword(const QString &currentPassword, const QString &newPassword, const QString &confirm);
    Q_INVOKABLE void requestPasswordChange();
    Q_INVOKABLE void continueToApplication();

    // Administrator tools
    Q_INVOKABLE void showAdminTools();
    Q_INVOKABLE void closeAdminTools();
    Q_INVOKABLE bool unlockAccount(const QString &id);
    Q_INVOKABLE bool resetPassword(const QString &id, const QString &password, const QString &confirm);
    Q_INVOKABLE QVariantList auditEntries(int maxEntries) const;
    Q_INVOKABLE QString auditIntegrityText() const;
    Q_INVOKABLE bool exportAuditLog(const QUrl &fileUrl);

    Q_INVOKABLE void clearMessage();
    Q_INVOKABLE void cancel();

    /// Called by the application on exit after a successful login.
    void recordLogout();

    // Idle lock (decided 2026-11-09: 10 minutes, not while flying)
    bool sessionLocked() const { return _sessionLocked; }
    bool sessionTerminated() const { return _sessionTerminated; }
    QString lockText() const { return _lockText; }
    void lockSession(const QString &reason);
    Q_INVOKABLE bool unlockSession(const QString &password);
    Q_INVOKABLE void requestExit();

    // For tests and integration
    AccountStore *store() { return _store.get(); }
    AuditLog *audit() { return _audit.get(); }
    static QString normalizeRecoveryCode(const QString &code);

signals:
    void stageChanged();
    void messageChanged();
    void sessionChanged();
    void accountsChanged();
    void finished(bool authenticated);
    void sessionLockedChanged();
    void exitRequested();

private:
    struct FailOutcome
    {
        bool lockedNow = false;
        bool locked = false;
        QString lockReason;
        int remaining = 0;
    };

    QDateTime _now();
    QDate _today();
    void _setStage(const QString &stage, const QString &info = QString());
    void _setMessage(const QString &message, const QStringList &messages = QStringList());
    void _fatal(const QString &reason, const QString &auditDetail);
    bool _save();
    void _log(const QString &type, const QString &account, const QString &result, const QString &detail = QString());
    void _checkAuditIntegrity();
    PasswordRecord _makeRecord(const QString &secret);
    bool _verify(const PasswordRecord &record, const QString &secret) const;
    bool _isReused(const Account &account, const QString &password) const;
    void _setPassword(Account &account, const QString &password);
    FailOutcome _registerFailure(FailureCounter &counter);
    QString _lockedMessage(const QString &reason) const;
    QString _issueRecoveryCode();
    void _afterAuthenticated(Account &account);
    QString _noticeText(const Account &account) const;
    bool _sentinelPresent() const;
    void _writeSentinel() const;
    bool _requireAdminSession();
    QString _nextSetupStage() const;

    Options _options;
    PasswordPolicy _policy;
    std::unique_ptr<AccountStore> _store;
    std::unique_ptr<AuditLog> _audit;

    QString _stage;
    QString _infoText;
    QString _message;
    QStringList _messages;
    QString _recoveryCodePlain;
    QString _auditWarning;
    QString _stageAfterRecoveryCode;

    bool _authenticated = false;
    QString _currentId;
    Role _currentRole = Role::User;
    bool _clockRollbackLogged = false;

    bool _sessionLocked = false;
    bool _sessionTerminated = false;
    QString _lockText;

    // Previous login, shown in the notice (SR-06)
    QDateTime _prevLoginUtc;
    QString _prevLoginTerminal;
    int _prevFailures = 0;
};

} // namespace KevaSecurity
