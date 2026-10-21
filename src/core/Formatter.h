#pragma once

#include "Number.h"

#include <QString>

// How results are written out.
struct NumberFormat
{
    enum Mode { General, Fixed, Scientific, Engineering, Binary, Octal, Hexadecimal };

    Mode mode = General;
    // Digits after the point; -1 shows as many as the value needs, up to
    // 15 significant digits.
    int precision = -1;
    // Thousands separated with thin spaces.
    bool groupDigits = false;
};

QString formatNumber(Number value, const NumberFormat &format = {});
