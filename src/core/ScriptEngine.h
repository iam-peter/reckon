#pragma once

#include <QJSEngine>
#include <QObject>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class Calculator;
class ScriptApi;

// Runs JavaScript against the calculator. Scripts see a `calc` object
// (see ScriptApi) and `print`, whose output collects in `output`; the
// curves a run plots are its `plots`.
class ScriptEngine : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Comes from Calculator")
    Q_PROPERTY(QString output READ output NOTIFY outputChanged)
    // What the last run plotted, as {expression, color} maps, the color
    // empty where the script gave none. Each run starts with none.
    Q_PROPERTY(QVariantList plots READ plots NOTIFY plotsChanged)

public:
    explicit ScriptEngine(Calculator *calculator);

    QString output() const { return m_output; }
    QVariantList plots() const { return m_plots; }

    // Runs the code; false when it threw, with the error in the output.
    Q_INVOKABLE bool run(const QString &code, const QString &fileName = {});
    Q_INVOKABLE void clearOutput();

    void print(const QString &text);
    void plot(const QString &expression, const QString &color);
    void clearPlots();

signals:
    void outputChanged();
    void plotsChanged();

private:
    Calculator *m_calculator;
    QJSEngine m_engine;
    ScriptApi *m_api;
    QString m_output;
    QVariantList m_plots;
};

// The `calc` object of scripts.
class ScriptApi : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double ans READ ans)

public:
    ScriptApi(Calculator *calculator, ScriptEngine *engine, QJSEngine *js);

    double ans() const;

    // Evaluates an expression as the calculator would, without touching
    // the history; a calculator error becomes a JavaScript error.
    Q_INVOKABLE double eval(const QString &expression);
    // Like eval, but into the history, as if typed.
    Q_INVOKABLE bool run(const QString &expression);
    Q_INVOKABLE double get(const QString &name);
    Q_INVOKABLE void set(const QString &name, double value);
    Q_INVOKABLE bool has(const QString &name);
    // Makes `name(...)` in expressions call the JavaScript function, with
    // as many arguments as it declares; a function declaring none, such
    // as one with rest parameters, takes any number. `arity` overrides
    // that, -1 for any number.
    Q_INVOKABLE void define(const QString &name, const QJSValue &function, int arity = -2);
    Q_INVOKABLE void undefine(const QString &name);
    Q_INVOKABLE QString format(double value);
    Q_INVOKABLE QVariantList history();
    Q_INVOKABLE void plot(const QString &expression, const QString &color = {});
    Q_INVOKABLE void clearPlots();
    Q_INVOKABLE void print(const QString &text);

private:
    // Throws a JavaScript error, and says so, while the history updates.
    bool busy(const QString &what);

    Calculator *m_calculator;
    ScriptEngine *m_engine;
    QJSEngine *m_js;
};
