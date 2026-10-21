pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Reckon
import Reckon.Core

// The history: each expression with its result under it, right-aligned,
// or what went wrong. A click on an expression or a result puts it into
// the editor.
ListView {
    id: display

    required property Calculator calculator
    required property font monoFont
    required property font resultFont

    signal insertRequested(string text)

    model: calculator.history
    clip: true
    spacing: 0
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar {}

    function scrollToEnd() {
        // After the delegate has its height.
        Qt.callLater(() => display.positionViewAtEnd())
    }

    Connections {
        target: display.calculator.history
        function onCountChanged() {
            display.scrollToEnd()
        }
    }

    delegate: Item {
        id: row

        required property int index
        required property string expression
        required property string result
        required property string error
        required property int errorPosition
        required property bool isError

        width: ListView.view.width
        implicitHeight: column.implicitHeight + 10
        height: implicitHeight

        Rectangle {
            anchors.fill: parent
            color: display.palette.highlight
            opacity: expressionArea.containsMouse || resultArea.containsMouse ? 0.08 : 0
        }

        Column {
            id: column
            x: 10
            y: 5
            width: parent.width - 20
            spacing: 2

            Text {
                id: expressionText
                width: parent.width
                text: row.expression
                font: display.monoFont
                color: Theme.expression
                wrapMode: Text.WrapAnywhere

                MouseArea {
                    id: expressionArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: mouse => {
                        if (mouse.button === Qt.RightButton)
                            rowMenu.popup()
                        else
                            display.insertRequested(row.expression)
                    }
                }
            }
            Text {
                id: resultText
                width: parent.width
                text: row.isError ? row.error : row.result
                font: row.isError ? display.monoFont : display.resultFont
                color: row.isError ? Theme.error : Theme.result
                horizontalAlignment: Text.AlignRight
                wrapMode: Text.WrapAnywhere

                MouseArea {
                    id: resultArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: row.isError ? Qt.ArrowCursor : Qt.PointingHandCursor
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: mouse => {
                        if (mouse.button === Qt.RightButton)
                            rowMenu.popup()
                        else if (!row.isError)
                            display.insertRequested(row.result)
                    }
                }
            }
        }

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.separator
            visible: row.index < display.count - 1
        }

        Menu {
            id: rowMenu
            MenuItem {
                text: qsTr("Copy expression")
                onTriggered: Clipboard.setText(row.expression)
            }
            MenuItem {
                text: qsTr("Copy result")
                enabled: !row.isError
                onTriggered: Clipboard.setText(row.result)
            }
            MenuItem {
                text: qsTr("Copy both")
                onTriggered: Clipboard.setText(`${row.expression} = ${row.isError ? row.error : row.result}`)
            }
        }
    }

    Label {
        anchors.centerIn: parent
        visible: display.count === 0
        text: qsTr("Type an expression below and press Enter")
        color: Theme.preview
    }
}
