/****************************************************************************
 *
 * Daeyoung Ground Control - account database (MIL edition)
 *
 * Stored as one DPAPI protected JSON document (accounts.dat). Passwords are
 * kept only as salted PBKDF2-HMAC-SHA256 hashes, together with one year of
 * previous hashes for the reuse check (SR-01, SR-14).
 *
 ****************************************************************************/

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QDate>
#include <QtCore/QDateTime>
#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QString>

class QJsonObject;

namespace KevaSecurity {

enum class Role { Admin, User };

QString roleToString(Role role);
Role roleFromString(const QString &text);

struct PasswordRecord
{
    QByteArray salt;
    QByteArray hash;
    int iterations = 0;
    QDateTime setAtUtc;

    bool isEmpty() const { return hash.isEmpty(); }
    QJsonObject toJson() const;
    static PasswordRecord fromJson(const QJsonObject &obj);
};

/// Failure counters, kept for real accounts and also for IDs that do not exist,
/// so that the login screen behaves the same either way.
struct FailureCounter
{
    int consecutive = 0;
    int daily = 0;
    QDate dailyDate;
    bool locked = false;
    QString lockReason;     // "consecutive" | "daily"
    QDateTime lockedAtUtc;

    QJsonObject toJson() const;
    static FailureCounter fromJson(const QJsonObject &obj);
};

struct Account
{
    QString id;                 // case-sensitive
    Role role = Role::User;
    PasswordRecord password;
    QList<PasswordRecord> history;
    bool mustChangePassword = false;
    QString mustChangeReason;   // "reset" | "expired" | "recovery"
    FailureCounter failures;
    QDateTime createdAtUtc;
    QDateTime lastLoginUtc;
    QString lastLoginTerminal;
    int failuresSinceLastLogin = 0;

    QJsonObject toJson() const;
    static Account fromJson(const QJsonObject &obj);
};

class AccountStore
{
public:
    explicit AccountStore(const QString &directory);

    QString directory() const { return _directory; }
    QString filePath() const;
    bool fileExists() const;

    bool load(QString *error);
    bool save(QString *error);

    QList<Account> &accounts() { return _accounts; }
    const QList<Account> &accounts() const { return _accounts; }
    Account *find(const QString &id);
    int count(Role role) const;

    FailureCounter &unknownIdCounter(const QString &id);

    // Recovery code (only its hash is stored)
    PasswordRecord recoveryCode;

    // Highest wall clock time seen, used to detect the PC clock being turned back
    QDateTime lastSeenUtc;

    // Mirror of the audit log tail, used to detect truncation of the log
    qint64 auditSeq = 0;
    QString auditHead;

    QDateTime initializedAtUtc;

private:
    QString _directory;
    QList<Account> _accounts;
    QHash<QString, FailureCounter> _unknownIds;
};

} // namespace KevaSecurity
