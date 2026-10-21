#include "Models.h"

int HistoryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_entries.size());
}

QVariant HistoryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size())
        return {};
    const HistoryEntry &entry = m_entries[index.row()];
    switch (role) {
    case ExpressionRole:
        return entry.expression;
    case ValueRole:
        return entry.value;
    case ResultRole:
        return entry.result;
    case ErrorRole:
        return entry.error;
    case ErrorPositionRole:
        return entry.errorPosition;
    case IsErrorRole:
        return !entry.error.isEmpty();
    }
    return {};
}

QHash<int, QByteArray> HistoryModel::roleNames() const
{
    return {
        {ExpressionRole, "expression"}, {ValueRole, "value"},
        {ResultRole, "result"}, {ErrorRole, "error"},
        {ErrorPositionRole, "errorPosition"}, {IsErrorRole, "isError"},
    };
}

void HistoryModel::append(const HistoryEntry &entry)
{
    beginInsertRows({}, int(m_entries.size()), int(m_entries.size()));
    m_entries.append(entry);
    endInsertRows();
    emit countChanged();
}

void HistoryModel::clear()
{
    setEntries({});
}

void HistoryModel::setEntries(const QList<HistoryEntry> &entries)
{
    beginResetModel();
    m_entries = entries;
    endResetModel();
    emit countChanged();
}

void HistoryModel::reformat(const std::function<QString(Number)> &format)
{
    for (HistoryEntry &entry : m_entries) {
        if (entry.error.isEmpty() && !entry.definition)
            entry.result = format(entry.value);
    }
    if (!m_entries.isEmpty())
        emit dataChanged(index(0), index(int(m_entries.size()) - 1), {ResultRole});
}

QString HistoryModel::expressionAt(int row) const
{
    return row >= 0 && row < m_entries.size() ? m_entries[row].expression : QString();
}

QString HistoryModel::resultAt(int row) const
{
    return row >= 0 && row < m_entries.size() ? m_entries[row].result : QString();
}

QStringList HistoryModel::expressions() const
{
    QStringList list;
    for (auto it = m_entries.crbegin(); it != m_entries.crend(); ++it) {
        if (!list.contains(it->expression))
            list.append(it->expression);
    }
    return list;
}

int NameListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_shown.size());
}

QVariant NameListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_shown.size())
        return {};
    const NamedEntry &entry = m_shown[index.row()];
    switch (role) {
    case NameRole:
        return entry.name;
    case ValueRole:
        return entry.value;
    case DescriptionRole:
        return entry.description;
    case CategoryRole:
        return entry.category;
    case BuiltinRole:
        return entry.builtin;
    case InsertTextRole:
        return entry.insertText;
    }
    return {};
}

QHash<int, QByteArray> NameListModel::roleNames() const
{
    return {
        {NameRole, "name"}, {ValueRole, "value"}, {DescriptionRole, "description"},
        {CategoryRole, "category"}, {BuiltinRole, "builtin"}, {InsertTextRole, "insertText"},
    };
}

void NameListModel::setEntries(const QList<NamedEntry> &entries)
{
    m_entries = entries;
    applyFilter();
}

void NameListModel::setFilter(const QString &filter)
{
    if (m_filter == filter)
        return;
    m_filter = filter;
    emit filterChanged();
    applyFilter();
}

void NameListModel::applyFilter()
{
    beginResetModel();
    m_shown.clear();
    for (const NamedEntry &entry : std::as_const(m_entries)) {
        if (m_filter.isEmpty() || entry.name.contains(m_filter, Qt::CaseInsensitive)
            || entry.description.contains(m_filter, Qt::CaseInsensitive)
            || entry.category.contains(m_filter, Qt::CaseInsensitive)) {
            m_shown.append(entry);
        }
    }
    endResetModel();
    emit countChanged();
}

QString NameListModel::nameAt(int row) const
{
    return row >= 0 && row < m_shown.size() ? m_shown[row].name : QString();
}

QString NameListModel::insertTextAt(int row) const
{
    return row >= 0 && row < m_shown.size() ? m_shown[row].insertText : QString();
}
