import QtQuick

Rectangle {
    property bool dark: true
    radius: 12
    color: dark ? "#202123" : "#ffffff"
    border.width: 1
    border.color: dark ? "#303134" : "#e6e7ea"
    Behavior on color { ColorAnimation { duration: 180 } }
    Behavior on border.color { ColorAnimation { duration: 180 } }
}
