#pragma once

#include "Number.h"

#include <QList>
#include <QString>

struct Token
{
    enum Type {
        Number,
        Identifier,
        Operator, // one of the operator strings, keyword operators included
        LeftParen,
        RightParen,
        Comma,
        Semicolon,
        Assign,
        Unknown, // a character the lexer has no use for (lenient mode only)
        End
    };

    Type type = End;
    QString text;
    ::Number value = 0; // Number tokens
    int position = 0;
    int length = 0;
};

// Splits an expression into tokens. Numbers are decimal (1.5e3, .5),
// hexadecimal (0x1F), binary (0b101) or octal (0o17); identifiers start
// with a letter or _ and go on with letters, digits and _, or are a
// history entry's result, $ and its number (see isResultName()); the keyword
// operators mod, div, and, or, xor, not, shl and shr come out as
// operators. Throws CalcError at a character it cannot read, unless
// `lenient` is set, when that character becomes an Unknown token (for
// the highlighter, which colors what it can).
QList<Token> tokenize(const QString &expression, bool lenient = false);

// $3, the name of history entry 3's result, which nothing can assign.
inline bool isResultName(const QString &name)
{
    return name.startsWith(u'$');
}
