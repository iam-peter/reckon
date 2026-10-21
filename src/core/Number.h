#pragma once

#include <QString>

// Every value reckon computes with. A typedef, so a wider type can take
// its place one day without touching the parser or the functions.
using Number = double;

// What went wrong where: thrown while lexing, parsing or evaluating and
// caught at the top of Calculator, which shows it on the expression.
struct CalcError
{
    QString message;
    // Offset into the expression, or -1 when it has no single place.
    int position = -1;
};
