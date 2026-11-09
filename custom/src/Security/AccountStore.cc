/****************************************************************************
 *
 * Daeyoung Ground Control - account database (MIL edition)
 *
 ****************************************************************************/

#include "AccountStore.h"
#include "SecurityConfig.h"
#include "SecurityCrypto.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>

namespace KevaSecurity {

namespace {

const QString kFileName = QStringLiteral("accounts.dat");
const QByteArray kFileMagic = QByteArrayLiteral("KEVAACC1");
constexpr int kFormatVersion = 1;

QString toIso(const QDateTime &dt) { return dt.isValid() ? dt.toUTC().toString(Qt::ISODate) : QString(); }
QDateTime fromIso(const QString &s)
{
    if (s.isEmpty()) {
        return QDateTime();
    }
    return QDateTime::fromString(s, Qt::ISODate).toUTC();
}
QString b64(const QByteArray &b) { return QString::fromLatin1(b.toBase64()); }
QByteArray unb64(const QJsonValue &v) { return QByteArray::fromBase64(v.toString().toLatin1()); }

} // namespace

QString roleToString(Role role)
{
    return role == Role::Admin ? QStringLiteral("admin") : QStringLiteral("user");
}

Role roleFromString(const QString &text)
{
    return text == QStringLiteral("admin") ? Role::Admin : Role::User;
}

QJsonObject PasswordRecord::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("salt")] = b64(salt);
    obj[QStringLiteral("hash")] = b64(hash);
    obj[QStringLiteral("iter")] = iterations;
    obj[QStringLiteral("alg")] = QStringLiteral("PBKDF2-HMAC-SHA256");
    obj[QStringLiteral("set")] = toIso(setAtUtc);
    return obj;
}

PasswordRecord PasswordRecord::fromJson(const QJsonObject &obj)
{
    PasswordRecord r;
    r.salt = unb64(obj.value(QStringLiteral("salt")));
    r.hash = unb64(obj.value(QStringLiteral("hash")));
    r.iterations = obj.value(QStringLiteral("iter")).toInt();
    r.setAtUtc = fromIso(obj.value(QStringLiteral("set")).toString());
    return r;
}

QJsonObject FailureCounter::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("consecutive")] = consecutive;
    obj[QStringLiteral("daily")] = daily;
    obj[QStringLiteral("dailyDate")] = dailyDate.isValid() ? dailyDate.toString(Qt::ISODate) : QString();
    obj[QStringLiteral("locked")] = locked;
    obj[QStringLiteral("lockReason")] = lockReason;
    obj[QStringLiteral("lockedAt")] = toIso(lockedAtUtc);
    return obj;
}

FailureCounter FailureCounter::fromJson(const QJsonObject &obj)
{
    FailureCounter c;
    c.consecutive = obj.value(QStringLiteral("consecutive")).toInt();
    c.daily = obj.value(QStringLiteral("daily")).toInt();
    c.dailyDate = QDate::fromString(obj.value(QStringLiteral("dailyDate")).toString(), Qt::ISODate);
    c.locked = obj.value(QStringLiteral("locked")).toBool();
    c.lockReason = obj.value(QStringLiteral("lockReason")).toString();
    c.lockedAtUtc = fromIso(obj.value(QStringLiteral("lockedAt")).toString());
    return c;
}

QJsonObject Account::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = id;
    obj[QStringLiteral("role")] = roleToString(role);
    obj[QStringLiteral("password")] = password.toJson();
    QJsonArray hist;
    for (const PasswordRecord &r : history) {
        hist.append(r.toJson());
    }
    obj[QStringLiteral("history")] = hist;
    obj[QStringLiteral("mustChange")] = mustChangePassword;
    obj[QStringLiteral("mustChangeReason")] = mustChangeReason;
    obj[QStringLiteral("failures")] = failures.toJson();
    obj[QStringLiteral("created")] = toIso(createdAtUtc);
    obj[QStringLiteral("lastLogin")] = toIso(lastLoginUtc);
    obj[QStringLiteral("lastLoginTerminal")] = lastLoginTerminal;
    obj[QStringLiteral("failuresSinceLastLogin")] = failuresSinceLastLogin;
    return obj;
}

Account Account::fromJson(const QJsonObject &obj)
{
    Account a;
    a.id = obj.value(QStringLiteral("id")).toString();
    a.role = roleFromString(obj.value(QStringLiteral("role")).toString());
    a.password = PasswordRecord::fromJson(obj.value(QStringLiteral("password")).toObject());
    const QJsonArray hist = obj.value(QStringLiteral("history")).toArray();
    for (const QJsonValue &v : hist) {
        a.history.append(PasswordRecord::fromJson(v.toObject()));
    }
    a.mustChangePassword = obj.value(QStringLiteral("mustChange")).toBool();
    a.mustChangeReason = obj.value(QStringLiteral("mustChangeReason")).toString();
    a.failures = FailureCounter::fromJson(obj.value(QStringLiteral("failures")).toObject());
    a.createdAtUtc = fromIso(obj.value(QStringLiteral("created")).toString());
    a.lastLoginUtc = fromIso(obj.value(QStringLiteral("lastLogin")).toString());
    a.lastLoginTerminal = obj.value(QStringLiteral("lastLoginTerminal")).toString();
    a.failuresSinceLastLogin = obj.value(QStringLiteral("failuresSinceLastLogin")).toInt();
    return a;
}

AccountStore::AccountStore(const QString &directory)
    : _directory(directory)
{
}

QString AccountStore::filePath() const
{
    return QDir(_directory).filePath(kFileName);
}

bool AccountStore::fileExists() const
{
    return QFile::exists(filePath());
}

Account *AccountStore::find(const QString &id)
{
    for (Account &a : _accounts) {
        if (a.id == id) {   // IDs are case-sensitive (decided 2026-11-09)
            return &a;
        }
    }
    return nullptr;
}

int AccountStore::count(Role role) const
{
    int n = 0;
    for (const Account &a : _accounts) {
        if (a.role == role) {
            ++n;
        }
    }
    return n;
}

FailureCounter &AccountStore::unknownIdCounter(const QString &id)
{
    if (!_unknownIds.contains(id) && _unknownIds.size() >= kMaxUnknownIdCounters) {
        // Drop the oldest entry so the file cannot grow without bound.
        QString oldestKey;
        QDate oldestDate = QDate(9999, 12, 31);
        for (auto it = _unknownIds.cbegin(); it != _unknownIds.cend(); ++it) {
            const QDate d = it.value().dailyDate.isValid() ? it.value().dailyDate : QDate(1970, 1, 1);
            if (d < oldestDate) {
                oldestDate = d;
                oldestKey = it.key();
            }
        }
        _unknownIds.remove(oldestKey);
    }
    return _unknownIds[id];
}

bool AccountStore::load(QString *error)
{
    QFile file(filePath());
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Cannot open the account database.");
        }
        return false;
    }
    const QByteArray raw = file.readAll();
    file.close();
    if (!raw.startsWith(kFileMagic)) {
        if (error) {
            *error = QStringLiteral("The account database is not in a recognised format.");
        }
        return false;
    }
    bool ok = false;
    const QByteArray plain = SecurityCrypto::unprotect(raw.mid(kFileMagic.size()), &ok);
    if (!ok) {
        if (error) {
            *error = QStringLiteral("The account database could not be decrypted on this PC or has been modified.");
        }
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(plain, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) {
            *error = QStringLiteral("The account database is damaged.");
        }
        return false;
    }
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("format")).toInt() != kFormatVersion) {
        if (error) {
            *error = QStringLiteral("The account database version is not supported.");
        }
        return false;
    }

    _accounts.clear();
    for (const QJsonValue &v : root.value(QStringLiteral("accounts")).toArray()) {
        _accounts.append(Account::fromJson(v.toObject()));
    }
    _unknownIds.clear();
    const QJsonObject unknown = root.value(QStringLiteral("unknownIds")).toObject();
    for (auto it = unknown.constBegin(); it != unknown.constEnd(); ++it) {
        _unknownIds.insert(it.key(), FailureCounter::fromJson(it.value().toObject()));
    }
    recoveryCode = PasswordRecord::fromJson(root.value(QStringLiteral("recovery")).toObject());
    lastSeenUtc = fromIso(root.value(QStringLiteral("lastSeen")).toString());
    auditSeq = static_cast<qint64>(root.value(QStringLiteral("auditSeq")).toDouble());
    auditHead = root.value(QStringLiteral("auditHead")).toString();
    initializedAtUtc = fromIso(root.value(QStringLiteral("initialized")).toString());
    return true;
}

bool AccountStore::save(QString *error)
{
    QJsonObject root;
    root[QStringLiteral("format")] = kFormatVersion;
    QJsonArray accounts;
    for (const Account &a : _accounts) {
        accounts.append(a.toJson());
    }
    root[QStringLiteral("accounts")] = accounts;
    QJsonObject unknown;
    for (auto it = _unknownIds.cbegin(); it != _unknownIds.cend(); ++it) {
        unknown.insert(it.key(), it.value().toJson());
    }
    root[QStringLiteral("unknownIds")] = unknown;
    root[QStringLiteral("recovery")] = recoveryCode.toJson();
    root[QStringLiteral("lastSeen")] = toIso(lastSeenUtc);
    root[QStringLiteral("auditSeq")] = static_cast<double>(auditSeq);
    root[QStringLiteral("auditHead")] = auditHead;
    root[QStringLiteral("initialized")] = toIso(initializedAtUtc);

    bool ok = false;
    const QByteArray blob = SecurityCrypto::protect(QJsonDocument(root).toJson(QJsonDocument::Compact), &ok);
    if (!ok) {
        if (error) {
            *error = QStringLiteral("The account database could not be encrypted.");
        }
        return false;
    }

    if (!QDir().mkpath(_directory)) {
        if (error) {
            *error = QStringLiteral("Cannot create the security data folder: %1").arg(_directory);
        }
        return false;
    }
    QSaveFile file(filePath());
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = QStringLiteral("Cannot write the account database: %1").arg(file.errorString());
        }
        return false;
    }
    file.write(kFileMagic);
    file.write(blob);
    if (!file.commit()) {
        if (error) {
            *error = QStringLiteral("Cannot save the account database: %1").arg(file.errorString());
        }
        return false;
    }
    return true;
}

} // namespace KevaSecurity
