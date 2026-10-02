import QtQuick

Item {
    id: tabs
    property bool dark: true
    property int currentIndex: 0
    property bool autoclickerEnabled: true
    property bool macroEnabled: true
    signal selected(int index)

    readonly property int leftInset: 14
    readonly property int gap: 8
    readonly property int autoclickerWidth: autoclickerLabel.implicitWidth + 34
    readonly property int macroWidth: macroLabel.implicitWidth + 34
    readonly property int selectedX: currentIndex === 0 ? leftInset : leftInset + autoclickerWidth + gap
    readonly property int selectedWidth: currentIndex === 0 ? autoclickerWidth : macroWidth
    readonly property color contentColor: dark ? "#171819" : "#f5f6f7"
    readonly property color accentColor: dark ? "#a66a45" : "#dba47d"
    implicitHeight: 34

    Canvas {
        id: outline
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            const left = tabs.selectedX
            const right = left + tabs.selectedWidth
            const baseline = height - 0.5
            const top = 1
            ctx.beginPath()
            ctx.moveTo(1, baseline)
            ctx.lineTo(left - 12, baseline)
            ctx.quadraticCurveTo(left, baseline, left, baseline - 12)
            ctx.lineTo(left, top + 9)
            ctx.quadraticCurveTo(left, top, left + 9, top)
            ctx.lineTo(right - 9, top)
            ctx.quadraticCurveTo(right, top, right, top + 9)
            ctx.lineTo(right, baseline - 12)
            ctx.quadraticCurveTo(right, baseline, right + 12, baseline)
            ctx.lineTo(width - 1, baseline)
            ctx.fillStyle = tabs.contentColor
            ctx.fill()
            ctx.lineWidth = 1
            ctx.strokeStyle = tabs.accentColor
            ctx.stroke()
        }
    }
    onDarkChanged: outline.requestPaint()
    onCurrentIndexChanged: outline.requestPaint()
    onWidthChanged: outline.requestPaint()
    onSelectedXChanged: outline.requestPaint()
    onSelectedWidthChanged: outline.requestPaint()

    Rectangle {
        x: tabs.leftInset
        y: tabs.currentIndex === 0 ? 0 : 6
        width: tabs.autoclickerWidth
        height: tabs.currentIndex === 0 ? parent.height : parent.height - 12
        radius: tabs.currentIndex === 0 ? 0 : 8
        color: tabs.currentIndex === 0 ? "transparent"
             : autoclickerMouse.containsMouse && tabs.autoclickerEnabled ? (tabs.dark ? "#38393c" : "#e7e8eb")
             : (tabs.dark ? "#2d2e31" : "#eff0f2")
        Text {
            id: autoclickerLabel
            anchors.centerIn: parent
            text: "Autoclicker"
            color: !tabs.autoclickerEnabled ? (tabs.dark ? "#77797d" : "#a0a2a8")
                 : tabs.currentIndex === 0 ? (tabs.dark ? "#f1b084" : "#a65320")
                 : (tabs.dark ? "#e7e7e8" : "#27292d")
            font.family: "Segoe UI"
            font.pixelSize: 13
            font.weight: Font.DemiBold
        }
        MouseArea {
            id: autoclickerMouse
            anchors.fill: parent
            hoverEnabled: true
            enabled: tabs.autoclickerEnabled
            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onPressed: tabs.forceActiveFocus()
            onClicked: tabs.selected(0)
        }
    }

    Rectangle {
        x: tabs.leftInset + tabs.autoclickerWidth + tabs.gap
        y: tabs.currentIndex === 1 ? 0 : 6
        width: tabs.macroWidth
        height: tabs.currentIndex === 1 ? parent.height : parent.height - 12
        radius: tabs.currentIndex === 1 ? 0 : 8
        color: tabs.currentIndex === 1 ? "transparent"
             : macroMouse.containsMouse && tabs.macroEnabled ? (tabs.dark ? "#38393c" : "#e7e8eb")
             : (tabs.dark ? "#2d2e31" : "#eff0f2")
        Text {
            id: macroLabel
            anchors.centerIn: parent
            text: "Macro"
            color: !tabs.macroEnabled ? (tabs.dark ? "#77797d" : "#a0a2a8")
                 : tabs.currentIndex === 1 ? (tabs.dark ? "#f1b084" : "#a65320")
                 : (tabs.dark ? "#e7e7e8" : "#27292d")
            font.family: "Segoe UI"
            font.pixelSize: 13
            font.weight: Font.DemiBold
        }
        MouseArea {
            id: macroMouse
            anchors.fill: parent
            hoverEnabled: true
            enabled: tabs.macroEnabled
            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onPressed: tabs.forceActiveFocus()
            onClicked: tabs.selected(1)
        }
    }
}
