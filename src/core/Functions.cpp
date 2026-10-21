#include "Functions.h"

#include "Environment.h"

#include <QHash>
#include <QRandomGenerator>

#include <algorithm>
#include <cmath>
#include <numeric>

using namespace Qt::StringLiterals;

namespace {

using Args = QList<Number>;

Number checked(Number result, const char *what)
{
    if (std::isnan(result))
        throw CalcError{u"%1 is not defined here"_s.arg(QLatin1StringView(what))};
    return result;
}

Number nth(const Args &args, int i, Number fallback)
{
    return i < args.size() ? args[i] : fallback;
}

Number integerArg(Number value, const char *what)
{
    if (!std::isfinite(value) || std::floor(value) != value)
        throw CalcError{u"%1 needs integers"_s.arg(QLatin1StringView(what))};
    return value;
}

Number sum(const Args &args)
{
    return std::accumulate(args.cbegin(), args.cend(), Number(0));
}

Number mean(const Args &args)
{
    return sum(args) / args.size();
}

Number variance(const Args &args)
{
    const Number m = mean(args);
    Number total = 0;
    for (Number x : args)
        total += (x - m) * (x - m);
    return total / args.size();
}

Number gcd2(Number a, Number b)
{
    a = std::fabs(integerArg(a, "gcd"));
    b = std::fabs(integerArg(b, "gcd"));
    while (b != 0) {
        const Number t = std::fmod(a, b);
        a = b;
        b = t;
    }
    return a;
}

Number binomial(Number n, Number k)
{
    integerArg(n, "binomial");
    integerArg(k, "binomial");
    if (k < 0 || k > n)
        return 0;
    k = std::min(k, n - k);
    Number result = 1;
    for (Number i = 1; i <= k; ++i)
        result = result * (n - k + i) / i;
    return std::round(result);
}

QList<FunctionInfo> makeFunctions()
{
    QList<FunctionInfo> f;
    auto add = [&f](const QString &name, const QString &usage, const QString &description,
                    const QString &category, int minArgs, int maxArgs, auto call) {
        f.append({name, usage, description, category, minArgs, maxArgs, call});
    };
    auto unary = [&add](const QString &name, const QString &description, const QString &category,
                        Number (*fn)(Number)) {
        add(name, name + u"(x)"_s, description, category, 1, 1,
            [fn, name](const Args &a, const Environment &) {
                return checked(fn(a[0]), name.toUtf8().constData());
            });
    };

    const QString general = u"General"_s;
    unary(u"abs"_s, u"Absolute value"_s, general, [](Number x) { return std::fabs(x); });
    unary(u"sign"_s, u"-1, 0 or 1 by the sign of x"_s, general,
          [](Number x) { return Number(x > 0 ? 1 : x < 0 ? -1 : 0); });
    unary(u"ceil"_s, u"Smallest integer not less than x"_s, general, [](Number x) { return std::ceil(x); });
    unary(u"floor"_s, u"Largest integer not greater than x"_s, general, [](Number x) { return std::floor(x); });
    add(u"round"_s, u"round(x [, places])"_s, u"x rounded to the nearest integer, or to places decimals"_s, general,
        1, 2, [](const Args &a, const Environment &) {
            const Number scale = std::pow(10.0, nth(a, 1, 0));
            return std::round(a[0] * scale) / scale;
        });
    unary(u"trunc"_s, u"x with its fraction cut off"_s, general, [](Number x) { return std::trunc(x); });
    unary(u"int"_s, u"The integer part of x"_s, general, [](Number x) { return std::trunc(x); });
    unary(u"frac"_s, u"The fractional part of x"_s, general, [](Number x) { return x - std::trunc(x); });
    unary(u"sqrt"_s, u"Square root"_s, general, [](Number x) { return std::sqrt(x); });
    unary(u"cbrt"_s, u"Cube root"_s, general, [](Number x) { return std::cbrt(x); });
    unary(u"exp"_s, u"e to the power of x"_s, general, [](Number x) { return std::exp(x); });
    unary(u"ln"_s, u"Natural logarithm"_s, general, [](Number x) { return std::log(x); });
    add(u"log"_s, u"log(x) or log(base, x)"_s, u"Logarithm to base 10, or to the given base"_s, general, 1, 2,
        [](const Args &a, const Environment &) {
            if (a.size() == 1)
                return checked(std::log10(a[0]), "log");
            return checked(std::log(a[1]) / std::log(a[0]), "log");
        });
    unary(u"lg"_s, u"Logarithm to base 10"_s, general, [](Number x) { return std::log10(x); });
    unary(u"lb"_s, u"Logarithm to base 2"_s, general, [](Number x) { return std::log2(x); });
    unary(u"log2"_s, u"Logarithm to base 2"_s, general, [](Number x) { return std::log2(x); });
    add(u"hypot"_s, u"hypot(x, y)"_s, u"Length of the hypotenuse, sqrt(x² + y²)"_s, general, 2, 2,
        [](const Args &a, const Environment &) { return std::hypot(a[0], a[1]); });
    unary(u"gamma"_s, u"Gamma function"_s, general, [](Number x) { return std::tgamma(x); });
    unary(u"lngamma"_s, u"Natural logarithm of the gamma function"_s, general,
          [](Number x) { return std::lgamma(x); });
    add(u"random"_s, u"random()"_s, u"A random number from 0 up to 1"_s, general, 0, 0,
        [](const Args &, const Environment &) { return QRandomGenerator::global()->generateDouble(); });

    const QString trig = u"Trigonometry"_s;
    auto angleIn = [&add, &trig](const QString &name, const QString &description, Number (*fn)(Number)) {
        add(name, name + u"(x)"_s, description, trig, 1, 1, [fn, name](const Args &a, const Environment &env) {
            return checked(fn(env.toRadians(a[0])), name.toUtf8().constData());
        });
    };
    auto angleOut = [&add, &trig](const QString &name, const QString &description, Number (*fn)(Number)) {
        add(name, name + u"(x)"_s, description, trig, 1, 1, [fn, name](const Args &a, const Environment &env) {
            return env.fromRadians(checked(fn(a[0]), name.toUtf8().constData()));
        });
    };
    angleIn(u"sin"_s, u"Sine"_s, [](Number x) { return std::sin(x); });
    angleIn(u"cos"_s, u"Cosine"_s, [](Number x) { return std::cos(x); });
    angleIn(u"tan"_s, u"Tangent"_s, [](Number x) { return std::tan(x); });
    angleIn(u"cot"_s, u"Cotangent"_s, [](Number x) { return 1 / std::tan(x); });
    angleIn(u"sec"_s, u"Secant"_s, [](Number x) { return 1 / std::cos(x); });
    angleIn(u"csc"_s, u"Cosecant"_s, [](Number x) { return 1 / std::sin(x); });
    angleOut(u"asin"_s, u"Arc sine"_s, [](Number x) { return std::asin(x); });
    angleOut(u"acos"_s, u"Arc cosine"_s, [](Number x) { return std::acos(x); });
    angleOut(u"atan"_s, u"Arc tangent"_s, [](Number x) { return std::atan(x); });
    angleOut(u"acot"_s, u"Arc cotangent"_s, [](Number x) { return std::atan(1 / x); });
    add(u"atan2"_s, u"atan2(y, x)"_s, u"Angle of the point (x, y)"_s, trig, 2, 2,
        [](const Args &a, const Environment &env) { return env.fromRadians(std::atan2(a[0], a[1])); });
    add(u"degrees"_s, u"degrees(x)"_s, u"Radians to degrees"_s, trig, 1, 1,
        [](const Args &a, const Environment &) { return a[0] * 180 / M_PI; });
    add(u"radians"_s, u"radians(x)"_s, u"Degrees to radians"_s, trig, 1, 1,
        [](const Args &a, const Environment &) { return a[0] * M_PI / 180; });

    const QString hyperbolic = u"Hyperbolic"_s;
    unary(u"sinh"_s, u"Hyperbolic sine"_s, hyperbolic, [](Number x) { return std::sinh(x); });
    unary(u"cosh"_s, u"Hyperbolic cosine"_s, hyperbolic, [](Number x) { return std::cosh(x); });
    unary(u"tanh"_s, u"Hyperbolic tangent"_s, hyperbolic, [](Number x) { return std::tanh(x); });
    unary(u"asinh"_s, u"Inverse hyperbolic sine"_s, hyperbolic, [](Number x) { return std::asinh(x); });
    unary(u"acosh"_s, u"Inverse hyperbolic cosine"_s, hyperbolic, [](Number x) { return std::acosh(x); });
    unary(u"atanh"_s, u"Inverse hyperbolic tangent"_s, hyperbolic, [](Number x) { return std::atanh(x); });

    const QString statistics = u"Statistics"_s;
    auto nary = [&add, &statistics](const QString &name, const QString &description, Number (*fn)(const Args &)) {
        add(name, name + u"(x, y, ...)"_s, description, statistics, 1, -1,
            [fn](const Args &a, const Environment &) { return fn(a); });
    };
    nary(u"min"_s, u"The smallest value"_s, [](const Args &a) { return *std::min_element(a.cbegin(), a.cend()); });
    nary(u"max"_s, u"The largest value"_s, [](const Args &a) { return *std::max_element(a.cbegin(), a.cend()); });
    nary(u"sum"_s, u"The sum of the values"_s, sum);
    nary(u"product"_s, u"The product of the values"_s,
         [](const Args &a) { return std::accumulate(a.cbegin(), a.cend(), Number(1), std::multiplies<>()); });
    nary(u"average"_s, u"The arithmetic mean"_s, mean);
    nary(u"mean"_s, u"The arithmetic mean"_s, mean);
    nary(u"geomean"_s, u"The geometric mean"_s, [](const Args &a) {
        Number logs = 0;
        for (Number x : a) {
            if (x <= 0)
                throw CalcError{u"geomean needs positive values"_s};
            logs += std::log(x);
        }
        return std::exp(logs / a.size());
    });
    nary(u"median"_s, u"The middle value"_s, [](const Args &a) {
        Args sorted = a;
        std::sort(sorted.begin(), sorted.end());
        const auto n = sorted.size();
        return n % 2 ? sorted[n / 2] : (sorted[n / 2 - 1] + sorted[n / 2]) / 2;
    });
    nary(u"variance"_s, u"The population variance"_s, variance);
    nary(u"stddev"_s, u"The population standard deviation"_s,
         [](const Args &a) { return std::sqrt(variance(a)); });

    const QString discrete = u"Integers"_s;
    add(u"binomial"_s, u"binomial(n, k)"_s, u"Ways to choose k of n, the binomial coefficient"_s, discrete, 2, 2,
        [](const Args &a, const Environment &) { return binomial(a[0], a[1]); });
    add(u"ncr"_s, u"ncr(n, k)"_s, u"Combinations of k out of n"_s, discrete, 2, 2,
        [](const Args &a, const Environment &) { return binomial(a[0], a[1]); });
    add(u"npr"_s, u"npr(n, k)"_s, u"Permutations of k out of n"_s, discrete, 2, 2,
        [](const Args &a, const Environment &) {
            integerArg(a[0], "npr");
            integerArg(a[1], "npr");
            if (a[1] < 0 || a[1] > a[0])
                return Number(0);
            Number result = 1;
            for (Number i = a[0] - a[1] + 1; i <= a[0]; ++i)
                result *= i;
            return result;
        });
    add(u"gcd"_s, u"gcd(x, y, ...)"_s, u"Greatest common divisor"_s, discrete, 2, -1,
        [](const Args &a, const Environment &) {
            Number result = a[0];
            for (qsizetype i = 1; i < a.size(); ++i)
                result = gcd2(result, a[i]);
            return result;
        });
    add(u"lcm"_s, u"lcm(x, y, ...)"_s, u"Least common multiple"_s, discrete, 2, -1,
        [](const Args &a, const Environment &) {
            Number result = std::fabs(integerArg(a[0], "lcm"));
            for (qsizetype i = 1; i < a.size(); ++i) {
                const Number g = gcd2(result, a[i]);
                result = g == 0 ? 0 : std::fabs(result * a[i]) / g;
            }
            return result;
        });
    add(u"mod"_s, u"mod(x, y)"_s, u"Remainder of x divided by y"_s, discrete, 2, 2,
        [](const Args &a, const Environment &) {
            if (a[1] == 0)
                throw CalcError{u"Division by zero"_s};
            return std::fmod(a[0], a[1]);
        });
    add(u"idiv"_s, u"idiv(x, y)"_s, u"x divided by y, rounded towards zero"_s, discrete, 2, 2,
        [](const Args &a, const Environment &) {
            if (a[1] == 0)
                throw CalcError{u"Division by zero"_s};
            return std::trunc(a[0] / a[1]);
        });
    add(u"mask"_s, u"mask(x, bits)"_s, u"The lowest bits of x"_s, discrete, 2, 2,
        [](const Args &a, const Environment &) {
            const qint64 bits = Environment::toInteger(a[1]);
            if (bits < 0 || bits > 62)
                throw CalcError{u"Bit count out of range"_s};
            return Number(Environment::toInteger(a[0]) & ((qint64(1) << bits) - 1));
        });
    return f;
}

} // namespace

QString FunctionInfo::arityDescription() const
{
    if (maxArgs < 0)
        return minArgs <= 1 ? u"at least one argument"_s : u"at least %1 arguments"_s.arg(minArgs);
    if (minArgs == maxArgs)
        return minArgs == 1 ? u"one argument"_s : u"%1 arguments"_s.arg(minArgs);
    return u"%1 to %2 arguments"_s.arg(minArgs).arg(maxArgs);
}

const QList<FunctionInfo> &builtinFunctions()
{
    static const QList<FunctionInfo> functions = makeFunctions();
    return functions;
}

const FunctionInfo *findBuiltinFunction(const QString &name)
{
    static const QHash<QString, const FunctionInfo *> index = [] {
        QHash<QString, const FunctionInfo *> map;
        for (const FunctionInfo &function : builtinFunctions())
            map.insert(function.name, &function);
        return map;
    }();
    return index.value(name, nullptr);
}
