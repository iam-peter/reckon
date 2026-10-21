#include "Formatter.h"

#include <QLocale>

#include <cmath>

using namespace Qt::StringLiterals;

namespace {

constexpr int AutoSignificantDigits = 15;

// Puts a thin space between every three digits before the point.
QString grouped(QString digits)
{
    const qsizetype point = digits.indexOf(u'.');
    qsizetype end = point < 0 ? digits.size() : point;
    qsizetype start = digits.startsWith(u'-') ? 1 : 0;
    for (qsizetype i = end - 3; i > start; i -= 3)
        digits.insert(i, u' ');
    return digits;
}

QString stripTrailingZeros(QString s)
{
    if (!s.contains(u'.'))
        return s;
    while (s.endsWith(u'0'))
        s.chop(1);
    if (s.endsWith(u'.'))
        s.chop(1);
    return s;
}

// "1e+20" -> "1e20", "1e-05" -> "1e-5".
QString tidyExponent(QString s)
{
    const qsizetype e = s.indexOf(u'e');
    if (e < 0)
        return s;
    QString mantissa = s.left(e);
    QString exponent = s.mid(e + 1);
    const bool negative = exponent.startsWith(u'-');
    exponent.remove(u'+');
    exponent.remove(u'-');
    while (exponent.size() > 1 && exponent.startsWith(u'0'))
        exponent.remove(0, 1);
    return stripTrailingZeros(mantissa) + u'e' + (negative ? u"-"_s : QString()) + exponent;
}

QString general(Number value, int precision, bool group)
{
    QString s;
    if (precision < 0) {
        s = tidyExponent(QString::number(value, 'g', AutoSignificantDigits));
    } else {
        // Fixed decimals, unless the number is too large or small for them.
        const Number magnitude = std::fabs(value);
        if (magnitude != 0 && (magnitude >= 1e15 || magnitude < std::pow(10.0, -precision) / 2))
            s = tidyExponent(QString::number(value, 'e', precision));
        else
            s = QString::number(value, 'f', precision);
    }
    return group ? grouped(s) : s;
}

QString fixed(Number value, int precision, bool group)
{
    const QString s = precision < 0 ? stripTrailingZeros(QString::number(value, 'f', AutoSignificantDigits))
                                    : QString::number(value, 'f', precision);
    return group ? grouped(s) : s;
}

QString scientific(Number value, int precision)
{
    const int digits = precision < 0 ? AutoSignificantDigits - 1 : precision;
    return tidyExponent(QString::number(value, 'e', digits));
}

QString engineering(Number value, int precision)
{
    if (value == 0)
        return precision < 0 ? u"0"_s : QString::number(0.0, 'f', precision);
    int exponent = int(std::floor(std::log10(std::fabs(value))));
    exponent -= ((exponent % 3) + 3) % 3;
    Number mantissa = value / std::pow(10.0, exponent);
    // Rounding may carry the mantissa to 1000.
    const int digits = precision < 0 ? AutoSignificantDigits - 3 : precision;
    QString m = QString::number(mantissa, 'f', digits);
    if (std::fabs(m.toDouble()) >= 1000) {
        exponent += 3;
        mantissa = value / std::pow(10.0, exponent);
        m = QString::number(mantissa, 'f', digits);
    }
    if (precision < 0)
        m = stripTrailingZeros(m);
    return m + u'e' + QString::number(exponent);
}

QString based(Number value, int base, const QString &prefix, int precision)
{
    if (!std::isfinite(value) || std::fabs(value) >= 9.2e18)
        return general(value, -1, false);
    const bool negative = value < 0;
    const Number magnitude = std::fabs(value);
    const qulonglong integer = qulonglong(std::floor(magnitude));
    QString s = QString::number(integer, base);
    Number fraction = magnitude - std::floor(magnitude);
    if (fraction > 0) {
        s += u'.';
        const int digits = precision < 0 ? 12 : precision;
        for (int i = 0; i < digits && (fraction > 0 || precision >= 0); ++i) {
            fraction *= base;
            const int digit = int(fraction);
            s += QString::number(digit, base);
            fraction -= digit;
        }
        if (precision < 0)
            s = stripTrailingZeros(s);
    }
    return (negative ? u"-"_s : QString()) + prefix + s;
}

} // namespace

QString formatNumber(Number value, const NumberFormat &format)
{
    if (std::isnan(value))
        return u"NaN"_s;
    if (std::isinf(value))
        return value < 0 ? u"-∞"_s : u"∞"_s;
    // -0 reads as 0.
    if (value == 0)
        value = 0;
    switch (format.mode) {
    case NumberFormat::General:
        return general(value, format.precision, format.groupDigits);
    case NumberFormat::Fixed:
        return fixed(value, format.precision, format.groupDigits);
    case NumberFormat::Scientific:
        return scientific(value, format.precision);
    case NumberFormat::Engineering:
        return engineering(value, format.precision);
    case NumberFormat::Binary:
        return based(value, 2, u"0b"_s, format.precision);
    case NumberFormat::Octal:
        return based(value, 8, u"0o"_s, format.precision);
    case NumberFormat::Hexadecimal:
        return based(value, 16, u"0x"_s, format.precision);
    }
    return QString::number(value);
}
