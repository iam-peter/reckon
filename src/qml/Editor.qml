pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Reckon
import Reckon.Core

// The expression line: syntax colored, with Up and Down through earlier
// expressions and a completer for function and variable names.
Control {
    id: editor

    required property Calculator calculator
    property bool autoComplete: true
    property bool highlight: true
    property alias text: input.text
    readonly property bool completing: completions.visible
    readonly property bool inputFocused: input.activeFocus

    signal accepted(string expression)

    padding: 6
    leftPadding: 10
    rightPadding: 10

    function insert(text: string) {
        input.remove(input.selectionStart, input.selectionEnd)
        input.insert(input.cursorPosition, text)
        input.forceActiveFocus()
    }

    function clear() {
        input.clear()
        historyIndex = -1
    }

    function backspace() {
        if (input.selectedText !== "")
            input.remove(input.selectionStart, input.selectionEnd)
        else if (input.cursorPosition > 0)
            input.remove(input.cursorPosition - 1, input.cursorPosition)
        input.forceActiveFocus()
    }

    function focusInput() {
        input.forceActiveFocus()
    }

    function submit() {
        const expression = input.text.trim()
        if (expression === "")
            return
        editor.accepted(expression)
        input.clear()
        historyIndex = -1
        draft = ""
        completions.close()
    }

    // Up and Down walk the history; the line being typed is kept as the draft.
    property int historyIndex: -1
    property string draft: ""

    function historyStep(delta: int) {
        const items = calculator.history.expressions()
        if (items.length === 0)
            return
        if (historyIndex === -1)
            draft = input.text
        const next = Math.min(Math.max(historyIndex + delta, -1), items.length - 1)
        if (next === historyIndex)
            return
        historyIndex = next
        input.text = next === -1 ? draft : items[next]
        input.cursorPosition = input.text.length
    }

    // The identifier under the cursor, for the completer.
    function currentWord(): string {
        const before = input.text.substring(0, input.cursorPosition)
        const match = before.match(/[A-Za-z_][A-Za-z0-9_]*$/)
        return match ? match[0] : ""
    }

    function updateCompletions() {
        if (!autoComplete || !input.activeFocus) {
            completions.close()
            return
        }
        const word = currentWord()
        if (word.length < 1) {
            completions.close()
            return
        }
        const list = calculator.completions(word)
        if (list.length === 0 || (list.length === 1 && list[0].insert === word)) {
            completions.close()
            return
        }
        completionModel.clear()
        for (const item of list)
            completionModel.append(item)
        completionList.currentIndex = 0
        if (!completions.visible)
            completions.open()
    }

    function acceptCompletion() {
        if (!completions.visible || completionList.currentIndex < 0)
            return false
        const item = completionModel.get(completionList.currentIndex)
        const word = currentWord()
        input.remove(input.cursorPosition - word.length, input.cursorPosition)
        input.insert(input.cursorPosition, item.insert)
        completions.close()
        return true
    }

    background: Rectangle {
        color: editor.palette.base
        border.color: input.activeFocus ? editor.palette.highlight : editor.palette.mid
        border.width: 1
        radius: 3
    }

    contentItem: TextEdit {
        id: input

        font: editor.font
        color: editor.palette.text
        selectionColor: editor.palette.highlight
        selectedTextColor: editor.palette.highlightedText
        selectByMouse: true
        wrapMode: TextEdit.NoWrap
        textFormat: TextEdit.PlainText
        inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText

        ExpressionHighlighter {
            document: input.textDocument
            calculator: editor.calculator
            enabled: editor.highlight
            numberColor: Theme.number
            functionColor: Theme.functionColor
            variableColor: Theme.variable
            operatorColor: Theme.operator
            errorColor: Theme.error
        }

        onTextChanged: {
            // Enter in a TextEdit would add a line; the line stays one.
            if (text.includes("\n")) {
                text = text.replace(/\n/g, "")
                return
            }
            editor.updateCompletions()
        }
        onActiveFocusChanged: if (!activeFocus) completions.close()

        Keys.onPressed: event => {
            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                if (!(completions.visible && event.modifiers === Qt.NoModifier && editor.acceptCompletion()))
                    editor.submit()
                event.accepted = true
            } else if (event.key === Qt.Key_Tab) {
                event.accepted = editor.acceptCompletion()
            } else if (event.key === Qt.Key_Escape) {
                if (completions.visible)
                    completions.close()
                else
                    editor.clear()
                event.accepted = true
            } else if (event.key === Qt.Key_Up) {
                if (completions.visible)
                    completionList.decrementCurrentIndex()
                else
                    editor.historyStep(1)
                event.accepted = true
            } else if (event.key === Qt.Key_Down) {
                if (completions.visible)
                    completionList.incrementCurrentIndex()
                else
                    editor.historyStep(-1)
                event.accepted = true
            }
        }
    }

    Popup {
        id: completions

        y: -height - 2
        width: Math.min(editor.width, 420)
        height: Math.min(completionList.contentHeight + 2, 240)
        padding: 1
        // The editor keeps the focus; the popup only shows.
        focus: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        contentItem: ListView {
            id: completionList
            model: ListModel { id: completionModel }
            clip: true
            keyNavigationWraps: true
            ScrollBar.vertical: ScrollBar {}
            delegate: ItemDelegate {
                id: completionRow
                required property int index
                required property var model
                width: ListView.view.width
                highlighted: ListView.isCurrentItem
                contentItem: Row {
                    spacing: 10
                    Label {
                        text: completionRow.model.text
                        font: editor.font
                        color: completionRow.highlighted ? completionRow.palette.highlightedText : completionRow.palette.text
                    }
                    Label {
                        text: completionRow.model.detail
                        elide: Text.ElideRight
                        color: completionRow.highlighted ? completionRow.palette.highlightedText : Theme.preview
                        width: completionRow.width - x - 20
                    }
                }
                onClicked: {
                    completionList.currentIndex = index
                    editor.acceptCompletion()
                    input.forceActiveFocus()
                }
            }
        }
    }
}
