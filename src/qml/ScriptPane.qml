pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Reckon
import Reckon.Core

// JavaScript against the calculator: a library of scripts kept as files,
// an editor for the current one, and the output of running it.
ColumnLayout {
    id: pane

    required property Calculator calculator
    required property font monoFont
    // Empty for the application data folder.
    property string scriptDirectory: ""
    property alias library: library
    property string current: ""

    spacing: 4

    ScriptLibrary {
        id: library
        directory: pane.scriptDirectory !== "" ? pane.scriptDirectory : library.defaultDirectory
    }

    readonly property string exampleScript: `// Scripts see the calculator as \`calc\`:
//   calc.eval("2 + 2")           evaluates an expression
//   calc.run("x = 5")            evaluates it into the history
//   calc.get("x"), calc.set("x", 1), calc.has("x"), calc.ans
//   calc.define("name", fn)      makes name(...) call fn in expressions
//   calc.plot("sin(x)", "red"), calc.clearPlots()
//   calc.format(1 / 3)           formats a number like a result
//   calc.history()               the expressions and results so far
// print(...) writes to the output. A script named "startup" runs when
// Reckon starts, a good place for functions used often.

calc.define("cube", x => x * x * x)
calc.define("hyp", (a, b) => Math.sqrt(a * a + b * b))
calc.define("sumsq", (...v) => v.reduce((s, x) => s + x * x, 0))

print("cube(3) =", calc.eval("cube(3)"))
print("hyp(3, 4) =", calc.eval("hyp(3, 4)"))

calc.clearPlots()
calc.plot("cube(x) / 20")
calc.plot("sin(x) * 4", "#1b7ced")
`

    function load(name: string) {
        flush()
        current = name
        AppSettings.script.current = name
        if (shown) {
            editor.text = name !== "" ? library.read(name) : ""
            editor.modified = false
        }
    }

    // Writes the editor's text to its file, if it changed.
    function flush() {
        if (current !== "" && shown && editor.modified) {
            library.write(current, editor.text)
            editor.modified = false
        }
    }

    function run() {
        flush()
        pane.calculator.script.run(shown ? editor.text : library.read(current), current)
    }

    function create() {
        const name = library.create(qsTr("Untitled"))
        if (name !== "")
            load(name)
        editor.forceActiveFocus()
    }

    function remove() {
        if (current === "")
            return
        library.remove(current)
        editor.modified = false
        current = ""
        load(library.names[0] ?? "")
    }

    // The script goes into the editor the first time the pane is on
    // screen: a TextArea that has never been shown drops text it is given
    // while hidden. Until then the current script is run from its file.
    property bool shown: false

    function show() {
        if (shown || !visible)
            return
        shown = true
        editor.text = current !== "" ? library.read(current) : ""
        editor.modified = false
    }

    onVisibleChanged: show()
    Component.onCompleted: {
        if (library.names.length === 0) {
            library.write(qsTr("Example"), exampleScript)
            load(qsTr("Example"))
        } else {
            const remembered = AppSettings.script.current
            load(library.names.includes(remembered) ? remembered : library.names[0])
        }
        // Once the layout has decided which page is on screen.
        Qt.callLater(show)
    }

    Connections {
        target: library
        function onNamesChanged() {
            if (!library.names.includes(pane.current))
                pane.load(library.names[0] ?? "")
        }
    }

    Timer {
        id: autosave
        interval: 800
        onTriggered: pane.flush()
    }

    PanelHeader {
        Layout.fillWidth: true
        title: qsTr("Script")
        IconButton { iconName: "plus"; tip: qsTr("New script"); onClicked: pane.create() }
        IconButton {
            iconName: "play"
            tip: qsTr("Run (Ctrl+Enter)")
            enabled: pane.current !== ""
            onClicked: pane.run()
        }
        IconButton {
            iconName: "pencil"
            tip: qsTr("Rename")
            enabled: pane.current !== ""
            onClicked: {
                renameField.text = pane.current
                renameDialog.open()
            }
        }
        IconButton {
            iconName: "trash-2"
            tip: qsTr("Delete")
            enabled: pane.current !== ""
            onClicked: deleteDialog.open()
        }
    }

    ComboBox {
        Layout.fillWidth: true
        Layout.leftMargin: 6
        Layout.rightMargin: 6
        model: library.names
        currentIndex: library.names.indexOf(pane.current)
        onActivated: index => pane.load(library.names[index])
    }

    SplitView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        orientation: Qt.Vertical
        handle: SplitterHandle {}

        Flickable {
            SplitView.fillHeight: true
            SplitView.minimumHeight: 80
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            ScrollBar.horizontal: ScrollBar {}

            TextArea.flickable: TextArea {
                id: editor
                objectName: "scriptEditor"

                property bool modified: false

                font: pane.monoFont
                placeholderText: pane.current === "" ? qsTr("Add a script with +") : qsTr("// JavaScript")
                readOnly: pane.current === ""
                wrapMode: TextEdit.NoWrap
                selectByMouse: true
                tabStopDistance: 4 * fontMetrics.averageCharacterWidth
                onTextChanged: {
                    modified = true
                    autosave.restart()
                }
                Keys.onPressed: event => {
                    if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                            && event.modifiers & Qt.ControlModifier) {
                        pane.run()
                        event.accepted = true
                    } else if (event.key === Qt.Key_Tab) {
                        editor.insert(editor.cursorPosition, "    ")
                        event.accepted = true
                    }
                }

                FontMetrics {
                    id: fontMetrics
                    font: pane.monoFont
                }
            }
        }

        ColumnLayout {
            SplitView.preferredHeight: 140
            SplitView.minimumHeight: 40
            spacing: 0

            PanelHeader {
                Layout.fillWidth: true
                title: qsTr("Output")
                IconButton {
                    iconName: "x"
                    tip: qsTr("Clear the output")
                    onClicked: pane.calculator.script.clearOutput()
                }
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true

                TextArea {
                    id: output
                    objectName: "scriptOutput"
                    readOnly: true
                    font: pane.monoFont
                    text: pane.calculator.script.output
                    wrapMode: TextEdit.Wrap
                    selectByMouse: true
                    onTextChanged: cursorPosition = length
                }
            }
        }
    }

    ModalDialog {
        id: renameDialog
        title: qsTr("Rename the script")
        standardButtons: Dialog.Ok | Dialog.Cancel
        onOpened: renameField.forceActiveFocus()
        onAccepted: {
            const name = renameField.text.trim()
            if (name !== "" && name !== pane.current && library.rename(pane.current, name)) {
                pane.current = name
                AppSettings.script.current = name
            }
        }
        contentItem: TextField {
            id: renameField
            onAccepted: renameDialog.accept()
        }
    }

    ModalDialog {
        id: deleteDialog
        title: qsTr("Delete the script")
        standardButtons: Dialog.Yes | Dialog.No
        onAccepted: pane.remove()
        contentItem: Label {
            wrapMode: Text.Wrap
            text: qsTr("Delete “%1”? This cannot be undone.").arg(pane.current)
        }
    }
}
