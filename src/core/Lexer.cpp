#include "Lexer.h"

#include <QSet>

using namespace Qt::StringLiterals;

namespace {

bool isIdentifierStart(QChar c)
{
    return c.isLetter() || c == u'_';
}

bool isIdentifierChar(QChar c)
{
    return c.isLetterOrNumber() || c == u'_';
}

int digitValue(QChar c, int base)
{
    int value = -1;
    if (c >= u'0' && c <= u'9')
        value = c.unicode() - u'0';
    else if (c >= u'a' && c <= u'z')
        value = c.unicode() - u'a' + 10;
    else if (c >= u'A' && c <= u'Z')
        value = c.unicode() - u'A' + 10;
    return value < base ? value : -1;
}

// A number in base 2, 8 or 16 after its 0b/0o/0x prefix, with an optional
// fraction in the same base.
Token readBasedNumber(const QString &s, int start, int base, bool lenient)
{
    int i = start + 2;
    Number value = 0;
    bool digits = false;
    for (; i < s.size() && digitValue(s[i], base) >= 0; ++i) {
        value = value * base + digitValue(s[i], base);
        digits = true;
    }
    if (i < s.size() && s[i] == u'.') {
        Number scale = 1.0 / base;
        for (++i; i < s.size() && digitValue(s[i], base) >= 0; ++i) {
            value += digitValue(s[i], base) * scale;
            scale /= base;
            digits = true;
        }
    }
    if (!digits) {
        if (!lenient)
            throw CalcError{u"Digits expected after %1"_s.arg(s.mid(start, 2)), start};
        // Half a prefix while typing: marked, not fatal.
        Token token;
        token.type = Token::Unknown;
        token.text = s.mid(start, i - start);
        token.position = start;
        token.length = i - start;
        return token;
    }
    Token token;
    token.type = Token::Number;
    token.text = s.mid(start, i - start);
    token.value = value;
    token.position = start;
    token.length = i - start;
    return token;
}

Token readDecimalNumber(const QString &s, int start, bool lenient)
{
    int i = start;
    while (i < s.size() && s[i].isDigit())
        ++i;
    if (i < s.size() && s[i] == u'.') {
        ++i;
        while (i < s.size() && s[i].isDigit())
            ++i;
    }
    // An exponent only if digits follow it, so "2e" leaves e as a name.
    if (i < s.size() && (s[i] == u'e' || s[i] == u'E')) {
        int j = i + 1;
        if (j < s.size() && (s[j] == u'+' || s[j] == u'-'))
            ++j;
        if (j < s.size() && s[j].isDigit()) {
            while (j < s.size() && s[j].isDigit())
                ++j;
            i = j;
        }
    }
    Token token;
    token.type = Token::Number;
    token.text = s.mid(start, i - start);
    token.position = start;
    token.length = i - start;
    bool ok = false;
    token.value = token.text.toDouble(&ok);
    if (!ok) {
        if (!lenient)
            throw CalcError{u"Invalid number %1"_s.arg(token.text), start};
        token.type = Token::Unknown;
    }
    return token;
}

const QSet<QString> &keywordOperators()
{
    static const QSet<QString> keywords = {
        u"mod"_s, u"div"_s, u"and"_s, u"or"_s, u"xor"_s, u"not"_s, u"shl"_s, u"shr"_s,
    };
    return keywords;
}

} // namespace

QList<Token> tokenize(const QString &s, bool lenient)
{
    QList<Token> tokens;
    int i = 0;
    while (i < s.size()) {
        const QChar c = s[i];
        if (c.isSpace()) {
            ++i;
            continue;
        }
        Token token;
        token.position = i;
        if (c.isDigit() || (c == u'.' && i + 1 < s.size() && s[i + 1].isDigit())) {
            if (c == u'0' && i + 1 < s.size()) {
                const QChar prefix = s[i + 1].toLower();
                if (prefix == u'x') {
                    tokens.append(readBasedNumber(s, i, 16, lenient));
                    i += tokens.last().length;
                    continue;
                }
                if (prefix == u'b') {
                    tokens.append(readBasedNumber(s, i, 2, lenient));
                    i += tokens.last().length;
                    continue;
                }
                if (prefix == u'o') {
                    tokens.append(readBasedNumber(s, i, 8, lenient));
                    i += tokens.last().length;
                    continue;
                }
            }
            tokens.append(readDecimalNumber(s, i, lenient));
            i += tokens.last().length;
            continue;
        }
        if (isIdentifierStart(c)) {
            int j = i + 1;
            while (j < s.size() && isIdentifierChar(s[j]))
                ++j;
            token.text = s.mid(i, j - i);
            token.length = j - i;
            token.type = keywordOperators().contains(token.text) ? Token::Operator : Token::Identifier;
            tokens.append(token);
            i = j;
            continue;
        }
        // $3: the result of history entry 3, read like a variable.
        if (c == u'$') {
            int j = i + 1;
            while (j < s.size() && s[j].isDigit())
                ++j;
            token.length = j - i;
            token.text = s.mid(i, j - i);
            if (j == i + 1) {
                if (!lenient)
                    throw CalcError{u"Expected an entry number after $"_s, i};
                token.type = Token::Unknown;
            } else {
                token.type = Token::Identifier;
            }
            tokens.append(token);
            i = j;
            continue;
        }
        // Two-character operators before the one-character ones they start with.
        static const QStringList twoChar = {u"**"_s, u"<<"_s, u">>"_s, u"<="_s, u">="_s, u"=="_s, u"!="_s};
        const QString pair = s.mid(i, 2);
        if (pair.size() == 2 && twoChar.contains(pair)) {
            token.type = Token::Operator;
            token.text = pair == u"**"_s ? u"^"_s : pair;
            token.length = 2;
            tokens.append(token);
            i += 2;
            continue;
        }
        token.length = 1;
        token.text = c;
        switch (c.unicode()) {
        case u'(':
            token.type = Token::LeftParen;
            break;
        case u')':
            token.type = Token::RightParen;
            break;
        case u',':
            token.type = Token::Comma;
            break;
        case u';':
            token.type = Token::Semicolon;
            break;
        case u'=':
            token.type = Token::Assign;
            break;
        case u'+': case u'-': case u'*': case u'/': case u'^': case u'%': case u'!':
        case u'&': case u'|': case u'~': case u'<': case u'>':
            token.type = Token::Operator;
            break;
        case u'×': // ×
            token.type = Token::Operator;
            token.text = u"*"_s;
            break;
        case u'÷': // ÷
            token.type = Token::Operator;
            token.text = u"/"_s;
            break;
        case u'−': // −
            token.type = Token::Operator;
            token.text = u"-"_s;
            break;
        case u'π': // π
            token.type = Token::Identifier;
            token.text = u"pi"_s;
            break;
        default:
            if (!lenient)
                throw CalcError{u"Unexpected character '%1'"_s.arg(c), i};
            token.type = Token::Unknown;
            break;
        }
        tokens.append(token);
        ++i;
    }
    Token end;
    end.type = Token::End;
    end.position = s.size();
    tokens.append(end);
    return tokens;
}
