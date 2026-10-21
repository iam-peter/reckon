import QtQuick
import QtQuick.Controls

// A key of the keypad: shows `text`, puts `insert` into the expression.
Button {
    id: button

    property string insert: text
    property bool accent: false

    focusPolicy: Qt.NoFocus
    implicitWidth: 40
    implicitHeight: 32
    font.pointSize: 10
    font.bold: accent
}
