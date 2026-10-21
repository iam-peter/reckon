pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Reckon
import Reckon.Core

ModalDialog {
    id: dialog

    width: 500
    title: qsTr("Settings")
    standardButtons: Dialog.Close

    contentItem: ColumnLayout {
        spacing: 8

        Label {
            text: qsTr("Results")
            font.bold: true
        }
        GridLayout {
            columns: 2
            columnSpacing: 12

            Label { text: qsTr("Angle unit") }
            ComboBox {
                Layout.fillWidth: true
                model: [qsTr("Radians"), qsTr("Degrees")]
                currentIndex: AppSettings.calculator.angleUnit
                onActivated: index => AppSettings.calculator.angleUnit = index
            }
            Label { text: qsTr("Format") }
            ComboBox {
                Layout.fillWidth: true
                model: [qsTr("General"), qsTr("Fixed"), qsTr("Scientific"), qsTr("Engineering"),
                        qsTr("Binary"), qsTr("Octal"), qsTr("Hexadecimal")]
                currentIndex: AppSettings.calculator.resultFormat
                onActivated: index => AppSettings.calculator.resultFormat = index
            }
            Label { text: qsTr("Decimals") }
            ComboBox {
                Layout.fillWidth: true
                readonly property list<int> values: [-1, 2, 3, 4, 8, 12, 15]
                model: [qsTr("Automatic"), "2", "3", "4", "8", "12", "15"]
                currentIndex: Math.max(0, values.indexOf(AppSettings.calculator.precision))
                onActivated: index => AppSettings.calculator.precision = values[index]
            }
        }
        CheckBox {
            text: qsTr("Group digits in threes")
            checked: AppSettings.calculator.groupDigits
            onToggled: AppSettings.calculator.groupDigits = checked
        }

        Label {
            text: qsTr("Editor")
            font.bold: true
            Layout.topMargin: 8
        }
        CheckBox {
            text: qsTr("Show the result while typing")
            checked: AppSettings.calculator.autoCalc
            onToggled: AppSettings.calculator.autoCalc = checked
        }
        CheckBox {
            text: qsTr("Complete names of functions and variables")
            checked: AppSettings.calculator.autoComplete
            onToggled: AppSettings.calculator.autoComplete = checked
        }
        CheckBox {
            text: qsTr("Color the expression")
            checked: AppSettings.calculator.highlight
            onToggled: AppSettings.calculator.highlight = checked
        }
        CheckBox {
            text: qsTr("Restore the session at startup")
            checked: AppSettings.calculator.restoreSession
            onToggled: AppSettings.calculator.restoreSession = checked
        }

        Label {
            text: qsTr("Fonts")
            font.bold: true
            Layout.topMargin: 8
        }
        GridLayout {
            columns: 3
            columnSpacing: 12

            Label { text: qsTr("Interface") }
            ComboBox {
                Layout.fillWidth: true
                model: Qt.fontFamilies()
                currentIndex: Math.max(0, model.indexOf(AppSettings.fonts.uiFamily))
                onActivated: index => AppSettings.fonts.uiFamily = model[index]
            }
            SpinBox {
                from: 6
                to: 30
                value: AppSettings.fonts.uiSize
                onValueModified: AppSettings.fonts.uiSize = value
            }
            Label { text: qsTr("Expressions") }
            ComboBox {
                Layout.fillWidth: true
                model: Qt.fontFamilies()
                currentIndex: Math.max(0, model.indexOf(AppSettings.fonts.codeFamily))
                onActivated: index => AppSettings.fonts.codeFamily = model[index]
            }
            SpinBox {
                from: 6
                to: 30
                value: AppSettings.fonts.codeSize
                onValueModified: AppSettings.fonts.codeSize = value
            }
            Label { text: qsTr("Results") }
            Item { Layout.fillWidth: true }
            SpinBox {
                from: 6
                to: 40
                value: AppSettings.fonts.resultSize
                onValueModified: AppSettings.fonts.resultSize = value
            }
        }
        Button {
            text: qsTr("Defaults: Inter and JetBrains Mono")
            onClicked: {
                AppSettings.fonts.uiFamily = "Inter"
                AppSettings.fonts.uiSize = 10
                AppSettings.fonts.codeFamily = "JetBrains Mono"
                AppSettings.fonts.codeSize = 11
                AppSettings.fonts.resultSize = 13
            }
        }
    }
}
