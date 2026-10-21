#pragma once

#include "Expression.h"
#include "Number.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

enum class AngleUnit { Radians, Degrees };

// A function the user defined with `f(x) = ...`.
struct UserFunction
{
    QStringList parameters;
    ExprPtr body;
    QString source; // the body as typed
};

// A function a script defined, called with its arguments evaluated.
struct ScriptFunction
{
    int arity = -1; // -1: any number of arguments
    std::function<Number(const QList<Number> &)> call;
};

// The variables, the functions the user and scripts defined and the
// angle unit: everything an expression is evaluated against. The
// constants pi and e, and `ans`, read like variables; pi and e cannot be
// assigned. Function parameters live in frames pushed for the duration of
// a call, so a parameter hides a variable of the same name.
class Environment
{
public:
    Environment();

    AngleUnit angleUnit = AngleUnit::Radians;

    // Throws for an unknown name.
    Number variable(const QString &name, int position = -1) const;
    bool hasVariable(const QString &name) const;
    static bool isConstant(const QString &name);
    // Throws for a constant or a function's name.
    void assign(const QString &name, Number value, int position = -1);
    void setVariable(const QString &name, Number value) { m_variables.insert(name, value); }
    void removeVariable(const QString &name) { m_variables.remove(name); }
    void clearVariables() { m_variables.clear(); }
    const QHash<QString, Number> &variables() const { return m_variables; }

    bool isFunction(const QString &name) const;
    bool isBuiltinFunction(const QString &name) const;
    void defineFunction(const QString &name, const UserFunction &function, int position = -1);
    void removeFunction(const QString &name) { m_userFunctions.remove(name); }
    void clearFunctions() { m_userFunctions.clear(); }
    const QHash<QString, UserFunction> &userFunctions() const { return m_userFunctions; }

    void defineScriptFunction(const QString &name, const ScriptFunction &function);
    void removeScriptFunction(const QString &name) { m_scriptFunctions.remove(name); }
    void clearScriptFunctions() { m_scriptFunctions.clear(); }
    const QHash<QString, ScriptFunction> &scriptFunctions() const { return m_scriptFunctions; }

    Number call(const QString &name, const QList<Number> &arguments, int position = -1);

    // For the parser: `name(` calls unless `name` is a variable, which a
    // parenthesis then multiplies.
    FunctionPredicate callPredicate() const;
    // "one argument", "2 arguments".
    static QString argumentCount(int count);

    // Evaluates `expr` with `name` bound to `value`, hiding any variable
    // of that name: the graph samples y = f(x) this way.
    Number evaluateWith(const ExprPtr &expr, const QString &name, Number value);

    // Angles go into the trigonometric functions in the current unit.
    Number toRadians(Number angle) const;
    Number fromRadians(Number radians) const;

    // Bitwise operators work on integers; anything else is an error.
    static qint64 toInteger(Number value, int position = -1);

private:
    QHash<QString, Number> m_variables;
    QHash<QString, UserFunction> m_userFunctions;
    QHash<QString, ScriptFunction> m_scriptFunctions;
    QList<QHash<QString, Number>> m_frames;
};
