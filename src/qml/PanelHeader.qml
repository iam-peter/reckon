import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The strip above a panel: its title on the left, buttons on the right.
RowLayout {
    id: header

    property alias title: label.text
    default property alias buttons: buttonRow.data

    spacing: 4

    Label {
        id: label
        Layout.fillWidth: true
        Layout.leftMargin: 8
        font.bold: true
        elide: Text.ElideRight
    }
    Row {
        id: buttonRow
        spacing: 0
    }
}
