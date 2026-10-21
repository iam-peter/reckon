pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Reckon
import Reckon.Core

ApplicationWindow {
    id: root

    // A session file given on the command line.
    property string initialSession: ""
    // The session of last time comes back unless a test says otherwise.
    property bool restoreSession: true
    // Where the scripts are kept; empty for the application data folder.
    property string scriptDirectory: ""

    readonly property font uiFont: Qt.font({family: AppSettings.fonts.uiFamily, pointSize: AppSettings.fonts.uiSize})
    readonly property font monoFont: Qt.font({family: AppSettings.fonts.codeFamily, pointSize: AppSettings.fonts.codeSize})
    readonly property font resultFont: Qt.font({family: AppSettings.fonts.codeFamily, pointSize: AppSettings.fonts.resultSize,
                                                weight: Font.DemiBold})
    readonly property Calculator calculator: calc

    width: AppSettings.window.width
    height: AppSettings.window.height
    minimumWidth: 480
    minimumHeight: 360
    visible: true
    font: uiFont
    title: calc.sessionFile !== "" ? `${sessionName(calc.sessionFile)} — Reckon` : "Reckon"

    function sessionName(path: string): string {
        return path.substring(path.lastIndexOf("/") + 1)
    }

    Calculator {
        id: calc
        angleUnit: AppSettings.calculator.angleUnit
        resultFormat: AppSettings.calculator.resultFormat
        precision: AppSettings.calculator.precision
        groupDigits: AppSettings.calculator.groupDigits
        onEvaluated: ok => {
            if (!ok) {
                const row = history.count - 1
                statusMessage.text = history.data(history.index(row, 0), 260) // ErrorRole
            } else {
                statusMessage.text = ""
            }
        }
    }

    Connections {
        target: calc.script
        function onPlotRequested(expression: string, color: string) {
            graphPane.addCurve(expression, color)
            AppSettings.view.graph = true
            AppSettings.view.toolTab = 0
        }
        function onPlotsCleared() {
            graphPane.clear()
        }
    }

    Component.onCompleted: {
        if (initialSession !== "") {
            if (!calc.loadSession(Qt.resolvedUrl(initialSession).toString().startsWith("file:")
                                  ? Qt.resolvedUrl(initialSession) : "file://" + initialSession))
                statusMessage.text = qsTr("Could not open %1").arg(initialSession)
        } else if (restoreSession && AppSettings.calculator.restoreSession) {
            calc.restoreState()
        }
        if (restoreSession && scriptPane.library.names.includes("startup"))
            calc.script.run(scriptPane.library.read("startup"), "startup")
        editor.focusInput()
    }

    onClosing: {
        scriptPane.flush()
        calc.saveState()
        AppSettings.window.width = width
        AppSettings.window.height = height
    }

    function evaluate(expression: string) {
        calc.evaluate(expression)
    }

    function insert(text: string) {
        editor.insert(text)
    }

    menuBar: MenuBar {
        Menu {
            title: qsTr("&Session")
            Action { text: qsTr("&New"); shortcut: StandardKey.New; onTriggered: calc.newSession() }
            Action { text: qsTr("&Open…"); shortcut: StandardKey.Open; onTriggered: openDialog.open() }
            Action {
                text: qsTr("&Save")
                shortcut: StandardKey.Save
                onTriggered: calc.sessionFile !== "" ? calc.saveSession("file://" + calc.sessionFile) : saveDialog.open()
            }
            Action { text: qsTr("Save &As…"); shortcut: StandardKey.SaveAs; onTriggered: saveDialog.open() }
            MenuSeparator {}
            Action { text: qsTr("&Quit"); shortcut: StandardKey.Quit; onTriggered: root.close() }
        }
        Menu {
            title: qsTr("&Edit")
            Action {
                text: qsTr("Copy &Result")
                shortcut: "Ctrl+R"
                enabled: calc.history.count > 0
                onTriggered: Clipboard.setText(calc.ans)
            }
            Action {
                text: qsTr("Copy &Expression")
                shortcut: "Ctrl+E"
                enabled: calc.history.count > 0
                onTriggered: Clipboard.setText(calc.history.expressionAt(calc.history.count - 1))
            }
            Action { text: qsTr("&Paste"); shortcut: StandardKey.Paste; onTriggered: editor.insert(Clipboard.text()) }
            MenuSeparator {}
            Action { text: qsTr("Clear &History"); onTriggered: calc.clearHistory() }
            Action { text: qsTr("Wipe &Variables"); onTriggered: calc.clearVariables() }
            Action { text: qsTr("Wipe &User Functions"); onTriggered: calc.clearUserFunctions() }
        }
        Menu {
            title: qsTr("&View")
            Action {
                text: qsTr("&Keypad")
                shortcut: "Ctrl+K"
                checkable: true
                checked: AppSettings.view.keypad
                onTriggered: AppSettings.view.keypad = checked
            }
            Action {
                text: qsTr("Side &Panel")
                shortcut: "Ctrl+P"
                checkable: true
                checked: AppSettings.view.sidePanel
                onTriggered: AppSettings.view.sidePanel = checked
            }
            MenuSeparator {}
            Action { text: qsTr("&Variables"); shortcut: "Ctrl+1"; onTriggered: root.showSideTab(0) }
            Action { text: qsTr("&Functions"); shortcut: "Ctrl+2"; onTriggered: root.showSideTab(1) }
            Action { text: qsTr("&Constants"); shortcut: "Ctrl+3"; onTriggered: root.showSideTab(2) }
            Action { text: qsTr("&History"); shortcut: "Ctrl+4"; onTriggered: root.showSideTab(3) }
            Action { text: qsTr("&User Functions"); shortcut: "Ctrl+5"; onTriggered: root.showSideTab(4) }
            MenuSeparator {}
            Action {
                text: qsTr("&Graph")
                shortcut: "Ctrl+G"
                checkable: true
                checked: AppSettings.view.graph
                onTriggered: {
                    AppSettings.view.graph = checked
                    if (checked)
                        AppSettings.view.toolTab = 0
                }
            }
            Action {
                text: qsTr("&Script")
                shortcut: "Ctrl+J"
                checkable: true
                checked: AppSettings.view.script
                onTriggered: {
                    AppSettings.view.script = checked
                    if (checked)
                        AppSettings.view.toolTab = 1
                }
            }
            MenuSeparator {}
            Action {
                text: qsTr("F&ull Screen")
                shortcut: StandardKey.FullScreen
                checkable: true
                checked: root.visibility === Window.FullScreen
                onTriggered: root.visibility = checked ? Window.FullScreen : Window.Windowed
            }
        }
        Menu {
            title: qsTr("Se&ttings")
            Menu {
                title: qsTr("&Angle Unit")
                Action {
                    text: qsTr("&Radians")
                    checkable: true
                    checked: AppSettings.calculator.angleUnit === Calculator.Radians
                    onTriggered: AppSettings.calculator.angleUnit = Calculator.Radians
                }
                Action {
                    text: qsTr("&Degrees")
                    checkable: true
                    checked: AppSettings.calculator.angleUnit === Calculator.Degrees
                    onTriggered: AppSettings.calculator.angleUnit = Calculator.Degrees
                }
            }
            Menu {
                id: formatMenu
                title: qsTr("Result &Format")
                Repeater {
                    model: [qsTr("&General"), qsTr("&Fixed"), qsTr("&Scientific"), qsTr("&Engineering"),
                            qsTr("&Binary"), qsTr("&Octal"), qsTr("&Hexadecimal")]
                    MenuItem {
                        required property int index
                        required property string modelData
                        text: modelData
                        checkable: true
                        checked: AppSettings.calculator.resultFormat === index
                        onTriggered: AppSettings.calculator.resultFormat = index
                    }
                }
            }
            Menu {
                title: qsTr("&Precision")
                Repeater {
                    model: [[-1, qsTr("&Automatic")], [2, qsTr("&2 decimals")], [3, qsTr("&3 decimals")],
                            [4, qsTr("&4 decimals")], [8, qsTr("&8 decimals")], [12, qsTr("1&2 decimals")],
                            [15, qsTr("1&5 decimals")]]
                    MenuItem {
                        required property var modelData
                        text: modelData[1]
                        checkable: true
                        checked: AppSettings.calculator.precision === modelData[0]
                        onTriggered: AppSettings.calculator.precision = modelData[0]
                    }
                }
            }
            Action {
                text: qsTr("Group &Digits")
                checkable: true
                checked: AppSettings.calculator.groupDigits
                onTriggered: AppSettings.calculator.groupDigits = checked
            }
            MenuSeparator {}
            Action { text: qsTr("&Settings…"); shortcut: StandardKey.Preferences; onTriggered: settingsDialog.open() }
        }
        Menu {
            title: qsTr("&Help")
            Action { text: qsTr("&About Reckon"); onTriggered: aboutDialog.open() }
        }
    }

    function showSideTab(index: int) {
        AppSettings.view.sidePanel = true
        AppSettings.view.sideTab = index
    }

    SplitView {
        anchors.fill: parent
        handle: SplitterHandle {}

        SidePanel {
            id: sidePanel
            objectName: "sidePanel"
            visible: AppSettings.view.sidePanel
            SplitView.preferredWidth: AppSettings.view.sideWidth
            SplitView.minimumWidth: 160
            onWidthChanged: if (visible && SplitView.view.resizing) AppSettings.view.sideWidth = width
            calculator: calc
            monoFont: root.monoFont
            uiFont: root.uiFont
            currentIndex: AppSettings.view.sideTab
            onCurrentIndexChanged: AppSettings.view.sideTab = currentIndex
            onInsertRequested: text => root.insert(text)
        }

        ColumnLayout {
            SplitView.fillWidth: true
            SplitView.minimumWidth: 300
            spacing: 4

            Display {
                id: display
                objectName: "display"
                Layout.fillWidth: true
                Layout.fillHeight: true
                calculator: calc
                monoFont: root.monoFont
                resultFont: root.resultFont
                onInsertRequested: text => root.insert(text)
                // Clicks on the display leave the editor in charge.
                TapHandler {
                    onTapped: editor.focusInput()
                }
            }

            Label {
                id: preview
                objectName: "preview"
                Layout.fillWidth: true
                Layout.rightMargin: 10
                visible: AppSettings.calculator.autoCalc && text !== ""
                text: AppSettings.calculator.autoCalc ? calc.preview(editor.text) : ""
                font: root.monoFont
                color: Theme.preview
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideLeft
                // Follows the variables too, which change on each evaluation.
                Connections {
                    target: calc
                    function onEvaluated() { preview.text = Qt.binding(() => AppSettings.calculator.autoCalc ? calc.preview(editor.text) : "") }
                    function onFormatChanged() { preview.text = Qt.binding(() => AppSettings.calculator.autoCalc ? calc.preview(editor.text) : "") }
                }
            }

            Editor {
                id: editor
                objectName: "editor"
                Layout.fillWidth: true
                Layout.leftMargin: 6
                Layout.rightMargin: 6
                calculator: calc
                font: root.monoFont
                autoComplete: AppSettings.calculator.autoComplete
                highlight: AppSettings.calculator.highlight
                onAccepted: expression => root.evaluate(expression)
            }

            Keypad {
                id: keypad
                objectName: "keypad"
                Layout.fillWidth: true
                Layout.leftMargin: 6
                Layout.rightMargin: 6
                Layout.bottomMargin: 6
                visible: AppSettings.view.keypad
                onInsertRequested: text => editor.insert(text)
                onEvaluateRequested: editor.submit()
                onBackspaceRequested: editor.backspace()
                onClearRequested: editor.clear()
            }
        }

        ColumnLayout {
            id: toolPane
            visible: AppSettings.view.graph || AppSettings.view.script
            SplitView.preferredWidth: AppSettings.view.toolWidth
            SplitView.minimumWidth: 260
            onWidthChanged: if (visible && SplitView.view.resizing) AppSettings.view.toolWidth = width
            spacing: 0

            TabBar {
                id: toolTabs
                objectName: "toolTabs"
                Layout.fillWidth: true
                currentIndex: AppSettings.view.toolTab
                onCurrentIndexChanged: AppSettings.view.toolTab = currentIndex
                TabButton {
                    text: qsTr("Graph")
                    enabled: AppSettings.view.graph
                    focusPolicy: Qt.NoFocus
                }
                TabButton {
                    text: qsTr("Script")
                    enabled: AppSettings.view.script
                    focusPolicy: Qt.NoFocus
                }
            }
            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.topMargin: 4
                currentIndex: toolTabs.currentIndex

                GraphPane {
                    id: graphPane
                    objectName: "graphPane"
                    calculator: calc
                    monoFont: root.monoFont
                    uiFont: root.uiFont
                }
                ScriptPane {
                    id: scriptPane
                    objectName: "scriptPane"
                    calculator: calc
                    monoFont: root.monoFont
                    scriptDirectory: root.scriptDirectory
                }
            }
        }
    }

    footer: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 12

            Label {
                id: statusMessage
                objectName: "statusMessage"
                Layout.fillWidth: true
                color: Theme.error
                elide: Text.ElideRight
            }
            ToolButton {
                text: AppSettings.calculator.angleUnit === Calculator.Degrees ? qsTr("Degrees") : qsTr("Radians")
                focusPolicy: Qt.NoFocus
                onClicked: AppSettings.calculator.angleUnit = AppSettings.calculator.angleUnit === Calculator.Degrees
                                                              ? Calculator.Radians : Calculator.Degrees
                ToolTip.text: qsTr("Angle unit of sin, cos and the others; click to switch")
                ToolTip.visible: hovered
                ToolTip.delay: 600
            }
            ToolButton {
                text: [qsTr("General"), qsTr("Fixed"), qsTr("Scientific"), qsTr("Engineering"),
                       qsTr("Binary"), qsTr("Octal"), qsTr("Hexadecimal")][AppSettings.calculator.resultFormat]
                focusPolicy: Qt.NoFocus
                onClicked: formatMenu.popup()
                ToolTip.text: qsTr("Result format")
                ToolTip.visible: hovered
                ToolTip.delay: 600
            }
            Label {
                text: AppSettings.calculator.precision < 0 ? qsTr("Auto") : qsTr("%1 decimals").arg(AppSettings.calculator.precision)
                color: Theme.preview
            }
        }
    }

    FileDialog {
        id: openDialog
        title: qsTr("Open a session")
        nameFilters: [qsTr("Reckon sessions (*.json)"), qsTr("All files (*)")]
        onAccepted: {
            if (!calc.loadSession(selectedFile))
                statusMessage.text = qsTr("Could not open %1").arg(selectedFile)
        }
    }

    FileDialog {
        id: saveDialog
        title: qsTr("Save the session")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("Reckon sessions (*.json)")]
        defaultSuffix: "json"
        onAccepted: {
            if (!calc.saveSession(selectedFile))
                statusMessage.text = qsTr("Could not save %1").arg(selectedFile)
        }
    }

    SettingsDialog {
        id: settingsDialog
    }

    AboutDialog {
        id: aboutDialog
    }
}
