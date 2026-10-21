pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Reckon
import Reckon.Core

// The lists at the side, one tab each: variables, functions, constants,
// the history and the user's functions.
ColumnLayout {
    id: panel

    required property Calculator calculator
    required property font monoFont
    property font uiFont: Application.font
    property alias currentIndex: tabs.currentIndex

    signal insertRequested(string text)
    signal editingFinished()

    spacing: 0

    TabBar {
        id: tabs

        Layout.fillWidth: true

        component Tab: TabButton {
            property string iconName
            property string tip
            icon.source: Icons.source(iconName)
            icon.color: palette.buttonText
            icon.width: 16
            icon.height: 16
            display: AbstractButton.IconOnly
            width: implicitWidth
            focusPolicy: Qt.NoFocus
            ToolTip.text: tip
            ToolTip.visible: hovered
            ToolTip.delay: 600
        }

        Tab { iconName: "variable"; tip: qsTr("Variables") }
        Tab { iconName: "square-function"; tip: qsTr("Functions") }
        Tab { iconName: "book-open"; tip: qsTr("Constants") }
        Tab { iconName: "history"; tip: qsTr("History") }
        Tab { iconName: "braces"; tip: qsTr("User functions") }
    }

    StackLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.topMargin: 6
        currentIndex: tabs.currentIndex

        NameList {
            model: panel.calculator.variables
            monoFont: panel.monoFont
            uiFont: panel.uiFont
            removable: true
            counted: true
            checkHandler: (text, current) => panel.calculator.checkVariable(text, current)
            addHandler: text => panel.calculator.addVariable(text)
            editHandler: (name, text) => panel.calculator.changeVariable(name, text)
            onEditingFinished: panel.editingFinished()
            placeholder: qsTr("Search variables")
            onInsertRequested: text => panel.insertRequested(text)
            onRemoveRequested: name => panel.calculator.removeVariable(name)
        }
        NameList {
            model: panel.calculator.functions
            monoFont: panel.monoFont
            uiFont: panel.uiFont
            placeholder: qsTr("Search functions")
            onInsertRequested: text => panel.insertRequested(text)
        }
        NameList {
            model: panel.calculator.constants
            monoFont: panel.monoFont
            uiFont: panel.uiFont
            placeholder: qsTr("Search constants")
            onInsertRequested: text => panel.insertRequested(text)
        }
        HistoryPanel {
            calculator: panel.calculator
            monoFont: panel.monoFont
            uiFont: panel.uiFont
            onInsertRequested: text => panel.insertRequested(text)
        }
        NameList {
            model: panel.calculator.userFunctions
            monoFont: panel.monoFont
            uiFont: panel.uiFont
            removable: true
            placeholder: qsTr("Search your functions")
            onInsertRequested: text => panel.insertRequested(text)
            onRemoveRequested: name => panel.calculator.removeUserFunction(name)
        }
    }
}
