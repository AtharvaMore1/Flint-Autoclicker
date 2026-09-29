import QtQuick

Rectangle {
    id: control
    property var options: []
    property int currentIndex: 0
    property bool dark: true
    property bool mouseIcons: false
    signal activated(int index)

    implicitWidth: options.length * 84
    implicitHeight: 32
    radius: 7
    antialiasing: true
    color: dark ? "#252628" : "#e9eaed"

    Row {
        anchors.fill: parent
        anchors.margins: 1
        Repeater {
            model: control.options
            delegate: Rectangle {
                id: segment
                required property int index
                required property string modelData
                readonly property bool selected: control.currentIndex === index
                width: (control.width - 2) / control.options.length
                height: control.height - 2
                radius: 6
                antialiasing: true
                color: selected ? (control.dark ? "#38322f" : "#f8e5d6")
                     : hitArea.pressed ? (control.dark ? "#414245" : "#d0d3d8")
                     : hitArea.containsMouse ? (control.dark ? "#333538" : "#dfe1e5")
                     : control.color
                opacity: control.enabled ? 1 : 0.5
                Behavior on color { ColorAnimation { duration: 120 } }

                // Square off the shared edges; only the group's ends are rounded.
                Rectangle { width: parent.width / 2; height: parent.height; anchors.left: parent.left; color: parent.color; visible: segment.index > 0 }
                Rectangle { width: parent.width / 2; height: parent.height; anchors.right: parent.right; color: parent.color; visible: segment.index < control.options.length - 1 }
                Row {
                    anchors.centerIn: parent
                    spacing: 5
                    MouseGlyph {
                        visible: control.mouseIcons
                        width: 18; height: 18
                        buttonPart: segment.index
                        tint: caption.color
                    }
                    Text {
                        id: caption
                        anchors.verticalCenter: parent.verticalCenter
                        text: segment.modelData
                        color: segment.selected ? (control.dark ? "#f1b084" : "#a65320")
                                                : (control.dark ? "#b0b2b6" : "#62666d")
                        font.family: "Segoe UI"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                    }
                }
                MouseArea {
                    id: hitArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onPressed: control.forceActiveFocus()
                    onClicked: control.activated(segment.index)
                }
            }
        }
    }
    Rectangle {
        anchors.fill: parent
        radius: control.radius
        color: "transparent"
        border.width: 1
        border.color: control.dark ? "#4a4c50" : "#c9ccd2"
    }
    // Native rounded rectangles keep the fill and outline on the same geometry.
    // Clip only the shared edges, so the group's outside corners stay rounded.
    Item {
        id: outline
        readonly property bool first: control.currentIndex === 0
        readonly property bool last: control.currentIndex === control.options.length - 1
        readonly property real segmentWidth: (control.width - 2) / Math.max(1, control.options.length)
        visible: control.currentIndex >= 0 && control.currentIndex < control.options.length
        x: first ? 0 : 1 + control.currentIndex * segmentWidth
        width: segmentWidth + (first ? 1 : 0) + (last ? 1 : 0)
        height: control.height
        opacity: control.enabled ? 1 : 0.4
        clip: true
        Rectangle {
            x: outline.first ? 0 : -control.radius
            width: outline.width + (outline.first ? 0 : control.radius) + (outline.last ? 0 : control.radius)
            height: outline.height
            radius: control.radius
            antialiasing: true
            color: "transparent"
            border.width: 1
            border.color: control.dark ? "#a66a45" : "#dba47d"
        }
        Rectangle { visible: !outline.first; width: 1; height: parent.height; color: control.dark ? "#a66a45" : "#dba47d" }
        Rectangle { visible: !outline.last; anchors.right: parent.right; width: 1; height: parent.height; color: control.dark ? "#a66a45" : "#dba47d" }
    }
}
