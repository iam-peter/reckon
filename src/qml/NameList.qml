pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Reckon
import Reckon.Core

// Variables, functions or constants: a search box over a list of names
// with their values or usage. A click inserts the name into the current
// tab's input; the user's own entries can be removed and, with the
// handlers set, added and edited as "name = expression" in one editor
// that says what is wrong while it is typed.
ColumnLayout {
    id: list

    required property NameListModel model
    required property font monoFont
    property font uiFont: Application.font
    property bool removable: false
    // Whether rows have a count of the places using them.
    property bool counted: false
    property string placeholder: qsTr("Search")
    // (text, current) => what is wrong with the editor's text, as a new
    // entry or as the new form of `current`; empty when nothing is.
    property var checkHandler: null
    // text => error: adds an entry.
    property var addHandler: null
    // (name, text) => error: changes the entry `name`.
    property var editHandler: null

    // The editor, and the entry it edits; empty while adding.
    property bool editorOpen: false
    property string editorFor: ""

    signal insertRequested(string text)
    signal removeRequested(string name)
    // Enter or Escape closed the editor: the tab's input is next.
    signal editingFinished()

    spacing: 4

    function openEditor(name: string, text: string) {
        editorFor = name
        editor.applyError = ""
        editor.text = text
        if (editor.visible)
            editor.forceActiveFocus()
        else
            editorOpen = true
    }

    function closeEditor(finished: bool) {
        editorOpen = false
        editorFor = ""
        if (finished)
            editingFinished()
    }

    function apply() {
        const text = editor.text
        if (text.trim() === "") {
            closeEditor(true)
            return
        }
        if (editor.problem !== "")
            return
        const error = editorFor === "" ? addHandler(text) : editHandler(editorFor, text)
        if (error !== "")
            editor.applyError = error
        else
            closeEditor(true)
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: 6
        Layout.rightMargin: 6
        spacing: 4
        TextField {
            Layout.fillWidth: true
            placeholderText: list.placeholder
            text: list.model.filter
            onTextChanged: list.model.filter = text
        }
        IconButton {
            objectName: "addVariable"
            visible: list.addHandler !== null
            iconName: "plus"
            tip: qsTr("Add a variable")
            onClicked: list.openEditor("", "")
        }
    }

    // Enter applies, Escape or clicking elsewhere leaves things as they
    // were; while the text is not acceptable it says why, and Enter waits.
    TextField {
        id: editor
        objectName: "variableEditor"

        // What is wrong with the text, as it is typed; nothing while empty.
        readonly property string problem: visible && list.checkHandler && text.trim() !== ""
                                          ? list.checkHandler(text, list.editorFor) : ""
        // What went wrong applying it, which only shows then.
        property string applyError: ""
        readonly property string message: problem !== "" ? problem : applyError

        Layout.fillWidth: true
        Layout.leftMargin: 6
        Layout.rightMargin: 6
        visible: list.editorOpen
        font: list.monoFont
        placeholderText: qsTr("name = expression")
        selectByMouse: true
        onVisibleChanged: {
            if (visible)
                forceActiveFocus()
        }
        onTextChanged: applyError = ""
        onAccepted: list.apply()
        onActiveFocusChanged: {
            if (!activeFocus && list.editorOpen)
                list.closeEditor(false)
        }
        Keys.onEscapePressed: event => {
            list.closeEditor(true)
            event.accepted = true
        }

        Rectangle {
            anchors.fill: parent
            radius: 2
            color: "transparent"
            border.color: Theme.error
            visible: editor.message !== ""
        }
    }
    Label {
        objectName: "variableProblem"
        Layout.fillWidth: true
        Layout.leftMargin: 8
        Layout.rightMargin: 8
        visible: list.editorOpen && editor.message !== ""
        text: editor.message
        color: Theme.error
        font.pointSize: list.uiFont.pointSize - 1
        wrapMode: Text.WordWrap
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

        // Room for a count of two digits, so values line up.
        TextMetrics {
            id: countWidth
            font.pointSize: list.uiFont.pointSize - 2
            text: "99"
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
            required property int references

            width: ListView.view.width
            hoverEnabled: true
            highlighted: list.editorOpen && list.editorFor === name
            onClicked: list.insertRequested(insertText)
            ToolTip.visible: hovered && description !== ""
            ToolTip.text: description
            ToolTip.delay: 700

            // Every row keeps the room of the count and the buttons, the
            // built-in ones too, so the values form one column.
            contentItem: RowLayout {
                spacing: 4
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
                    // The leading digits say the most.
                    elide: Text.ElideRight
                }
                Item {
                    visible: list.counted
                    implicitWidth: countWidth.advanceWidth + 12
                    implicitHeight: count.implicitHeight
                    // How many entries, functions and curves use it.
                    Label {
                        id: count
                        objectName: "referenceCount"
                        anchors.centerIn: parent
                        visible: row.references > 0
                        text: row.references
                        font.pointSize: list.uiFont.pointSize - 2
                        color: Theme.preview
                        leftPadding: 5
                        rightPadding: 5
                        background: Rectangle {
                            radius: height / 2
                            color: "transparent"
                            border.color: Theme.separator
                        }
                    }
                }
                IconButton {
                    objectName: "editVariable"
                    visible: list.editHandler !== null
                    enabled: !row.builtin
                    opacity: row.builtin ? 0 : row.hovered || row.highlighted ? 1 : 0.35
                    iconName: "pencil"
                    tip: qsTr("Edit")
                    implicitWidth: 22
                    implicitHeight: 22
                    padding: 2
                    onClicked: list.openEditor(row.name, `${row.name} = ${row.editText}`)
                }
                IconButton {
                    objectName: "removeName"
                    visible: list.removable
                    enabled: !row.builtin
                    opacity: row.builtin ? 0 : row.hovered ? 1 : 0.35
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
