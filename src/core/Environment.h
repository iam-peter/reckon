#pragma once

#include "Expression.h"
#include "Number.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>
#include <utility>

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

// The names the history binds: `$3`, `ans`, and the variables and
// functions its entries define, which take precedence over the
// environment's own. An entry being evaluated depends on every name its
// evaluation touches, and the scope hears of each through note().
class BindingScope
{
public:
    virtual ~BindingScope() = default;

    // A name the expression being evaluated depends on, whether it exists
    // or not, so the expression updates once it does.
    virtual void note(const QString &name) = 0;
    // True if the scope has `name`; then it decides what the name is.
    virtual bool owns(const QString &name) const = 0;
    virtual bool isVariable(const QString &name) const = 0;
    virtual bool isFunction(const QString &name) const = 0;
    // The value or the function of a name the scope owns. Throws when the
    // entry behind it has an error or the names depend on each other.
    virtual Number variable(const QString &name, int position) = 0;
    virtual const UserFunction &function(const QString &name, int position) = 0;
    // The entry that owns `name`, "$3", to say where to change it.
    virtual QString owner(const QString &name) const = 0;
    // While an entry is evaluated: assignments inside an expression would
    // change a binding behind the history's back.
    virtual bool isEvaluating() const = 0;
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

    // The history's names, looked up before the environment's own.
    BindingScope *scope() const { return m_scope; }
    void setScope(BindingScope *scope) { m_scope = scope; }
    // The history evaluates an entry with the frames of a call that asked
    // for its value set aside, so parameters never leak into it.
    QList<QHash<QString, Number>> takeFrames() { return std::exchange(m_frames, {}); }
    void restoreFrames(QList<QHash<QString, Number>> frames) { m_frames = std::move(frames); }

private:
    void note(const QString &name) const
    {
        if (m_scope)
            m_scope->note(name);
    }

    BindingScope *m_scope = nullptr;
    QHash<QString, Number> m_variables;
    QHash<QString, UserFunction> m_userFunctions;
    QHash<QString, ScriptFunction> m_scriptFunctions;
    QList<QHash<QString, Number>> m_frames;
};
