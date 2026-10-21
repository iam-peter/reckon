pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Reckon
import Reckon.Core

// Variables, functions or constants: a search box over a list of names
// with their values or usage. A click inserts the name into the
// expression; the user's own entries can be removed.
ColumnLayout {
    id: list

    required property NameListModel model
    required property font monoFont
    property font uiFont: Application.font
    property bool removable: false
    property string placeholder: qsTr("Search")

    signal insertRequested(string text)
    signal removeRequested(string name)

    spacing: 4

    TextField {
        Layout.fillWidth: true
        Layout.leftMargin: 6
        Layout.rightMargin: 6
        placeholderText: list.placeholder
        text: list.model.filter
        onTextChanged: list.model.filter = text
    }

    ListView {
        id: view

        Layout.fillWidth: true
        Layout.fillHeight: true
        model: list.model
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        section.property: "category"
        section.delegate: Label {
            required property string section
            width: ListView.view.width
            leftPadding: 8
            topPadding: 6
            bottomPadding: 2
            text: section
            font.bold: true
            font.pointSize: list.uiFont.pointSize - 1
            color: Theme.preview
            visible: section !== ""
            height: section !== "" ? implicitHeight : 0
        }

        delegate: ItemDelegate {
            id: row

            required property int index
            required property string name
            required property string value
            required property string description
            required property bool builtin
            required property string insertText

            width: ListView.view.width
            hoverEnabled: true
            onClicked: list.insertRequested(insertText)
            ToolTip.visible: hovered && description !== ""
            ToolTip.text: description
            ToolTip.delay: 700

            contentItem: RowLayout {
                spacing: 8
                Label {
                    text: row.name
                    font: list.monoFont
                    elide: Text.ElideRight
                    Layout.preferredWidth: Math.min(implicitWidth, row.width * 0.5)
                }
                Label {
                    Layout.fillWidth: true
                    text: row.value
                    font: list.monoFont
                    color: Theme.expression
                    horizontalAlignment: Text.AlignRight
                    elide: Text.ElideMiddle
                }
                IconButton {
                    visible: list.removable && !row.builtin
                    opacity: row.hovered ? 1 : 0.35
                    iconName: "x"
                    tip: qsTr("Remove")
                    implicitWidth: 22
                    implicitHeight: 22
                    padding: 2
                    onClicked: list.removeRequested(row.name)
                }
            }
        }

        Label {
            anchors.centerIn: parent
            visible: view.count === 0
            text: list.model.filter !== "" ? qsTr("Nothing matches") : qsTr("Nothing here yet")
            color: Theme.preview
        }
    }
}
