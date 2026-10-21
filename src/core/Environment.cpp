#include "Environment.h"

#include "Functions.h"
#include "Lexer.h"

#include <QtMath>

#include <cmath>
#include <numbers>

using namespace Qt::StringLiterals;

namespace {

constexpr int MaxCallDepth = 200;

} // namespace

Environment::Environment()
{
    m_variables.insert(u"ans"_s, 0);
}

bool Environment::isConstant(const QString &name)
{
    return name == u"pi"_s || name == u"e"_s;
}

Number Environment::variable(const QString &name, int position) const
{
    for (auto frame = m_frames.crbegin(); frame != m_frames.crend(); ++frame) {
        const auto it = frame->constFind(name);
        if (it != frame->cend())
            return *it;
    }
    note(name);
    if (m_scope && m_scope->owns(name)) {
        if (!m_scope->isVariable(name))
            throw CalcError{u"%1 is a function"_s.arg(name), position};
        return m_scope->variable(name, position);
    }
    const auto it = m_variables.constFind(name);
    if (it != m_variables.cend())
        return *it;
    if (name == u"pi"_s)
        return std::numbers::pi;
    if (name == u"e"_s)
        return std::numbers::e;
    throw CalcError{u"Unknown variable %1"_s.arg(name), position};
}

bool Environment::hasVariable(const QString &name) const
{
    note(name);
    if (m_scope && m_scope->owns(name))
        return m_scope->isVariable(name);
    return m_variables.contains(name) || isConstant(name);
}

void Environment::assign(const QString &name, Number value, int position)
{
    if (isResultName(name))
        throw CalcError{u"%1 cannot be assigned"_s.arg(name), position};
    if (m_scope && m_scope->isEvaluating())
        throw CalcError{u"%1 can only be assigned at the start"_s.arg(name), position};
    if (m_scope && m_scope->owns(name))
        throw CalcError{u"%1 is defined by %2"_s.arg(name, m_scope->owner(name)), position};
    if (isConstant(name))
        throw CalcError{u"%1 is a constant"_s.arg(name), position};
    if (isFunction(name))
        throw CalcError{u"%1 is a function"_s.arg(name), position};
    m_variables.insert(name, value);
}

bool Environment::isFunction(const QString &name) const
{
    note(name);
    if (m_scope && m_scope->owns(name))
        return m_scope->isFunction(name);
    return m_userFunctions.contains(name) || m_scriptFunctions.contains(name) || isBuiltinFunction(name);
}

bool Environment::isBuiltinFunction(const QString &name) const
{
    return findBuiltinFunction(name) != nullptr;
}

void Environment::defineFunction(const QString &name, const UserFunction &function, int position)
{
    if (isBuiltinFunction(name))
        throw CalcError{u"%1 is a built-in function"_s.arg(name), position};
    if (isConstant(name) || m_variables.contains(name))
        throw CalcError{u"%1 is a variable"_s.arg(name), position};
    m_scriptFunctions.remove(name);
    m_userFunctions.insert(name, function);
}

void Environment::defineScriptFunction(const QString &name, const ScriptFunction &function)
{
    m_userFunctions.remove(name);
    m_scriptFunctions.insert(name, function);
}

Number Environment::call(const QString &name, const QList<Number> &arguments, int position)
{
    note(name);
    const UserFunction *user = nullptr;
    if (m_scope && m_scope->owns(name)) {
        if (!m_scope->isFunction(name))
            throw CalcError{u"%1 is a variable"_s.arg(name), position};
        user = &m_scope->function(name, position);
    } else if (const auto it = m_userFunctions.constFind(name); it != m_userFunctions.cend()) {
        user = &*it;
    }
    if (user) {
        // A copy: the history may evaluate the entry that defines the
        // function again while the body runs, replacing it.
        const UserFunction function = *user;
        if (arguments.size() != function.parameters.size())
            throw CalcError{u"%1 takes %2"_s.arg(name, argumentCount(int(function.parameters.size()))), position};
        if (m_frames.size() >= MaxCallDepth)
            throw CalcError{u"%1 calls itself too deeply"_s.arg(name), position};
        QHash<QString, Number> frame;
        for (int i = 0; i < arguments.size(); ++i)
            frame.insert(function.parameters[i], arguments[i]);
        m_frames.append(frame);
        // Pop the frame while unwinding rather than in a catch block that
        // rethrows: MSVC keeps every active catch block on the stack, and
        // MaxCallDepth nested rethrows overflow the 1 MB Windows stack.
        struct FramePopper {
            QList<QHash<QString, Number>> &frames;
            ~FramePopper() { frames.removeLast(); }
        } popper{m_frames};
        return function.body->evaluate(*this);
    }
    if (const auto it = m_scriptFunctions.constFind(name); it != m_scriptFunctions.cend()) {
        if (it->arity >= 0 && arguments.size() != it->arity)
            throw CalcError{u"%1 takes %2"_s.arg(name, argumentCount(it->arity)), position};
        try {
            return it->call(arguments);
        } catch (CalcError &error) {
            if (error.position < 0)
                error.position = position;
            throw;
        }
    }
    if (const FunctionInfo *builtin = findBuiltinFunction(name)) {
        if (arguments.size() < builtin->minArgs
            || (builtin->maxArgs >= 0 && arguments.size() > builtin->maxArgs)) {
            throw CalcError{u"%1 takes %2"_s.arg(name, builtin->arityDescription()), position};
        }
        try {
            return builtin->call(arguments, *this);
        } catch (CalcError &error) {
            if (error.position < 0)
                error.position = position;
            throw;
        }
    }
    throw CalcError{u"Unknown function %1"_s.arg(name), position};
}

FunctionPredicate Environment::callPredicate() const
{
    return [this](const QString &name) { return !hasVariable(name) || isFunction(name); };
}

QString Environment::argumentCount(int count)
{
    return count == 1 ? u"one argument"_s : u"%1 arguments"_s.arg(count);
}

Number Environment::evaluateWith(const ExprPtr &expr, const QString &name, Number value)
{
    m_frames.append({{name, value}});
    try {
        const Number result = expr->evaluate(*this);
        m_frames.removeLast();
        return result;
    } catch (...) {
        m_frames.removeLast();
        throw;
    }
}

Number Environment::toRadians(Number angle) const
{
    note(u"#angle"_s);
    return angleUnit == AngleUnit::Degrees ? qDegreesToRadians(angle) : angle;
}

Number Environment::fromRadians(Number radians) const
{
    note(u"#angle"_s);
    return angleUnit == AngleUnit::Degrees ? qRadiansToDegrees(radians) : radians;
}

qint64 Environment::toInteger(Number value, int position)
{
    if (!std::isfinite(value) || std::floor(value) != value)
        throw CalcError{u"Bitwise operations need integers"_s, position};
    if (std::fabs(value) > 9.0e18)
        throw CalcError{u"Integer too large"_s, position};
    return qint64(value);
}
