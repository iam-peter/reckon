#pragma once

#include <QObject>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

// The scripts kept as .js files in a folder, by name without the
// extension. The folder is the application data folder's `scripts` unless
// set otherwise.
class ScriptLibrary : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString directory READ directory WRITE setDirectory NOTIFY directoryChanged)
    Q_PROPERTY(QString defaultDirectory READ defaultDirectory CONSTANT)
    Q_PROPERTY(QStringList names READ names NOTIFY namesChanged)

public:
    explicit ScriptLibrary(QObject *parent = nullptr);

    QString directory() const { return m_directory; }
    void setDirectory(const QString &directory);
    // The application data folder's `scripts`.
    static QString defaultDirectory();
    QStringList names() const { return m_names; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE QString read(const QString &name) const;
    Q_INVOKABLE bool write(const QString &name, const QString &code);
    Q_INVOKABLE bool remove(const QString &name);
    Q_INVOKABLE bool rename(const QString &from, const QString &to);
    // A new empty script named `name`, or `name 2` and so on when taken;
    // returns the name it got.
    Q_INVOKABLE QString create(const QString &name);
    Q_INVOKABLE QString path(const QString &name) const;

signals:
    void directoryChanged();
    void namesChanged();

private:
    QString m_directory;
    QStringList m_names;
};
