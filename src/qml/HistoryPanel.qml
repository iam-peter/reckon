pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Reckon
import Reckon.Core

// The expressions typed so far, newest at the bottom: a click puts one
// back into the editor.
ColumnLayout {
    id: panel

    required property Calculator calculator
    required property font monoFont
    property font uiFont: Application.font

    signal insertRequested(string text)

    spacing: 4

    TextField {
        id: filter
        Layout.fillWidth: true
        Layout.leftMargin: 6
        Layout.rightMargin: 6
        placeholderText: qsTr("Search")
    }

    ListView {
        id: view

        Layout.fillWidth: true
        Layout.fillHeight: true
        model: panel.calculator.history
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        Connections {
            target: panel.calculator.history
            function onCountChanged() {
                Qt.callLater(() => view.positionViewAtEnd())
            }
        }

        delegate: ItemDelegate {
            id: row

            required property string expression
            required property string result
            required property bool isError

            readonly property bool matches: filter.text === "" || expression.toLowerCase().includes(filter.text.toLowerCase())

            width: ListView.view.width
            height: matches ? implicitHeight : 0
            visible: matches
            onClicked: panel.insertRequested(expression)

            contentItem: Column {
                spacing: 1
                Label {
                    width: parent.width
                    text: row.expression
                    font: panel.monoFont
                    elide: Text.ElideRight
                }
                Label {
                    width: parent.width
                    text: row.isError ? qsTr("error") : row.result
                    font.pointSize: panel.uiFont.pointSize - 1
                    color: row.isError ? Theme.error : Theme.preview
                    elide: Text.ElideRight
                }
            }
        }
    }
}
