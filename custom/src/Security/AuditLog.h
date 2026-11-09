/****************************************************************************
 *
 * Daeyoung Ground Control - security audit log (SR-05, SR-07, SR-08)
 *
 * Every entry is written twice:
 *   1. audit.log in the security data folder. Each line is one DPAPI
 *      protected JSON record; records are hash chained (each carries the
 *      SHA-256 of the previous one) so edits, deletions and truncation can
 *      be detected.
 *   2. The Windows Application event log, which standard users cannot clear.
 *
 ****************************************************************************/

#pragma once

#include <QtCore/QDateTime>
#include <QtCore/QList>
#include <QtCore/QString>

namespace KevaSecurity {

struct AuditEntry
{
    qint64 seq = 0;
    QDateTime timeUtc;
    QString type;
    QString account;
    QString result;
    QString terminal;
    QString detail;
    QString prev;
    QString hash;
};

class AuditLog
{
public:
    AuditLog(const QString &directory, const QString &terminal, const QString &eventSourceName);

    QString filePath() const;
    bool fileExists() const;
    qint64 fileSize() const;

    /// Reads the existing log to find its tail and verify the hash chain.
    bool open(QString *error);

    bool append(const QString &type, const QString &account, const QString &result, const QString &detail, QString *error = nullptr);

    /// All entries, oldest first. chainValid reports whether every record verified.
    QList<AuditEntry> readAll(bool *chainValid, QString *error) const;

    qint64 lastSeq() const { return _lastSeq; }
    QString lastHash() const { return _lastHash; }
    bool chainValid() const { return _chainValid; }
    QString chainProblem() const { return _chainProblem; }

    /// Exports entries as CSV (UTF-8 with BOM so Excel opens it correctly).
    bool exportCsv(const QString &path, QString *error) const;

    static QString computeHash(const QString &prev, const QByteArray &canonicalJson);

private:
    void _writeEventLog(const QString &type, const QString &message) const;

    QString _directory;
    QString _terminal;
    QString _eventSourceName;
    qint64 _lastSeq = 0;
    QString _lastHash;
    bool _chainValid = true;
    QString _chainProblem;
};

} // namespace KevaSecurity
