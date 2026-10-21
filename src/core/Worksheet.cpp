#include "Worksheet.h"

#include "Functions.h"
#include "Lexer.h"

#include <cmath>

using namespace Qt::StringLiterals;

namespace {

// Entries reading entries further down the history nest their
// evaluation; this many deep is far beyond any real history and still
// well within a 1 MB stack.
constexpr int MaxNesting = 128;

// What an update compares to tell whether an entry's result changed.
QString resultKey(const Worksheet::Entry &entry)
{
    if (!entry.error.isEmpty())
        return u'!' + entry.error;
    if (!entry.definition.isEmpty())
        return entry.definition;
    return QString::number(entry.value, 'g', 17);
}

} // namespace

Worksheet::Worksheet(Environment *env)
    : m_env(env)
{
    m_env->setScope(this);
}

Worksheet::~Worksheet()
{
    if (m_env->scope() == this)
        m_env->setScope(nullptr);
}

void Worksheet::analyze(Entry &entry)
{
    entry.names.clear();
    entry.definesFunction = false;
    QList<Token> tokens;
    try {
        tokens = tokenize(entry.source);
    } catch (const CalcError &) {
        return;
    }
    auto type = [&tokens](int i) { return tokens[qMin(i, int(tokens.size()) - 1)].type; };
    // f(a, b) =
    if (type(0) == Token::Identifier && type(1) == Token::LeftParen) {
        int i = 2;
        while (type(i) == Token::Identifier) {
            ++i;
            if (type(i) != Token::Comma)
                break;
            ++i;
        }
        if (i > 2 && type(i) == Token::RightParen && type(i + 1) == Token::Assign) {
            entry.names = {tokens[0].text};
            entry.definesFunction = true;
            return;
        }
    }
    // a = b = ...
    for (int i = 0; type(i) == Token::Identifier && type(i + 1) == Token::Assign; i += 2)
        entry.names.append(tokens[i].text);
}

bool Worksheet::canBind(const QString &name)
{
    return !isResultName(name) && name != u"ans"_s && !Environment::isConstant(name)
        && !findBuiltinFunction(name);
}

void Worksheet::reindex()
{
    m_indexOfId.clear();
    for (int i = 0; i < size(); ++i)
        m_indexOfId.insert(m_entries[i].id, i);
}

QHash<QString, int> Worksheet::ownerIds() const
{
    QHash<QString, int> ids;
    for (auto it = m_owners.cbegin(); it != m_owners.cend(); ++it)
        ids.insert(it.key(), m_entries[it.value()].id);
    return ids;
}

QSet<QString> Worksheet::rebind(const QHash<QString, int> &before)
{
    reindex();
    m_owners.clear();
    for (int i = 0; i < size(); ++i) {
        for (const QString &name : std::as_const(m_entries[i].names)) {
            if (canBind(name))
                m_owners.insert(name, i); // later entries win
        }
    }
    const QHash<QString, int> after = ownerIds();
    QSet<QString> changed;
    for (auto it = before.cbegin(); it != before.cend(); ++it) {
        if (after.value(it.key(), -1) != it.value())
            changed.insert(it.key());
    }
    for (auto it = after.cbegin(); it != after.cend(); ++it) {
        if (before.value(it.key(), -1) != it.value())
            changed.insert(it.key());
    }
    return changed;
}

QSet<QString> Worksheet::outputs(const Entry &entry) const
{
    QSet<QString> names(entry.names.cbegin(), entry.names.cend());
    names.insert(entry.name());
    return names;
}

QString Worksheet::label(int index) const
{
    const Entry &entry = m_entries[index];
    for (const QString &name : entry.names) {
        if (m_owners.value(name, -1) == index)
            return name;
    }
    return entry.name();
}

int Worksheet::append(const QString &source)
{
    const QHash<QString, int> before = ownerIds();
    Entry entry;
    entry.id = m_nextId++;
    entry.source = source;
    analyze(entry);
    m_entries.push_back(entry);
    const int index = size() - 1;
    update(rebind(before), {index}, index);
    return index;
}

void Worksheet::edit(int index, const QString &source)
{
    if (index < 0 || index >= size())
        return;
    const QHash<QString, int> before = ownerIds();
    Entry &entry = m_entries[index];
    QSet<QString> changed(entry.names.cbegin(), entry.names.cend());
    entry.source = source;
    analyze(entry);
    changed.unite(rebind(before));
    update(changed, {index}, index);
}

void Worksheet::remove(int index)
{
    if (index < 0 || index >= size())
        return;
    const QHash<QString, int> before = ownerIds();
    QSet<QString> changed = outputs(m_entries[index]);
    m_entries.erase(m_entries.begin() + index);
    changed.unite(rebind(before));
    update(changed, {});
}

void Worksheet::clear()
{
    m_entries.clear();
    m_owners.clear();
    m_indexOfId.clear();
    m_nextId = 1;
}

void Worksheet::load(const QList<std::pair<int, QString>> &entries)
{
    clear();
    for (const auto &[id, source] : entries) {
        Entry entry;
        entry.id = id > 0 && !m_indexOfId.contains(id) ? id : m_nextId;
        entry.source = source;
        analyze(entry);
        m_entries.push_back(entry);
        m_indexOfId.insert(entry.id, size() - 1);
        m_nextId = qMax(m_nextId, entry.id + 1);
    }
    rebind({});
    for (int i = 0; i < size(); ++i) {
        if (m_entries[i].state == Entry::State::Dirty)
            evaluate(i);
    }
}

void Worksheet::namesChanged(const QSet<QString> &names)
{
    update(names, {});
}

void Worksheet::update(QSet<QString> changed, QSet<int> dirty, int fresh)
{
    for (int index : std::as_const(dirty))
        changed.unite(outputs(m_entries[index]));
    // Whatever read a changed name changes too, and so on.
    for (bool grew = true; grew;) {
        grew = false;
        for (int i = 0; i < size(); ++i) {
            if (!dirty.contains(i) && m_entries[i].dependencies.intersects(changed)) {
                dirty.insert(i);
                changed.unite(outputs(m_entries[i]));
                grew = true;
            }
        }
    }
    QHash<int, QString> before;
    for (int index : std::as_const(dirty)) {
        m_entries[index].state = Entry::State::Dirty;
        before.insert(index, resultKey(m_entries[index]));
    }
    // In order, so an entry usually finds what it reads evaluated already.
    for (int i = 0; i < size(); ++i) {
        if (m_entries[i].state == Entry::State::Dirty)
            evaluate(i);
    }
    for (int index : std::as_const(dirty)) {
        if (index != fresh && resultKey(m_entries[index]) != before.value(index))
            ++m_entries[index].revision;
    }
}

void Worksheet::checkTargets(const Statement &statement, int index) const
{
    Q_UNUSED(index)
    for (const AssignTarget &target : statement.targets) {
        // Bound here, or by a later entry that replaced this one.
        if (m_owners.contains(target.name))
            continue;
        if (Environment::isConstant(target.name))
            throw CalcError{u"%1 is a constant"_s.arg(target.name), target.position};
        if (findBuiltinFunction(target.name))
            throw CalcError{u"%1 is a function"_s.arg(target.name), target.position};
        throw CalcError{u"%1 cannot be assigned"_s.arg(target.name), target.position};
    }
}

void Worksheet::evaluate(int index)
{
    Entry &entry = m_entries[index];
    entry.state = Entry::State::Evaluating;
    entry.dependencies.clear();
    entry.loopMessage.clear();
    m_stack.append(index);
    // A call that asked for this entry's value keeps its parameters to
    // itself.
    QList<QHash<QString, Number>> frames = m_env->takeFrames();
    try {
        const Statement statement = parseStatement(entry.source, m_env->callPredicate());
        if (statement.isDefinition()) {
            const QString &name = statement.functionName;
            if (!m_owners.contains(name)) {
                if (Environment::isConstant(name))
                    throw CalcError{u"%1 is a constant"_s.arg(name), 0};
                throw CalcError{u"%1 is a built-in function"_s.arg(name), 0};
            }
            entry.function = {statement.parameters, statement.expr, statement.bodySource};
            entry.definition = u"%1(%2) = %3"_s.arg(name, statement.parameters.join(u", "_s), statement.bodySource);
            entry.value = 0;
        } else {
            checkTargets(statement, index);
            const Number value = statement.value->evaluate(*m_env);
            if (std::isnan(value))
                throw CalcError{u"Not a number"_s};
            if (std::isinf(value))
                throw CalcError{u"Result is too large"_s};
            entry.value = value;
            entry.definition.clear();
            entry.function = {};
        }
        entry.error.clear();
        entry.errorPosition = -1;
    } catch (const CalcError &error) {
        entry.error = error.message;
        entry.errorPosition = error.position;
        entry.definition.clear();
        entry.function = {};
    }
    m_env->restoreFrames(std::move(frames));
    m_stack.removeLast();
    // In a loop, every entry in it says so rather than that the next one
    // has an error.
    if (!entry.loopMessage.isEmpty())
        entry.error = entry.loopMessage;
    entry.state = Entry::State::Clean;
}

void Worksheet::ensureEvaluated(int index, int position)
{
    const Entry &entry = m_entries[index];
    if (entry.state == Entry::State::Clean)
        return;
    if (entry.state == Entry::State::Dirty) {
        if (m_stack.size() >= MaxNesting)
            throw CalcError{u"Entries depend on each other too deeply"_s, position};
        evaluate(index);
        return;
    }
    // Being evaluated already, further up: the names depend on each other.
    const int from = int(m_stack.indexOf(index));
    QStringList path;
    for (int k = from; k < m_stack.size(); ++k)
        path.append(label(m_stack[k]));
    const QString message = path.size() == 1
        ? u"%1 depends on itself"_s.arg(path.first())
        : u"Binding loop: %1"_s.arg((path + QStringList{path.first()}).join(u" → "_s));
    for (int k = from; k < m_stack.size(); ++k)
        m_entries[m_stack[k]].loopMessage = message;
    throw CalcError{message, position};
}

// `ans`: the nearest result above the entry asking, skipping definitions
// and errors, or above the end of the history when no entry asks.
Number Worksheet::answer(int position)
{
    const int start = m_stack.isEmpty() ? size() - 1 : m_stack.last() - 1;
    for (int k = start; k >= 0; --k) {
        note(m_entries[k].name());
        if (m_entries[k].definesFunction)
            continue;
        ensureEvaluated(k, position);
        if (m_entries[k].error.isEmpty())
            return m_entries[k].value;
    }
    return m_env->variables().value(u"ans"_s, 0);
}

void Worksheet::note(const QString &name)
{
    if (!m_stack.isEmpty())
        m_entries[m_stack.last()].dependencies.insert(name);
}

bool Worksheet::owns(const QString &name) const
{
    if (name == u"ans"_s)
        return !m_entries.empty();
    return isResultName(name) || m_owners.contains(name);
}

bool Worksheet::isVariable(const QString &name) const
{
    if (name == u"ans"_s || isResultName(name))
        return true;
    const int index = m_owners.value(name, -1);
    return index >= 0 && !m_entries[index].definesFunction;
}

bool Worksheet::isFunction(const QString &name) const
{
    const int index = m_owners.value(name, -1);
    return index >= 0 && m_entries[index].definesFunction;
}

Number Worksheet::variable(const QString &name, int position)
{
    if (name == u"ans"_s)
        return answer(position);
    int index = -1;
    if (isResultName(name)) {
        index = indexOfId(name.mid(1).toInt());
        if (index < 0)
            throw CalcError{u"There is no %1"_s.arg(name), position};
        if (m_entries[index].definesFunction)
            throw CalcError{u"%1 defines a function"_s.arg(name), position};
    } else {
        index = m_owners.value(name, -1);
        if (index < 0)
            throw CalcError{u"Unknown variable %1"_s.arg(name), position};
    }
    ensureEvaluated(index, position);
    const Entry &entry = m_entries[index];
    if (!entry.error.isEmpty())
        throw CalcError{u"%1 has an error"_s.arg(name), position};
    return entry.value;
}

const UserFunction &Worksheet::function(const QString &name, int position)
{
    const int index = m_owners.value(name, -1);
    if (index < 0)
        throw CalcError{u"Unknown function %1"_s.arg(name), position};
    ensureEvaluated(index, position);
    const Entry &entry = m_entries[index];
    if (!entry.error.isEmpty())
        throw CalcError{u"%1 has an error"_s.arg(name), position};
    return entry.function;
}

QString Worksheet::owner(const QString &name) const
{
    if (name == u"ans"_s)
        return u"the history"_s;
    if (isResultName(name))
        return name;
    const int index = m_owners.value(name, -1);
    return index >= 0 ? m_entries[index].name() : QString();
}

QStringList Worksheet::variableNames() const
{
    QStringList names;
    for (auto it = m_owners.cbegin(); it != m_owners.cend(); ++it) {
        if (!m_entries[it.value()].definesFunction)
            names.append(it.key());
    }
    return names;
}

QStringList Worksheet::functionNames() const
{
    QStringList names;
    for (auto it = m_owners.cbegin(); it != m_owners.cend(); ++it) {
        if (m_entries[it.value()].definesFunction)
            names.append(it.key());
    }
    return names;
}
