/****************************************************************************
 *
 * Daeyoung Ground Control - authentication and account management (MIL edition)
 *
 ****************************************************************************/

#include "AuthManager.h"
#include "SecurityConfig.h"
#include "SecurityCrypto.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QSettings>
#include <QtCore/QVariantMap>

#include <algorithm>

namespace KevaSecurity {

namespace {

const QString kSentinelKey = QStringLiteral("KevaSecurity/initializedAt");
const QString kRecoveryCounterId = QStringLiteral("#recovery");
const QString kRecoveryAlphabet = QStringLiteral("ABCDEFGHJKLMNPQRSTUVWXYZ23456789");

QString localTime(const QDateTime &utc)
{
    return utc.isValid() ? utc.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QStringLiteral("-");
}

const QString kMsgCheckIdPw = QStringLiteral("Login Failed – Check your ID and/or PW. (%1)");
const QString kMsgLocked = QStringLiteral("Login Failed – Account has been locked. Please contact the System Administrator.");
const QString kMsgDailyLocked = QStringLiteral("Login Failed – Daily limit has been exceeded. Account has been locked. Please contact the System Administrator.");
const QString kMsgContactAdmin = QStringLiteral("Please contact the System Administrator.");

} // namespace

AuthManager::AuthManager(const Options &options, QObject *parent)
    : QObject(parent)
    , _options(options)
    , _store(std::make_unique<AccountStore>(options.dataDirectory))
    , _audit(std::make_unique<AuditLog>(options.dataDirectory, options.terminal, options.eventSourceName))
{
    if (_options.iterations <= 0) {
        _options.iterations = kPbkdf2Iterations;
    }
}

AuthManager::~AuthManager() = default;

// ---------------------------------------------------------------------------
// Stage handling

QString AuthManager::title() const
{
    if (_stage == QStringLiteral("setupAdmin")) {
        return QStringLiteral("Create Administrator Account (%1/%2)").arg(_store->count(Role::Admin) + 1).arg(kAdminAccountCount);
    }
    if (_stage == QStringLiteral("setupUser")) {
        return QStringLiteral("Create User Account");
    }
    if (_stage == QStringLiteral("recoveryCode")) {
        return QStringLiteral("Administrator Recovery Code");
    }
    if (_stage == QStringLiteral("recovery")) {
        return QStringLiteral("Administrator Recovery");
    }
    if (_stage == QStringLiteral("changePassword")) {
        return QStringLiteral("Change Password");
    }
    if (_stage == QStringLiteral("notice")) {
        return QStringLiteral("Login Successful");
    }
    if (_stage == QStringLiteral("adminTools")) {
        return QStringLiteral("Account Management");
    }
    if (_stage == QStringLiteral("fatal")) {
        return QStringLiteral("Security Error");
    }
    return QString();
}

void AuthManager::_setStage(const QString &stage, const QString &info)
{
    _stage = stage;
    _infoText = info;
    _messages.clear();
    _message.clear();
    emit messageChanged();
    emit stageChanged();
}

void AuthManager::_setMessage(const QString &message, const QStringList &messages)
{
    _message = message;
    _messages = messages;
    emit messageChanged();
}

void AuthManager::clearMessage()
{
    _setMessage(QString());
}

QString AuthManager::passwordRules() const
{
    return PasswordPolicy::rulesText();
}

void AuthManager::_fatal(const QString &reason, const QString &auditDetail)
{
    _authenticated = false;
    _log(QStringLiteral("INTEGRITY_ALERT"), QString(), QStringLiteral("blocked"), auditDetail);
    _setStage(QStringLiteral("fatal"), reason + QStringLiteral("\n\n") + kMsgContactAdmin);
}

// ---------------------------------------------------------------------------
// Time

QDateTime AuthManager::_now()
{
    const QDateTime wall = _options.clock ? _options.clock().toUTC() : QDateTime::currentDateTimeUtc();
    const QDateTime &seen = _store->lastSeenUtc;
    if (seen.isValid() && wall.secsTo(seen) > kClockRollbackToleranceSecs) {
        // The PC clock was turned back. Keep using the latest time seen so daily
        // limits and expiry cannot be reset this way, and record it once.
        if (!_clockRollbackLogged) {
            _clockRollbackLogged = true;
            _log(QStringLiteral("CLOCK_ROLLBACK"), QString(), QStringLiteral("detected"),
                 QStringLiteral("clock=%1 lastSeen=%2").arg(wall.toString(Qt::ISODate), seen.toString(Qt::ISODate)));
        }
        return seen;
    }
    if (!seen.isValid() || wall > seen) {
        _store->lastSeenUtc = wall;
    }
    return wall;
}

QDate AuthManager::_today()
{
    return _now().toLocalTime().date();
}

// ---------------------------------------------------------------------------
// Persistence and logging

bool AuthManager::_save()
{
    _store->auditSeq = _audit->lastSeq();
    _store->auditHead = _audit->lastHash();
    QString error;
    if (!_store->save(&error)) {
        _setMessage(error + QStringLiteral(" ") + kMsgContactAdmin);
        return false;
    }
    return true;
}

void AuthManager::_log(const QString &type, const QString &account, const QString &result, const QString &detail)
{
    QString error;
    if (!_audit->append(type, account, result, detail, &error)) {
        qWarning() << "KevaSecurity: audit log write failed:" << error;
    }
}

void AuthManager::_checkAuditIntegrity()
{
    QStringList problems;
    if (!_audit->chainValid()) {
        problems << _audit->chainProblem();
    }
    if (_store->auditSeq > _audit->lastSeq()) {
        problems << QStringLiteral("Audit log is shorter than expected (expected record %1, found %2).")
                        .arg(_store->auditSeq).arg(_audit->lastSeq());
    }
    if (_audit->fileSize() > kAuditLogWarnBytes) {
        problems << QStringLiteral("Audit log is larger than %1 MB. Export and archive it.").arg(kAuditLogWarnBytes / (1024 * 1024));
    }
    _auditWarning = problems.join(QLatin1Char('\n'));
    if (!_audit->chainValid() || _store->auditSeq > _audit->lastSeq()) {
        _log(QStringLiteral("INTEGRITY_ALERT"), QString(), QStringLiteral("audit log"), problems.join(QStringLiteral(" ")));
    }
}

bool AuthManager::_sentinelPresent() const
{
    if (!_options.useInstallSentinel) {
        return false;
    }
    // Registry on Windows (NativeFormat), independent of QGC's INI settings file,
    // so removing the application settings does not remove the marker.
    QSettings user(QSettings::NativeFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName());
    QSettings machine(QSettings::NativeFormat, QSettings::SystemScope, QCoreApplication::organizationName(), QCoreApplication::applicationName());
    return user.contains(kSentinelKey) || machine.contains(kSentinelKey);
}

void AuthManager::_writeSentinel() const
{
    if (!_options.useInstallSentinel) {
        return;
    }
    const QString stamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    QSettings user(QSettings::NativeFormat, QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName());
    user.setValue(kSentinelKey, stamp);
    // Succeeds only when running elevated (e.g. first run right after install); harmless otherwise.
    QSettings machine(QSettings::NativeFormat, QSettings::SystemScope, QCoreApplication::organizationName(), QCoreApplication::applicationName());
    if (machine.isWritable()) {
        machine.setValue(kSentinelKey, stamp);
    }
}

// ---------------------------------------------------------------------------
// Password records

PasswordRecord AuthManager::_makeRecord(const QString &secret)
{
    PasswordRecord r;
    r.salt = SecurityCrypto::randomBytes(kSaltBytes);
    r.iterations = _options.iterations;
    r.hash = SecurityCrypto::pbkdf2Sha256(secret.toUtf8(), r.salt, r.iterations, kHashBytes);
    r.setAtUtc = _now();
    return r;
}

bool AuthManager::_verify(const PasswordRecord &record, const QString &secret) const
{
    if (record.isEmpty() || record.iterations <= 0) {
        return false;
    }
    const QByteArray hash = SecurityCrypto::pbkdf2Sha256(secret.toUtf8(), record.salt, record.iterations, record.hash.size());
    return !hash.isEmpty() && SecurityCrypto::constantTimeEquals(hash, record.hash);
}

bool AuthManager::_isReused(const Account &account, const QString &password) const
{
    if (_verify(account.password, password)) {
        return true;
    }
    // A previous password stays blocked until one year after it was replaced.
    const QDateTime now = _options.clock ? _options.clock().toUTC() : QDateTime::currentDateTimeUtc();
    const QDateTime effectiveNow = (_store->lastSeenUtc.isValid() && _store->lastSeenUtc > now) ? _store->lastSeenUtc : now;
    QDateTime replacedAt = account.password.setAtUtc;
    for (const PasswordRecord &r : account.history) {
        if (replacedAt.daysTo(effectiveNow) <= kPasswordHistoryDays && _verify(r, password)) {
            return true;
        }
        replacedAt = r.setAtUtc;
    }
    return false;
}

void AuthManager::_setPassword(Account &account, const QString &password)
{
    const QDateTime now = _now();
    if (!account.password.isEmpty()) {
        account.history.prepend(account.password);
    }
    account.password = _makeRecord(password);

    // A previous password counts as "used in the last year" until one year after it was replaced.
    QDateTime replacedAt = account.password.setAtUtc;
    QList<PasswordRecord> kept;
    for (const PasswordRecord &r : account.history) {
        if (replacedAt.daysTo(now) <= kPasswordHistoryDays) {
            kept.append(r);
        }
        replacedAt = r.setAtUtc;
    }
    account.history = kept;
}

// ---------------------------------------------------------------------------
// Lockout (SR-03, SR-04)

AuthManager::FailOutcome AuthManager::_registerFailure(FailureCounter &counter)
{
    const QDate today = _today();
    if (counter.dailyDate != today) {
        counter.daily = 0;
        counter.dailyDate = today;
    }
    counter.consecutive++;
    counter.daily++;

    FailOutcome out;
    if (!counter.locked) {
        if (counter.daily >= kMaxDailyFailures) {
            counter.locked = true;
            counter.lockReason = QStringLiteral("daily");
            counter.lockedAtUtc = _now();
            out.lockedNow = true;
        } else if (counter.consecutive >= kMaxConsecutiveFailures) {
            counter.locked = true;
            counter.lockReason = QStringLiteral("consecutive");
            counter.lockedAtUtc = _now();
            out.lockedNow = true;
        }
    }
    out.locked = counter.locked;
    out.lockReason = counter.lockReason;
    out.remaining = std::max(0, std::min(kMaxConsecutiveFailures - counter.consecutive, kMaxDailyFailures - counter.daily));
    return out;
}

QString AuthManager::_lockedMessage(const QString &reason) const
{
    return reason == QStringLiteral("daily") ? kMsgDailyLocked : kMsgLocked;
}

// ---------------------------------------------------------------------------
// Start

QString AuthManager::_nextSetupStage() const
{
    if (_store->count(Role::Admin) < kAdminAccountCount) {
        return QStringLiteral("setupAdmin");
    }
    if (_store->recoveryCode.isEmpty()) {
        return QStringLiteral("recoveryCode");
    }
    if (_store->count(Role::User) < kUserAccountCount) {
        return QStringLiteral("setupUser");
    }
    return QString();
}

void AuthManager::start()
{
    if (!_options.dictionaryPath.isEmpty() && !_policy.loadDictionary(_options.dictionaryPath)) {
        qWarning() << "KevaSecurity: dictionary not loaded from" << _options.dictionaryPath;
    }
    if (!_options.commonPasswordsPath.isEmpty() && !_policy.loadCommonPasswords(_options.commonPasswordsPath)) {
        qWarning() << "KevaSecurity: common password list not loaded from" << _options.commonPasswordsPath;
    }

    // Open the audit log first so even a failed start is recorded in the right chain position.
    _audit->open(nullptr);

    QString error;
    if (!SecurityCrypto::selfTest(&error)) {
        _fatal(error, QStringLiteral("crypto self test failed: %1").arg(error));
        return;
    }

    if (!_store->fileExists()) {
        if (_sentinelPresent() || _audit->fileExists()) {
            _fatal(QStringLiteral("Security data for this GCS is missing."),
                   QStringLiteral("account database missing after setup"));
            return;
        }
        _log(QStringLiteral("SETUP_STARTED"), QString(), QStringLiteral("ok"));
        _setStage(QStringLiteral("setupAdmin"),
                  QStringLiteral("First run. Create the two administrator (security officer) accounts, then the user account."));
        return;
    }

    if (!_store->load(&error)) {
        _fatal(error, QStringLiteral("account database load failed: %1").arg(error));
        return;
    }

    _now();     // records clock rollback if the PC clock was turned back
    _checkAuditIntegrity();
    _save();

    const QString setup = _nextSetupStage();
    if (setup == QStringLiteral("recoveryCode")) {
        _recoveryCodePlain = _issueRecoveryCode();
        _setStage(setup);
    } else if (!setup.isEmpty()) {
        _setStage(setup, QStringLiteral("Setup is not complete. Continue creating accounts."));
    } else {
        _setStage(QStringLiteral("login"));
    }
}

// ---------------------------------------------------------------------------
// Setup

bool AuthManager::createAccount(const QString &id, const QString &password, const QString &confirm)
{
    const bool admin = _stage == QStringLiteral("setupAdmin");
    if (!admin && _stage != QStringLiteral("setupUser")) {
        return false;
    }

    QString idError = _policy.checkId(id);
    if (idError.isEmpty()) {
        for (const Account &a : _store->accounts()) {
            if (a.id.compare(id, Qt::CaseInsensitive) == 0) {
                idError = QStringLiteral("This ID is already in use.");
                break;
            }
        }
    }
    if (!idError.isEmpty()) {
        _setMessage(idError, { idError });
        return false;
    }
    if (password != confirm) {
        _setMessage(QStringLiteral("PW and PW confirm do not match."), { QStringLiteral("PW and PW confirm do not match.") });
        return false;
    }
    const QStringList violations = _policy.checkPassword(id, password);
    if (!violations.isEmpty()) {
        _setMessage(violations.first(), violations);
        return false;
    }

    Account account;
    account.id = id;
    account.role = admin ? Role::Admin : Role::User;
    account.createdAtUtc = _now();
    _setPassword(account, password);

    const bool firstAccount = _store->accounts().isEmpty();
    if (firstAccount) {
        _store->initializedAtUtc = _now();
    }
    _store->accounts().append(account);
    _log(QStringLiteral("ACCOUNT_CREATED"), id, QStringLiteral("ok"), QStringLiteral("role=%1").arg(roleToString(account.role)));
    if (!_save()) {
        _store->accounts().removeLast();
        return false;
    }
    if (firstAccount) {
        _writeSentinel();
    }
    emit accountsChanged();

    const QString next = _nextSetupStage();
    if (next == QStringLiteral("recoveryCode")) {
        _recoveryCodePlain = _issueRecoveryCode();
        _setStage(next);
    } else if (!next.isEmpty()) {
        _setStage(next, admin ? QStringLiteral("Administrator account '%1' created.").arg(id) : QString());
    } else {
        _log(QStringLiteral("SETUP_COMPLETED"), QString(), QStringLiteral("ok"));
        _save();
        _setStage(QStringLiteral("login"), QStringLiteral("Setup complete. Log in with your account."));
    }
    return true;
}

QString AuthManager::_issueRecoveryCode()
{
    const QByteArray random = SecurityCrypto::randomBytes(kRecoveryCodeGroups * kRecoveryCodeGroupLength);
    QString code;
    for (int i = 0; i < random.size(); ++i) {
        if (i > 0 && i % kRecoveryCodeGroupLength == 0) {
            code.append(QLatin1Char('-'));
        }
        code.append(kRecoveryAlphabet.at(static_cast<unsigned char>(random.at(i)) % kRecoveryAlphabet.size()));
    }
    _store->recoveryCode = _makeRecord(normalizeRecoveryCode(code));
    _log(QStringLiteral("RECOVERY_CODE_ISSUED"), _currentId, QStringLiteral("ok"));
    _save();
    return code;
}

QString AuthManager::normalizeRecoveryCode(const QString &code)
{
    QString out;
    for (const QChar c : code.toUpper()) {
        if (c.isLetterOrNumber()) {
            out.append(c);
        }
    }
    return out;
}

void AuthManager::acknowledgeRecoveryCode()
{
    if (_stage != QStringLiteral("recoveryCode")) {
        return;
    }
    _recoveryCodePlain.clear();
    if (_authenticated) {
        // Issued again after a recovery: continue with the session
        _setStage(QStringLiteral("notice"), _noticeText(*_store->find(_currentId)));
        return;
    }
    const QString next = _nextSetupStage();
    if (!next.isEmpty()) {
        _setStage(next);
    } else {
        _setStage(QStringLiteral("login"), _stageAfterRecoveryCode);
    }
    _stageAfterRecoveryCode.clear();
}

// ---------------------------------------------------------------------------
// Login

bool AuthManager::login(const QString &id, const QString &password)
{
    if (_stage != QStringLiteral("login")) {
        return false;
    }
    if (id.isEmpty() || password.isEmpty()) {
        _setMessage(QStringLiteral("Enter your ID and PW."));
        return false;
    }

    Account *account = _store->find(id);
    FailureCounter &counter = account ? account->failures : _store->unknownIdCounter(id);

    if (counter.locked) {
        _log(QStringLiteral("LOGIN_REJECTED_LOCKED"), id, QStringLiteral("locked"),
             account ? QStringLiteral("reason=%1").arg(counter.lockReason) : QStringLiteral("unknown ID"));
        _save();
        _setMessage(_lockedMessage(counter.lockReason));
        return false;
    }

    bool ok = false;
    if (account) {
        ok = _verify(account->password, password);
    } else {
        // Same amount of work as a real check so response time does not reveal valid IDs
        PasswordRecord dummy;
        dummy.salt = QByteArray(kSaltBytes, '\x5a');
        dummy.iterations = _options.iterations;
        dummy.hash = QByteArray(kHashBytes, '\0');
        (void) _verify(dummy, password);
    }

    if (!ok) {
        if (account) {
            account->failuresSinceLastLogin++;
        }
        const FailOutcome outcome = _registerFailure(counter);
        _log(QStringLiteral("LOGIN_FAILURE"), id, QStringLiteral("failed"),
             QStringLiteral("%1consecutive=%2 daily=%3").arg(account ? QString() : QStringLiteral("unknown ID; "))
                 .arg(counter.consecutive).arg(counter.daily));
        if (outcome.lockedNow) {
            _log(account ? QStringLiteral("ACCOUNT_LOCKED") : QStringLiteral("UNKNOWN_ID_LOCKED"), id,
                 QStringLiteral("locked"), QStringLiteral("reason=%1").arg(outcome.lockReason));
        }
        _save();
        if (outcome.locked) {
            _setMessage(_lockedMessage(outcome.lockReason));
        } else {
            _setMessage(kMsgCheckIdPw.arg(outcome.remaining == 1 ? QStringLiteral("1 attempt remaining")
                                                                 : QStringLiteral("%1 attempts remaining").arg(outcome.remaining)));
        }
        return false;
    }

    // Success
    counter.consecutive = 0;
    _prevLoginUtc = account->lastLoginUtc;
    _prevLoginTerminal = account->lastLoginTerminal;
    _prevFailures = account->failuresSinceLastLogin;
    account->lastLoginUtc = _now();
    account->lastLoginTerminal = _options.terminal;
    account->failuresSinceLastLogin = 0;

    if (!account->mustChangePassword && account->password.setAtUtc.daysTo(_now()) >= kPasswordExpiryDays) {
        account->mustChangePassword = true;
        account->mustChangeReason = QStringLiteral("expired");
        _log(QStringLiteral("PASSWORD_EXPIRED"), id, QStringLiteral("change required"));
    }
    _log(QStringLiteral("LOGIN_SUCCESS"), id, QStringLiteral("ok"), QStringLiteral("role=%1").arg(roleToString(account->role)));
    _afterAuthenticated(*account);
    return true;
}

void AuthManager::_afterAuthenticated(Account &account)
{
    _authenticated = true;
    _currentId = account.id;
    _currentRole = account.role;
    _save();
    emit sessionChanged();
    emit accountsChanged();

    if (account.mustChangePassword) {
        const QString why = account.mustChangeReason == QStringLiteral("expired")
                                ? QStringLiteral("Your password has expired (%1 days). Set a new password to continue.").arg(kPasswordExpiryDays)
                                : QStringLiteral("Your password was reset by an administrator. Set a new password to continue.");
        _setStage(QStringLiteral("changePassword"), why);
    } else {
        _setStage(QStringLiteral("notice"), _noticeText(account));
    }
}

QString AuthManager::_noticeText(const Account &account) const
{
    QStringList lines;
    lines << QStringLiteral("Welcome, %1 (%2).").arg(account.id, account.role == Role::Admin ? QStringLiteral("Administrator") : QStringLiteral("User"));
    if (_prevLoginUtc.isValid()) {
        lines << QStringLiteral("Last successful login: %1 on %2").arg(localTime(_prevLoginUtc), _prevLoginTerminal);
    } else {
        lines << QStringLiteral("This is the first login of this account.");
    }
    lines << QStringLiteral("Failed login attempts since the last login: %1").arg(_prevFailures);

    const qint64 age = account.password.setAtUtc.daysTo(QDateTime::currentDateTimeUtc());
    const qint64 left = kPasswordExpiryDays - age;
    if (left <= kPasswordExpiryWarnDays) {
        lines << QString();
        lines << QStringLiteral("Your password expires in %1 day(s). Change it soon.").arg(std::max<qint64>(0, left));
    }
    if (account.role == Role::Admin && !_auditWarning.isEmpty()) {
        lines << QString();
        lines << QStringLiteral("Audit log warning: %1").arg(_auditWarning);
    }
    if (account.role == Role::Admin) {
        lines << QString();
        lines << QStringLiteral("Administrator accounts can monitor flights but cannot send flight commands.");
    }
    return lines.join(QLatin1Char('\n'));
}

void AuthManager::showRecovery()
{
    if (_stage == QStringLiteral("login")) {
        _setStage(QStringLiteral("recovery"),
                  QStringLiteral("Use the recovery code issued at setup to set a new password for an administrator account. "
                                 "All administrator accounts are unlocked and a new recovery code is issued."));
    }
}

void AuthManager::backToLogin()
{
    if (_stage == QStringLiteral("recovery")) {
        _setStage(QStringLiteral("login"));
    }
}

bool AuthManager::recoverAdmin(const QString &code, const QString &adminId, const QString &password, const QString &confirm)
{
    if (_stage != QStringLiteral("recovery")) {
        return false;
    }
    FailureCounter &counter = _store->unknownIdCounter(kRecoveryCounterId);
    if (counter.locked) {
        _log(QStringLiteral("RECOVERY_REJECTED_LOCKED"), adminId, QStringLiteral("locked"));
        _save();
        _setMessage(QStringLiteral("Recovery is locked after repeated failures. ") + kMsgContactAdmin);
        return false;
    }

    Account *account = _store->find(adminId);
    const bool codeOk = _verify(_store->recoveryCode, normalizeRecoveryCode(code));
    if (!codeOk || !account || account->role != Role::Admin) {
        const FailOutcome outcome = _registerFailure(counter);
        _log(QStringLiteral("RECOVERY_FAILURE"), adminId, QStringLiteral("failed"),
             codeOk ? QStringLiteral("not an administrator ID") : QStringLiteral("wrong recovery code"));
        _save();
        _setMessage(outcome.locked ? QStringLiteral("Recovery is locked after repeated failures. ") + kMsgContactAdmin
                                   : QStringLiteral("Recovery code or administrator ID is not correct. (%1 attempt(s) remaining)").arg(outcome.remaining));
        return false;
    }
    if (password != confirm) {
        _setMessage(QStringLiteral("PW and PW confirm do not match."), { QStringLiteral("PW and PW confirm do not match.") });
        return false;
    }
    QStringList violations = _policy.checkPassword(adminId, password);
    if (violations.isEmpty() && _isReused(*account, password)) {
        violations << QStringLiteral("Password must not reuse a password used in the last 12 months.");
    }
    if (!violations.isEmpty()) {
        _setMessage(violations.first(), violations);
        return false;
    }

    counter.consecutive = 0;
    _setPassword(*account, password);
    account->mustChangePassword = false;
    account->mustChangeReason.clear();
    for (Account &a : _store->accounts()) {
        if (a.role == Role::Admin && a.failures.locked) {
            a.failures.locked = false;
            a.failures.consecutive = 0;
            a.failures.lockReason.clear();
            _log(QStringLiteral("ACCOUNT_UNLOCKED"), a.id, QStringLiteral("ok"), QStringLiteral("by recovery code"));
        }
    }
    _log(QStringLiteral("RECOVERY_USED"), adminId, QStringLiteral("ok"), QStringLiteral("administrator password set"));
    _save();
    emit accountsChanged();

    _recoveryCodePlain = _issueRecoveryCode();
    _stageAfterRecoveryCode = QStringLiteral("Recovery complete. Log in with the new password.");
    _setStage(QStringLiteral("recoveryCode"));
    return true;
}

// ---------------------------------------------------------------------------
// After login

bool AuthManager::changePassword(const QString &currentPassword, const QString &newPassword, const QString &confirm)
{
    if (_stage != QStringLiteral("changePassword") || !_authenticated) {
        return false;
    }
    Account *account = _store->find(_currentId);
    if (!account) {
        return false;
    }
    if (!_verify(account->password, currentPassword)) {
        _log(QStringLiteral("PASSWORD_CHANGE_FAILURE"), _currentId, QStringLiteral("failed"), QStringLiteral("wrong current password"));
        _save();
        _setMessage(QStringLiteral("Current PW is not correct."), { QStringLiteral("Current PW is not correct.") });
        return false;
    }
    if (newPassword != confirm) {
        _setMessage(QStringLiteral("PW and PW confirm do not match."), { QStringLiteral("PW and PW confirm do not match.") });
        return false;
    }
    QStringList violations = _policy.checkPassword(_currentId, newPassword);
    if (violations.isEmpty() && _isReused(*account, newPassword)) {
        violations << QStringLiteral("Password must not reuse a password used in the last 12 months.");
    }
    if (!violations.isEmpty()) {
        _setMessage(violations.first(), violations);
        return false;
    }

    _setPassword(*account, newPassword);
    const QString reason = account->mustChangeReason.isEmpty() ? QStringLiteral("voluntary") : account->mustChangeReason;
    account->mustChangePassword = false;
    account->mustChangeReason.clear();
    _log(QStringLiteral("PASSWORD_CHANGED"), _currentId, QStringLiteral("ok"), QStringLiteral("reason=%1").arg(reason));
    _save();
    _setStage(QStringLiteral("notice"), _noticeText(*account));
    return true;
}

void AuthManager::requestPasswordChange()
{
    if (_authenticated && (_stage == QStringLiteral("notice") || _stage == QStringLiteral("adminTools"))) {
        _setStage(QStringLiteral("changePassword"), QStringLiteral("Enter your current PW and a new PW."));
    }
}

void AuthManager::continueToApplication()
{
    if (!_authenticated || _stage != QStringLiteral("notice")) {
        return;
    }
    _log(QStringLiteral("SESSION_START"), _currentId, QStringLiteral("ok"), QStringLiteral("role=%1").arg(roleToString(_currentRole)));
    _save();
    emit finished(true);
}

void AuthManager::recordLogout()
{
    if (_authenticated) {
        _log(QStringLiteral("LOGOUT"), _currentId, QStringLiteral("ok"), QStringLiteral("application closed"));
        _save();
        _authenticated = false;
        emit sessionChanged();
    }
}

// ---------------------------------------------------------------------------
// Idle lock

void AuthManager::lockSession(const QString &reason)
{
    if (!_authenticated || _sessionLocked) {
        return;
    }
    _sessionLocked = true;
    _sessionTerminated = false;
    _lockText = QStringLiteral("%1\nEnter the PW of '%2' to continue.").arg(reason, _currentId);
    _log(QStringLiteral("SESSION_LOCKED"), _currentId, QStringLiteral("ok"), reason);
    _save();
    _setMessage(QString());
    emit sessionLockedChanged();
}

bool AuthManager::unlockSession(const QString &password)
{
    if (!_authenticated || !_sessionLocked || _sessionTerminated) {
        return false;
    }
    Account *account = _store->find(_currentId);
    if (!account) {
        return false;
    }
    if (account->failures.locked) {
        _sessionTerminated = true;
        _log(QStringLiteral("SESSION_UNLOCK_REJECTED_LOCKED"), _currentId, QStringLiteral("locked"));
        _save();
        _lockText = _lockedMessage(account->failures.lockReason) + QStringLiteral("\nThe application will close.");
        emit sessionLockedChanged();
        return false;
    }
    if (password.isEmpty()) {
        _setMessage(QStringLiteral("Enter your PW."));
        return false;
    }
    if (!_verify(account->password, password)) {
        account->failuresSinceLastLogin++;
        const FailOutcome outcome = _registerFailure(account->failures);
        _log(QStringLiteral("SESSION_UNLOCK_FAILURE"), _currentId, QStringLiteral("failed"),
             QStringLiteral("consecutive=%1 daily=%2").arg(account->failures.consecutive).arg(account->failures.daily));
        if (outcome.lockedNow) {
            _log(QStringLiteral("ACCOUNT_LOCKED"), _currentId, QStringLiteral("locked"), QStringLiteral("reason=%1").arg(outcome.lockReason));
        }
        _save();
        if (outcome.locked) {
            _sessionTerminated = true;
            _lockText = _lockedMessage(outcome.lockReason) + QStringLiteral("\nThe application will close.");
            _setMessage(QString());
            emit sessionLockedChanged();
        } else {
            _setMessage(QStringLiteral("Unlock Failed \u2013 Check your PW. (%1)")
                            .arg(outcome.remaining == 1 ? QStringLiteral("1 attempt remaining")
                                                        : QStringLiteral("%1 attempts remaining").arg(outcome.remaining)));
        }
        return false;
    }
    account->failures.consecutive = 0;
    _sessionLocked = false;
    _lockText.clear();
    _log(QStringLiteral("SESSION_UNLOCKED"), _currentId, QStringLiteral("ok"));
    _save();
    _setMessage(QString());
    emit sessionLockedChanged();
    return true;
}

void AuthManager::requestExit()
{
    _log(QStringLiteral("APPLICATION_EXIT_REQUESTED"), _currentId, QStringLiteral("ok"),
         _sessionLocked ? QStringLiteral("from lock screen") : QString());
    _save();
    emit exitRequested();
}

void AuthManager::cancel()
{
    if (_authenticated) {
        _log(QStringLiteral("LOGOUT"), _currentId, QStringLiteral("ok"), QStringLiteral("closed before entering the application"));
        _authenticated = false;
    }
    _save();
    emit finished(false);
}

// ---------------------------------------------------------------------------
// Administrator tools (SR-05, SR-09)

bool AuthManager::_requireAdminSession()
{
    if (!isAdmin()) {
        _setMessage(QStringLiteral("Administrator rights are required."));
        return false;
    }
    return true;
}

void AuthManager::showAdminTools()
{
    if (isAdmin() && _stage == QStringLiteral("notice")) {
        _setStage(QStringLiteral("adminTools"));
        emit accountsChanged();
    }
}

void AuthManager::closeAdminTools()
{
    if (_stage == QStringLiteral("adminTools")) {
        _setStage(QStringLiteral("notice"), _noticeText(*_store->find(_currentId)));
    }
}

QVariantList AuthManager::accounts() const
{
    QVariantList list;
    for (const Account &a : _store->accounts()) {
        QVariantMap m;
        m[QStringLiteral("id")] = a.id;
        m[QStringLiteral("role")] = a.role == Role::Admin ? QStringLiteral("Administrator") : QStringLiteral("User");
        m[QStringLiteral("locked")] = a.failures.locked;
        m[QStringLiteral("lockReason")] = a.failures.lockReason == QStringLiteral("daily") ? QStringLiteral("daily limit")
                                         : a.failures.lockReason == QStringLiteral("consecutive") ? QStringLiteral("3 consecutive failures")
                                         : QString();
        m[QStringLiteral("mustChange")] = a.mustChangePassword;
        m[QStringLiteral("lastLogin")] = localTime(a.lastLoginUtc);
        m[QStringLiteral("passwordAgeDays")] = a.password.setAtUtc.daysTo(QDateTime::currentDateTimeUtc());
        m[QStringLiteral("isSelf")] = a.id == _currentId;
        list.append(m);
    }
    return list;
}

bool AuthManager::unlockAccount(const QString &id)
{
    if (!_requireAdminSession()) {
        return false;
    }
    Account *account = _store->find(id);
    if (!account) {
        return false;
    }
    account->failures.locked = false;
    account->failures.lockReason.clear();
    account->failures.consecutive = 0;
    _log(QStringLiteral("ACCOUNT_UNLOCKED"), id, QStringLiteral("ok"), QStringLiteral("by %1").arg(_currentId));
    _save();
    emit accountsChanged();
    _setMessage(QStringLiteral("Account '%1' unlocked.").arg(id));
    return true;
}

bool AuthManager::resetPassword(const QString &id, const QString &password, const QString &confirm)
{
    if (!_requireAdminSession()) {
        return false;
    }
    Account *account = _store->find(id);
    if (!account) {
        return false;
    }
    if (id == _currentId) {
        _setMessage(QStringLiteral("Use Change My PW for your own account."));
        return false;
    }
    if (password != confirm) {
        _setMessage(QStringLiteral("PW and PW confirm do not match."), { QStringLiteral("PW and PW confirm do not match.") });
        return false;
    }
    QStringList violations = _policy.checkPassword(id, password);
    if (violations.isEmpty() && _isReused(*account, password)) {
        violations << QStringLiteral("Password must not reuse a password used in the last 12 months.");
    }
    if (!violations.isEmpty()) {
        _setMessage(violations.first(), violations);
        return false;
    }
    _setPassword(*account, password);
    account->mustChangePassword = true;
    account->mustChangeReason = QStringLiteral("reset");
    account->failures.locked = false;
    account->failures.lockReason.clear();
    account->failures.consecutive = 0;
    _log(QStringLiteral("PASSWORD_RESET"), id, QStringLiteral("ok"), QStringLiteral("by %1; change required at next login").arg(_currentId));
    _save();
    emit accountsChanged();
    _setMessage(QStringLiteral("Temporary PW set for '%1'. It must be changed at the next login.").arg(id));
    return true;
}

QVariantList AuthManager::auditEntries(int maxEntries) const
{
    QVariantList list;
    if (!isAdmin()) {
        return list;
    }
    bool valid = true;
    const QList<AuditEntry> entries = _audit->readAll(&valid, nullptr);
    const int total = static_cast<int>(entries.size());
    const int start = std::max(0, total - std::max(1, maxEntries));
    for (int i = total - 1; i >= start; --i) {
        const AuditEntry &e = entries.at(i);
        QVariantMap m;
        m[QStringLiteral("seq")] = e.seq;
        m[QStringLiteral("time")] = localTime(e.timeUtc);
        m[QStringLiteral("type")] = e.type;
        m[QStringLiteral("account")] = e.account;
        m[QStringLiteral("result")] = e.result;
        m[QStringLiteral("detail")] = e.detail;
        list.append(m);
    }
    return list;
}

QString AuthManager::auditIntegrityText() const
{
    if (!isAdmin()) {
        return QString();
    }
    bool valid = true;
    QString problem;
    const QList<AuditEntry> entries = _audit->readAll(&valid, &problem);
    if (!valid) {
        return QStringLiteral("Integrity check FAILED: %1").arg(problem);
    }
    if (_store->auditSeq > _audit->lastSeq()) {
        return QStringLiteral("Integrity check FAILED: records missing at the end of the log.");
    }
    return QStringLiteral("Integrity check passed (%1 records).").arg(static_cast<int>(entries.size()));
}

bool AuthManager::exportAuditLog(const QUrl &fileUrl)
{
    if (!_requireAdminSession()) {
        return false;
    }
    const QString path = fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString();
    QString error;
    if (!_audit->exportCsv(path, &error)) {
        _setMessage(QStringLiteral("Export failed: %1").arg(error));
        return false;
    }
    _log(QStringLiteral("AUDIT_EXPORTED"), _currentId, QStringLiteral("ok"), path);
    _save();
    _setMessage(QStringLiteral("Audit log exported to %1").arg(path));
    return true;
}

} // namespace KevaSecurity
