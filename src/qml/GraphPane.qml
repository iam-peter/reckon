pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Reckon
import Reckon.Core

// Curves y = f(x) over the calculator's variables and functions: a list
// of expressions, each in its color, and the plot under it.
ColumnLayout {
    id: pane

    required property Calculator calculator
    required property font monoFont
    property font uiFont: Application.font
    property alias graph: graph
    // The curve field typed in last, where names from the side panel go.
    property TextField activeField: null

    spacing: 4

    // Into the curve typed in last, or a new curve when there is none.
    function insert(text: string) {
        if (!activeField) {
            addCurve(text, "")
            return
        }
        activeField.remove(activeField.selectionStart, activeField.selectionEnd)
        activeField.insert(activeField.cursorPosition, text)
        activeField.forceActiveFocus()
    }

    ListModel {
        id: curves
    }

    // Every change goes to the settings and the plot.
    function store() {
        const list = []
        for (let i = 0; i < curves.count; ++i) {
            const curve = curves.get(i)
            list.push({expression: curve.expression, color: String(curve.color), visible: curve.visible})
        }
        AppSettings.graph.curves = list
        graph.curves = list
        pane.calculator.setCurveExpressions(list.map(curve => curve.expression))
    }

    // A variable a curve used went: the curve says its last value instead.
    Connections {
        target: pane.calculator
        function onVariableRemoved(name: string, value: string) {
            let changed = false
            for (let i = 0; i < curves.count; ++i) {
                const expression = curves.get(i).expression
                const replaced = pane.calculator.replaceInCurve(expression, name, value)
                if (replaced !== expression) {
                    curves.setProperty(i, "expression", replaced)
                    changed = true
                }
            }
            if (changed)
                pane.store()
        }
        // A variable a curve used has another name.
        function onVariableRenamed(from: string, to: string) {
            let changed = false
            for (let i = 0; i < curves.count; ++i) {
                const expression = curves.get(i).expression
                const renamed = pane.calculator.renameInCurve(expression, from, to)
                if (renamed !== expression) {
                    curves.setProperty(i, "expression", renamed)
                    changed = true
                }
            }
            if (changed)
                pane.store()
        }
    }

    function addCurve(expression: string, color: string) {
        curves.append({
            expression: expression,
            color: color !== "" ? color : String(Theme.curveColor(curves.count)),
            visible: true,
        })
        store()
    }

    function clear() {
        curves.clear()
        store()
    }

    function setColor(index: int, color: string) {
        curves.setProperty(index, "color", color)
        store()
    }

    Component.onCompleted: {
        const stored = AppSettings.graph.curves
        if (stored && stored.length > 0) {
            for (const curve of stored)
                curves.append({expression: curve.expression, color: curve.color, visible: curve.visible !== false})
        } else {
            curves.append({expression: "sin(x)", color: String(Theme.curveColor(0)), visible: true})
        }
        store()
    }

    PanelHeader {
        Layout.fillWidth: true
        title: qsTr("Graph")
        IconButton { iconName: "plus"; tip: qsTr("Add a curve"); onClicked: pane.addCurve("", "") }
        IconButton { iconName: "zoom-in"; tip: qsTr("Zoom in"); onClicked: graph.zoomIn() }
        IconButton { iconName: "zoom-out"; tip: qsTr("Zoom out"); onClicked: graph.zoomOut() }
        IconButton { iconName: "maximize"; tip: qsTr("Standard view"); onClicked: graph.resetView() }
        IconButton {
            iconName: "hash"
            tip: qsTr("Grid")
            checkable: true
            checked: AppSettings.graph.showGrid
            onToggled: AppSettings.graph.showGrid = checked
        }
        IconButton {
            iconName: "copy"
            tip: qsTr("Copy the graph as an image")
            onClicked: graph.grabToImage(result => Clipboard.setImage(result.image))
        }
        IconButton { iconName: "download"; tip: qsTr("Save the graph as an image…"); onClicked: saveDialog.open() }
    }

    ListView {
        id: curveList

        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(contentHeight, 160)
        model: curves
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        delegate: Item {
            id: row

            required property int index
            required property string expression
            required property string color
            required property var model

            readonly property string error: graph.errors[index] ?? ""

            width: ListView.view.width
            implicitHeight: rowLayout.implicitHeight + (error !== "" ? errorLabel.implicitHeight + 2 : 0) + 4
            height: implicitHeight

            RowLayout {
                id: rowLayout
                x: 6
                width: parent.width - 12
                spacing: 6

                Rectangle {
                    width: 16
                    height: 16
                    radius: 8
                    color: row.color
                    border.color: Qt.darker(row.color, 1.3)
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            colorDialog.index = row.index
                            colorDialog.selectedColor = row.color
                            colorDialog.open()
                        }
                    }
                }
                Label {
                    text: "y ="
                    font: pane.monoFont
                    color: Theme.expression
                }
                TextField {
                    id: field
                    Layout.fillWidth: true
                    text: row.expression
                    font: pane.monoFont
                    placeholderText: qsTr("expression in x")
                    onActiveFocusChanged: {
                        if (activeFocus)
                            pane.activeField = field
                    }
                    onEditingFinished: {
                        if (text !== row.expression) {
                            curves.setProperty(row.index, "expression", text)
                            pane.store()
                        }
                    }
                    Keys.onReturnPressed: field.focus = false
                }
                IconButton {
                    iconName: row.model.visible ? "eye" : "eye-off"
                    tip: row.model.visible ? qsTr("Hide") : qsTr("Show")
                    onClicked: {
                        curves.setProperty(row.index, "visible", !row.model.visible)
                        pane.store()
                    }
                }
                IconButton {
                    iconName: "x"
                    tip: qsTr("Remove")
                    onClicked: {
                        curves.remove(row.index)
                        pane.store()
                    }
                }
            }
            Label {
                id: errorLabel
                anchors.top: rowLayout.bottom
                anchors.topMargin: 2
                x: 52
                width: parent.width - 60
                text: row.error
                visible: row.error !== ""
                color: Theme.error
                font.pointSize: pane.uiFont.pointSize - 1
                elide: Text.ElideRight
            }
        }
    }

    GraphView {
        id: graph

        Layout.fillWidth: true
        Layout.fillHeight: true
        calculator: pane.calculator
        showGrid: AppSettings.graph.showGrid
        backgroundColor: Theme.graphBackground
        gridColor: Theme.graphGrid
        axisColor: Theme.graphAxis
        textColor: Theme.graphAxis
        font: pane.uiFont
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: 6
        Layout.rightMargin: 6
        Layout.bottomMargin: 4
        spacing: 6

        component RangeField: TextField {
            required property real value
            property var apply
            Layout.preferredWidth: 84
            text: Number(value.toPrecision(5)).toString()
            font: pane.monoFont
            horizontalAlignment: TextInput.AlignRight
            onEditingFinished: {
                const number = Number(text)
                if (Number.isFinite(number))
                    apply(number)
                text = Qt.binding(() => Number(value.toPrecision(5)).toString())
            }
        }

        Label { text: "x" }
        RangeField { value: graph.xMin; apply: v => graph.setRange(v, graph.xMax, graph.yMin, graph.yMax) }
        Label { text: qsTr("to") }
        RangeField { value: graph.xMax; apply: v => graph.setRange(graph.xMin, v, graph.yMin, graph.yMax) }
        Item { Layout.fillWidth: true }
        Label { text: "y" }
        RangeField { value: graph.yMin; apply: v => graph.setRange(graph.xMin, graph.xMax, v, graph.yMax) }
        Label { text: qsTr("to") }
        RangeField { value: graph.yMax; apply: v => graph.setRange(graph.xMin, graph.xMax, graph.yMin, v) }
    }

    ColorDialog {
        id: colorDialog
        property int index: -1
        onAccepted: pane.setColor(index, String(selectedColor))
    }

    FileDialog {
        id: saveDialog
        title: qsTr("Save the graph")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("PNG images (*.png)")]
        defaultSuffix: "png"
        onAccepted: {
            const path = selectedFile.toString().replace(/^file:\/\//, "")
            graph.grabToImage(result => result.saveToFile(path))
        }
    }
}
