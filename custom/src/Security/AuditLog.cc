/****************************************************************************
 *
 * Daeyoung Ground Control - security audit log (SR-05, SR-07, SR-08)
 *
 ****************************************************************************/

#include "AuditLog.h"
#include "SecurityCrypto.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>
#include <QtCore/QTextStream>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace KevaSecurity {

namespace {

const QString kFileName = QStringLiteral("audit.log");
const QString kGenesis = QStringLiteral("GENESIS");

QJsonObject entryToJson(const AuditEntry &e)
{
    QJsonObject obj;
    obj[QStringLiteral("seq")] = static_cast<double>(e.seq);
    obj[QStringLiteral("t")] = e.timeUtc.toUTC().toString(Qt::ISODateWithMs);
    obj[QStringLiteral("type")] = e.type;
    obj[QStringLiteral("acct")] = e.account;
    obj[QStringLiteral("res")] = e.result;
    obj[QStringLiteral("term")] = e.terminal;
    obj[QStringLiteral("detail")] = e.detail;
    obj[QStringLiteral("prev")] = e.prev;
    return obj;
}

AuditEntry entryFromJson(const QJsonObject &obj)
{
    AuditEntry e;
    e.seq = static_cast<qint64>(obj.value(QStringLiteral("seq")).toDouble());
    e.timeUtc = QDateTime::fromString(obj.value(QStringLiteral("t")).toString(), Qt::ISODateWithMs).toUTC();
    e.type = obj.value(QStringLiteral("type")).toString();
    e.account = obj.value(QStringLiteral("acct")).toString();
    e.result = obj.value(QStringLiteral("res")).toString();
    e.terminal = obj.value(QStringLiteral("term")).toString();
    e.detail = obj.value(QStringLiteral("detail")).toString();
    e.prev = obj.value(QStringLiteral("prev")).toString();
    e.hash = obj.value(QStringLiteral("h")).toString();
    return e;
}

QString csvField(const QString &s)
{
    QString out = s;
    out.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    // Neutralise spreadsheet formula injection
    if (!out.isEmpty() && QStringLiteral("=+-@").contains(out.at(0))) {
        out.prepend(QLatin1Char('\''));
    }
    return QLatin1Char('"') + out + QLatin1Char('"');
}

} // namespace

AuditLog::AuditLog(const QString &directory, const QString &terminal, const QString &eventSourceName)
    : _directory(directory)
    , _terminal(terminal)
    , _eventSourceName(eventSourceName)
{
}

QString AuditLog::filePath() const
{
    return QDir(_directory).filePath(kFileName);
}

bool AuditLog::fileExists() const
{
    return QFile::exists(filePath());
}

qint64 AuditLog::fileSize() const
{
    return QFileInfo(filePath()).size();
}

QString AuditLog::computeHash(const QString &prev, const QByteArray &canonicalJson)
{
    return QString::fromLatin1(SecurityCrypto::sha256(prev.toUtf8() + '\n' + canonicalJson).toHex());
}

QList<AuditEntry> AuditLog::readAll(bool *chainValid, QString *error) const
{
    QList<AuditEntry> entries;
    bool valid = true;
    QString problem;

    QFile file(filePath());
    if (!file.exists()) {
        if (chainValid) {
            *chainValid = true;
        }
        return entries;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Cannot open the audit log.");
        }
        if (chainValid) {
            *chainValid = false;
        }
        return entries;
    }

    QString expectedPrev = kGenesis;
    qint64 expectedSeq = 1;
    int lineNo = 0;
    while (!file.atEnd()) {
        const QByteArray line = file.readLine().trimmed();
        ++lineNo;
        if (line.isEmpty()) {
            continue;
        }
        bool ok = false;
        const QByteArray plain = SecurityCrypto::unprotect(QByteArray::fromBase64(line), &ok);
        QJsonObject obj;
        if (ok) {
            obj = QJsonDocument::fromJson(plain).object();
        }
        if (!ok || obj.isEmpty()) {
            valid = false;
            if (problem.isEmpty()) {
                problem = QStringLiteral("Record on line %1 cannot be read.").arg(lineNo);
            }
            AuditEntry broken;
            broken.type = QStringLiteral("UNREADABLE_RECORD");
            broken.detail = QStringLiteral("line %1").arg(lineNo);
            entries.append(broken);
            continue;
        }
        AuditEntry e = entryFromJson(obj);
        obj.remove(QStringLiteral("h"));
        const QString hash = computeHash(e.prev, QJsonDocument(obj).toJson(QJsonDocument::Compact));
        if (hash != e.hash || e.prev != expectedPrev || e.seq != expectedSeq) {
            valid = false;
            if (problem.isEmpty()) {
                problem = QStringLiteral("Hash chain broken at record %1.").arg(e.seq);
            }
        }
        expectedPrev = e.hash;
        expectedSeq = e.seq + 1;
        entries.append(e);
    }

    if (chainValid) {
        *chainValid = valid;
    }
    if (!valid && error) {
        *error = problem;
    }
    return entries;
}

bool AuditLog::open(QString *error)
{
    QString problem;
    bool valid = true;
    const QList<AuditEntry> entries = readAll(&valid, &problem);
    _chainValid = valid;
    _chainProblem = problem;
    _lastSeq = 0;
    _lastHash = kGenesis;
    for (auto it = entries.crbegin(); it != entries.crend(); ++it) {
        if (!it->hash.isEmpty()) {
            _lastSeq = it->seq;
            _lastHash = it->hash;
            break;
        }
    }
    if (!valid && error) {
        *error = problem;
    }
    return true;
}

bool AuditLog::append(const QString &type, const QString &account, const QString &result, const QString &detail, QString *error)
{
    AuditEntry e;
    e.seq = _lastSeq + 1;
    e.timeUtc = QDateTime::currentDateTimeUtc();
    e.type = type;
    e.account = account;
    e.result = result;
    e.terminal = _terminal;
    e.detail = detail;
    e.prev = _lastHash.isEmpty() ? kGenesis : _lastHash;

    QJsonObject obj = entryToJson(e);
    e.hash = computeHash(e.prev, QJsonDocument(obj).toJson(QJsonDocument::Compact));
    obj[QStringLiteral("h")] = e.hash;

    bool ok = false;
    const QByteArray blob = SecurityCrypto::protect(QJsonDocument(obj).toJson(QJsonDocument::Compact), &ok);
    if (!ok) {
        if (error) {
            *error = QStringLiteral("Audit record could not be encrypted.");
        }
        return false;
    }
    if (!QDir().mkpath(_directory)) {
        if (error) {
            *error = QStringLiteral("Cannot create the audit log folder.");
        }
        return false;
    }
    QFile file(filePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        if (error) {
            *error = QStringLiteral("Cannot write the audit log: %1").arg(file.errorString());
        }
        return false;
    }
    file.write(blob.toBase64());
    file.write("\n");
    file.flush();
    file.close();

    _lastSeq = e.seq;
    _lastHash = e.hash;

    _writeEventLog(type, QStringLiteral("%1 | account=%2 | result=%3 | terminal=%4 | %5 | seq=%6")
                             .arg(type, account.isEmpty() ? QStringLiteral("-") : account, result, _terminal, detail)
                             .arg(e.seq));
    return true;
}

bool AuditLog::exportCsv(const QString &path, QString *error) const
{
    bool valid = true;
    QString problem;
    const QList<AuditEntry> entries = readAll(&valid, &problem);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    QByteArray out("\xEF\xBB\xBF");
    out += "Seq,Time (local),Event,Account,Result,Terminal,Detail\n";
    for (const AuditEntry &e : entries) {
        const QStringList cols = {
            QString::number(e.seq),
            e.timeUtc.isValid() ? e.timeUtc.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QString(),
            e.type, e.account, e.result, e.terminal, e.detail,
        };
        QStringList quoted;
        for (const QString &c : cols) {
            quoted << csvField(c);
        }
        out += quoted.join(QLatin1Char(',')).toUtf8() + '\n';
    }
    if (!valid) {
        out += csvField(QStringLiteral("WARNING: integrity check failed - %1").arg(problem)).toUtf8() + '\n';
    }
    file.write(out);
    if (!file.commit()) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    return true;
}

void AuditLog::_writeEventLog(const QString &type, const QString &message) const
{
#ifdef Q_OS_WIN
    HANDLE source = RegisterEventSourceW(nullptr, reinterpret_cast<LPCWSTR>(_eventSourceName.utf16()));
    if (!source) {
        return;
    }
    const bool warning = type.contains(QStringLiteral("FAIL")) || type.contains(QStringLiteral("LOCK")) ||
                         type.contains(QStringLiteral("REJECT")) || type.contains(QStringLiteral("INTEGRITY")) ||
                         type.contains(QStringLiteral("CLOCK"));
    const QString text = QStringLiteral("[DaeyoungGCS Security] ") + message;
    LPCWSTR strings[1] = { reinterpret_cast<LPCWSTR>(text.utf16()) };
    ReportEventW(source,
                 warning ? EVENTLOG_WARNING_TYPE : EVENTLOG_INFORMATION_TYPE,
                 0,
                 warning ? 2000 : 1000,
                 nullptr, 1, 0, strings, nullptr);
    DeregisterEventSource(source);
#else
    Q_UNUSED(type)
    Q_UNUSED(message)
#endif
}

} // namespace KevaSecurity
