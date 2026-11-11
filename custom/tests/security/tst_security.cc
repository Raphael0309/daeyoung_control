/****************************************************************************
 *
 * Unit tests for the KEVA security module (MIL edition)
 *
 ****************************************************************************/

#include "AccessPolicy.h"
#include "AuthManager.h"
#include "AuditLog.h"
#include "PasswordPolicy.h"
#include "SecurityConfig.h"
#include "SecurityCrypto.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QSettings>
#include <QtCore/QTemporaryDir>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

using namespace KevaSecurity;

namespace {

const QString kAdmin1 = QStringLiteral("secoff1");
const QString kAdmin2 = QStringLiteral("syseng2");
const QString kUser = QStringLiteral("pilotKim");
const QString kPwA1 = QStringLiteral("Kv7#mQ2!pZx");
const QString kPwA2 = QStringLiteral("Tn4$wR8@jLb");
const QString kPwU = QStringLiteral("Hq5&zM3^vYc");
const QString kPwNew1 = QStringLiteral("Gd6*eX9%uWk");
const QString kPwNew2 = QStringLiteral("Bp2!fN7#sJr");
const QString kPwNew3 = QStringLiteral("Rw9@cL4$kTn");

struct Env
{
    QTemporaryDir dir;
    QDateTime now = QDateTime(QDate(2026, 11, 9), QTime(1, 0), Qt::UTC);

    AuthManager::Options options()
    {
        AuthManager::Options o;
        o.dataDirectory = dir.path();
        o.terminal = QStringLiteral("TEST-PC");
        o.productName = QStringLiteral("Daeyoung Ground Control");
        o.productVersion = QStringLiteral("1.0.1");
        o.companyName = QStringLiteral("Daeyoung M&S Co.");
        o.eventSourceName = QStringLiteral("DaeyoungGCS-Test");
        o.dictionaryPath = QStringLiteral(KEVA_TEST_DICTIONARY);
        o.commonPasswordsPath = QStringLiteral(KEVA_TEST_COMMON_PASSWORDS);
        o.iterations = 1000;
        o.useInstallSentinel = false;
        QDateTime *clock = &now;
        o.clock = [clock]() { return *clock; };
        return o;
    }

    std::unique_ptr<AuthManager> make()
    {
        auto a = std::make_unique<AuthManager>(options());
        a->start();
        return a;
    }
};

void completeSetup(AuthManager &a)
{
    QCOMPARE(a.stage(), QStringLiteral("setupAdmin"));
    QVERIFY2(a.createAccount(kAdmin1, kPwA1, kPwA1), qPrintable(a.messages().join('|')));
    QCOMPARE(a.stage(), QStringLiteral("setupAdmin"));
    QVERIFY2(a.createAccount(kAdmin2, kPwA2, kPwA2), qPrintable(a.messages().join('|')));
    QCOMPARE(a.stage(), QStringLiteral("recoveryCode"));
    QVERIFY(!a.recoveryCode().isEmpty());
    a.acknowledgeRecoveryCode();
    QCOMPARE(a.stage(), QStringLiteral("setupUser"));
    QVERIFY2(a.createAccount(kUser, kPwU, kPwU), qPrintable(a.messages().join('|')));
    QCOMPARE(a.stage(), QStringLiteral("login"));
}

} // namespace

class TestSecurity : public QObject
{
    Q_OBJECT

private slots:
    // Crypto
    void pbkdf2Vectors();
    void pbkdf2PlatformMatchesPortable();
    void protectRoundTrip();
    void selfTest();

    // Policy
    void policyAcceptsGoodPasswords();
    void policyRejects_data();
    void policyRejects();
    void idRules_data();
    void idRules();

    // Flows
    void setupFlowAndPersistence();
    void setupRejectsBadInput();
    void loginSuccessAndNotice();
    void consecutiveLockout();
    void dailyLockoutAcrossSuccess();
    void dailyCounterResetsNextDay();
    void unknownIdBehavesLikeRealId();
    void lockedAccountRejectsCorrectPassword();
    void adminUnlockAndReset();
    void passwordExpiry();
    void passwordReuseRejected();
    void clockRollbackDoesNotResetDailyLimit();
    void recoveryCodeFlow();
    void auditChainDetectsTampering();
    void missingStoreAfterSetupIsFatal();
    void setupMarkerBlocksSilentReset();
    void userIsNotAdmin();

    // Session lock and roles
    void sessionLockAndUnlock();
    void sessionUnlockFailuresLockAccount();
    void accessPolicy();
};

// ---------------------------------------------------------------------------

void TestSecurity::pbkdf2Vectors()
{
    QCOMPARE(SecurityCrypto::pbkdf2Sha256Portable("password", "salt", 1, 32).toHex(),
             QByteArray("120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b"));
    QCOMPARE(SecurityCrypto::pbkdf2Sha256Portable("password", "salt", 2, 32).toHex(),
             QByteArray("ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43"));
    QCOMPARE(SecurityCrypto::pbkdf2Sha256("password", "salt", 4096, 32).toHex(),
             QByteArray("c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a"));
    QCOMPARE(SecurityCrypto::pbkdf2Sha256Portable("passwordPASSWORDpassword", "saltSALTsaltSALTsaltSALTsaltSALTsalt", 4096, 40).toHex(),
             QByteArray("348c89dbcbd32b2f32d814b8116e84cf2b17347ebc1800181c4e2a1fb8dd53e1c635518c7dac47e9"));
}

void TestSecurity::pbkdf2PlatformMatchesPortable()
{
    for (int i = 0; i < 20; ++i) {
        const QByteArray pw = SecurityCrypto::randomBytes(1 + i);
        const QByteArray salt = SecurityCrypto::randomBytes(16);
        QCOMPARE(SecurityCrypto::pbkdf2Sha256(pw, salt, 100 + i, 32), SecurityCrypto::pbkdf2Sha256Portable(pw, salt, 100 + i, 32));
    }
}

void TestSecurity::protectRoundTrip()
{
    bool ok = false;
    const QByteArray plain("secret account data");
    const QByteArray blob = SecurityCrypto::protect(plain, &ok);
    QVERIFY(ok);
    bool ok2 = false;
    QCOMPARE(SecurityCrypto::unprotect(blob, &ok2), plain);
    QVERIFY(ok2);
    if (SecurityCrypto::platformProtectionAvailable()) {
        QVERIFY(!blob.contains(plain));
        QByteArray tampered = blob;
        tampered[tampered.size() / 2] = static_cast<char>(tampered[tampered.size() / 2] ^ 0x01);
        bool ok3 = true;
        SecurityCrypto::unprotect(tampered, &ok3);
        QVERIFY(!ok3);
    }
}

void TestSecurity::selfTest()
{
    QString error;
    QVERIFY2(SecurityCrypto::selfTest(&error), qPrintable(error));
}

// ---------------------------------------------------------------------------

void TestSecurity::policyAcceptsGoodPasswords()
{
    PasswordPolicy p;
    QVERIFY(p.loadDictionary(QStringLiteral(KEVA_TEST_DICTIONARY)));
    QVERIFY(p.loadCommonPasswords(QStringLiteral(KEVA_TEST_COMMON_PASSWORDS)));
    for (const QString &pw : { kPwA1, kPwA2, kPwU, kPwNew1, kPwNew2, kPwNew3 }) {
        QVERIFY2(p.checkPassword(kUser, pw).isEmpty(), qPrintable(pw + ": " + p.checkPassword(kUser, pw).join('|')));
    }
}

void TestSecurity::policyRejects_data()
{
    QTest::addColumn<QString>("password");
    QTest::addColumn<QString>("expectedFragment");
    QTest::newRow("short") << "Kv7#mQ2!" << "at least 9";
    QTest::newRow("no special") << "Kv7xmQ2wpZ" << "combine letters";
    QTest::newRow("no digit") << "Kv#xmQ!wpZ" << "combine letters";
    QTest::newRow("no letter") << "17#4%92!58" << "combine letters";
    QTest::newRow("space") << "Kv7# mQ2!pZ" << "no spaces";
    QTest::newRow("korean") << "Kv7#mQ2!pZ한" << "no spaces";
    QTest::newRow("contains id") << "pilotKim#7x" << "user ID";
    QTest::newRow("common") << "P@ssw0rd!1" << "dictionary";
    QTest::newRow("dictionary") << "Drone#2048" << "dictionary";
    QTest::newRow("dictionary mixed") << "8Fal#con9!" << "dictionary";
    QTest::newRow("repeat") << "Kv7#mmmQ2!pZ" << "repeat";
    QTest::newRow("repeat case") << "Kv7#mMmQ2!pZ" << "repeat";
    QTest::newRow("ascending digits") << "Kv7#m123!pZ" << "sequential";
    QTest::newRow("descending letters") << "Kv7#mQ2!zyx" << "sequential";
    QTest::newRow("keyboard") << "Kv7#asdQ2!p" << "sequential";
    QTest::newRow("keyboard reversed") << "Kv7#ewqZ2!p" << "sequential";
}

void TestSecurity::policyRejects()
{
    QFETCH(QString, password);
    QFETCH(QString, expectedFragment);
    PasswordPolicy p;
    p.loadDictionary(QStringLiteral(KEVA_TEST_DICTIONARY));
    p.loadCommonPasswords(QStringLiteral(KEVA_TEST_COMMON_PASSWORDS));
    const QStringList errors = p.checkPassword(kUser, password);
    QVERIFY2(!errors.isEmpty(), qPrintable(password));
    QVERIFY2(errors.join('|').contains(expectedFragment), qPrintable(errors.join('|')));
}

void TestSecurity::idRules_data()
{
    QTest::addColumn<QString>("id");
    QTest::addColumn<bool>("valid");
    QTest::newRow("ok") << "pilotKim" << true;
    QTest::newRow("ok digits") << "op2026" << true;
    QTest::newRow("short") << "abc" << false;
    QTest::newRow("long") << "abcdefghijklmnopqrstu" << false;
    QTest::newRow("symbol") << "pilot_kim" << false;
    QTest::newRow("root") << "root" << false;
    QTest::newRow("Administrator") << "Administrator" << false;
    QTest::newRow("admin fragment") << "kimAdmin1" << false;
    QTest::newRow("guest") << "Guest" << false;
}

void TestSecurity::idRules()
{
    QFETCH(QString, id);
    QFETCH(bool, valid);
    PasswordPolicy p;
    QCOMPARE(p.checkId(id).isEmpty(), valid);
}

// ---------------------------------------------------------------------------

void TestSecurity::setupFlowAndPersistence()
{
    Env env;
    {
        auto a = env.make();
        completeSetup(*a);
        QVERIFY(QFile::exists(env.dir.filePath("accounts.dat")));
        QVERIFY(QFile::exists(env.dir.filePath("audit.log")));
    }
    // Restart: goes straight to login, accounts survive
    auto b = env.make();
    QCOMPARE(b->stage(), QStringLiteral("login"));
    QCOMPARE(b->store()->accounts().size(), 3);
    QCOMPARE(b->store()->count(Role::Admin), 2);
    // Passwords are never stored in clear
    QFile f(env.dir.filePath("accounts.dat"));
    QVERIFY(f.open(QIODevice::ReadOnly));
    QVERIFY(!f.readAll().contains(kPwA1.toUtf8()));
}

void TestSecurity::setupRejectsBadInput()
{
    Env env;
    auto a = env.make();
    QVERIFY(!a->createAccount(QStringLiteral("admin"), kPwA1, kPwA1));
    QVERIFY(a->message().contains("not allowed"));
    QVERIFY(!a->createAccount(kAdmin1, kPwA1, kPwA2));
    QVERIFY(a->message().contains("do not match"));
    QVERIFY(!a->createAccount(kAdmin1, QStringLiteral("abc"), QStringLiteral("abc")));
    QVERIFY(a->messages().size() >= 2);
    QVERIFY(a->createAccount(kAdmin1, kPwA1, kPwA1));
    QVERIFY(!a->createAccount(QStringLiteral("SECOFF1"), kPwA2, kPwA2));
    QVERIFY(a->message().contains("already in use"));
}

void TestSecurity::loginSuccessAndNotice()
{
    Env env;
    auto a = env.make();
    completeSetup(*a);
    QVERIFY(a->login(kUser, kPwU));
    QCOMPARE(a->stage(), QStringLiteral("notice"));
    QVERIFY(a->infoText().contains("first login"));
    QSignalSpy done(a.get(), &AuthManager::finished);
    a->continueToApplication();
    QCOMPARE(done.count(), 1);
    QCOMPARE(done.first().first().toBool(), true);
    a->recordLogout();

    // Second session shows the previous login and failures in between
    env.now = env.now.addSecs(3600);
    auto b = env.make();
    QVERIFY(!b->login(kUser, QStringLiteral("Wrong#Pw9x")));
    QVERIFY(b->login(kUser, kPwU));
    QVERIFY(b->infoText().contains("Last successful login"));
    QVERIFY(b->infoText().contains("TEST-PC"));
    QVERIFY(b->infoText().contains("since the last login: 1"));
}

void TestSecurity::consecutiveLockout()
{
    Env env;
    auto a = env.make();
    completeSetup(*a);
    QVERIFY(!a->login(kUser, QStringLiteral("bad1")));
    QVERIFY2(a->message().contains("2 attempts remaining"), qPrintable(a->message()));
    QVERIFY(!a->login(kUser, QStringLiteral("bad2")));
    QVERIFY2(a->message().contains("1 attempt remaining"), qPrintable(a->message()));
    QVERIFY(!a->login(kUser, QStringLiteral("bad3")));
    QCOMPARE(a->message(), QStringLiteral("Login Failed – Account has been locked. Please contact the System Administrator."));
    QVERIFY(a->store()->find(kUser)->failures.locked);
}

void TestSecurity::dailyLockoutAcrossSuccess()
{
    Env env;
    auto a = env.make();
    completeSetup(*a);
    // 2 failures, success, 2 failures, success, then failures 5 and 6 -> daily lock
    for (int round = 0; round < 2; ++round) {
        QVERIFY(!a->login(kUser, QStringLiteral("bad")));
        QVERIFY(!a->login(kUser, QStringLiteral("bad")));
        QVERIFY(a->login(kUser, kPwU));
        a->recordLogout();
        a = env.make();
    }
    QVERIFY(!a->login(kUser, QStringLiteral("bad")));
    // remaining = min(3-1, 6-5) = 1
    QVERIFY2(a->message().contains("1 attempt remaining"), qPrintable(a->message()));
    QVERIFY(!a->login(kUser, QStringLiteral("bad")));
    QVERIFY2(a->message().contains("Daily limit has been exceeded"), qPrintable(a->message()));
    QCOMPARE(a->store()->find(kUser)->failures.lockReason, QStringLiteral("daily"));
}

void TestSecurity::dailyCounterResetsNextDay()
{
    Env env;
    auto a = env.make();
    completeSetup(*a);
    QVERIFY(!a->login(kUser, QStringLiteral("bad")));
    QVERIFY(!a->login(kUser, QStringLiteral("bad")));
    QVERIFY(a->login(kUser, kPwU));
    a->recordLogout();
    env.now = env.now.addDays(1);
    a = env.make();
    QVERIFY(!a->login(kUser, QStringLiteral("bad")));
    QCOMPARE(a->store()->find(kUser)->failures.daily, 1);
}

void TestSecurity::unknownIdBehavesLikeRealId()
{
    Env env;
    auto a = env.make();
    completeSetup(*a);
    QVERIFY(!a->login(QStringLiteral("nobody99"), QStringLiteral("bad")));
    const QString unknownMsg = a->message();
    QVERIFY(!a->login(kAdmin2, QStringLiteral("bad")));
    QCOMPARE(a->message(), unknownMsg);
    QVERIFY(!a->login(QStringLiteral("nobody99"), QStringLiteral("bad")));
    QVERIFY(!a->login(QStringLiteral("nobody99"), QStringLiteral("bad")));
    QVERIFY(a->message().contains("Account has been locked"));
}

void TestSecurity::lockedAccountRejectsCorrectPassword()
{
    Env env;
    auto a = env.make();
    completeSetup(*a);
    for (int i = 0; i < 3; ++i) {
        a->login(kUser, QStringLiteral("bad"));
    }
    QVERIFY(!a->login(kUser, kPwU));
    QVERIFY(a->message().contains("locked"));
    QCOMPARE(a->stage(), QStringLiteral("login"));
}

void TestSecurity::adminUnlockAndReset()
{
    Env env;
    auto a = env.make();
    completeSetup(*a);
    for (int i = 0; i < 3; ++i) {
        a->login(kUser, QStringLiteral("bad"));
    }
    QVERIFY(!a->unlockAccount(kUser));     // not logged in
    QVERIFY(a->login(kAdmin1, kPwA1));
    QVERIFY(a->isAdmin());
    a->showAdminTools();
    QCOMPARE(a->stage(), QStringLiteral("adminTools"));
    QVERIFY(a->unlockAccount(kUser));
    QVERIFY(!a->store()->find(kUser)->failures.locked);
    QVERIFY(!a->resetPassword(kAdmin1, kPwNew1, kPwNew1));     // not for self
    QVERIFY(a->resetPassword(kUser, kPwNew1, kPwNew1));
    QVERIFY(a->store()->find(kUser)->mustChangePassword);
    QVERIFY(!a->auditEntries(50).isEmpty());
    QVERIFY(a->auditIntegrityText().contains("passed"));
    a->recordLogout();

    auto b = env.make();
    QVERIFY(b->login(kUser, kPwNew1));
    QCOMPARE(b->stage(), QStringLiteral("changePassword"));
    QVERIFY(!b->changePassword(kPwNew1, kPwNew1, kPwNew1));     // reuse of the temporary password
    QVERIFY(b->message().contains("reuse"));
    QVERIFY(b->changePassword(kPwNew1, kPwNew2, kPwNew2));
    QCOMPARE(b->stage(), QStringLiteral("notice"));
    QVERIFY(b->auditEntries(10).isEmpty());     // users cannot read the audit log
}

void TestSecurity::passwordExpiry()
{
    Env env;
    auto a = env.make();
    completeSetup(*a);
    env.now = env.now.addDays(kPasswordExpiryDays - kPasswordExpiryWarnDays + 1);
    a = env.make();
    QVERIFY(a->login(kUser, kPwU));
    QCOMPARE(a->stage(), QStringLiteral("notice"));
    a->recordLogout();

    env.now = env.now.addDays(kPasswordExpiryWarnDays);
    a = env.make();
    QVERIFY(a->login(kUser, kPwU));
    QCOMPARE(a->stage(), QStringLiteral("changePassword"));
    QVERIFY(a->infoText().contains("expired"));
    QVERIFY(a->changePassword(kPwU, kPwNew1, kPwNew1));
}

void TestSecurity::passwordReuseRejected()
{
    Env env;
    auto a = env.make();
    completeSetup(*a);
    QVERIFY(a->login(kUser, kPwU));
    a->requestPasswordChange();
    QVERIFY(a->changePassword(kPwU, kPwNew1, kPwNew1));
    a->requestPasswordChange();
    QVERIFY(a->changePassword(kPwNew1, kPwNew2, kPwNew2));
    a->requestPasswordChange();
    QVERIFY(!a->changePassword(kPwNew2, kPwU, kPwU));       // used 0 days ago
    QVERIFY(a->message().contains("reuse"));
    a->recordLogout();

    // More than a year after it was replaced, the first password may be used again
    env.now = env.now.addDays(kPasswordHistoryDays + 2);
    a = env.make();
    QVERIFY(a->login(kUser, kPwNew2));
    QCOMPARE(a->stage(), QStringLiteral("changePassword"));      // expired
    QVERIFY(!a->changePassword(kPwNew2, kPwNew2, kPwNew2));
    QVERIFY2(a->changePassword(kPwNew2, kPwU, kPwU), qPrintable(a->messages().join('|')));
}

void TestSecurity::clockRollbackDoesNotResetDailyLimit()
{
    Env env;
    auto a = env.make();
    completeSetup(*a);
    env.now = QDateTime(QDate(2026, 11, 9), QTime(14, 0), Qt::UTC);
    for (int i = 0; i < 2; ++i) {
        a->login(kUser, QStringLiteral("bad"));
    }
    QVERIFY(a->login(kUser, kPwU));
    a->recordLogout();
    // Turn the clock back a day: the stored daily count must still apply
    env.now = env.now.addDays(-1);
    a = env.make();
    a->login(kUser, QStringLiteral("bad"));
    QCOMPARE(a->store()->find(kUser)->failures.daily, 3);
    bool found = false;
    for (const AuditEntry &e : a->audit()->readAll(nullptr, nullptr)) {
        found = found || e.type == QStringLiteral("CLOCK_ROLLBACK");
    }
    QVERIFY(found);
}

void TestSecurity::recoveryCodeFlow()
{
    Env env;
    auto a = std::make_unique<AuthManager>(env.options());
    a->start();
    QVERIFY(a->createAccount(kAdmin1, kPwA1, kPwA1));
    QVERIFY(a->createAccount(kAdmin2, kPwA2, kPwA2));
    const QString code = a->recoveryCode();
    QCOMPARE(code.size(), kRecoveryCodeGroups * kRecoveryCodeGroupLength + kRecoveryCodeGroups - 1);
    a->acknowledgeRecoveryCode();
    QVERIFY(a->recoveryCode().isEmpty());
    QVERIFY(a->createAccount(kUser, kPwU, kPwU));

    // Lock both administrators
    for (const QString &id : { kAdmin1, kAdmin2 }) {
        for (int i = 0; i < 3; ++i) {
            a->login(id, QStringLiteral("bad"));
        }
    }
    a->showRecovery();
    QCOMPARE(a->stage(), QStringLiteral("recovery"));
    QVERIFY(!a->recoverAdmin(QStringLiteral("AAAA-BBBB-CCCC-DDDD-EEEE"), kAdmin1, kPwNew1, kPwNew1));
    QVERIFY(!a->recoverAdmin(code, kUser, kPwNew1, kPwNew1));       // not an administrator
    QVERIFY(a->recoverAdmin(code.toLower().remove('-'), kAdmin1, kPwNew1, kPwNew1));
    QCOMPARE(a->stage(), QStringLiteral("recoveryCode"));
    const QString newCode = a->recoveryCode();
    QVERIFY(newCode != code);
    a->acknowledgeRecoveryCode();
    QCOMPARE(a->stage(), QStringLiteral("login"));
    QVERIFY(!a->store()->find(kAdmin2)->failures.locked);
    QVERIFY(a->login(kAdmin1, kPwNew1));

    // The old code no longer works
    a->recordLogout();
    a = env.make();
    a->showRecovery();
    QVERIFY(!a->recoverAdmin(code, kAdmin1, kPwNew2, kPwNew2));
}

void TestSecurity::auditChainDetectsTampering()
{
    Env env;
    {
        auto a = env.make();
        completeSetup(*a);
        a->login(kUser, QStringLiteral("bad"));
        QVERIFY(a->login(kUser, kPwU));
        a->recordLogout();
    }
    AuditLog log(env.dir.path(), QStringLiteral("TEST-PC"), QString());
    bool valid = false;
    const QList<AuditEntry> entries = log.readAll(&valid, nullptr);
    QVERIFY(valid);
    QVERIFY(entries.size() >= 8);

    // Delete one line in the middle
    QFile f(env.dir.filePath("audit.log"));
    QVERIFY(f.open(QIODevice::ReadOnly));
    QList<QByteArray> lines = f.readAll().split('\n');
    f.close();
    lines.removeAt(3);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(lines.join('\n'));
    f.close();
    log.readAll(&valid, nullptr);
    QVERIFY(!valid);

    // Admin sees the problem
    auto b = env.make();
    QVERIFY(b->login(kAdmin1, kPwA1));
    QVERIFY(b->infoText().contains("Audit log warning"));
    QVERIFY(b->auditIntegrityText().contains("FAILED"));
}

void TestSecurity::missingStoreAfterSetupIsFatal()
{
    Env env;
    {
        auto a = env.make();
        completeSetup(*a);
    }
    QVERIFY(QFile::remove(env.dir.filePath("accounts.dat")));
    auto b = env.make();
    QCOMPARE(b->stage(), QStringLiteral("fatal"));
    QVERIFY(!b->createAccount(kAdmin1, kPwA1, kPwA1));
    QVERIFY(!b->login(kUser, kPwU));
    QSignalSpy done(b.get(), &AuthManager::finished);
    b->cancel();
    QCOMPARE(done.first().first().toBool(), false);
}

void TestSecurity::setupMarkerBlocksSilentReset()
{
    QCoreApplication::setOrganizationName(QStringLiteral("KevaSecurityTest"));
    QCoreApplication::setApplicationName(QStringLiteral("tst_security"));
    auto clearMarkers = []() {
        QSettings(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("KevaSecurityTest"), QStringLiteral("tst_security")).clear();
        QSettings machine(QSettings::NativeFormat, QSettings::SystemScope, QStringLiteral("KevaSecurityTest"), QStringLiteral("tst_security"));
        if (machine.isWritable()) {
            machine.clear();
        }
    };
    clearMarkers();

    Env env;
    AuthManager::Options o = env.options();
    o.useInstallSentinel = true;
    {
        AuthManager a(o);
        a.start();
        completeSetup(a);
    }
    // Remove every file: the registry marker still says setup was done
    QVERIFY(QFile::remove(env.dir.filePath("accounts.dat")));
    QVERIFY(QFile::remove(env.dir.filePath("audit.log")));
    AuthManager b(o);
    b.start();
    QCOMPARE(b.stage(), QStringLiteral("fatal"));

    clearMarkers();
}

void TestSecurity::userIsNotAdmin()
{
    Env env;
    auto a = env.make();
    completeSetup(*a);
    QVERIFY(a->login(kUser, kPwU));
    QVERIFY(!a->isAdmin());
    a->showAdminTools();
    QCOMPARE(a->stage(), QStringLiteral("notice"));
    QVERIFY(!a->unlockAccount(kAdmin1));
    QVERIFY(!a->resetPassword(kAdmin1, kPwNew1, kPwNew1));
}

void TestSecurity::sessionLockAndUnlock()
{
    Env env;
    auto a = env.make();
    completeSetup(*a);
    a->lockSession(QStringLiteral("idle"));
    QVERIFY(!a->sessionLocked());           // nobody logged in yet
    QVERIFY(a->login(kUser, kPwU));
    a->continueToApplication();
    QSignalSpy lockSpy(a.get(), &AuthManager::sessionLockedChanged);
    a->lockSession(QStringLiteral("Session locked after 10 minutes of inactivity."));
    QVERIFY(a->sessionLocked());
    QCOMPARE(lockSpy.count(), 1);
    QVERIFY(a->lockText().contains(kUser));
    QVERIFY(!a->unlockSession(QStringLiteral("wrong")));
    QVERIFY2(a->message().contains("2 attempts remaining"), qPrintable(a->message()));
    QVERIFY(a->sessionLocked());
    QVERIFY(a->unlockSession(kPwU));
    QVERIFY(!a->sessionLocked());
    QCOMPARE(a->store()->find(kUser)->failures.consecutive, 0);

    bool locked = false, unlocked = false;
    for (const AuditEntry &e : a->audit()->readAll(nullptr, nullptr)) {
        locked = locked || e.type == QStringLiteral("SESSION_LOCKED");
        unlocked = unlocked || e.type == QStringLiteral("SESSION_UNLOCKED");
    }
    QVERIFY(locked && unlocked);
    QSignalSpy exitSpy(a.get(), &AuthManager::exitRequested);
    a->requestExit();
    QCOMPARE(exitSpy.count(), 1);
}

void TestSecurity::sessionUnlockFailuresLockAccount()
{
    Env env;
    auto a = env.make();
    completeSetup(*a);
    QVERIFY(a->login(kUser, kPwU));
    a->continueToApplication();
    a->lockSession(QStringLiteral("idle"));
    for (int i = 0; i < 3; ++i) {
        QVERIFY(!a->unlockSession(QStringLiteral("wrong")));
    }
    QVERIFY(a->sessionTerminated());
    QVERIFY(a->lockText().contains("Account has been locked"));
    QVERIFY(a->store()->find(kUser)->failures.locked);
    QVERIFY(!a->unlockSession(kPwU));       // correct PW no longer helps
    QVERIFY(a->sessionLocked());
}

void TestSecurity::accessPolicy()
{
    using R = AccessPolicy::Role;
    QVERIFY(!AccessPolicy::canSendFlightCommands(R::Administrator));
    QVERIFY(AccessPolicy::canSendFlightCommands(R::User));
    QVERIFY(!AccessPolicy::canPlanFlights(R::Administrator));
    QVERIFY(AccessPolicy::canConfigureVehicle(R::Administrator));
    QVERIFY(!AccessPolicy::canConfigureVehicle(R::User));
    for (int cmd : { 20, 21, 22, 176, 178, 186, 192, 300, 400, 3000 }) {
        QVERIFY2(AccessPolicy::isFlightCommand(cmd), qPrintable(QString::number(cmd)));
    }
    for (int cmd : { 241 /* PREFLIGHT_CALIBRATION */, 246 /* REBOOT */, 512 /* REQUEST_MESSAGE */, 2000 /* IMAGE_START_CAPTURE */, 1000 /* GIMBAL_PITCHYAW */ }) {
        QVERIFY2(!AccessPolicy::isFlightCommand(cmd), qPrintable(QString::number(cmd)));
    }
    QVERIFY(AccessPolicy::settingsPageAllowed(R::User, QStringLiteral("qrc:/qml/QGroundControl/AppSettings/MapSettings.qml")));
    QVERIFY(!AccessPolicy::settingsPageAllowed(R::User, QStringLiteral("qrc:/qml/QGroundControl/AppSettings/PX4LogTransferSettings.qml")));
    QVERIFY(!AccessPolicy::settingsPageAllowed(R::User, QStringLiteral("qrc:/qml/QGroundControl/Controls/AppMessages.qml")));
    QVERIFY(AccessPolicy::settingsPageAllowed(R::Administrator, QStringLiteral("qrc:/qml/QGroundControl/Controls/AppMessages.qml")));
}

QTEST_GUILESS_MAIN(TestSecurity)
#include "tst_security.moc"
