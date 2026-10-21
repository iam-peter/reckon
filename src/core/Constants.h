#pragma once

#include "Number.h"

#include <QList>
#include <QString>

// A constant of nature or mathematics, for the constants list. Clicking
// one puts its value into the expression.
struct ConstantInfo
{
    QString name;
    Number value;
    QString unit;
    QString category;
};

const QList<ConstantInfo> &constants();
