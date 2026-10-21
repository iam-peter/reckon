#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

// The fonts reckon ships with: Inter for the interface and JetBrains Mono
// for expressions and results, both under the SIL Open Font License.
class Fonts : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    using QObject::QObject;

    // Call once before any UI is created.
    static void registerBundled();
};
