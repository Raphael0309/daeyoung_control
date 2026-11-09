/****************************************************************************
 *
 * Daeyoung Ground Control - cryptographic primitives (MIL edition)
 *
 ****************************************************************************/

#include "SecurityCrypto.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QMessageAuthenticationCode>
#include <QtCore/QRandomGenerator>
#include <QtCore/QtEndian>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>
#endif

namespace KevaSecurity {

namespace {

// Application specific secondary entropy for DPAPI. It is not a secret; it only
// stops other software on the same machine from decrypting our blobs by accident.
const QByteArray kDpapiEntropy = QByteArrayLiteral("DaeyoungGCS/KevaSecurity/v1");

#ifndef Q_OS_WIN
// Marks data that was "protected" on a platform without DPAPI (development only).
const QByteArray kPlainMarker = QByteArrayLiteral("KEVA-PLAIN1:");
#endif

} // namespace

QByteArray SecurityCrypto::randomBytes(int count)
{
    QByteArray out(count, Qt::Uninitialized);
#ifdef Q_OS_WIN
    const NTSTATUS status = BCryptGenRandom(nullptr,
                                            reinterpret_cast<PUCHAR>(out.data()),
                                            static_cast<ULONG>(out.size()),
                                            BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (!BCRYPT_SUCCESS(status)) {
        return QByteArray();
    }
#else
    for (int i = 0; i < count; ++i) {
        out[i] = static_cast<char>(QRandomGenerator::system()->bounded(256));
    }
#endif
    return out;
}

QByteArray SecurityCrypto::pbkdf2Sha256(const QByteArray &password, const QByteArray &salt, int iterations, int keyLength)
{
    if (iterations < 1 || keyLength < 1) {
        return QByteArray();
    }
#ifdef Q_OS_WIN
    BCRYPT_ALG_HANDLE alg = nullptr;
    NTSTATUS status = BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (!BCRYPT_SUCCESS(status)) {
        return QByteArray();
    }
    QByteArray key(keyLength, Qt::Uninitialized);
    // BCryptDeriveKeyPBKDF2 takes non-const buffers; it does not modify the inputs.
    QByteArray pw(password);
    QByteArray s(salt);
    status = BCryptDeriveKeyPBKDF2(alg,
                                   reinterpret_cast<PUCHAR>(pw.data()), static_cast<ULONG>(pw.size()),
                                   reinterpret_cast<PUCHAR>(s.data()), static_cast<ULONG>(s.size()),
                                   static_cast<ULONGLONG>(iterations),
                                   reinterpret_cast<PUCHAR>(key.data()), static_cast<ULONG>(key.size()),
                                   0);
    BCryptCloseAlgorithmProvider(alg, 0);
    pw.fill('\0');
    if (!BCRYPT_SUCCESS(status)) {
        return QByteArray();
    }
    return key;
#else
    return pbkdf2Sha256Portable(password, salt, iterations, keyLength);
#endif
}

QByteArray SecurityCrypto::pbkdf2Sha256Portable(const QByteArray &password, const QByteArray &salt, int iterations, int keyLength)
{
    if (iterations < 1 || keyLength < 1) {
        return QByteArray();
    }
    constexpr int hashLength = 32;
    const int blocks = (keyLength + hashLength - 1) / hashLength;
    QByteArray out;
    out.reserve(blocks * hashLength);

    QMessageAuthenticationCode mac(QCryptographicHash::Sha256, password);
    for (int block = 1; block <= blocks; ++block) {
        char counter[4];
        qToBigEndian<quint32>(static_cast<quint32>(block), counter);

        mac.reset();
        mac.addData(salt);
        mac.addData(counter, 4);
        QByteArray u = mac.result();
        QByteArray t = u;
        for (int i = 1; i < iterations; ++i) {
            mac.reset();
            mac.addData(u);
            u = mac.result();
            for (int j = 0; j < hashLength; ++j) {
                t[j] = static_cast<char>(t[j] ^ u[j]);
            }
        }
        out.append(t);
    }
    out.truncate(keyLength);
    return out;
}

QByteArray SecurityCrypto::sha256(const QByteArray &data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256);
}

bool SecurityCrypto::constantTimeEquals(const QByteArray &a, const QByteArray &b)
{
    if (a.size() != b.size()) {
        return false;
    }
    unsigned char diff = 0;
    for (int i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    }
    return diff == 0;
}

QByteArray SecurityCrypto::protect(const QByteArray &plain, bool *ok)
{
    if (ok) {
        *ok = false;
    }
#ifdef Q_OS_WIN
    DATA_BLOB in;
    in.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(plain.constData()));
    in.cbData = static_cast<DWORD>(plain.size());
    DATA_BLOB entropy;
    entropy.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(kDpapiEntropy.constData()));
    entropy.cbData = static_cast<DWORD>(kDpapiEntropy.size());
    DATA_BLOB out = { 0, nullptr };
    if (!CryptProtectData(&in, L"DaeyoungGCS", &entropy, nullptr, nullptr,
                          CRYPTPROTECT_LOCAL_MACHINE | CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        return QByteArray();
    }
    const QByteArray result(reinterpret_cast<const char *>(out.pbData), static_cast<int>(out.cbData));
    LocalFree(out.pbData);
    if (ok) {
        *ok = true;
    }
    return result;
#else
    if (ok) {
        *ok = true;
    }
    return kPlainMarker + plain;
#endif
}

QByteArray SecurityCrypto::unprotect(const QByteArray &blob, bool *ok)
{
    if (ok) {
        *ok = false;
    }
#ifdef Q_OS_WIN
    DATA_BLOB in;
    in.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(blob.constData()));
    in.cbData = static_cast<DWORD>(blob.size());
    DATA_BLOB entropy;
    entropy.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(kDpapiEntropy.constData()));
    entropy.cbData = static_cast<DWORD>(kDpapiEntropy.size());
    DATA_BLOB out = { 0, nullptr };
    if (!CryptUnprotectData(&in, nullptr, &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        return QByteArray();
    }
    const QByteArray result(reinterpret_cast<const char *>(out.pbData), static_cast<int>(out.cbData));
    SecureZeroMemory(out.pbData, out.cbData);
    LocalFree(out.pbData);
    if (ok) {
        *ok = true;
    }
    return result;
#else
    if (!blob.startsWith(kPlainMarker)) {
        return QByteArray();
    }
    if (ok) {
        *ok = true;
    }
    return blob.mid(kPlainMarker.size());
#endif
}

bool SecurityCrypto::platformProtectionAvailable()
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

bool SecurityCrypto::selfTest(QString *error)
{
    // RFC 7914 section 11 / widely published PBKDF2-HMAC-SHA256 vectors
    const QByteArray expected1 = QByteArray::fromHex("120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b");
    const QByteArray expected4096 = QByteArray::fromHex("c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a");

    if (pbkdf2Sha256(QByteArrayLiteral("password"), QByteArrayLiteral("salt"), 1, 32) != expected1 ||
        pbkdf2Sha256(QByteArrayLiteral("password"), QByteArrayLiteral("salt"), 4096, 32) != expected4096) {
        if (error) {
            *error = QStringLiteral("Password hash provider failed its self test.");
        }
        return false;
    }
    if (pbkdf2Sha256Portable(QByteArrayLiteral("password"), QByteArrayLiteral("salt"), 4096, 32) != expected4096) {
        if (error) {
            *error = QStringLiteral("Portable password hash failed its self test.");
        }
        return false;
    }
    const QByteArray random = randomBytes(32);
    if (random.size() != 32) {
        if (error) {
            *error = QStringLiteral("Random number generator is not available.");
        }
        return false;
    }
    bool ok = false;
    const QByteArray sample = QByteArrayLiteral("keva-self-test");
    const QByteArray blob = protect(sample, &ok);
    bool ok2 = false;
    if (!ok || unprotect(blob, &ok2) != sample || !ok2) {
        if (error) {
            *error = QStringLiteral("Data protection (DPAPI) is not available.");
        }
        return false;
    }
    return true;
}

} // namespace KevaSecurity
