import QtQuick
import QtQuick.Controls.Basic as Basic

Basic.ComboBox {
    id: control
    property bool dark: true
    implicitHeight: 32
    leftPadding: 10
    rightPadding: 28
    font.family: "Segoe UI"
    font.pixelSize: 13
    contentItem: Text {
        text: control.displayText
        font: control.font
        color: control.dark ? "#e9eaeb" : "#222427"
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    indicator: Text {
        x: control.width - 24
        y: (control.height - height) / 2
        text: "⌄"
        font.pixelSize: 18
        color: control.dark ? "#b0b2b6" : "#62666d"
    }
    background: Rectangle {
        radius: 8
        color: control.hovered ? (control.dark ? "#38393c" : "#e7e8eb") : (control.dark ? "#2d2e31" : "#eff0f2")
        border.width: control.activeFocus || control.popup.visible ? 1 : 0
        border.color: "#e99458"
    }
    delegate: Basic.ItemDelegate {
        width: control.width - 8
        text: control.textAt(index)
        highlighted: control.highlightedIndex === index
        implicitHeight: 32
        contentItem: Text {
            text: parent.text
            font: control.font
            verticalAlignment: Text.AlignVCenter
            color: index === control.currentIndex ? (control.dark ? "#f1b084" : "#a65320") : (control.dark ? "#e9eaeb" : "#222427")
        }
        background: Rectangle { radius: 6; color: parent.highlighted ? (control.dark ? "#38322f" : "#f8e5d6") : "transparent" }
    }
    popup: Basic.Popup {
        y: control.height + 4
        width: control.width
        padding: 4
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 340)
        onOpened: { contentItem.positionViewAtBeginning(); contentItem.positionViewAtIndex(control.currentIndex, ListView.Contain) }
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            Basic.ScrollIndicator.vertical: Basic.ScrollIndicator {}
        }
        background: Rectangle { radius: 8; color: control.dark ? "#242528" : "#ffffff"; border.color: control.dark ? "#45474b" : "#d8dae0" }
    }
}
