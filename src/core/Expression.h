#pragma once

#include "Number.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <memory>

class Environment;

// A node of a parsed expression. Evaluating it reads variables and calls
// functions through the environment and throws CalcError on anything it
// cannot do.
struct Expr
{
    virtual ~Expr() = default;
    virtual Number evaluate(Environment &env) const = 0;

    int position = 0;
};

using ExprPtr = std::shared_ptr<const Expr>;

// A name a statement assigns, `x` in `x = 2`.
struct AssignTarget
{
    QString name;
    int position = 0;
};

// One line of input: an expression, which may assign variables along the
// way, or the definition of a function `f(x, y) = body`.
struct Statement
{
    ExprPtr expr; // the expression, or the definition's body
    QString functionName; // set for a definition
    QStringList parameters;
    QString bodySource; // the definition's body as typed, kept to show and save it
    // `a = b = expr` at the start: a and b, and expr, which `expr` assigns
    // to them when evaluated. The history binds the names to `value`
    // instead, without assigning.
    QList<AssignTarget> targets;
    ExprPtr value;

    bool isDefinition() const { return !functionName.isEmpty(); }
};

// Tells the parser whether `name(` is a call or a variable times a
// parenthesis (see Environment::callPredicate()). Without one, every
// `name(` is a call.
using FunctionPredicate = std::function<bool(const QString &name)>;

// Throws CalcError where the text makes no sense.
Statement parseStatement(const QString &text, const FunctionPredicate &isFunction = {});
ExprPtr parseExpression(const QString &text, const FunctionPredicate &isFunction = {});
