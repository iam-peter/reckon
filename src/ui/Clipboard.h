#pragma once

#include <QClipboard>
#include <QGuiApplication>
#include <QImage>
#include <QObject>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

class Clipboard : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    using QObject::QObject;

    Q_INVOKABLE void setText(const QString &text) { QGuiApplication::clipboard()->setText(text); }
    Q_INVOKABLE QString text() const { return QGuiApplication::clipboard()->text(); }
    // The image of a grabToImage() result.
    Q_INVOKABLE void setImage(const QVariant &image) { QGuiApplication::clipboard()->setImage(image.value<QImage>()); }
};
