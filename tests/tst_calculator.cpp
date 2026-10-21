#include "core/Calculator.h"
#include "core/ScriptEngine.h"
#include "core/ScriptLibrary.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

class TestCalculator : public QObject
{
    Q_OBJECT

private slots:
    void evaluatesIntoHistory()
    {
        Calculator calc;
        QVERIFY(calc.evaluate(u"1 + 2"_s));
        QCOMPARE(calc.history()->count(), 1);
        QCOMPARE(calc.history()->resultAt(0), u"3"_s);
        QCOMPARE(calc.ans(), u"3"_s);
        QVERIFY(calc.evaluate(u"ans * 2"_s));
        QCOMPARE(calc.ans(), u"6"_s);
        QVERIFY(!calc.evaluate(u"1 / 0"_s));
        QCOMPARE(calc.history()->data(calc.history()->index(2), HistoryModel::ErrorRole).toString(),
                 u"Division by zero"_s);
        QCOMPARE(calc.history()->data(calc.history()->index(2), HistoryModel::ErrorPositionRole).toInt(), 2);
        // The failed expression leaves ans alone and empty input is ignored.
        QCOMPARE(calc.ans(), u"6"_s);
        QVERIFY(!calc.evaluate(u"   "_s));
        QCOMPARE(calc.history()->count(), 3);
        QCOMPARE(calc.history()->expressions(), (QStringList{u"1 / 0"_s, u"ans * 2"_s, u"1 + 2"_s}));
    }

    void definitionsShowAsThemselves()
    {
        Calculator calc;
        QVERIFY(calc.evaluate(u"f(x) = 2 x"_s));
        QCOMPARE(calc.history()->resultAt(0), u"f(x) = 2 x"_s);
        QCOMPARE(calc.userFunctions()->count(), 1);
        QCOMPARE(calc.userFunctions()->data(calc.userFunctions()->index(0), NameListModel::ValueRole).toString(),
                 u"f(x)"_s);
        QVERIFY(calc.evaluate(u"f(4)"_s));
        QCOMPARE(calc.ans(), u"8"_s);
        calc.removeUserFunction(u"f"_s);
        QVERIFY(!calc.evaluate(u"f(4)"_s));
    }

    void previewChangesNothing()
    {
        Calculator calc;
        QCOMPARE(calc.preview(u"x = 5"_s), u"5"_s);
        QVERIFY(!calc.isVariable(u"x"_s));
        QCOMPARE(calc.preview(u"1 +"_s), QString());
        QCOMPARE(calc.preview(u"f(x) = x"_s), QString());
        QCOMPARE(calc.history()->count(), 0);
        QCOMPARE(calc.ans(), u"0"_s);
    }

    void variablesListed()
    {
        Calculator calc;
        calc.evaluate(u"beta = 2"_s);
        calc.evaluate(u"alpha = 1"_s);
        NameListModel *variables = calc.variables();
        // ans, pi and e first, then the user's in alphabetical order.
        QCOMPARE(variables->count(), 5);
        QCOMPARE(variables->nameAt(0), u"ans"_s);
        QCOMPARE(variables->nameAt(3), u"alpha"_s);
        QCOMPARE(variables->nameAt(4), u"beta"_s);
        QVERIFY(variables->data(variables->index(1), NameListModel::BuiltinRole).toBool());
        variables->setFilter(u"al"_s);
        QCOMPARE(variables->count(), 1);
        variables->setFilter({});
        calc.removeVariable(u"alpha"_s);
        QCOMPARE(variables->count(), 4);
        calc.clearVariables();
        QCOMPARE(variables->count(), 3);
    }

    void formatsFollowSettings()
    {
        Calculator calc;
        calc.evaluate(u"10 / 4"_s);
        calc.evaluate(u"255"_s);
        calc.setResultFormat(Calculator::Hexadecimal);
        QCOMPARE(calc.history()->resultAt(1), u"0xff"_s);
        QCOMPARE(calc.history()->resultAt(0), u"0x2.8"_s);
        calc.setResultFormat(Calculator::Fixed);
        calc.setPrecision(3);
        QCOMPARE(calc.history()->resultAt(0), u"2.500"_s);
        QCOMPARE(calc.ans(), u"255.000"_s);
        calc.setAngleUnit(Calculator::Degrees);
        calc.setResultFormat(Calculator::General);
        calc.setPrecision(-1);
        calc.evaluate(u"cos(60)"_s);
        QCOMPARE(calc.ans(), u"0.5"_s);
    }

    void completions()
    {
        Calculator calc;
        calc.evaluate(u"sigma = 3"_s);
        const QVariantList list = calc.completions(u"si"_s);
        QStringList texts;
        for (const QVariant &item : list)
            texts.append(item.toMap().value(u"text"_s).toString());
        QCOMPARE(texts, (QStringList{u"sigma"_s, u"sign(x)"_s, u"sin(x)"_s, u"sinh(x)"_s}));
        QCOMPARE(list[2].toMap().value(u"insert"_s).toString(), u"sin("_s);
    }

    void sessionRoundTrip()
    {
        Calculator calc;
        calc.evaluate(u"x = 1.5"_s);
        calc.evaluate(u"f(a) = a + x"_s);
        calc.evaluate(u"f(1)"_s);
        calc.evaluate(u"1 / 0"_s);
        const QString json = calc.sessionJson();

        Calculator other;
        QVERIFY(other.loadSessionJson(json));
        QCOMPARE(other.history()->count(), 4);
        QCOMPARE(other.history()->resultAt(1), u"f(a) = a + x"_s);
        QCOMPARE(other.history()->resultAt(2), u"2.5"_s);
        QCOMPARE(other.history()->data(other.history()->index(3), HistoryModel::ErrorRole).toString(),
                 u"Division by zero"_s);
        QVERIFY(other.evaluate(u"f(2)"_s));
        QCOMPARE(other.ans(), u"3.5"_s);

        QTemporaryDir dir;
        const QUrl file = QUrl::fromLocalFile(dir.filePath(u"session.json"_s));
        QVERIFY(calc.saveSession(file));
        QCOMPARE(calc.sessionFile(), file.toLocalFile());
        Calculator third;
        QVERIFY(third.loadSession(file));
        QCOMPARE(third.variables()->count(), 4);
        third.newSession();
        QCOMPARE(third.history()->count(), 0);
        QCOMPARE(third.variables()->count(), 3);
        QCOMPARE(third.sessionFile(), QString());
        QVERIFY(!third.loadSession(QUrl::fromLocalFile(dir.filePath(u"missing.json"_s))));
    }

    void scriptsSeeTheCalculator()
    {
        Calculator calc;
        ScriptEngine *script = calc.script();
        calc.evaluate(u"x = 4"_s);
        QVERIFY(script->run(u"print(calc.eval('x * 2'), calc.get('x'), calc.ans)"_s));
        QCOMPARE(script->output(), u"8 4 4\n"_s);
        script->clearOutput();
        QVERIFY(script->run(u"calc.set('y', 7); calc.run('y + 1')"_s));
        QCOMPARE(calc.ans(), u"8"_s);
        QCOMPARE(calc.history()->count(), 2);
        // A calculator error is a JavaScript error with its message.
        QVERIFY(!script->run(u"calc.eval('1 / 0')"_s));
        QVERIFY(script->output().contains(u"Division by zero"_s));
        script->clearOutput();
        QVERIFY(!script->run(u"calc.set('pi', 3)"_s));
        QVERIFY(script->output().contains(u"pi is a constant"_s));
        QCOMPARE(script->run(u"calc.history().length"_s), true);
        QVERIFY(script->output().endsWith(u"2\n"_s));
    }

    void scriptsDefineFunctions()
    {
        Calculator calc;
        ScriptEngine *script = calc.script();
        QVERIFY(script->run(u"calc.define('cube', x => x * x * x); calc.define('sumsq', (...a) => a.reduce((s, v) => s + v * v, 0))"_s));
        QVERIFY(calc.evaluate(u"cube(3)"_s));
        QCOMPARE(calc.ans(), u"27"_s);
        QVERIFY(calc.evaluate(u"sumsq(1, 2, 3)"_s));
        QCOMPARE(calc.ans(), u"14"_s);
        QVERIFY(!calc.evaluate(u"cube(1, 2)"_s));
        QCOMPARE(calc.history()->data(calc.history()->index(2), HistoryModel::ErrorRole).toString(),
                 u"cube takes one argument"_s);
        // Listed with the functions, and gone when undefined.
        bool listed = false;
        for (int row = 0; row < calc.functions()->count(); ++row)
            listed = listed || calc.functions()->nameAt(row) == u"cube"_s;
        QVERIFY(listed);
        QVERIFY(!script->run(u"calc.define('sin', x => x)"_s));
        QVERIFY(!script->run(u"calc.define('bad', 3)"_s));
        QVERIFY(script->run(u"calc.define('fails', x => { throw new Error('nope') })"_s));
        QVERIFY(!calc.evaluate(u"fails(1)"_s));
        QVERIFY(calc.history()->data(calc.history()->index(3), HistoryModel::ErrorRole).toString().contains(u"nope"_s));
        QVERIFY(script->run(u"calc.undefine('cube')"_s));
        QVERIFY(!calc.evaluate(u"cube(2)"_s));
    }

    void scriptsPlot()
    {
        Calculator calc;
        ScriptEngine *script = calc.script();
        QSignalSpy plots(script, &ScriptEngine::plotRequested);
        QSignalSpy cleared(script, &ScriptEngine::plotsCleared);
        QVERIFY(script->run(u"calc.clearPlots(); calc.plot('sin(x)', 'red'); calc.plot('x^2')"_s));
        QCOMPARE(cleared.count(), 1);
        QCOMPARE(plots.count(), 2);
        QCOMPARE(plots[0][0].toString(), u"sin(x)"_s);
        QCOMPARE(plots[0][1].toString(), u"red"_s);
        QCOMPARE(plots[1][1].toString(), QString());
    }

    void scriptLibrary()
    {
        QTemporaryDir dir;
        ScriptLibrary library;
        library.setDirectory(dir.path());
        QCOMPARE(library.names(), QStringList());
        QCOMPARE(library.create(u"Untitled"_s), u"Untitled"_s);
        QCOMPARE(library.create(u"Untitled"_s), u"Untitled 2"_s);
        QVERIFY(library.write(u"Untitled"_s, u"print(1)"_s));
        QCOMPARE(library.read(u"Untitled"_s), u"print(1)"_s);
        QCOMPARE(library.names(), (QStringList{u"Untitled"_s, u"Untitled 2"_s}));
        QVERIFY(library.rename(u"Untitled 2"_s, u"Second"_s));
        QVERIFY(!library.rename(u"Second"_s, u"Untitled"_s));
        QCOMPARE(library.names(), (QStringList{u"Second"_s, u"Untitled"_s}));
        QVERIFY(library.remove(u"Second"_s));
        QCOMPARE(library.names(), QStringList{u"Untitled"_s});
        QCOMPARE(library.read(u"missing"_s), QString());
    }
};

QTEST_GUILESS_MAIN(TestCalculator)
#include "tst_calculator.moc"
