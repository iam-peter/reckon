#include "ScriptEngine.h"

#include "Calculator.h"


#include <cmath>

using namespace Qt::StringLiterals;

ScriptEngine::ScriptEngine(Calculator *calculator)
    : QObject(calculator)
    , m_calculator(calculator)
    , m_api(new ScriptApi(calculator, this, &m_engine))
{
    m_engine.installExtensions(QJSEngine::ConsoleExtension);
    QJSValue api = m_engine.newQObject(m_api);
    QJSEngine::setObjectOwnership(m_api, QJSEngine::CppOwnership);
    m_engine.globalObject().setProperty(u"calc"_s, api);
    m_engine.evaluate(u"function print(...args) { calc.print(args.map(String).join(' ')) }"_s);
}

bool ScriptEngine::run(const QString &code, const QString &fileName)
{
    const QJSValue result = m_engine.evaluate(code, fileName);
    m_calculator->refreshLists();
    if (result.isError()) {
        const QString line = result.property(u"lineNumber"_s).toString();
        print(u"Error on line %1: %2"_s.arg(line, result.toString()));
        return false;
    }
    if (!result.isUndefined())
        print(result.toString());
    return true;
}

void ScriptEngine::clearOutput()
{
    if (m_output.isEmpty())
        return;
    m_output.clear();
    emit outputChanged();
}

void ScriptEngine::print(const QString &text)
{
    m_output += text + u'\n';
    emit outputChanged();
}

ScriptApi::ScriptApi(Calculator *calculator, ScriptEngine *engine, QJSEngine *js)
    : QObject(engine)
    , m_calculator(calculator)
    , m_engine(engine)
    , m_js(js)
{
}

double ScriptApi::ans() const
{
    return m_calculator->environment().variable(u"ans"_s);
}

double ScriptApi::eval(const QString &expression)
{
    try {
        const ExprPtr expr = parseExpression(expression, m_calculator->environment().callPredicate());
        return expr->evaluate(m_calculator->environment());
    } catch (const CalcError &error) {
        m_js->throwError(error.message);
        return std::nan("");
    }
}

bool ScriptApi::run(const QString &expression)
{
    return m_calculator->evaluate(expression);
}

double ScriptApi::get(const QString &name)
{
    try {
        return m_calculator->environment().variable(name);
    } catch (const CalcError &error) {
        m_js->throwError(error.message);
        return std::nan("");
    }
}

void ScriptApi::set(const QString &name, double value)
{
    try {
        m_calculator->environment().assign(name, value);
        m_calculator->refreshLists();
    } catch (const CalcError &error) {
        m_js->throwError(error.message);
    }
}

bool ScriptApi::has(const QString &name)
{
    return m_calculator->environment().hasVariable(name);
}

void ScriptApi::define(const QString &name, const QJSValue &function, int arity)
{
    if (!function.isCallable()) {
        m_js->throwError(u"calc.define needs a function"_s);
        return;
    }
    if (name.isEmpty() || !name[0].isLetter() || m_calculator->environment().isBuiltinFunction(name)) {
        m_js->throwError(u"Cannot define %1"_s.arg(name));
        return;
    }
    ScriptFunction script;
    if (arity >= -1) {
        script.arity = arity;
    } else {
        // length leaves rest parameters out, so (...args) => looks like a
        // function of none: those take any number.
        const int length = function.property(u"length"_s).toInt();
        script.arity = length == 0 ? -1 : length;
    }
    script.call = [function, name](const QList<Number> &arguments) mutable {
        QJSValueList values;
        for (Number argument : arguments)
            values.append(argument);
        const QJSValue result = function.call(values);
        if (result.isError())
            throw CalcError{u"%1: %2"_s.arg(name, result.toString())};
        if (!result.isNumber())
            throw CalcError{u"%1 did not return a number"_s.arg(name)};
        return result.toNumber();
    };
    m_calculator->environment().defineScriptFunction(name, script);
    m_calculator->refreshLists();
}

void ScriptApi::undefine(const QString &name)
{
    m_calculator->environment().removeScriptFunction(name);
    m_calculator->refreshLists();
}

QString ScriptApi::format(double value)
{
    return m_calculator->formatValue(value);
}

QVariantList ScriptApi::history()
{
    QVariantList list;
    for (const HistoryEntry &entry : m_calculator->history()->entries()) {
        list.append(QVariantMap{
            {u"expression"_s, entry.expression},
            {u"value"_s, entry.value},
            {u"result"_s, entry.result},
            {u"error"_s, entry.error},
        });
    }
    return list;
}

void ScriptApi::plot(const QString &expression, const QString &color)
{
    emit m_engine->plotRequested(expression, color);
}

void ScriptApi::clearPlots()
{
    emit m_engine->plotsCleared();
}

void ScriptApi::print(const QString &text)
{
    m_engine->print(text);
}
