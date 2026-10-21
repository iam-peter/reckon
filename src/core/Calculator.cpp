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
    emit evaluated(true);
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
    const bool ok = m_sheet.entries()[index].error.isEmpty();
    emit evaluated(ok);
    return ok;
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
    const bool ok = m_sheet.entries()[row].error.isEmpty();
    emit evaluated(ok);
    return ok;
}

void Calculator::removeEntry(int row)
{
    if (row < 0 || row >= m_sheet.size() || m_sheet.isEvaluating())
        return;
    m_sheet.remove(row);
    syncHistory();
    refreshVariables();
    refreshUserFunctions();
    emit evaluated(true);
}

void Calculator::namesChanged(const QSet<QString> &names)
{
    m_sheet.namesChanged(names);
    syncHistory();
    refreshLists();
    emit evaluated(true);
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

// Removing and clearing touch the environment's own variables and
// functions; the ones the history defines go with their entries.
void Calculator::removeVariable(const QString &name)
{
    if (name == u"ans"_s || m_sheet.owns(name) || m_sheet.isEvaluating())
        return;
    m_env.removeVariable(name);
    namesChanged({name});
}

void Calculator::clearVariables()
{
    if (m_sheet.isEvaluating())
        return;
    const QStringList names = m_env.variables().keys();
    m_env.clearVariables();
    m_env.setVariable(u"ans"_s, 0);
    namesChanged(QSet<QString>(names.cbegin(), names.cend()));
}

void Calculator::removeUserFunction(const QString &name)
{
    if (m_sheet.owns(name) || m_sheet.isEvaluating())
        return;
    m_env.removeFunction(name);
    namesChanged({name});
}

void Calculator::clearUserFunctions()
{
    if (m_sheet.isEvaluating())
        return;
    const QStringList names = m_env.userFunctions().keys();
    m_env.clearFunctions();
    namesChanged(QSet<QString>(names.cbegin(), names.cend()));
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
    emit evaluated(true);
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
            entries.append({name, formatValue(variables.value(name)), QString(), QString(), false, name});
            continue;
        }
        // Bound by an entry: it goes when that entry is changed or removed.
        const Worksheet::Entry &entry = m_sheet.entries()[owner];
        entries.append({name, entry.error.isEmpty() ? formatValue(entry.value) : entry.error,
                        u"Defined by %1"_s.arg(entry.name()), QString(), true, name});
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
                        QString(), owner >= 0, name + u'('});
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
    emit evaluated(true);
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
    emit evaluated(true);
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
