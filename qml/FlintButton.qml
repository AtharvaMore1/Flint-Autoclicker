import QtQuick

Rectangle {
    id: button
    property string label: ""
    property int labelSize: 13
    property bool dark: true
    property bool selected: false
    property bool primary: false
    property bool danger: false
    property bool quiet: false
    property bool destructiveHover: false
    readonly property bool hovered: enabled && mouse.containsMouse
    readonly property bool pressed: enabled && mouse.pressed
    signal clicked()

    implicitWidth: Math.max(76, content.implicitWidth + 26)
    implicitHeight: 36
    radius: 8
    antialiasing: true
    color: !enabled ? (dark ? "#242528" : "#e8e9ec")
           : destructiveHover && hovered ? (pressed ? "#a92318" : "#c42b1c")
           : danger ? (mouse.containsMouse ? "#e9776b" : "#d65f55")
           : primary ? (mouse.containsMouse ? "#f3a367" : "#e99458")
           : selected ? (dark ? "#38322f" : "#f8e5d6")
           : quiet ? (pressed ? (dark ? "#45484e" : "#d5d8de")
                      : hovered ? (dark ? "#36383c" : "#e5e7eb")
                      : "transparent")
           : mouse.containsMouse ? (dark ? "#38393c" : "#e7e8eb")
           : (dark ? "#2d2e31" : "#eff0f2")
    border.width: selected && !primary ? 1 : 0
    border.color: dark ? "#a66a45" : "#dba47d"
    Behavior on color { ColorAnimation { duration: 150 } }

    Text {
        id: content
        anchors.centerIn: parent
        text: button.label
        color: !button.enabled ? (button.dark ? "#77797d" : "#a0a2a8")
             : button.destructiveHover && button.hovered ? "#ffffff"
             : button.danger ? "#ffffff"
             : button.primary ? "#1b1410"
             : button.selected ? (button.dark ? "#f1b084" : "#a65320")
             : (button.dark ? "#e7e7e8" : "#27292d")
        font.family: "Segoe UI"
        font.pixelSize: button.labelSize
        font.weight: Font.DemiBold
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: parent.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        enabled: parent.enabled
        onPressed: button.forceActiveFocus()
        onClicked: button.clicked()
    }
}
