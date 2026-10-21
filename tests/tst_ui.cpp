// Loads the real UI offscreen and fails on any QML warning. Set
// RECKON_SCREENSHOT_DIR to also save screenshots of the window.

#include "core/Calculator.h"
#include "core/ScriptLibrary.h"
#include "ui/Fonts.h"

#include <QDir>
#include <QGuiApplication>
#include <QStyleHints>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QtQml/qqmlextensionplugin.h>
#include <QtQuickTest/quicktest.h>

Q_IMPORT_QML_PLUGIN(Reckon_CorePlugin)
Q_IMPORT_QML_PLUGIN(ReckonPlugin)

using namespace Qt::StringLiterals;

namespace {

void screenshot(QQuickWindow *window, const QString &name)
{
    const QString dir = qEnvironmentVariable("RECKON_SCREENSHOT_DIR");
    if (dir.isEmpty())
        return;
    QDir().mkpath(dir);
    window->grabWindow().save(dir + u'/' + name + u".png"_s);
}

void type(QQuickWindow *window, const QString &text)
{
    for (const QChar &c : text)
        QTest::keyClick(window, c.unicode() >= 'a' && c.unicode() <= 'z' ? Qt::Key(c.toUpper().unicode()) : Qt::Key(c.unicode()),
                        Qt::NoModifier, 10);
}

} // namespace

class TestUi : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Settings and the saved state go to a throwaway location, not the user's.
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(u"reckon-test"_s);
        QCoreApplication::setOrganizationDomain(u"reckon.test"_s);
        QCoreApplication::setApplicationName(u"reckon-test"_s);
        QSettings().clear();
        if (qEnvironmentVariableIsEmpty("RECKON_TEST_DEFAULT_RHI"))
            QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
        QQuickStyle::setStyle(u"Fusion"_s);
        Fonts::registerBundled();
        QSettings settings;
        settings.setValue(u"view/graph"_s, true);
        settings.setValue(u"view/script"_s, true);
        settings.setValue(u"view/keypad"_s, true);
        settings.setValue(u"view/sidePanel"_s, true);
    }

    void loadsWithoutWarnings()
    {
        QQmlApplicationEngine engine;
        QList<QQmlError> warnings;
        connect(&engine, &QQmlEngine::warnings, this, [&warnings](const QList<QQmlError> &w) { warnings += w; });

        // Scripts go to a folder of this run, where the example is the only one.
        QTemporaryDir scripts;
        engine.setInitialProperties({{u"initialSession"_s, QString()}, {u"restoreSession"_s, false},
                                     {u"scriptDirectory"_s, scripts.path()}});
        engine.loadFromModule("Reckon", "Main");
        QCOMPARE(engine.rootObjects().size(), 1);
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QCOMPARE(window->property("font").value<QFont>().family(), u"Inter"_s);

        auto *calc = window->property("calculator").value<Calculator *>();
        QVERIFY(calc);
        auto *editor = window->findChild<QQuickItem *>(u"editor"_s);
        QVERIFY(editor);
        QTRY_VERIFY(editor->property("inputFocused").toBool());

        // Typing an expression and pressing Enter puts it into the history.
        type(window, u"x = 2 + 3"_s);
        QTest::keyClick(window, Qt::Key_Return);
        QTRY_COMPARE(calc->history()->count(), 1);
        QCOMPARE(calc->history()->resultAt(0), u"5"_s);
        type(window, u"sin(x) + 1 / 0"_s);
        QTest::keyClick(window, Qt::Key_Return);
        QTRY_COMPARE(calc->history()->count(), 2);
        auto *status = window->findChild<QQuickItem *>(u"statusMessage"_s);
        QVERIFY(status);
        QCOMPARE(status->property("text").toString(), u"Division by zero"_s);

        // The preview shows the result while typing.
        type(window, u"x * 10"_s);
        auto *preview = window->findChild<QQuickItem *>(u"preview"_s);
        QVERIFY(preview);
        QTRY_COMPARE(preview->property("text").toString(), u"50"_s);
        // Completing "sq" with Tab gives sqrt(.
        QTest::keyClick(window, Qt::Key_Escape);
        type(window, u"sq"_s);
        QTRY_VERIFY(editor->property("completing").toBool());
        QTest::keyClick(window, Qt::Key_Tab);
        QCOMPARE(editor->property("text").toString(), u"sqrt("_s);
        QTest::keyClick(window, Qt::Key_Escape);
        // Up brings the last expression back.
        QTest::keyClick(window, Qt::Key_Up);
        QCOMPARE(editor->property("text").toString(), u"sin(x) + 1 / 0"_s);
        QTest::keyClick(window, Qt::Key_Escape);

        // The script pane runs against the same calculator and plots.
        auto *scriptPane = window->findChild<QQuickItem *>(u"scriptPane"_s);
        QVERIFY(scriptPane);
        auto *library = scriptPane->property("library").value<ScriptLibrary *>();
        QVERIFY(library);
        QCOMPARE(library->directory(), scripts.path());
        QVERIFY(QMetaObject::invokeMethod(scriptPane, "run"));
        QCOMPARE(library->names(), QStringList{u"Example"_s});
        QTRY_VERIFY(calc->script()->output().contains(u"cube(3) = 27"_s));
        auto *graphPane = window->findChild<QQuickItem *>(u"graphPane"_s);
        QVERIFY(graphPane);
        auto *graph = graphPane->property("graph").value<QQuickItem *>();
        QVERIFY(graph);
        QTRY_COMPARE(graph->property("curves").toList().size(), 2);
        QCOMPARE(graph->property("errors").toStringList(), (QStringList{QString(), QString()}));
        QVariantList values;
        QVERIFY(QMetaObject::invokeMethod(graph, "valuesAt", Q_RETURN_ARG(QVariantList, values), Q_ARG(qreal, 2.0)));
        QCOMPARE(values.size(), 2);
        QCOMPARE(values[0].toDouble(), 0.4);

        // A few more for the pictures: the graph tab, then the script tab.
        for (const QString &expression : {u"f(x) = x^2 - 3x + 2"_s, u"f(4)"_s, u"sqrt(2) pi"_s,
                                          u"0xFF and 0b1111"_s, u"12!"_s, u"average(3, 5, 8)"_s}) {
            type(window, expression);
            QTest::keyClick(window, Qt::Key_Return);
        }
        QTRY_COMPARE(calc->history()->count(), 8);
        QVERIFY(QMetaObject::invokeMethod(graphPane, "addCurve", Q_ARG(QString, u"f(x)"_s), Q_ARG(QString, QString())));
        type(window, u"2 ans"_s);
        QTRY_COMPARE(preview->property("text").toString(), u"10.6666666666667"_s);
        screenshot(window, u"main"_s);
        auto *toolTabs = window->findChild<QQuickItem *>(u"toolTabs"_s);
        QVERIFY(toolTabs);
        toolTabs->setProperty("currentIndex", 1);
        QTest::qWait(50);
        auto *scriptEditor = window->findChild<QQuickItem *>(u"scriptEditor"_s);
        QVERIFY(scriptEditor);
        QCOMPARE(scriptEditor->property("text").toString().left(10), u"// Scripts"_s);
        QTest::qWait(100);
        // The script is drawn, not only held: its lines show up as pixels
        // that differ from the editor's background.
        const QImage grab = window->grabWindow();
        const QRectF editorRect = scriptEditor->mapRectToScene(QRectF(0, 0, 300, 200));
        const QRgb background = grab.pixel(editorRect.bottomRight().toPoint());
        int inked = 0;
        for (int y = int(editorRect.top()); y < int(editorRect.bottom()); ++y) {
            for (int x = int(editorRect.left()); x < int(editorRect.right()); ++x)
                inked += grab.pixel(x, y) != background;
        }
        QVERIFY2(inked > 3000, qPrintable(u"only %1 pixels of script text drawn"_s.arg(inked)));
        screenshot(window, u"script"_s);
        toolTabs->setProperty("currentIndex", 0);
        QTest::keyClick(window, Qt::Key_Escape);

        // A later assignment takes the name over; the earlier entry says so.
        type(window, u"x = 7"_s);
        QTest::keyClick(window, Qt::Key_Return);
        QTRY_COMPARE(calc->history()->count(), 9);
        QCOMPARE(calc->history()->data(calc->history()->index(0), HistoryModel::ReplacedRole).toString(),
                 u"x is redefined in $9"_s);
        QVERIFY(QMetaObject::invokeMethod(calc, "removeEntry", Q_ARG(int, 8)));
        QTRY_COMPARE(calc->history()->count(), 8);
        // The editor keeps the focus while the row goes.
        QTest::qWait(50);
        QVERIFY(editor->property("inputFocused").toBool());

        // A double-click edits an entry in place; Enter updates it and
        // what depends on it, and the editor takes over again.
        auto *display = window->findChild<QQuickItem *>(u"display"_s);
        QVERIFY(display);
        QVERIFY(QMetaObject::invokeMethod(display, "positionViewAtBeginning"));
        QQuickItem *first = nullptr;
        QTRY_VERIFY(QMetaObject::invokeMethod(display, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, first),
                                              Q_ARG(int, 0))
                    && first);
        QQuickItem *expressionText = nullptr;
        QQuickItem *resultText = nullptr;
        for (QQuickItem *item : first->findChildren<QQuickItem *>()) {
            if (item->property("text").toString() == u"x = 2 + 3"_s && !expressionText)
                expressionText = item;
            if (item->property("text").toString() == u"5"_s && !resultText)
                resultText = item;
        }
        QVERIFY(expressionText);
        QVERIFY(resultText);
        type(window, u"10 + "_s);
        QTest::mouseClick(window, Qt::LeftButton, {}, resultText->mapToScene({resultText->width() - 4, 6}).toPoint());
        // A click on a result binds to it by name.
        QTRY_COMPARE(editor->property("text").toString(), u"10 + $1"_s);
        QTest::keyClick(window, Qt::Key_Return);
        QTRY_COMPARE(calc->history()->count(), 9);
        QCOMPARE(calc->history()->resultAt(8), u"15"_s);
        // The new entry scrolled the view to the end, after a moment.
        QTest::qWait(50);
        QVERIFY(QMetaObject::invokeMethod(display, "positionViewAtBeginning"));
        QTRY_VERIFY(display->property("atYBeginning").toBool());
        const qreal rowHeight = first->height();
        QTest::mouseDClick(window, Qt::LeftButton, {}, expressionText->mapToScene({4, 6}).toPoint());
        auto *entryEditor = first->findChild<QQuickItem *>(u"entryEditor"_s);
        QVERIFY(entryEditor);
        QTRY_VERIFY(entryEditor->hasActiveFocus());
        QCOMPARE(entryEditor->property("text").toString(), u"x = 2 + 3"_s);
        // The editor takes the expression's place without moving the rows
        // below, also while the change gives no result yet.
        QCOMPARE(entryEditor->height(), expressionText->height());
        QCOMPARE(first->height(), rowHeight);
        type(window, u"x = "_s);
        QTest::qWait(20);
        QCOMPARE(first->height(), rowHeight);
        QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier);
        type(window, u"x = 4"_s);
        QTest::keyClick(window, Qt::Key_Return);
        QTRY_COMPARE(calc->history()->resultAt(0), u"4"_s);
        QCOMPARE(calc->history()->resultAt(8), u"14"_s);
        QVERIFY(!entryEditor->isVisible());
        QTRY_VERIFY(editor->property("inputFocused").toBool());
        // Escape leaves the entry as it was. A double-click right after
        // the last one would count as part of it.
        QTest::qWait(QGuiApplication::styleHints()->mouseDoubleClickInterval() + 50);
        QTest::mouseDClick(window, Qt::LeftButton, {}, expressionText->mapToScene({4, 6}).toPoint());
        QTRY_VERIFY(entryEditor->hasActiveFocus());
        type(window, u"x = 100"_s);
        QTest::qWait(50);
        screenshot(window, u"edit"_s);
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!entryEditor->isVisible());
        QCOMPARE(calc->history()->expressionAt(0), u"x = 4"_s);
        // Nor does an entry with an error, whose message is smaller.
        QQuickItem *second = nullptr;
        QVERIFY(QMetaObject::invokeMethod(display, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, second), Q_ARG(int, 1)));
        QVERIFY(second);
        const qreal errorRowHeight = second->height();
        QVERIFY(QMetaObject::invokeMethod(display, "edit", Q_ARG(int, 1)));
        auto *errorEditor = second->findChild<QQuickItem *>(u"entryEditor"_s);
        QTRY_VERIFY(errorEditor->hasActiveFocus());
        QCOMPARE(second->height(), errorRowHeight);
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!errorEditor->isVisible());

        window->close();
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.isEmpty() ? QString() : warnings.first().toString()));
    }
};

QTEST_MAIN(TestUi)
#include "tst_ui.moc"
