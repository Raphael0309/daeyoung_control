/****************************************************************************
 *
 * Daeyoung Ground Control - cryptographic primitives (MIL edition)
 *
 * On Windows everything goes through the operating system:
 *   - PBKDF2-HMAC-SHA256 and random numbers: CNG (bcrypt.dll)
 *   - data protection at rest: DPAPI, machine scope (crypt32.dll)
 * Other platforms use a portable PBKDF2 implementation and no data
 * protection; they exist for development and unit tests only.
 *
 ****************************************************************************/

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>

namespace KevaSecurity {

class SecurityCrypto
{
public:
    /// Cryptographically secure random bytes (CNG on Windows).
    static QByteArray randomBytes(int count);

    /// PBKDF2-HMAC-SHA256 using the platform provider (CNG on Windows).
    static QByteArray pbkdf2Sha256(const QByteArray &password, const QByteArray &salt, int iterations, int keyLength);

    /// Portable PBKDF2-HMAC-SHA256 (RFC 8018), used off Windows and to cross-check CNG.
    static QByteArray pbkdf2Sha256Portable(const QByteArray &password, const QByteArray &salt, int iterations, int keyLength);

    static QByteArray sha256(const QByteArray &data);

    /// Compares without early exit so timing does not reveal how many bytes matched.
    static bool constantTimeEquals(const QByteArray &a, const QByteArray &b);

    /// Encrypts data so it can only be decrypted on this machine (DPAPI machine scope).
    static QByteArray protect(const QByteArray &plain, bool *ok);
    static QByteArray unprotect(const QByteArray &blob, bool *ok);

    /// True when protect()/unprotect() really encrypt (Windows DPAPI).
    static bool platformProtectionAvailable();

    /// Known-answer test of the hash provider. Login is refused when this fails.
    static bool selfTest(QString *error);
};

} // namespace KevaSecurity
