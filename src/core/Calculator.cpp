#include "Calculator.h"

#include "Constants.h"
#include "Functions.h"
#include "ScriptEngine.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

#include <cmath>

using namespace Qt::StringLiterals;

Calculator::Calculator(QObject *parent)
    : QObject(parent)
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
    emit angleUnitChanged();
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
    if (expression.isEmpty())
        return false;
    HistoryEntry entry;
    entry.expression = expression;
    try {
        const Statement statement = parseStatement(expression, functionPredicate());
        if (statement.isDefinition()) {
            m_env.defineFunction(statement.functionName,
                                 {statement.parameters, statement.expr, statement.bodySource}, 0);
            entry.definition = true;
            entry.result = u"%1(%2) = %3"_s.arg(statement.functionName, statement.parameters.join(u", "_s),
                                                statement.bodySource);
        } else {
            const Number value = statement.expr->evaluate(m_env);
            if (std::isnan(value))
                throw CalcError{u"Not a number"_s};
            if (std::isinf(value))
                throw CalcError{u"Result is too large"_s};
            m_env.setVariable(u"ans"_s, value);
            entry.value = value;
            entry.result = formatValue(value);
        }
    } catch (const CalcError &error) {
        entry.error = error.message;
        entry.errorPosition = error.position;
    }
    m_history->append(entry);
    refreshVariables();
    refreshUserFunctions();
    emit evaluated(entry.error.isEmpty());
    return entry.error.isEmpty();
}

QString Calculator::preview(const QString &expression) const
{
    if (expression.trimmed().isEmpty())
        return {};
    try {
        const Statement statement = parseStatement(expression, functionPredicate());
        if (statement.isDefinition())
            return {};
        // On a copy, so assignments in the expression change nothing yet.
        Environment scratch = m_env;
        const Number value = statement.expr->evaluate(scratch);
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
        if (matches(it.key()))
            add(u"%1(%2)"_s.arg(it.key(), it->parameters.join(u", "_s)), it.key() + u'(', it->source);
    }
    const auto &scriptFunctions = m_env.scriptFunctions();
    for (auto it = scriptFunctions.cbegin(); it != scriptFunctions.cend(); ++it) {
        if (matches(it.key()))
            add(it.key() + u"(…)"_s, it.key() + u'(', u"Defined by a script"_s);
    }
    const auto &variables = m_env.variables();
    for (auto it = variables.cbegin(); it != variables.cend(); ++it) {
        if (matches(it.key()))
            add(it.key(), it.key(), formatValue(it.value()));
    }
    if (matches(u"pi"_s))
        add(u"pi"_s, u"pi"_s, formatValue(M_PI));
    if (matches(u"e"_s))
        add(u"e"_s, u"e"_s, formatValue(M_E));
    std::sort(list.begin(), list.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(u"text"_s).toString().compare(b.toMap().value(u"text"_s).toString(),
                                                              Qt::CaseInsensitive) < 0;
    });
    return list;
}

void Calculator::removeVariable(const QString &name)
{
    if (name == u"ans"_s)
        return;
    m_env.removeVariable(name);
    refreshVariables();
}

void Calculator::clearVariables()
{
    m_env.clearVariables();
    m_env.setVariable(u"ans"_s, 0);
    refreshVariables();
}

void Calculator::removeUserFunction(const QString &name)
{
    m_env.removeFunction(name);
    refreshUserFunctions();
}

void Calculator::clearUserFunctions()
{
    m_env.clearFunctions();
    refreshUserFunctions();
}

void Calculator::clearHistory()
{
    m_history->clear();
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
    std::sort(names.begin(), names.end(), [](const QString &a, const QString &b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    entries.append({u"ans"_s, formatValue(variables.value(u"ans"_s)), u"The last result"_s, QString(), true, u"ans"_s});
    entries.append({u"pi"_s, formatValue(M_PI), u"Archimedes' constant"_s, QString(), true, u"pi"_s});
    entries.append({u"e"_s, formatValue(M_E), u"Euler's number"_s, QString(), true, u"e"_s});
    for (const QString &name : std::as_const(names))
        entries.append({name, formatValue(variables.value(name)), QString(), QString(), false, name});
    m_variables->setEntries(entries);
}

void Calculator::refreshUserFunctions()
{
    QList<NamedEntry> entries;
    const auto &functions = m_env.userFunctions();
    QStringList names = functions.keys();
    std::sort(names.begin(), names.end(), [](const QString &a, const QString &b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    for (const QString &name : std::as_const(names)) {
        const UserFunction &function = functions[name];
        entries.append({name, u"%1(%2)"_s.arg(name, function.parameters.join(u", "_s)), function.source,
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
    m_env.clearVariables();
    m_env.setVariable(u"ans"_s, 0);
    m_env.clearFunctions();
    m_history->clear();
    setSessionFile({});
    refreshVariables();
    refreshUserFunctions();
    emit evaluated(true);
}

QString Calculator::sessionJson() const
{
    QJsonObject variables;
    const auto &vars = m_env.variables();
    for (auto it = vars.cbegin(); it != vars.cend(); ++it)
        variables.insert(it.key(), it.value());

    QJsonArray functions;
    const auto &funcs = m_env.userFunctions();
    for (auto it = funcs.cbegin(); it != funcs.cend(); ++it) {
        functions.append(QJsonObject{
            {u"name"_s, it.key()},
            {u"parameters"_s, QJsonArray::fromStringList(it->parameters)},
            {u"body"_s, it->source},
        });
    }

    QJsonArray history;
    for (const HistoryEntry &entry : m_history->entries()) {
        QJsonObject object{{u"expression"_s, entry.expression}};
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
        {u"version"_s, 1},
        {u"variables"_s, variables},
        {u"functions"_s, functions},
        {u"history"_s, history},
    };
    return QString::fromUtf8(QJsonDocument(session).toJson());
}

bool Calculator::loadSessionJson(const QString &json)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &error);
    if (!document.isObject())
        return false;
    const QJsonObject session = document.object();

    m_env.clearVariables();
    m_env.setVariable(u"ans"_s, 0);
    m_env.clearFunctions();
    const QJsonObject variables = session.value(u"variables"_s).toObject();
    for (auto it = variables.cbegin(); it != variables.cend(); ++it)
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

    QList<HistoryEntry> entries;
    for (const QJsonValue &value : session.value(u"history"_s).toArray()) {
        const QJsonObject object = value.toObject();
        HistoryEntry entry;
        entry.expression = object.value(u"expression"_s).toString();
        if (object.contains(u"error"_s)) {
            entry.error = object.value(u"error"_s).toString();
            entry.errorPosition = object.value(u"errorPosition"_s).toInt(-1);
        } else if (object.contains(u"definition"_s)) {
            entry.definition = true;
            entry.result = object.value(u"definition"_s).toString();
        } else {
            entry.value = object.value(u"value"_s).toDouble();
            entry.result = formatValue(entry.value);
        }
        entries.append(entry);
    }
    m_history->setEntries(entries);
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
