#include "Calculator.h"

#include "Constants.h"
#include "Functions.h"
#include "Lexer.h"
#include "ScriptEngine.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

#include <cmath>
#include <numbers>

using namespace Qt::StringLiterals;

Calculator::Calculator(QObject *parent)
    : QObject(parent)
    , m_sheet(&m_env)
    , m_history(new HistoryModel(this))
    , m_variables(new NameListModel(this))
    , m_userFunctions(new NameListModel(this))
    , m_functions(new NameListModel(this))
    , m_constants(new NameListModel(this))
    , m_script(new ScriptEngine(this))
{
    QList<NamedEntry> constantEntries;
    for (const ConstantInfo &constant : ::constants()) {
        const QString value = formatNumber(constant.value);
        constantEntries.append({constant.name, value + (constant.unit.isEmpty() ? QString() : u' ' + constant.unit),
                                QString(), constant.category, true, value});
    }
    m_constants->setEntries(constantEntries);
    refreshLists();
}

Calculator::AngleUnit Calculator::angleUnit() const
{
    return m_env.angleUnit == ::AngleUnit::Degrees ? Degrees : Radians;
}

void Calculator::setAngleUnit(AngleUnit unit)
{
    if (angleUnit() == unit)
        return;
    m_env.angleUnit = unit == Degrees ? ::AngleUnit::Degrees : ::AngleUnit::Radians;
    // Entries with angles in them follow the unit.
    m_sheet.namesChanged({u"#angle"_s});
    syncHistory();
    refreshVariables();
    emit angleUnitChanged();
    finish({});
}

void Calculator::setResultFormat(ResultFormat format)
{
    if (resultFormat() == format)
        return;
    m_format.mode = NumberFormat::Mode(format);
    emit formatChanged();
    m_history->reformat([this](Number value) { return formatValue(value); });
    refreshVariables();
}

void Calculator::setPrecision(int precision)
{
    precision = qBound(-1, precision, 50);
    if (m_format.precision == precision)
        return;
    m_format.precision = precision;
    emit formatChanged();
    m_history->reformat([this](Number value) { return formatValue(value); });
    refreshVariables();
}

void Calculator::setGroupDigits(bool group)
{
    if (m_format.groupDigits == group)
        return;
    m_format.groupDigits = group;
    emit formatChanged();
    m_history->reformat([this](Number value) { return formatValue(value); });
    refreshVariables();
}

QString Calculator::ans() const
{
    return formatValue(m_env.variable(u"ans"_s));
}

FunctionPredicate Calculator::functionPredicate() const
{
    return m_env.callPredicate();
}

bool Calculator::evaluate(const QString &text)
{
    const QString expression = text.trimmed();
    // Not from a script function the history is evaluating right now.
    if (expression.isEmpty() || m_sheet.isEvaluating())
        return false;
    const int index = m_sheet.append(expression);
    syncHistory();
    refreshVariables();
    refreshUserFunctions();
    finish(m_sheet.entries()[index].error);
    return m_error.isEmpty();
}

void Calculator::finish(const QString &error)
{
    m_error = error;
    emit evaluated(error.isEmpty());
}

bool Calculator::edit(int row, const QString &text)
{
    if (row < 0 || row >= m_sheet.size() || m_sheet.isEvaluating())
        return false;
    const QString expression = text.trimmed();
    if (expression.isEmpty()) {
        removeEntry(row);
        return true;
    }
    m_sheet.edit(row, expression);
    syncHistory();
    refreshVariables();
    refreshUserFunctions();
    finish(m_sheet.entries()[row].error);
    return m_error.isEmpty();
}

void Calculator::removeEntry(int row)
{
    if (row < 0 || row >= m_sheet.size() || m_sheet.isEvaluating())
        return;
    m_sheet.remove(row);
    syncHistory();
    refreshVariables();
    refreshUserFunctions();
    finish({});
}

void Calculator::namesChanged(const QSet<QString> &names)
{
    m_sheet.namesChanged(names);
    syncHistory();
    refreshLists();
    finish({});
}

HistoryEntry Calculator::historyEntry(int index) const
{
    const Worksheet::Entry &sheet = m_sheet.entries()[index];
    HistoryEntry entry;
    entry.id = sheet.id;
    entry.expression = sheet.source;
    entry.error = sheet.error;
    entry.errorPosition = sheet.errorPosition;
    entry.revision = sheet.revision;
    if (sheet.error.isEmpty()) {
        entry.definition = !sheet.definition.isEmpty();
        entry.value = sheet.value;
        entry.result = entry.definition ? sheet.definition : formatValue(sheet.value);
    }
    QStringList replaced;
    for (const QString &name : sheet.names) {
        const int owner = m_sheet.ownerIndex(name);
        if (owner >= 0 && owner != index)
            replaced.append(u"%1 is redefined in %2"_s.arg(name, m_sheet.entries()[owner].name()));
    }
    entry.replaced = replaced.join(u", "_s);
    return entry;
}

void Calculator::syncHistory()
{
    QList<HistoryEntry> entries;
    entries.reserve(m_sheet.size());
    for (int i = 0; i < m_sheet.size(); ++i)
        entries.append(historyEntry(i));
    m_history->sync(entries);
}

QString Calculator::preview(const QString &expression) const
{
    if (expression.trimmed().isEmpty())
        return {};
    try {
        const Statement statement = parseStatement(expression, functionPredicate());
        if (statement.isDefinition())
            return {};
        for (const AssignTarget &target : statement.targets) {
            if (isResultName(target.name) || target.name == u"ans"_s || Environment::isConstant(target.name)
                || findBuiltinFunction(target.name)) {
                return {};
            }
        }
        // On a copy, so assignments inside the expression change nothing
        // yet; the ones at the start would bind, which needs no assigning.
        Environment scratch = m_env;
        const Number value = statement.value->evaluate(scratch);
        if (!std::isfinite(value))
            return {};
        return formatValue(value);
    } catch (const CalcError &) {
        return {};
    }
}

QString Calculator::formatValue(double value) const
{
    return formatNumber(value, m_format);
}

QVariantList Calculator::completions(const QString &prefix) const
{
    QVariantList list;
    auto add = [&list](const QString &text, const QString &insert, const QString &detail) {
        list.append(QVariantMap{{u"text"_s, text}, {u"insert"_s, insert}, {u"detail"_s, detail}});
    };
    auto matches = [&prefix](const QString &name) {
        return name.startsWith(prefix, Qt::CaseInsensitive);
    };
    for (const FunctionInfo &function : builtinFunctions()) {
        if (matches(function.name))
            add(function.usage, function.name + u'(', function.description);
    }
    const auto &userFunctions = m_env.userFunctions();
    for (auto it = userFunctions.cbegin(); it != userFunctions.cend(); ++it) {
        if (matches(it.key()) && !m_sheet.owns(it.key()))
            add(u"%1(%2)"_s.arg(it.key(), it->parameters.join(u", "_s)), it.key() + u'(', it->source);
    }
    const auto &scriptFunctions = m_env.scriptFunctions();
    for (auto it = scriptFunctions.cbegin(); it != scriptFunctions.cend(); ++it) {
        if (matches(it.key()))
            add(it.key() + u"(…)"_s, it.key() + u'(', u"Defined by a script"_s);
    }
    const auto &variables = m_env.variables();
    for (auto it = variables.cbegin(); it != variables.cend(); ++it) {
        if (it.key() == u"ans"_s && matches(it.key()))
            add(it.key(), it.key(), formatValue(m_env.variable(it.key())));
        else if (matches(it.key()) && !m_sheet.owns(it.key()))
            add(it.key(), it.key(), formatValue(it.value()));
    }
    for (const QString &name : m_sheet.variableNames()) {
        const Worksheet::Entry &entry = m_sheet.entries()[m_sheet.ownerIndex(name)];
        if (matches(name))
            add(name, name, entry.error.isEmpty() ? formatValue(entry.value) : entry.error);
    }
    for (const QString &name : m_sheet.functionNames()) {
        const Worksheet::Entry &entry = m_sheet.entries()[m_sheet.ownerIndex(name)];
        if (matches(name) && entry.error.isEmpty())
            add(u"%1(%2)"_s.arg(name, entry.function.parameters.join(u", "_s)), name + u'(',
                entry.function.source);
    }
    if (matches(u"pi"_s))
        add(u"pi"_s, u"pi"_s, formatValue(std::numbers::pi));
    if (matches(u"e"_s))
        add(u"e"_s, u"e"_s, formatValue(std::numbers::e));
    std::sort(list.begin(), list.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(u"text"_s).toString().compare(b.toMap().value(u"text"_s).toString(),
                                                              Qt::CaseInsensitive) < 0;
    });
    return list;
}

// A variable an entry defines goes from every entry assigning it, which
// keep their expressions: "x = a * 2" becomes "a * 2", so the entry and
// its $n stay. A function an entry defines goes with its definitions.
void Calculator::unbindVariable(const QString &name)
{
    for (int index : m_sheet.definersOf(name)) {
        if (!m_sheet.entries()[index].definesFunction)
            m_sheet.edit(index, m_sheet.sourceWithout(index, name));
    }
}

void Calculator::unbindFunction(const QString &name)
{
    const QList<int> definers = m_sheet.definersOf(name);
    for (auto index = definers.crbegin(); index != definers.crend(); ++index) {
        if (m_sheet.entries()[*index].definesFunction)
            m_sheet.remove(*index);
    }
}

void Calculator::removeVariable(const QString &name)
{
    if (name == u"ans"_s || isResultName(name) || m_sheet.isEvaluating())
        return;
    if (m_sheet.ownerIndex(name) >= 0 && !m_sheet.isFunction(name))
        unbindVariable(name);
    m_env.removeVariable(name);
    namesChanged({name});
}

void Calculator::clearVariables()
{
    if (m_sheet.isEvaluating())
        return;
    QStringList names = m_env.variables().keys();
    for (const QString &name : m_sheet.variableNames()) {
        unbindVariable(name);
        names.append(name);
    }
    m_env.clearVariables();
    m_env.setVariable(u"ans"_s, 0);
    namesChanged(QSet<QString>(names.cbegin(), names.cend()));
}

void Calculator::removeUserFunction(const QString &name)
{
    if (m_sheet.isEvaluating())
        return;
    if (m_sheet.isFunction(name))
        unbindFunction(name);
    m_env.removeFunction(name);
    namesChanged({name});
}

void Calculator::clearUserFunctions()
{
    if (m_sheet.isEvaluating())
        return;
    QStringList names = m_env.userFunctions().keys();
    for (const QString &name : m_sheet.functionNames()) {
        unbindFunction(name);
        names.append(name);
    }
    m_env.clearFunctions();
    namesChanged(QSet<QString>(names.cbegin(), names.cend()));
}

QString Calculator::setVariable(const QString &name, const QString &text)
{
    const QString expression = text.trimmed();
    if (m_sheet.isEvaluating())
        return u"The history is updating"_s;
    if (expression.isEmpty())
        return u"Expected a value"_s;
    if (name == u"ans"_s || isResultName(name) || Environment::isConstant(name) || findBuiltinFunction(name))
        return u"%1 cannot be changed"_s.arg(name);
    if (m_sheet.isFunction(name) || (m_sheet.ownerIndex(name) < 0 && m_env.isFunction(name)))
        return u"%1 is a function"_s.arg(name);

    // What the expression gives now, on a copy: an error changes nothing,
    // not even for a moment.
    Number value = 0;
    try {
        Environment scratch = m_env;
        value = parseExpression(expression, functionPredicate())->evaluate(scratch);
    } catch (const CalcError &error) {
        return error.message;
    }
    if (std::isnan(value))
        return u"Not a number"_s;
    if (std::isinf(value))
        return u"Result is too large"_s;

    const int owner = m_sheet.ownerIndex(name);
    if (owner >= 0) {
        // Bound: the entry defining it changes, and what depends on it
        // follows. One assigning more names lets go of this one, which a
        // new entry defines.
        // What only shows once bound, a loop, takes the change back.
        const QString before = m_sheet.entries()[owner].source;
        const QString assignment = u"%1 = %2"_s.arg(name, expression);
        QString error;
        if (m_sheet.entries()[owner].names.size() == 1) {
            m_sheet.edit(owner, assignment);
            error = m_sheet.entries()[owner].error;
            if (!error.isEmpty())
                m_sheet.edit(owner, before);
        } else {
            m_sheet.edit(owner, m_sheet.sourceWithout(owner, name));
            const int added = m_sheet.append(assignment);
            error = m_sheet.entries()[added].error;
            if (!error.isEmpty()) {
                m_sheet.remove(added);
                m_sheet.edit(owner, before);
            }
        }
        syncHistory();
        refreshLists();
        if (!error.isEmpty())
            return error;
        finish({});
        return {};
    }

    // The environment's own takes the value the expression has now.
    m_env.setVariable(name, value);
    namesChanged({name});
    return {};
}

// The entries go; what they defined stays, as plain values and functions
// with nothing left to bind them.
void Calculator::clearHistory()
{
    if (m_sheet.isEvaluating())
        return;
    const Number answer = m_env.variable(u"ans"_s);
    for (const QString &name : m_sheet.variableNames()) {
        const Worksheet::Entry &entry = m_sheet.entries()[m_sheet.ownerIndex(name)];
        if (entry.error.isEmpty())
            m_env.setVariable(name, entry.value);
    }
    QList<std::pair<QString, UserFunction>> functions;
    for (const QString &name : m_sheet.functionNames()) {
        const Worksheet::Entry &entry = m_sheet.entries()[m_sheet.ownerIndex(name)];
        if (entry.error.isEmpty())
            functions.append({name, entry.function});
    }
    m_sheet.clear();
    m_env.setVariable(u"ans"_s, answer);
    for (const auto &[name, function] : std::as_const(functions)) {
        m_env.removeVariable(name);
        try {
            m_env.defineFunction(name, function);
        } catch (const CalcError &) {
        }
    }
    syncHistory();
    refreshLists();
    finish({});
}

void Calculator::refreshLists()
{
    refreshVariables();
    refreshUserFunctions();
    refreshFunctions();
}

void Calculator::refreshVariables()
{
    QList<NamedEntry> entries;
    const auto &variables = m_env.variables();
    QStringList names = variables.keys();
    names.removeAll(u"ans"_s);
    for (const QString &name : m_sheet.variableNames()) {
        if (!names.contains(name))
            names.append(name);
    }
    std::sort(names.begin(), names.end(), [](const QString &a, const QString &b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    entries.append({u"ans"_s, formatValue(m_env.variable(u"ans"_s)), u"The last result"_s, QString(), true, u"ans"_s});
    entries.append({u"pi"_s, formatValue(std::numbers::pi), u"Archimedes' constant"_s, QString(), true, u"pi"_s});
    entries.append({u"e"_s, formatValue(std::numbers::e), u"Euler's number"_s, QString(), true, u"e"_s});
    for (const QString &name : std::as_const(names)) {
        const int owner = m_sheet.ownerIndex(name);
        if (owner < 0) {
            entries.append({name, formatValue(variables.value(name)), QString(), QString(), false, name,
                            QString::number(variables.value(name), 'g', 15)});
            continue;
        }
        // Bound by an entry: editing it changes that entry.
        const Worksheet::Entry &entry = m_sheet.entries()[owner];
        entries.append({name, entry.error.isEmpty() ? formatValue(entry.value) : entry.error,
                        u"Defined by %1"_s.arg(entry.name()), QString(), false, name, m_sheet.valueSource(owner)});
    }
    m_variables->setEntries(entries);
}

void Calculator::refreshUserFunctions()
{
    QList<NamedEntry> entries;
    const auto &functions = m_env.userFunctions();
    QStringList names = functions.keys();
    for (const QString &name : m_sheet.functionNames()) {
        if (!names.contains(name) && m_sheet.entries()[m_sheet.ownerIndex(name)].error.isEmpty())
            names.append(name);
    }
    std::sort(names.begin(), names.end(), [](const QString &a, const QString &b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    for (const QString &name : std::as_const(names)) {
        const int owner = m_sheet.ownerIndex(name);
        if (owner >= 0 && !m_sheet.isFunction(name))
            continue; // a variable now, which hides the function
        const UserFunction &function = owner >= 0 ? m_sheet.entries()[owner].function : functions[name];
        entries.append({name, u"%1(%2)"_s.arg(name, function.parameters.join(u", "_s)),
                        owner >= 0 ? u"%1, defined by %2"_s.arg(function.source, m_sheet.entries()[owner].name())
                                   : function.source,
                        QString(), false, name + u'('});
    }
    m_userFunctions->setEntries(entries);
}

void Calculator::refreshFunctions()
{
    QList<NamedEntry> entries;
    for (const FunctionInfo &function : builtinFunctions())
        entries.append({function.name, function.usage, function.description, function.category, true,
                        function.name + u'('});
    const auto &scriptFunctions = m_env.scriptFunctions();
    QStringList names = scriptFunctions.keys();
    names.sort(Qt::CaseInsensitive);
    for (const QString &name : std::as_const(names)) {
        const int arity = scriptFunctions[name].arity;
        QStringList parameters;
        for (int i = 0; i < arity; ++i)
            parameters.append(u"x%1"_s.arg(i + 1));
        entries.append({name, u"%1(%2)"_s.arg(name, arity < 0 ? u"..."_s : parameters.join(u", "_s)),
                        u"Defined by a script"_s, u"Script"_s, false, name + u'('});
    }
    m_functions->setEntries(entries);
}

void Calculator::setSessionFile(const QString &file)
{
    if (m_sessionFile == file)
        return;
    m_sessionFile = file;
    emit sessionFileChanged();
}

void Calculator::newSession()
{
    if (m_sheet.isEvaluating())
        return;
    m_sheet.clear();
    m_env.clearVariables();
    m_env.setVariable(u"ans"_s, 0);
    m_env.clearFunctions();
    syncHistory();
    setSessionFile({});
    refreshVariables();
    refreshUserFunctions();
    finish({});
}

// Version 2: the history is what defines its variables and functions and
// is evaluated again on loading; "variables" and "functions" hold only
// the ones no entry defines, which scripts set or a cleared history left.
// Results are saved for other programs to read.
QString Calculator::sessionJson() const
{
    QJsonObject variables;
    const auto &vars = m_env.variables();
    for (auto it = vars.cbegin(); it != vars.cend(); ++it) {
        if (it.key() == u"ans"_s || !m_sheet.owns(it.key()))
            variables.insert(it.key(), it.value());
    }

    QJsonArray functions;
    const auto &funcs = m_env.userFunctions();
    for (auto it = funcs.cbegin(); it != funcs.cend(); ++it) {
        if (m_sheet.owns(it.key()))
            continue;
        functions.append(QJsonObject{
            {u"name"_s, it.key()},
            {u"parameters"_s, QJsonArray::fromStringList(it->parameters)},
            {u"body"_s, it->source},
        });
    }

    QJsonArray history;
    for (const HistoryEntry &entry : m_history->entries()) {
        QJsonObject object{{u"id"_s, entry.id}, {u"expression"_s, entry.expression}};
        if (!entry.error.isEmpty()) {
            object.insert(u"error"_s, entry.error);
            object.insert(u"errorPosition"_s, entry.errorPosition);
        } else if (entry.definition) {
            object.insert(u"definition"_s, entry.result);
        } else {
            object.insert(u"value"_s, entry.value);
        }
        history.append(object);
    }

    const QJsonObject session{
        {u"version"_s, 2},
        {u"variables"_s, variables},
        {u"functions"_s, functions},
        {u"history"_s, history},
    };
    return QString::fromUtf8(QJsonDocument(session).toJson());
}

bool Calculator::loadSessionJson(const QString &json)
{
    if (m_sheet.isEvaluating())
        return false;
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &error);
    if (!document.isObject())
        return false;
    const QJsonObject session = document.object();

    m_sheet.clear();
    m_env.clearVariables();
    m_env.setVariable(u"ans"_s, 0);
    m_env.clearFunctions();
    const QJsonObject variables = session.value(u"variables"_s).toObject();
    for (auto it = variables.begin(); it != variables.end(); ++it)
        m_env.setVariable(it.key(), it.value().toDouble());

    const QJsonArray functions = session.value(u"functions"_s).toArray();
    for (const QJsonValue &value : functions) {
        const QJsonObject object = value.toObject();
        const QString name = object.value(u"name"_s).toString();
        QStringList parameters;
        for (const QJsonValue &parameter : object.value(u"parameters"_s).toArray())
            parameters.append(parameter.toString());
        const QString body = object.value(u"body"_s).toString();
        try {
            m_env.defineFunction(name, {parameters, parseExpression(body, functionPredicate()), body});
        } catch (const CalcError &) {
            // A function that no longer parses is left out.
        }
    }

    QList<std::pair<int, QString>> entries;
    for (const QJsonValue &value : session.value(u"history"_s).toArray()) {
        const QJsonObject object = value.toObject();
        entries.append({object.value(u"id"_s).toInt(0), object.value(u"expression"_s).toString()});
    }
    m_sheet.load(entries);
    // Version 1 saved the variables and functions the history defined as
    // well; the entries define them now.
    for (const QString &name : m_sheet.variableNames() + m_sheet.functionNames()) {
        m_env.removeVariable(name);
        m_env.removeFunction(name);
    }
    syncHistory();
    refreshVariables();
    refreshUserFunctions();
    finish({});
    return true;
}

bool Calculator::saveSession(const QUrl &url)
{
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    file.write(sessionJson().toUtf8());
    if (!file.commit())
        return false;
    setSessionFile(path);
    return true;
}

bool Calculator::loadSession(const QUrl &url)
{
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    if (!loadSessionJson(QString::fromUtf8(file.readAll())))
        return false;
    setSessionFile(path);
    return true;
}

QString Calculator::stateFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + u"/session.json"_s;
}

bool Calculator::saveState() const
{
    const QString path = stateFile();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    file.write(sessionJson().toUtf8());
    return file.commit();
}

bool Calculator::restoreState()
{
    QFile file(stateFile());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    return loadSessionJson(QString::fromUtf8(file.readAll()));
}
