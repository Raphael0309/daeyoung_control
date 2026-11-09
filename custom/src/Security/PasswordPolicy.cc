/****************************************************************************
 *
 * Daeyoung Ground Control - password and ID rules (SR-10, SR-13)
 *
 ****************************************************************************/

#include "PasswordPolicy.h"
#include "SecurityConfig.h"

#include <QtCore/QFile>
#include <QtCore/QHash>
#include <QtCore/QTextStream>

#include <algorithm>

namespace KevaSecurity {

namespace {

// Keyboard rows checked for runs like "qwe" or "lkj" (both directions).
const QStringList kKeyboardRows = {
    QStringLiteral("1234567890"),
    QStringLiteral("qwertyuiop"),
    QStringLiteral("asdfghjkl"),
    QStringLiteral("zxcvbnm"),
};

// IDs that reveal a privilege level or a default account (exact match, case-insensitive).
const QStringList kForbiddenIds = {
    QStringLiteral("root"), QStringLiteral("admin"), QStringLiteral("administrator"),
    QStringLiteral("sa"), QStringLiteral("system"), QStringLiteral("guest"),
    QStringLiteral("test"), QStringLiteral("user"), QStringLiteral("operator"),
    QStringLiteral("manager"), QStringLiteral("superuser"), QStringLiteral("supervisor"),
    QStringLiteral("sysadmin"), QStringLiteral("default"), QStringLiteral("owner"),
};

// Fragments that may not appear anywhere in an ID (case-insensitive).
const QStringList kForbiddenIdFragments = {
    QStringLiteral("admin"), QStringLiteral("root"), QStringLiteral("system"),
    QStringLiteral("super"),
};

bool isAsciiLetter(QChar c) { return (c >= QLatin1Char('a') && c <= QLatin1Char('z')) || (c >= QLatin1Char('A') && c <= QLatin1Char('Z')); }
bool isAsciiDigit(QChar c)  { return c >= QLatin1Char('0') && c <= QLatin1Char('9'); }
bool isAsciiSpecial(QChar c) { return c.unicode() >= 0x21 && c.unicode() <= 0x7e && !isAsciiLetter(c) && !isAsciiDigit(c); }

QSet<QString> readList(const QString &path, bool *ok)
{
    QSet<QString> result;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        *ok = false;
        return result;
    }
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        result.insert(line.toLower());
    }
    *ok = true;
    return result;
}

} // namespace

PasswordPolicy::PasswordPolicy() = default;

bool PasswordPolicy::loadDictionary(const QString &path)
{
    bool ok = false;
    const QSet<QString> words = readList(path, &ok);
    _dictionary.unite(words);
    return ok && !words.isEmpty();
}

bool PasswordPolicy::loadCommonPasswords(const QString &path)
{
    bool ok = false;
    const QSet<QString> words = readList(path, &ok);
    _commonPasswords.unite(words);
    return ok && !words.isEmpty();
}

void PasswordPolicy::addDictionaryWords(const QStringList &words)
{
    for (const QString &w : words) {
        _dictionary.insert(w.toLower());
    }
}

void PasswordPolicy::addCommonPasswords(const QStringList &passwords)
{
    for (const QString &p : passwords) {
        _commonPasswords.insert(p.toLower());
    }
}

bool PasswordPolicy::hasRepeatedCharacters(const QString &password, int runLength)
{
    const QString p = password.toLower();
    int run = 1;
    for (int i = 1; i < p.size(); ++i) {
        run = (p[i] == p[i - 1]) ? run + 1 : 1;
        if (run >= runLength) {
            return true;
        }
    }
    return false;
}

bool PasswordPolicy::hasSequentialCharacters(const QString &password, int runLength)
{
    const QString p = password.toLower();

    // Alphabetic or numeric runs: abc, cba, 123, 321
    int up = 1;
    int down = 1;
    for (int i = 1; i < p.size(); ++i) {
        const QChar a = p[i - 1];
        const QChar b = p[i];
        const bool sameClass = (isAsciiLetter(a) && isAsciiLetter(b)) || (isAsciiDigit(a) && isAsciiDigit(b));
        const int delta = b.unicode() - a.unicode();
        up = (sameClass && delta == 1) ? up + 1 : 1;
        down = (sameClass && delta == -1) ? down + 1 : 1;
        if (up >= runLength || down >= runLength) {
            return true;
        }
    }

    // Keyboard runs: qwe, ewq, asd, 890 ...
    for (int i = 0; i + runLength <= p.size(); ++i) {
        const QString chunk = p.mid(i, runLength);
        QString reversed = chunk;
        std::reverse(reversed.begin(), reversed.end());
        for (const QString &row : kKeyboardRows) {
            if (row.contains(chunk) || row.contains(reversed)) {
                return true;
            }
        }
    }
    return false;
}

bool PasswordPolicy::isForbiddenId(const QString &id)
{
    const QString lower = id.toLower();
    if (kForbiddenIds.contains(lower)) {
        return true;
    }
    for (const QString &fragment : kForbiddenIdFragments) {
        if (lower.contains(fragment)) {
            return true;
        }
    }
    return false;
}

bool PasswordPolicy::isDictionaryBased(const QString &password) const
{
    const QString lower = password.toLower();
    if (_commonPasswords.contains(lower)) {
        return true;
    }
    // Remove digits and special characters; what is left may not be a dictionary word.
    // A second pass reads look-alike characters inside a word as letters (p@ssw0rd -> password).
    static const QHash<QChar, QChar> leet = {
        { QLatin1Char('@'), QLatin1Char('a') }, { QLatin1Char('4'), QLatin1Char('a') },
        { QLatin1Char('$'), QLatin1Char('s') }, { QLatin1Char('5'), QLatin1Char('s') },
        { QLatin1Char('0'), QLatin1Char('o') }, { QLatin1Char('3'), QLatin1Char('e') },
        { QLatin1Char('1'), QLatin1Char('i') }, { QLatin1Char('!'), QLatin1Char('i') },
        { QLatin1Char('7'), QLatin1Char('t') }, { QLatin1Char('+'), QLatin1Char('t') },
    };
    QString letters;
    QString deLeeted;
    for (int i = 0; i < lower.size(); ++i) {
        const QChar c = lower.at(i);
        if (isAsciiLetter(c)) {
            letters.append(c);
            deLeeted.append(c);
        } else if (leet.contains(c) && i > 0 && i + 1 < lower.size() &&
                   isAsciiLetter(lower.at(i - 1)) && isAsciiLetter(lower.at(i + 1))) {
            deLeeted.append(leet.value(c));
        }
    }
    return (letters.size() >= 3 && _dictionary.contains(letters)) ||
           (deLeeted.size() >= 3 && _dictionary.contains(deLeeted));
}

QStringList PasswordPolicy::checkPassword(const QString &id, const QString &password) const
{
    QStringList errors;

    bool hasLetter = false;
    bool hasDigit = false;
    bool hasSpecial = false;
    bool invalidChar = false;
    for (const QChar c : password) {
        if (isAsciiLetter(c)) {
            hasLetter = true;
        } else if (isAsciiDigit(c)) {
            hasDigit = true;
        } else if (isAsciiSpecial(c)) {
            hasSpecial = true;
        } else {
            invalidChar = true;
        }
    }

    if (password.size() < kPasswordMinLength) {
        errors << QStringLiteral("Password must be at least %1 characters long.").arg(kPasswordMinLength);
    }
    if (password.size() > kPasswordMaxLength) {
        errors << QStringLiteral("Password must be at most %1 characters long.").arg(kPasswordMaxLength);
    }
    if (invalidChar) {
        errors << QStringLiteral("Password may contain only English letters, digits and special characters (no spaces).");
    }
    if (!hasLetter || !hasDigit || !hasSpecial) {
        errors << QStringLiteral("Password must combine letters, digits and special characters.");
    }
    if (!id.isEmpty() && (password.compare(id, Qt::CaseInsensitive) == 0 ||
                          (id.size() >= 3 && password.contains(id, Qt::CaseInsensitive)))) {
        errors << QStringLiteral("Password must not be the same as, or contain, the user ID.");
    }
    if (isDictionaryBased(password)) {
        errors << QStringLiteral("Password must not be a common password or a dictionary word.");
    }
    if (hasRepeatedCharacters(password, kRepeatLength)) {
        errors << QStringLiteral("Password must not repeat the same character %1 or more times in a row.").arg(kRepeatLength);
    }
    if (hasSequentialCharacters(password, kSequenceLength)) {
        errors << QStringLiteral("Password must not contain %1 or more sequential characters (e.g. abc, 321, qwe).").arg(kSequenceLength);
    }
    return errors;
}

QString PasswordPolicy::checkId(const QString &id) const
{
    if (id.size() < kIdMinLength || id.size() > kIdMaxLength) {
        return QStringLiteral("ID must be %1 to %2 characters long.").arg(kIdMinLength).arg(kIdMaxLength);
    }
    for (const QChar c : id) {
        if (!isAsciiLetter(c) && !isAsciiDigit(c)) {
            return QStringLiteral("ID may contain only English letters and digits.");
        }
    }
    if (isForbiddenId(id)) {
        return QStringLiteral("This ID is not allowed. Do not use privileged or default names such as root or administrator.");
    }
    return QString();
}

QString PasswordPolicy::rulesText()
{
    return QStringLiteral(
        "Password rules\n"
        "- At least %1 characters, combining letters, digits and special characters\n"
        "- Must not be the same as, or contain, the ID\n"
        "- Must not be a common password or a dictionary word\n"
        "- No character repeated %2 or more times in a row (aaa, 111)\n"
        "- No %3 or more sequential characters (abc, 321, qwe)\n"
        "- Must not reuse a password used in the last 12 months\n"
        "- Expires every %4 days\n"
        "\n"
        "ID rules\n"
        "- %5 to %6 English letters or digits, case-sensitive\n"
        "- Privileged or default names (root, admin, ...) are not allowed")
        .arg(kPasswordMinLength).arg(kRepeatLength).arg(kSequenceLength)
        .arg(kPasswordExpiryDays).arg(kIdMinLength).arg(kIdMaxLength);
}

} // namespace KevaSecurity
