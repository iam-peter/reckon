#pragma once

#include "Environment.h"
#include "Formatter.h"
#include "Models.h"
#include "ScriptEngine.h"

#include <QObject>
#include <QUrl>
#include <QtQml/qqmlregistration.h>


// The calculator QML talks to: evaluates what is typed into the history,
// keeps the variables and functions, formats results, and saves and
// loads sessions.
class Calculator : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(HistoryModel *history READ history CONSTANT)
    Q_PROPERTY(NameListModel *variables READ variables CONSTANT)
    Q_PROPERTY(NameListModel *userFunctions READ userFunctions CONSTANT)
    Q_PROPERTY(NameListModel *functions READ functions CONSTANT)
    Q_PROPERTY(NameListModel *constants READ constants CONSTANT)
    Q_PROPERTY(ScriptEngine *script READ script CONSTANT)
    Q_PROPERTY(AngleUnit angleUnit READ angleUnit WRITE setAngleUnit NOTIFY angleUnitChanged)
    Q_PROPERTY(ResultFormat resultFormat READ resultFormat WRITE setResultFormat NOTIFY formatChanged)
    Q_PROPERTY(int precision READ precision WRITE setPrecision NOTIFY formatChanged)
    Q_PROPERTY(bool groupDigits READ groupDigits WRITE setGroupDigits NOTIFY formatChanged)
    // The last result, formatted.
    Q_PROPERTY(QString ans READ ans NOTIFY evaluated)
    // The file the session was loaded from or saved to; empty for a new one.
    Q_PROPERTY(QString sessionFile READ sessionFile NOTIFY sessionFileChanged)

public:
    enum AngleUnit { Radians, Degrees };
    Q_ENUM(AngleUnit)
    enum ResultFormat { General, Fixed, Scientific, Engineering, Binary, Octal, Hexadecimal };
    Q_ENUM(ResultFormat)

    explicit Calculator(QObject *parent = nullptr);

    HistoryModel *history() const { return m_history; }
    NameListModel *variables() const { return m_variables; }
    NameListModel *userFunctions() const { return m_userFunctions; }
    NameListModel *functions() const { return m_functions; }
    NameListModel *constants() const { return m_constants; }
    ScriptEngine *script() const { return m_script; }

    AngleUnit angleUnit() const;
    void setAngleUnit(AngleUnit unit);
    ResultFormat resultFormat() const { return ResultFormat(m_format.mode); }
    void setResultFormat(ResultFormat format);
    int precision() const { return m_format.precision; }
    void setPrecision(int precision);
    bool groupDigits() const { return m_format.groupDigits; }
    void setGroupDigits(bool group);
    QString ans() const;
    QString sessionFile() const { return m_sessionFile; }

    // Evaluates into the history and `ans`. False on an error, which is in
    // the history too.
    Q_INVOKABLE bool evaluate(const QString &expression);
    // The result the expression would give, formatted, without changing
    // anything: empty on an error or for a definition.
    Q_INVOKABLE QString preview(const QString &expression) const;
    Q_INVOKABLE QString formatValue(double value) const;
    // Names starting with `prefix`, as maps with text (the name, functions
    // with their parenthesis), insert (what to put in) and detail.
    Q_INVOKABLE QVariantList completions(const QString &prefix) const;
    Q_INVOKABLE bool isFunction(const QString &name) const { return m_env.isFunction(name); }
    Q_INVOKABLE bool isVariable(const QString &name) const { return m_env.hasVariable(name); }

    Q_INVOKABLE void removeVariable(const QString &name);
    Q_INVOKABLE void clearVariables();
    Q_INVOKABLE void removeUserFunction(const QString &name);
    Q_INVOKABLE void clearUserFunctions();
    Q_INVOKABLE void clearHistory();

    // Sessions: the history, variables and functions as JSON.
    Q_INVOKABLE void newSession();
    Q_INVOKABLE bool saveSession(const QUrl &file);
    Q_INVOKABLE bool loadSession(const QUrl &file);
    // The session carried over between runs, in the application data folder.
    Q_INVOKABLE bool saveState() const;
    Q_INVOKABLE bool restoreState();
    QString sessionJson() const;
    bool loadSessionJson(const QString &json);
    static QString stateFile();

    Environment &environment() { return m_env; }
    // The lists follow the environment; after a script defined functions.
    void refreshLists();

signals:
    void evaluated(bool ok);
    void angleUnitChanged();
    void formatChanged();
    void sessionFileChanged();

private:
    void refreshVariables();
    void refreshUserFunctions();
    void refreshFunctions();
    void setSessionFile(const QString &file);
    FunctionPredicate functionPredicate() const;

    Environment m_env;
    NumberFormat m_format;
    HistoryModel *m_history;
    NameListModel *m_variables;
    NameListModel *m_userFunctions;
    NameListModel *m_functions;
    NameListModel *m_constants;
    ScriptEngine *m_script;
    QString m_sessionFile;
};
