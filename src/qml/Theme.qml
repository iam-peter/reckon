pragma Singleton

import QtQuick

// Colors of the display and the editor, for a light or a dark palette.
QtObject {
    id: theme

    readonly property SystemPalette system: SystemPalette {}
    readonly property bool dark: system.window.hslLightness < 0.5

    // The display: what was typed, what came of it, and what went wrong.
    readonly property color expression: dark ? "#b8bcc4" : "#5a5f66"
    readonly property color result: dark ? "#e8ecf2" : "#1f2328"
    readonly property color error: dark ? "#ff7b72" : "#d1242f"
    readonly property color separator: dark ? "#33ffffff" : "#1f000000"
    readonly property color preview: dark ? "#8b949e" : "#6e7781"

    // Syntax colors of the editor.
    readonly property color number: dark ? "#79c0ff" : "#0550ae"
    readonly property color functionColor: dark ? "#d2a8ff" : "#8250df"
    readonly property color variable: dark ? "#7ee787" : "#116329"
    readonly property color operator: dark ? "#a5a5a5" : "#6e6e6e"

    // The graph.
    readonly property color graphBackground: dark ? "#1c1f24" : "#ffffff"
    readonly property color graphGrid: dark ? "#2e333b" : "#e4e4e4"
    readonly property color graphAxis: dark ? "#c9ced6" : "#404040"
    readonly property list<color> curves: [
        "#e4572e", "#1b7ced", "#2a9d3f", "#d6008f", "#f28c00", "#7d3cc6", "#00a3a3", "#8c6d1f"
    ]

    function curveColor(index: int): color {
        return curves[index % curves.length]
    }
}
