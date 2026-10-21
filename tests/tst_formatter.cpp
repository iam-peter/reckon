#include "core/Formatter.h"

#include <QTest>

#include <cmath>

using namespace Qt::StringLiterals;

class TestFormatter : public QObject
{
    Q_OBJECT

    static QString fmt(Number value, NumberFormat::Mode mode = NumberFormat::General, int precision = -1,
                       bool group = false)
    {
        return formatNumber(value, {mode, precision, group});
    }

private slots:
    void general()
    {
        QCOMPARE(fmt(0.1 + 0.2), u"0.3"_s);
        QCOMPARE(fmt(42), u"42"_s);
        QCOMPARE(fmt(-0.0), u"0"_s);
        QCOMPARE(fmt(1.0 / 3), u"0.333333333333333"_s);
        QCOMPARE(fmt(1e20), u"1e20"_s);
        QCOMPARE(fmt(1.5e-7), u"1.5e-7"_s);
        QCOMPARE(fmt(123456789012345.0), u"123456789012345"_s);
        QCOMPARE(fmt(2.5, NumberFormat::General, 3), u"2.500"_s);
        QCOMPARE(fmt(1e20, NumberFormat::General, 3), u"1e20"_s);
    }

    void fixed()
    {
        QCOMPARE(fmt(3.14159, NumberFormat::Fixed, 2), u"3.14"_s);
        QCOMPARE(fmt(2, NumberFormat::Fixed, 2), u"2.00"_s);
        QCOMPARE(fmt(1e20, NumberFormat::Fixed, 0), u"100000000000000000000"_s);
        QCOMPARE(fmt(2.5, NumberFormat::Fixed), u"2.5"_s);
    }

    void scientific()
    {
        QCOMPARE(fmt(12345, NumberFormat::Scientific, 2), u"1.23e4"_s);
        QCOMPARE(fmt(0.00012, NumberFormat::Scientific, 1), u"1.2e-4"_s);
        QCOMPARE(fmt(12345, NumberFormat::Scientific), u"1.2345e4"_s);
    }

    void engineering()
    {
        QCOMPARE(fmt(12345, NumberFormat::Engineering), u"12.345e3"_s);
        QCOMPARE(fmt(0.00012, NumberFormat::Engineering), u"120e-6"_s);
        QCOMPARE(fmt(999.9999, NumberFormat::Engineering, 2), u"1.00e3"_s);
        QCOMPARE(fmt(0, NumberFormat::Engineering), u"0"_s);
    }

    void bases()
    {
        QCOMPARE(fmt(10, NumberFormat::Binary), u"0b1010"_s);
        QCOMPARE(fmt(255, NumberFormat::Hexadecimal), u"0xff"_s);
        QCOMPARE(fmt(-255, NumberFormat::Hexadecimal), u"-0xff"_s);
        QCOMPARE(fmt(8, NumberFormat::Octal), u"0o10"_s);
        QCOMPARE(fmt(2.5, NumberFormat::Binary), u"0b10.1"_s);
        QCOMPARE(fmt(0.5, NumberFormat::Hexadecimal, 2), u"0x0.80"_s);
    }

    void grouping()
    {
        QCOMPARE(fmt(1234567, NumberFormat::General, -1, true), u"1 234 567"_s);
        QCOMPARE(fmt(-1234.5, NumberFormat::Fixed, 1, true), u"-1 234.5"_s);
        QCOMPARE(fmt(123, NumberFormat::General, -1, true), u"123"_s);
    }

    void special()
    {
        QCOMPARE(fmt(std::nan("")), u"NaN"_s);
        QCOMPARE(fmt(INFINITY), u"∞"_s);
        QCOMPARE(fmt(-INFINITY), u"-∞"_s);
    }
};

QTEST_GUILESS_MAIN(TestFormatter)
#include "tst_formatter.moc"
