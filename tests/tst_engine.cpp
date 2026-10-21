#include "core/Environment.h"
#include "core/Expression.h"
#include "core/Lexer.h"

#include <QTest>

#include <cmath>
#include <numbers>

using namespace Qt::StringLiterals;

class TestEngine : public QObject
{
    Q_OBJECT

    Environment env;

    Number eval(const QString &text)
    {
        return parseStatement(text, env.callPredicate()).expr->evaluate(env);
    }

    QString errorOf(const QString &text, int *position = nullptr)
    {
        try {
            eval(text);
        } catch (const CalcError &error) {
            if (position)
                *position = error.position;
            return error.message;
        }
        return {};
    }

private slots:
    void init()
    {
        env = Environment();
    }

    void lexesNumbers()
    {
        const QList<Token> tokens = tokenize(u"1.5e3 .5 0x1F 0b101 0o17 2e"_s);
        QCOMPARE(tokens.size(), 8);
        QCOMPARE(tokens[0].value, 1500.0);
        QCOMPARE(tokens[1].value, 0.5);
        QCOMPARE(tokens[2].value, 31.0);
        QCOMPARE(tokens[3].value, 5.0);
        QCOMPARE(tokens[4].value, 15.0);
        // "2e" is 2 times e, not an exponent without digits.
        QCOMPARE(tokens[5].value, 2.0);
        QCOMPARE(tokens[6].type, Token::Identifier);
        QCOMPARE(tokens[6].text, u"e"_s);
        QCOMPARE(tokens[7].type, Token::End);
    }

    void lexesOperatorsAndKeywords()
    {
        const QList<Token> tokens = tokenize(u"a ** b << 1 mod 2 != x"_s);
        QCOMPARE(tokens[1].text, u"^"_s);
        QCOMPARE(tokens[3].text, u"<<"_s);
        QCOMPARE(tokens[5].type, Token::Operator);
        QCOMPARE(tokens[5].text, u"mod"_s);
        QCOMPARE(tokens[7].text, u"!="_s);
    }

    void lenientLexingMarksUnknownCharacters()
    {
        QVERIFY_THROWS_EXCEPTION(CalcError, tokenize(u"1 $ 2"_s));
        const QList<Token> tokens = tokenize(u"1 $ 2"_s, true);
        QCOMPARE(tokens[1].type, Token::Unknown);
        QCOMPARE(tokens[1].position, 2);
        // A prefix without digits, as while typing 0xFF.
        QVERIFY_THROWS_EXCEPTION(CalcError, tokenize(u"0x"_s));
        const QList<Token> half = tokenize(u"0x + 1"_s, true);
        QCOMPARE(half[0].type, Token::Unknown);
        QCOMPARE(half[0].length, 2);
        QCOMPARE(half[1].text, u"+"_s);
    }

    void arithmetic()
    {
        QCOMPARE(eval(u"1 + 2 * 3"_s), 7.0);
        QCOMPARE(eval(u"(1 + 2) * 3"_s), 9.0);
        QCOMPARE(eval(u"7 / 2"_s), 3.5);
        QCOMPARE(eval(u"2 ^ 10"_s), 1024.0);
        QCOMPARE(eval(u"10 mod 3"_s), 1.0);
        QCOMPARE(eval(u"10 % 3"_s), 1.0);
        QCOMPARE(eval(u"7 div 2"_s), 3.0);
        QCOMPARE(eval(u"-7 div 2"_s), -3.0);
        QCOMPARE(eval(u"3 × 4 ÷ 2"_s), 6.0);
    }

    void precedence()
    {
        QCOMPARE(eval(u"-2^2"_s), -4.0);
        QCOMPARE(eval(u"2^3^2"_s), 512.0);
        QCOMPARE(eval(u"2^-1"_s), 0.5);
        QCOMPARE(eval(u"2^3!"_s), 64.0);
        QCOMPARE(eval(u"1 + 2 == 3"_s), 1.0);
        QCOMPARE(eval(u"1 < 2 and 2 < 3"_s), 1.0);
        QCOMPARE(eval(u"1 + 1 << 2"_s), 8.0);
    }

    void implicitMultiplication()
    {
        env.setVariable(u"x"_s, 3);
        QCOMPARE(eval(u"2(3 + 4)"_s), 14.0);
        QCOMPARE(eval(u"2x"_s), 6.0);
        QCOMPARE(eval(u"(1 + 1)(2 + 2)"_s), 8.0);
        QCOMPARE(eval(u"2 pi"_s), 2 * std::numbers::pi);
        // A variable before a parenthesis multiplies; a function calls.
        QCOMPARE(eval(u"x(2)"_s), 6.0);
        QCOMPARE(eval(u"2 sin(0)"_s), 0.0);
    }

    void factorial()
    {
        QCOMPARE(eval(u"5!"_s), 120.0);
        QCOMPARE(eval(u"3!!"_s), 720.0);
        QCOMPARE(eval(u"0!"_s), 1.0);
        QVERIFY(std::fabs(eval(u"0.5!"_s) - std::sqrt(std::numbers::pi) / 2) < 1e-12);
        QCOMPARE(errorOf(u"(-1)!"_s), u"Factorial of a negative integer"_s);
    }

    void bitwise()
    {
        QCOMPARE(eval(u"0xF0 and 0x0F"_s), 0.0);
        QCOMPARE(eval(u"0xF0 | 0x0F"_s), 255.0);
        QCOMPARE(eval(u"0b1100 xor 0b1010"_s), 6.0);
        QCOMPARE(eval(u"1 shl 3"_s), 8.0);
        QCOMPARE(eval(u"256 >> 4"_s), 16.0);
        QCOMPARE(eval(u"~0"_s), -1.0);
        QCOMPARE(eval(u"not 5"_s), -6.0);
        QCOMPARE(errorOf(u"1.5 and 1"_s), u"Bitwise operations need integers"_s);
    }

    void variablesAndAssignment()
    {
        QCOMPARE(eval(u"x = 5"_s), 5.0);
        QCOMPARE(env.variable(u"x"_s), 5.0);
        QCOMPARE(eval(u"a = b = 2"_s), 2.0);
        QCOMPARE(env.variable(u"a"_s), 2.0);
        QCOMPARE(env.variable(u"b"_s), 2.0);
        QCOMPARE(eval(u"x * 2 + a"_s), 12.0);
        QCOMPARE(eval(u"(y = 3) + y"_s), 6.0);
        QCOMPARE(errorOf(u"pi = 3"_s), u"pi is a constant"_s);
        QCOMPARE(errorOf(u"sin = 3"_s), u"sin is a function"_s);
        int position = -1;
        QCOMPARE(errorOf(u"1 + unknown"_s, &position), u"Unknown variable unknown"_s);
        QCOMPARE(position, 4);
    }

    void userFunctions()
    {
        const Statement definition = parseStatement(u"f(x, y) = x^2 + y"_s);
        QVERIFY(definition.isDefinition());
        QCOMPARE(definition.functionName, u"f"_s);
        QCOMPARE(definition.parameters, (QStringList{u"x"_s, u"y"_s}));
        QCOMPARE(definition.bodySource, u"x^2 + y"_s);
        env.defineFunction(u"f"_s, {definition.parameters, definition.expr, definition.bodySource});
        QCOMPARE(eval(u"f(3, 1)"_s), 10.0);
        // The parameter hides the variable of the same name, and only while called.
        env.setVariable(u"x"_s, 100);
        QCOMPARE(eval(u"f(2, 0) + x"_s), 104.0);
        QCOMPARE(errorOf(u"f(1)"_s), u"f takes 2 arguments"_s);
        env.setVariable(u"g"_s, 1);
        try {
            env.defineFunction(u"g"_s, {{u"x"_s}, definition.expr, {}});
            QFAIL("A variable's name was taken for a function");
        } catch (const CalcError &error) {
            QCOMPARE(error.message, u"g is a variable"_s);
        }

        const Statement recursive = parseStatement(u"r(n) = r(n + 1)"_s);
        env.defineFunction(u"r"_s, {recursive.parameters, recursive.expr, recursive.bodySource});
        QCOMPARE(errorOf(u"r(1)"_s), u"r calls itself too deeply"_s);
        QVERIFY_THROWS_EXCEPTION(CalcError, env.defineFunction(u"sin"_s, {{u"x"_s}, definition.expr, {}}));
        QVERIFY_THROWS_EXCEPTION(CalcError, parseStatement(u"f(x, x) = x"_s));
    }

    void builtinFunctions()
    {
        QCOMPARE(eval(u"sqrt(16)"_s), 4.0);
        QCOMPARE(eval(u"abs(-3)"_s), 3.0);
        QCOMPARE(eval(u"round(pi, 2)"_s), 3.14);
        QCOMPARE(eval(u"log(1000)"_s), 3.0);
        QCOMPARE(eval(u"log(2, 8)"_s), 3.0);
        QCOMPARE(eval(u"min(3, 1, 2)"_s), 1.0);
        QCOMPARE(eval(u"max(3, 1, 2)"_s), 3.0);
        QCOMPARE(eval(u"average(1, 2, 3, 4)"_s), 2.5);
        QCOMPARE(eval(u"median(5, 1, 3)"_s), 3.0);
        QCOMPARE(eval(u"gcd(12, 18)"_s), 6.0);
        QCOMPARE(eval(u"lcm(4, 6)"_s), 12.0);
        QCOMPARE(eval(u"binomial(5, 2)"_s), 10.0);
        QCOMPARE(eval(u"npr(5, 2)"_s), 20.0);
        QCOMPARE(eval(u"mask(0xABCD, 8)"_s), 0xCD);
        QCOMPARE(eval(u"hypot(3, 4)"_s), 5.0);
        QCOMPARE(errorOf(u"sin()"_s), u"sin takes one argument"_s);
        QCOMPARE(errorOf(u"log(1, 2, 3)"_s), u"log takes 1 to 2 arguments"_s);
        QCOMPARE(errorOf(u"nothing(1)"_s), u"Unknown function nothing"_s);
        QCOMPARE(errorOf(u"sqrt(-1)"_s), u"sqrt is not defined here"_s);
    }

    void angleUnits()
    {
        QVERIFY(std::fabs(eval(u"sin(pi / 2)"_s) - 1) < 1e-12);
        env.angleUnit = AngleUnit::Degrees;
        QVERIFY(std::fabs(eval(u"sin(90)"_s) - 1) < 1e-12);
        QVERIFY(std::fabs(eval(u"acos(0)"_s) - 90) < 1e-12);
        QVERIFY(std::fabs(eval(u"atan2(1, 1)"_s) - 45) < 1e-12);
    }

    void errors()
    {
        int position = -1;
        QCOMPARE(errorOf(u"1 / 0"_s, &position), u"Division by zero"_s);
        QCOMPARE(position, 2);
        QCOMPARE(errorOf(u"2 +"_s), u"Expression is incomplete"_s);
        QCOMPARE(errorOf(u"(1 + 2"_s), u"Expected )"_s);
        QCOMPARE(errorOf(u"1 + 2)"_s), u"Unexpected ')'"_s);
        QCOMPARE(errorOf(u"2 3"_s), u"Unexpected '3'"_s);
        QCOMPARE(errorOf(u"0 ^ -1"_s), u"Division by zero"_s);
    }

    void evaluateWithBindsOneName()
    {
        env.setVariable(u"x"_s, 100);
        const ExprPtr expr = parseExpression(u"x^2"_s);
        QCOMPARE(env.evaluateWith(expr, u"x"_s, 3), 9.0);
        QCOMPARE(env.variable(u"x"_s), 100.0);
    }
};

QTEST_GUILESS_MAIN(TestEngine)
#include "tst_engine.moc"
