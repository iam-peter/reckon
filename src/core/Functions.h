#pragma once

#include "Number.h"

#include <QList>
#include <QString>

#include <functional>

class Environment;

// A built-in function: its name and usage for the function list and the
// completer, how many arguments it takes, and the function itself.
struct FunctionInfo
{
    QString name;
    QString usage; // "sin(x)", "min(x, y, ...)"
    QString description;
    QString category;
    int minArgs = 1;
    int maxArgs = 1; // -1: any number
    std::function<Number(const QList<Number> &, const Environment &)> call;

    // "one argument", "1 or 2 arguments", "at least one argument".
    QString arityDescription() const;
};

const QList<FunctionInfo> &builtinFunctions();
const FunctionInfo *findBuiltinFunction(const QString &name);
