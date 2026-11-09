/****************************************************************************
 *
 * Daeyoung Ground Control - password and ID rules (SR-10, SR-13)
 *
 ****************************************************************************/

#pragma once

#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace KevaSecurity {

class PasswordPolicy
{
public:
    PasswordPolicy();

    /// Loads newline separated word lists. Lines starting with '#' are ignored.
    bool loadDictionary(const QString &path);
    bool loadCommonPasswords(const QString &path);
    void addDictionaryWords(const QStringList &words);
    void addCommonPasswords(const QStringList &passwords);
    int dictionarySize() const { return _dictionary.size(); }

    /// Returns one user facing message per violated rule; empty when the password is acceptable.
    /// Reuse of old passwords is checked by the account store, not here.
    QStringList checkPassword(const QString &id, const QString &password) const;

    /// Returns an error message, or an empty string when the ID is acceptable.
    /// Uniqueness is checked by the account store.
    QString checkId(const QString &id) const;

    /// Short rule summary shown on the account creation and password change screens.
    static QString rulesText();

    // Individual rules, public for unit tests
    static bool hasRepeatedCharacters(const QString &password, int runLength);
    static bool hasSequentialCharacters(const QString &password, int runLength);
    static bool isForbiddenId(const QString &id);
    bool isDictionaryBased(const QString &password) const;

private:
    QSet<QString> _dictionary;
    QSet<QString> _commonPasswords;
};

} // namespace KevaSecurity
