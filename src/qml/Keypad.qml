import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Keys to click instead of typing: digits, operators and the common
// functions, which go into the expression at the cursor.
GridLayout {
    id: keypad

    signal insertRequested(string text)
    signal evaluateRequested()
    signal backspaceRequested()
    signal clearRequested()

    columns: 8
    rowSpacing: 3
    columnSpacing: 3

    component Key: KeypadButton {
        Layout.fillWidth: true
        Layout.preferredWidth: 40
        onClicked: keypad.insertRequested(insert)
    }

    Key { text: "7" }
    Key { text: "8" }
    Key { text: "9" }
    Key { text: "÷"; insert: "/" }
    Key { text: "(" }
    Key { text: ")" }
    KeypadButton { Layout.fillWidth: true; Layout.preferredWidth: 40; text: "⌫"; onClicked: keypad.backspaceRequested() }
    KeypadButton { Layout.fillWidth: true; Layout.preferredWidth: 40; text: "C"; onClicked: keypad.clearRequested() }

    Key { text: "4" }
    Key { text: "5" }
    Key { text: "6" }
    Key { text: "×"; insert: "*" }
    Key { text: "xʸ"; insert: "^" }
    Key { text: "√"; insert: "sqrt(" }
    Key { text: "sin"; insert: "sin(" }
    Key { text: "cos"; insert: "cos(" }

    Key { text: "1" }
    Key { text: "2" }
    Key { text: "3" }
    Key { text: "−"; insert: "-" }
    Key { text: "n!"; insert: "!" }
    Key { text: "ln"; insert: "ln(" }
    Key { text: "tan"; insert: "tan(" }
    Key { text: "log"; insert: "log(" }

    Key { text: "0" }
    Key { text: "." }
    Key { text: "e" }
    Key { text: "+" }
    Key { text: "π"; insert: "pi" }
    Key { text: "ans" }
    Key { text: "exp"; insert: "exp(" }
    KeypadButton {
        Layout.fillWidth: true
        Layout.preferredWidth: 40
        text: "="
        accent: true
        onClicked: keypad.evaluateRequested()
    }
}
