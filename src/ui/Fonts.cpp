#include "Fonts.h"

#include <QDirIterator>
#include <QFontDatabase>

void Fonts::registerBundled()
{
    QDirIterator fonts(QStringLiteral(":/qt/qml/Reckon/fonts"), {QStringLiteral("*.ttf")});
    while (fonts.hasNext()) {
        if (QFontDatabase::addApplicationFont(fonts.next()) < 0)
            qWarning("Could not load the font %s", qPrintable(fonts.filePath()));
    }
}
