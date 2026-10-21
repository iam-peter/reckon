import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Reckon

ModalDialog {
    id: dialog

    title: qsTr("About Reckon")
    standardButtons: Dialog.Close

    contentItem: ColumnLayout {
        spacing: 8

        Image {
            Layout.alignment: Qt.AlignHCenter
            source: Qt.resolvedUrl("../logo/reckon.svg")
            sourceSize: Qt.size(96, 96)
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: "Reckon " + Application.version
            font.bold: true
            font.pointSize: dialog.font.pointSize + 3
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            Layout.maximumWidth: 360
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("A calculator in the spirit of SpeedCrunch: expressions with a history, "
                       + "variables and your own functions, plus graphs and JavaScript.")
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: "<a href=\"https://github.com/iam-peter/reckon\">github.com/iam-peter/reckon</a>"
            onLinkActivated: link => Qt.openUrlExternally(link)
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("MIT license. Icons by Lucide, fonts Inter and JetBrains Mono.")
            color: Theme.preview
            font.pointSize: dialog.font.pointSize - 1
        }
    }
}
