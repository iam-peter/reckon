#include "ScriptLibrary.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

ScriptLibrary::ScriptLibrary(QObject *parent)
    : QObject(parent)
    , m_directory(defaultDirectory())
{
    refresh();
}

QString ScriptLibrary::defaultDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + u"/scripts"_s;
}

void ScriptLibrary::setDirectory(const QString &directory)
{
    if (m_directory == directory)
        return;
    m_directory = directory;
    emit directoryChanged();
    refresh();
}

void ScriptLibrary::refresh()
{
    QStringList names;
    const QFileInfoList files = QDir(m_directory).entryInfoList({u"*.js"_s}, QDir::Files, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &file : files)
        names.append(file.completeBaseName());
    std::sort(names.begin(), names.end(), [](const QString &a, const QString &b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    if (names == m_names)
        return;
    m_names = names;
    emit namesChanged();
}

QString ScriptLibrary::path(const QString &name) const
{
    return m_directory + u'/' + name + u".js"_s;
}

QString ScriptLibrary::read(const QString &name) const
{
    QFile file(path(name));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}

bool ScriptLibrary::write(const QString &name, const QString &code)
{
    QDir().mkpath(m_directory);
    QSaveFile file(path(name));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    file.write(code.toUtf8());
    if (!file.commit())
        return false;
    refresh();
    return true;
}

bool ScriptLibrary::remove(const QString &name)
{
    const bool removed = QFile::remove(path(name));
    refresh();
    return removed;
}

bool ScriptLibrary::rename(const QString &from, const QString &to)
{
    if (to.isEmpty() || to.contains(u'/') || to.contains(u'\\') || m_names.contains(to))
        return false;
    const bool renamed = QFile::rename(path(from), path(to));
    refresh();
    return renamed;
}

QString ScriptLibrary::create(const QString &name)
{
    QString candidate = name;
    for (int i = 2; m_names.contains(candidate); ++i)
        candidate = u"%1 %2"_s.arg(name).arg(i);
    if (!write(candidate, QString()))
        return {};
    return candidate;
}
