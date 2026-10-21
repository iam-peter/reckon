pragma Singleton

import QtQuick
import QtCore

// Preferences, kept between sessions.
QtObject {
    id: root

    readonly property Settings calculator: Settings {
        category: "calculator"

        property int angleUnit: 0 // Calculator.Radians
        property int resultFormat: 0 // Calculator.General
        property int precision: -1
        property bool groupDigits: false
        property bool autoCalc: true
        property bool autoComplete: true
        property bool highlight: true
        property bool restoreSession: true
    }

    readonly property Settings view: Settings {
        category: "view"

        property bool keypad: false
        property bool sidePanel: true
        property int sideTab: 0
        property int page: 0 // the tab: calculator, graph, script
        property real sideWidth: 300
    }

    readonly property Settings fonts: Settings {
        category: "fonts"

        property string uiFamily: "Inter"
        property real uiSize: 10
        property string codeFamily: "JetBrains Mono"
        property real codeSize: 11
        property real resultSize: 13
    }

    readonly property Settings graph: Settings {
        category: "graph"

        property var curves: []
        property bool showGrid: true
    }

    readonly property Settings script: Settings {
        category: "script"

        property string current: ""
    }

    readonly property Settings window: Settings {
        category: "window"

        property int width: 900
        property int height: 720
    }
}
