// Loads the real UI offscreen and fails on any QML warning. Set
// RECKON_SCREENSHOT_DIR to also save screenshots of the window.

#include "core/Calculator.h"
#include "core/ScriptLibrary.h"
#include "ui/Fonts.h"

#include <QDir>
#include <QGuiApplication>
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

        window->close();
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.isEmpty() ? QString() : warnings.first().toString()));
    }
};

QTEST_MAIN(TestUi)
#include "tst_ui.moc"
