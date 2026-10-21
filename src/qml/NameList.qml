pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Reckon
import Reckon.Core

// Variables, functions or constants: a search box over a list of names
// with their values or usage. A click inserts the name into the current
// tab's input; the user's own entries can be removed, and with an
// editHandler their values edited in place.
ColumnLayout {
    id: list

    required property NameListModel model
    required property font monoFont
    property font uiFont: Application.font
    property bool removable: false
    // (name, text) => error: makes the user's entries editable, the
    // handler applying the new text and saying what is wrong with it.
    property var editHandler: null
    property string placeholder: qsTr("Search")
    // The entry being edited, by name, which survives the list changing.
    property string editingName: ""
    property string editError: ""

    function startEditing(name: string) {
        editError = ""
        editingName = name
    }

    function commit(name: string, text: string) {
        const error = editHandler(name, text)
        if (error !== "") {
            editError = error
            return
        }
        editingName = ""
        editError = ""
    }

    function cancel() {
        editingName = ""
        editError = ""
    }

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
            required property string editText

            readonly property bool editing: list.editingName !== "" && list.editingName === name

            width: ListView.view.width
            hoverEnabled: true
            onClicked: {
                if (!editing)
                    list.insertRequested(insertText)
            }
            ToolTip.visible: hovered && description !== ""
            ToolTip.text: description
            ToolTip.delay: 700

            contentItem: ColumnLayout {
                spacing: 2
                RowLayout {
                    spacing: 8
                    Label {
                        text: row.name
                        font: list.monoFont
                        elide: Text.ElideRight
                        Layout.preferredWidth: Math.min(implicitWidth, row.width * 0.5)
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: !row.editing
                        text: row.value
                        font: list.monoFont
                        color: Theme.expression
                        horizontalAlignment: Text.AlignRight
                        elide: Text.ElideMiddle
                    }
                    // Enter applies the change, Escape or clicking elsewhere
                    // leaves the value as it was.
                    TextField {
                        id: editField
                        objectName: "variableEditor"
                        Layout.fillWidth: true
                        visible: row.editing
                        font: list.monoFont
                        topPadding: 2
                        bottomPadding: 2
                        selectByMouse: true
                        onVisibleChanged: {
                            if (!visible)
                                return
                            text = row.editText
                            selectAll()
                            forceActiveFocus()
                        }
                        onAccepted: list.commit(row.name, text)
                        onActiveFocusChanged: {
                            if (!activeFocus && row.editing)
                                list.cancel()
                        }
                        Keys.onEscapePressed: event => {
                            list.cancel()
                            event.accepted = true
                        }
                    }
                    IconButton {
                        objectName: "editVariable"
                        visible: list.editHandler !== null && !row.builtin && !row.editing
                        opacity: row.hovered ? 1 : 0.35
                        iconName: "pencil"
                        tip: qsTr("Edit")
                        implicitWidth: 22
                        implicitHeight: 22
                        padding: 2
                        onClicked: list.startEditing(row.name)
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
                Label {
                    Layout.fillWidth: true
                    visible: row.editing && list.editError !== ""
                    text: list.editError
                    color: Theme.error
                    font.pointSize: list.uiFont.pointSize - 1
                    wrapMode: Text.WordWrap
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
