/****************************************************************************
 *
 * Daeyoung Ground Control - security policy constants (MIL edition)
 *
 * Every value below traces back to the security requirements tracker
 * (SR-xx ids) agreed with the customer. Change them only together with
 * that document.
 *
 ****************************************************************************/

#pragma once

#include <QtCore/QtGlobal>

namespace KevaSecurity {

// SR-01: one-way hash, SHA-224 or stronger, with salt
constexpr int kPbkdf2Iterations         = 600000;   // PBKDF2-HMAC-SHA256
constexpr int kSaltBytes                = 16;
constexpr int kHashBytes                = 32;

// SR-03 / SR-04: lockout
constexpr int kMaxConsecutiveFailures   = 3;
constexpr int kMaxDailyFailures         = 6;
constexpr qint64 kClockRollbackToleranceSecs = 300;
constexpr int kMaxUnknownIdCounters     = 200;

// SR-10 / SR-13: password and ID rules
constexpr int kPasswordMinLength        = 9;
constexpr int kPasswordMaxLength        = 64;
constexpr int kIdMinLength              = 4;
constexpr int kIdMaxLength              = 20;
constexpr int kSequenceLength           = 3;    // abc, 321, qwe ...
constexpr int kRepeatLength             = 3;    // aaa, 111 ...

// SR-14: expiry and reuse
constexpr int kPasswordExpiryDays       = 90;
constexpr int kPasswordExpiryWarnDays   = 7;
constexpr int kPasswordHistoryDays      = 365;

// Account layout decided on 2026-11-09: two administrators, one user per GCS
constexpr int kAdminAccountCount        = 2;
constexpr int kUserAccountCount         = 1;

// Recovery code issued once to the security officer
constexpr int kRecoveryCodeGroups       = 5;
constexpr int kRecoveryCodeGroupLength  = 4;

// Audit log
constexpr qint64 kAuditLogWarnBytes     = 20 * 1024 * 1024;

// Idle lock (applied by the main window, not while flying)
constexpr int kIdleLockMinutes          = 10;

} // namespace KevaSecurity
