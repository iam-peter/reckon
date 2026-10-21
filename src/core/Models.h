#pragma once

#include "Number.h"

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QtQml/qqmlregistration.h>

// What was typed and what came of it, oldest first.
struct HistoryEntry
{
    int id = 0; // the result is `$id`
    QString expression;
    Number value = 0;
    QString result; // the value as formatted, or empty for an error
    QString error;
    int errorPosition = -1;
    bool definition = false; // `f(x) = ...`: the result is the definition, not a number
    // "x is redefined in $7": a later entry took over what this one defines.
    QString replaced;
    // Goes up whenever a change elsewhere changed the result.
    int revision = 0;

    bool operator==(const HistoryEntry &) const = default;
};

class HistoryModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Comes from Calculator")
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        ExpressionRole = Qt::UserRole + 1, ValueRole, ResultRole, ErrorRole, ErrorPositionRole, IsErrorRole,
        NameRole, IsDefinitionRole, ReplacedRole, RevisionRole,
    };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex &parent = {}) const override;
    int count() const { return rowCount(); }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void append(const HistoryEntry &entry);
    void clear();
    void setEntries(const QList<HistoryEntry> &entries);
    // Brings the rows in line with `entries`, matched by id: rows are
    // removed, inserted and changed rather than the model reset, so the
    // views keep their place.
    void sync(const QList<HistoryEntry> &entries);
    const QList<HistoryEntry> &entries() const { return m_entries; }
    // Rewrites every result after the number format changed.
    void reformat(const std::function<QString(Number)> &format);

    Q_INVOKABLE QString expressionAt(int row) const;
    Q_INVOKABLE QString resultAt(int row) const;
    // The most recent expression, then older ones: for the editor's history keys.
    Q_INVOKABLE QStringList expressions() const;

signals:
    void countChanged();

private:
    QList<HistoryEntry> m_entries;
};

// A named thing with a value and a description: variables, functions and
// constants are listed through it, filtered by a search text.
struct NamedEntry
{
    QString name;
    QString value; // the variable's value, the function's usage, the constant's value
    QString description;
    QString category;
    bool builtin = false;
    QString insertText; // what a click puts into the expression
};

class NameListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Comes from Calculator")
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role { NameRole = Qt::UserRole + 1, ValueRole, DescriptionRole, CategoryRole, BuiltinRole, InsertTextRole };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex &parent = {}) const override;
    int count() const { return rowCount(); }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setEntries(const QList<NamedEntry> &entries);
    const QList<NamedEntry> &allEntries() const { return m_entries; }

    QString filter() const { return m_filter; }
    void setFilter(const QString &filter);

    Q_INVOKABLE QString nameAt(int row) const;
    Q_INVOKABLE QString insertTextAt(int row) const;

signals:
    void filterChanged();
    void countChanged();

private:
    void applyFilter();

    QList<NamedEntry> m_entries;
    QList<NamedEntry> m_shown;
    QString m_filter;
};
