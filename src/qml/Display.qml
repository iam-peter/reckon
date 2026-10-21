pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Reckon
import Reckon.Core

// The history: each entry with its name, $1, $2..., its expression and
// its result under it, right-aligned, or what went wrong. Entries are
// live: a double-click on an expression edits it in place, and every
// entry depending on it follows. A click on a result puts its name into
// the editor, which binds to it; the menu inserts the value instead.
ListView {
    id: display

    required property Calculator calculator
    required property font monoFont
    required property font resultFont

    // The row being edited in place, or -1.
    property int editingRow: -1
    readonly property bool editing: editingRow >= 0

    signal insertRequested(string text)
    // Enter or Escape ended an edit in place.
    signal editingFinished()

    model: calculator.history
    clip: true
    spacing: 0
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar {}

    function scrollToEnd() {
        // After the delegate has its height.
        Qt.callLater(() => display.positionViewAtEnd())
    }

    function edit(row: int) {
        editingRow = row
        positionViewAtIndex(row, ListView.Contain)
    }

    Connections {
        target: display.calculator.history
        function onCountChanged() {
            if (display.editingRow >= display.count)
                display.editingRow = -1
            if (!display.editing)
                display.scrollToEnd()
        }
    }

    delegate: Item {
        id: row

        required property int index
        required property string name
        required property string expression
        required property string result
        required property string error
        required property int errorPosition
        required property bool isError
        required property bool isDefinition
        required property string replaced
        required property int revision

        // A row being removed has index -1, which is no row to edit.
        readonly property bool editing: index >= 0 && display.editingRow === index
        readonly property bool hovered: expressionArea.containsMouse || resultArea.containsMouse

        width: ListView.view.width
        implicitHeight: column.implicitHeight + 10
        height: implicitHeight

        function startEditing() {
            display.edit(index)
        }

        function commit() {
            if (!editing)
                return
            const text = editField.text
            display.editingRow = -1
            if (text.trim() !== expression)
                display.calculator.edit(index, text)
        }

        function cancel() {
            if (editing)
                display.editingRow = -1
        }

        // For the menu, whose items have a `display` of their own.
        function insert(text: string) {
            display.insertRequested(text)
        }

        function remove() {
            display.calculator.removeEntry(index)
        }

        // Each change that reached this entry from elsewhere lights it up.
        onRevisionChanged: flash.restart()

        Rectangle {
            id: highlight
            anchors.fill: parent
            color: display.palette.highlight
            opacity: row.editing ? 0.1 : row.hovered ? 0.06 : 0
        }

        Rectangle {
            id: flashRect
            anchors.fill: parent
            color: display.palette.highlight
            opacity: 0
            SequentialAnimation {
                id: flash
                NumberAnimation { target: flashRect; property: "opacity"; to: 0.28; duration: 90 }
                NumberAnimation { target: flashRect; property: "opacity"; to: 0; duration: 900; easing.type: Easing.OutCubic }
            }
        }

        Text {
            id: nameText
            x: 10
            y: 5 + (expressionText.height - height) / 2
            width: nameWidth.advanceWidth
            text: row.name
            font.family: display.monoFont.family
            font.pointSize: Math.max(7, display.monoFont.pointSize - 2)
            color: Theme.preview
            opacity: row.replaced !== "" ? 0.6 : 1

            // Room for $999 keeps the expressions lined up.
            TextMetrics {
                id: nameWidth
                font: nameText.font
                text: "$999"
            }
        }

        Column {
            id: column
            x: nameText.x + nameText.width + 8
            y: 5
            width: parent.width - x - 10
            spacing: 2

            Text {
                id: expressionText
                width: parent.width
                visible: !row.editing
                text: row.expression
                font: display.monoFont
                color: Theme.expression
                opacity: row.replaced !== "" ? 0.6 : 1
                wrapMode: Text.WrapAnywhere

                MouseArea {
                    id: expressionArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.IBeamCursor
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: mouse => {
                        if (mouse.button === Qt.RightButton)
                            rowMenu.popup()
                    }
                    onDoubleClicked: mouse => {
                        if (mouse.button === Qt.LeftButton)
                            row.startEditing()
                    }
                }
            }

            // Takes the expression's place line for line: the same font and
            // wrapping and no padding, so the row keeps its height; the
            // frame is drawn around it, outside its bounds.
            TextArea {
                id: editField
                objectName: "entryEditor"
                width: parent.width
                visible: row.editing
                font: display.monoFont
                color: Theme.result
                wrapMode: TextEdit.WrapAnywhere
                padding: 0
                leftInset: -4
                rightInset: -4
                topInset: -2
                bottomInset: -2
                selectByMouse: true
                background: Rectangle {
                    color: display.palette.base
                    border.color: display.palette.highlight
                    radius: 3
                }

                onVisibleChanged: {
                    if (!visible)
                        return
                    text = row.expression
                    selectAll()
                    forceActiveFocus()
                }
                // A line of input: Enter keeps the change rather than
                // starting another line.
                Keys.onReturnPressed: event => {
                    row.commit()
                    event.accepted = true
                    display.editingFinished()
                }
                Keys.onEnterPressed: event => {
                    row.commit()
                    event.accepted = true
                    display.editingFinished()
                }
                onActiveFocusChanged: {
                    // Clicking elsewhere keeps the change, as in a spreadsheet.
                    if (!activeFocus)
                        row.commit()
                }
                Keys.onEscapePressed: event => {
                    row.cancel()
                    event.accepted = true
                    display.editingFinished()
                }
            }

            Text {
                id: replacedText
                width: parent.width
                visible: row.replaced !== ""
                text: row.replaced
                font.family: display.monoFont.family
                font.pointSize: Math.max(7, display.monoFont.pointSize - 2)
                font.italic: true
                color: Theme.preview
                wrapMode: Text.WordWrap
            }

            Text {
                id: resultText
                width: parent.width
                // While editing: what the change would give.
                readonly property string pending: row.editing ? display.calculator.preview(editField.text) : ""
                // A space while the change gives nothing yet keeps the line.
                text: row.editing ? (pending !== "" ? pending : "\u00a0") : row.isError ? row.error : row.result
                font: row.isError ? display.monoFont : display.resultFont
                color: row.editing ? Theme.preview : row.isError ? Theme.error : Theme.result
                opacity: row.replaced !== "" && !row.editing ? 0.6 : 1
                horizontalAlignment: Text.AlignRight
                wrapMode: Text.WrapAnywhere

                MouseArea {
                    id: resultArea
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: !row.editing
                    cursorShape: row.isError || row.isDefinition ? Qt.ArrowCursor : Qt.PointingHandCursor
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: mouse => {
                        if (mouse.button === Qt.RightButton)
                            rowMenu.popup()
                        else if (!row.isError && !row.isDefinition)
                            display.insertRequested(row.name)
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
                text: qsTr("Edit")
                onTriggered: row.startEditing()
            }
            MenuItem {
                text: qsTr("Insert %1").arg(row.name)
                enabled: !row.isError && !row.isDefinition
                onTriggered: row.insert(row.name)
            }
            MenuItem {
                text: qsTr("Insert value")
                enabled: !row.isError && !row.isDefinition
                onTriggered: row.insert(row.result)
            }
            MenuItem {
                text: qsTr("Insert expression")
                onTriggered: row.insert(row.expression)
            }
            MenuSeparator {}
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
            MenuSeparator {}
            MenuItem {
                text: qsTr("Delete")
                onTriggered: row.remove()
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
