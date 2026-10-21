#include "core/Calculator.h"
#include "core/ScriptEngine.h"
#include "core/ScriptLibrary.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

namespace {

QString resultAt(const Calculator &calc, int row)
{
    return calc.history()->resultAt(row);
}

QString errorAt(const Calculator &calc, int row)
{
    return calc.history()->data(calc.history()->index(row), HistoryModel::ErrorRole).toString();
}

QVariant roleAt(const Calculator &calc, int row, int role)
{
    return calc.history()->data(calc.history()->index(row), role);
}

} // namespace

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
        // The entry defines f; the list cannot take it away from it.
        calc.removeUserFunction(u"f"_s);
        QVERIFY(calc.evaluate(u"f(4)"_s));
        calc.removeEntry(0);
        QCOMPARE(calc.userFunctions()->count(), 0);
        QCOMPARE(errorAt(calc, 0), u"Unknown function f"_s);
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
        // Bound by entries, which say so, and stay until those change.
        QCOMPARE(variables->data(variables->index(3), NameListModel::DescriptionRole).toString(),
                 u"Defined by $2"_s);
        QVERIFY(variables->data(variables->index(3), NameListModel::BuiltinRole).toBool());
        calc.removeVariable(u"alpha"_s);
        calc.clearVariables();
        QCOMPARE(variables->count(), 5);
        // Variables a script set are the environment's own.
        QVERIFY(calc.script()->run(u"calc.set('delta', 3)"_s));
        QCOMPARE(variables->count(), 6);
        QVERIFY(!variables->data(variables->index(5), NameListModel::BuiltinRole).toBool());
        calc.removeVariable(u"delta"_s);
        QCOMPARE(variables->count(), 5);
        calc.removeEntry(1);
        QCOMPARE(variables->count(), 4);
    }

    void editsUpdateWhatDependsOnThem()
    {
        Calculator calc;
        calc.evaluate(u"a = 3"_s);
        calc.evaluate(u"b = a * 2"_s);
        calc.evaluate(u"$2 + 1"_s);
        calc.evaluate(u"10"_s);
        QCOMPARE(resultAt(calc, 2), u"7"_s);
        QCOMPARE(roleAt(calc, 2, HistoryModel::NameRole).toString(), u"$3"_s);
        QVERIFY(calc.edit(0, u"a = 5"_s));
        QCOMPARE(resultAt(calc, 0), u"5"_s);
        QCOMPARE(resultAt(calc, 1), u"10"_s);
        QCOMPARE(resultAt(calc, 2), u"11"_s);
        // The entries that followed say so; the one edited and the one
        // that does not depend on it do not.
        QCOMPARE(roleAt(calc, 0, HistoryModel::RevisionRole).toInt(), 0);
        QCOMPARE(roleAt(calc, 1, HistoryModel::RevisionRole).toInt(), 1);
        QCOMPARE(roleAt(calc, 2, HistoryModel::RevisionRole).toInt(), 1);
        QCOMPARE(roleAt(calc, 3, HistoryModel::RevisionRole).toInt(), 0);
        // Names stay with their entries.
        QCOMPARE(roleAt(calc, 2, HistoryModel::NameRole).toString(), u"$3"_s);
        QVERIFY(!calc.edit(1, u"b = a / 0"_s));
        QCOMPARE(errorAt(calc, 1), u"Division by zero"_s);
        QCOMPARE(errorAt(calc, 2), u"$2 has an error"_s);
        QCOMPARE(roleAt(calc, 2, HistoryModel::ErrorPositionRole).toInt(), 0);
        QVERIFY(!calc.evaluate(u"$9"_s));
        QCOMPARE(errorAt(calc, 4), u"There is no $9"_s);
    }

    void latestAssignmentWins()
    {
        Calculator calc;
        calc.evaluate(u"x = 1"_s);
        calc.evaluate(u"y = x * 2"_s);
        QCOMPARE(resultAt(calc, 1), u"2"_s);
        calc.evaluate(u"x = 5"_s);
        QCOMPARE(resultAt(calc, 1), u"10"_s);
        QCOMPARE(roleAt(calc, 0, HistoryModel::ReplacedRole).toString(), u"x is redefined in $3"_s);
        QCOMPARE(roleAt(calc, 2, HistoryModel::ReplacedRole).toString(), QString());
        // Taking the later assignment back gives x to the earlier one again.
        calc.edit(2, u"z = 0"_s);
        QCOMPARE(resultAt(calc, 1), u"2"_s);
        QCOMPARE(roleAt(calc, 0, HistoryModel::ReplacedRole).toString(), QString());
        // Order does not matter: a name defined further down binds too.
        QVERIFY(!calc.evaluate(u"w = v + 1"_s));
        QCOMPARE(errorAt(calc, 3), u"Unknown variable v"_s);
        calc.evaluate(u"v = 41"_s);
        QCOMPARE(resultAt(calc, 3), u"42"_s);
        // So does a name that turns from a function into a variable.
        calc.evaluate(u"g(t) = t + 1"_s);
        calc.evaluate(u"g(1)"_s);
        QCOMPARE(resultAt(calc, 6), u"2"_s);
        calc.edit(5, u"g = 4"_s);
        QCOMPARE(resultAt(calc, 6), u"4"_s); // g times 1
    }

    void bindingLoopsAreErrors()
    {
        Calculator calc;
        QVERIFY(!calc.evaluate(u"x = x + 1"_s));
        QCOMPARE(errorAt(calc, 0), u"x depends on itself"_s);
        calc.evaluate(u"a = b + 1"_s);
        calc.evaluate(u"b = a + 1"_s);
        QCOMPARE(errorAt(calc, 1), u"Binding loop: a → b → a"_s);
        QCOMPARE(errorAt(calc, 2), u"Binding loop: a → b → a"_s);
        calc.evaluate(u"c = a * 2"_s);
        QCOMPARE(errorAt(calc, 3), u"a has an error"_s);
        calc.edit(2, u"b = 2"_s);
        QCOMPARE(resultAt(calc, 1), u"3"_s);
        QCOMPARE(resultAt(calc, 3), u"6"_s);
        // Through entry names and functions too.
        calc.evaluate(u"$7 + 1"_s);
        calc.evaluate(u"f(t) = t + $5"_s);
        calc.evaluate(u"f(1)"_s);
        QCOMPARE(errorAt(calc, 4), u"Binding loop: $5 → $7 → $5"_s);
        // Assigning inside an expression would bind behind the history's back.
        QVERIFY(!calc.evaluate(u"1 + (y = 2)"_s));
        QCOMPARE(errorAt(calc, 7), u"y can only be assigned at the start"_s);
    }

    void ansIsTheEntryAbove()
    {
        Calculator calc;
        calc.evaluate(u"2"_s);
        calc.evaluate(u"1 / 0"_s);
        calc.evaluate(u"f(x) = x"_s);
        calc.evaluate(u"ans * 3"_s);
        QCOMPARE(resultAt(calc, 3), u"6"_s);
        calc.edit(0, u"5"_s);
        QCOMPARE(resultAt(calc, 3), u"15"_s);
        calc.edit(1, u"1 / 2"_s);
        QCOMPARE(resultAt(calc, 3), u"1.5"_s);
        QCOMPARE(calc.ans(), u"1.5"_s);
    }

    void unchangedEntriesKeepTheirValues()
    {
        Calculator calc;
        calc.evaluate(u"r = random()"_s);
        calc.evaluate(u"s = 1"_s);
        calc.evaluate(u"r + s"_s);
        const double r = roleAt(calc, 0, HistoryModel::ValueRole).toDouble();
        calc.edit(1, u"s = 2"_s);
        QCOMPARE(roleAt(calc, 0, HistoryModel::ValueRole).toDouble(), r);
        QCOMPARE(roleAt(calc, 2, HistoryModel::ValueRole).toDouble(), r + 2);
        calc.evaluate(u"1 + 1"_s);
        QCOMPARE(roleAt(calc, 0, HistoryModel::ValueRole).toDouble(), r);
    }

    void functionsBindToo()
    {
        Calculator calc;
        calc.evaluate(u"k = 2"_s);
        calc.evaluate(u"f(x) = x * k"_s);
        calc.evaluate(u"f(3)"_s);
        QCOMPARE(resultAt(calc, 2), u"6"_s);
        calc.edit(0, u"k = 10"_s);
        QCOMPARE(resultAt(calc, 2), u"30"_s);
        calc.edit(1, u"f(x) = x + k"_s);
        QCOMPARE(resultAt(calc, 2), u"13"_s);
        // A parameter never reaches an entry the body reads.
        calc.evaluate(u"m = x"_s);
        calc.evaluate(u"h(x) = m"_s);
        QVERIFY(!calc.evaluate(u"h(1)"_s));
        QCOMPARE(errorAt(calc, 5), u"m has an error"_s);
        QCOMPARE(errorAt(calc, 3), u"Unknown variable x"_s);
        // Entries with angles follow the unit.
        calc.evaluate(u"sin(pi / 2)"_s);
        QCOMPARE(resultAt(calc, 6), u"1"_s);
        calc.setAngleUnit(Calculator::Degrees);
        QCOMPARE(resultAt(calc, 6), u"0.0274121335920443"_s);
    }

    void clearingTheHistoryKeepsWhatItDefined()
    {
        Calculator calc;
        calc.evaluate(u"x = 3"_s);
        calc.evaluate(u"f(a) = a * x"_s);
        calc.evaluate(u"7"_s);
        calc.clearHistory();
        QCOMPARE(calc.history()->count(), 0);
        QVERIFY(calc.isVariable(u"x"_s));
        QCOMPARE(calc.ans(), u"7"_s);
        QVERIFY(calc.evaluate(u"f(2)"_s));
        QCOMPARE(calc.ans(), u"6"_s);
        // Plain values again, which the list can remove.
        calc.removeUserFunction(u"f"_s);
        QVERIFY(!calc.evaluate(u"f(2)"_s));
        QCOMPARE(roleAt(calc, 0, HistoryModel::NameRole).toString(), u"$1"_s);
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
        // Still bound after loading, under the same names.
        QCOMPARE(roleAt(other, 4, HistoryModel::NameRole).toString(), u"$5"_s);
        other.edit(0, u"x = 10"_s);
        QCOMPARE(resultAt(other, 2), u"11"_s);
        QCOMPARE(resultAt(other, 4), u"12"_s);
        // Version 1 sessions saved the bound names as plain ones too.
        Calculator old;
        QVERIFY(old.loadSessionJson(uR"({"version": 1, "variables": {"x": 1.5, "ans": 2.5},
            "history": [{"expression": "x = 1.5", "value": 1.5}, {"expression": "x * 2", "value": 3}]})"_s));
        QCOMPARE(old.variables()->count(), 4);
        old.edit(0, u"x = 4"_s);
        QCOMPARE(resultAt(old, 1), u"8"_s);

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
        // A bound name changes through its entry only.
        script->clearOutput();
        QVERIFY(!script->run(u"calc.set('x', 3)"_s));
        QVERIFY(script->output().contains(u"x is defined by $1"_s));
    }

    void scriptsTakePartInBindings()
    {
        Calculator calc;
        ScriptEngine *script = calc.script();
        // What a script function reads is a dependency like any other.
        QVERIFY(script->run(u"calc.define('twice', () => 2 * calc.get('k'), 0)"_s));
        calc.evaluate(u"k = 2"_s);
        calc.evaluate(u"twice()"_s);
        QCOMPARE(resultAt(calc, 1), u"4"_s);
        calc.edit(0, u"k = 5"_s);
        QCOMPARE(resultAt(calc, 1), u"10"_s);
        // Defining a function a script uses updates the entries using it.
        QVERIFY(!calc.evaluate(u"later(1)"_s));
        QVERIFY(script->run(u"calc.define('later', x => x + 1)"_s));
        QCOMPARE(resultAt(calc, 2), u"2"_s);
        // And so does a variable a script sets.
        QVERIFY(!calc.evaluate(u"p * 2"_s));
        QVERIFY(script->run(u"calc.set('p', 4)"_s));
        QCOMPARE(resultAt(calc, 3), u"8"_s);
        QVERIFY(script->run(u"calc.eval('p = 5')"_s));
        QCOMPARE(resultAt(calc, 3), u"10"_s);
        // A script function the history calls cannot change it meanwhile.
        QVERIFY(script->run(u"calc.define('meddle', () => { calc.set('q', 1); return 1 }, 0)"_s));
        QVERIFY(!calc.evaluate(u"meddle()"_s));
        QVERIFY(errorAt(calc, 4).contains(u"cannot change anything while the history updates"_s));
        QVERIFY(!script->run(u"calc.define('k', x => x)"_s));
        QVERIFY(script->output().contains(u"k is defined by $1"_s));
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
