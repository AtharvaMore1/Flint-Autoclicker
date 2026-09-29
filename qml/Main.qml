import QtQuick
import QtQuick.Controls.Basic as Basic

Basic.ApplicationWindow {
    id: root
    width: 400
    height: desiredHeight
    minimumWidth: 400
    minimumHeight: desiredHeight
    maximumWidth: 400
    maximumHeight: desiredHeight
    visible: false
    title: "FlintAutoClicker"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "transparent"
    background: Rectangle {
        radius: 10
        color: backend.dark ? "#171819" : "#f5f6f7"
        Behavior on color { ColorAnimation { duration: 180 } }
    }

    property int activeTab: 0
    onClosing: if (macro.busy) macro.stop()
    Connections {
        target: macro
        function onStateChanged() { if (macro.busy) root.activeTab = 1 }
    }
    readonly property int desiredHeight: backend.action === 3 ? 756 : 720
    readonly property color ink: backend.dark ? "#e9eaeb" : "#222427"
    readonly property color muted: backend.dark ? "#a0a2a5" : "#676b70"
    readonly property bool mouseAction: backend.action !== 3 || backend.customIsMouse
    Behavior on color { ColorAnimation { duration: 180 } }

    Rectangle {
        anchors.fill: parent
        z: 100
        color: "transparent"
        border.width: 1
        border.color: backend.dark ? "#38393c" : "#dedfe2"
        radius: 10
    }

    Rectangle {
        id: topbar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 72
        radius: 10
        color: backend.dark ? "#1e1f21" : "#ffffff"

        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 10; color: parent.color }
        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: backend.dark ? "#323336" : "#e8e9eb" }
        MouseArea { anchors.fill: parent; onPressed: { root.contentItem.forceActiveFocus(); root.startSystemMove() } }

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 18
            anchors.top: parent.top
            anchors.topMargin: 10
            text: "FlintAutoClicker"
            color: root.ink
            font.family: "Segoe UI"
            font.pixelSize: 15
            font.weight: Font.Bold
        }
        Row {
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.top: parent.top
            anchors.topMargin: 5
            spacing: 4
            FlintButton { width: 34; height: 30; label: backend.dark ? "☀" : "☾"; dark: backend.dark; quiet: true; onClicked: backend.dark = !backend.dark }
            FlintButton { width: 34; height: 30; label: "−"; dark: backend.dark; quiet: true; onClicked: root.showMinimized() }
            FlintButton { width: 34; height: 30; label: "×"; dark: backend.dark; quiet: true; destructiveHover: true; onClicked: root.close() }
        }
        Row {
            anchors.left: parent.left
            anchors.leftMargin: 14
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 4
            spacing: 6
            FlintButton { height: 28; label: "Autoclicker"; selected: root.activeTab === 0; dark: backend.dark; enabled: !macro.busy; onClicked: root.activeTab = 0 }
            FlintButton { height: 28; label: "Macro"; selected: root.activeTab === 1; dark: backend.dark; enabled: !backend.running; onClicked: root.activeTab = 1 }
        }
        FlintButton {
            anchors.right: parent.right
            anchors.rightMargin: 14
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 4
            height: 28
            label: "Reset defaults"
            visible: root.activeTab === 0
            quiet: true
            dark: backend.dark
            enabled: !backend.running && !macro.busy
            onClicked: backend.resetDefaults()
        }
    }

    Item {
        id: body
        anchors.top: topbar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 12
        visible: root.activeTab === 0

        TapHandler {
            onTapped: function(point) {
                const focused = root.activeFocusItem
                if (!focused || focused === root.contentItem) return
                const local = focused.mapFromItem(body, point.position.x, point.position.y)
                if (local.x < 0 || local.y < 0 || local.x >= focused.width || local.y >= focused.height)
                    root.contentItem.forceActiveFocus()
            }
        }

        Column {
            id: stack
            width: body.width
            spacing: 8

            FlintCard {
                width: parent.width
                height: backend.action === 3 ? 196 : 160
                dark: backend.dark
                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 6
                    Text { text: "Action"; color: root.ink; font.pixelSize: 14; font.weight: Font.Bold }
                    Row {
                        width: parent.width
                        height: 34
                        spacing: 6
                        SegmentedControl {
                            width: parent.width - 82; height: 34
                            options: ["Left", "Middle", "Right"]
                            mouseIcons: true
                            currentIndex: backend.action < 3 ? backend.action : -1
                            dark: backend.dark; enabled: !backend.running
                            onActivated: function(index) { backend.action = index }
                        }
                        FlintButton { width: 76; height: 34; label: "Custom"; selected: backend.action === 3; dark: backend.dark; enabled: !backend.running; onClicked: backend.action = 3 }
                    }
                    Row {
                        width: parent.width
                        height: 30
                        visible: backend.action === 3
                        Text { width: parent.width - customButton.width; anchors.verticalCenter: parent.verticalCenter; text: "Key or mouse button"; color: root.muted; font.pixelSize: 13 }
                        FlintButton {
                            id: customButton
                            height: 30
                            label: backend.captureTarget === "action" ? "Press input…" : backend.actionShortcut
                            dark: backend.dark
                            enabled: !backend.running
                            onClicked: backend.beginActionCapture()
                        }
                    }
                    Row {
                        width: parent.width
                        height: 30
                        Text { width: parent.width - hotkeyButton.width; anchors.verticalCenter: parent.verticalCenter; text: "Start / stop hotkey"; color: root.muted; font.pixelSize: 13 }
                        FlintButton {
                            id: hotkeyButton
                            height: 30
                            label: backend.captureTarget === "hotkey" ? "Press a key…" : backend.hotkeyShortcut
                            dark: backend.dark
                            enabled: !backend.running
                            onClicked: backend.beginHotkeyCapture()
                        }
                    }
                    Row {
                        width: parent.width
                        height: 30
                        Text { width: parent.width - modeButtons.width; anchors.verticalCenter: parent.verticalCenter; text: "Hotkey mode"; color: root.muted; font.pixelSize: 13 }
                        SegmentedControl {
                            id: modeButtons
                            width: 150
                            height: 30
                            options: ["Toggle", "Hold"]
                            currentIndex: backend.holdToClick ? 1 : 0
                            dark: backend.dark; enabled: !backend.running
                            onActivated: function(index) { backend.holdToClick = index === 1 }
                        }
                    }
                }
            }

            FlintCard {
                width: parent.width
                height: 118
                dark: backend.dark
                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 6
                    Text { text: "Timing"; color: root.ink; font.pixelSize: 14; font.weight: Font.Bold }
                    Item {
                        width: parent.width
                        height: 30
                        Text { anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; text: "Interval"; color: root.muted; font.pixelSize: 13 }
                        Text { id: intervalUnit; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; width: 32; text: "ms"; color: root.muted; font.pixelSize: 13 }
                        NumberField { anchors.right: intervalUnit.left; anchors.rightMargin: 6; width: 104; height: 30; from: 1; to: 3600000; value: backend.intervalMs; dark: backend.dark; enabled: !backend.running; onValueModified: backend.intervalMs = value }
                    }
                    Item {
                        width: parent.width
                        height: 30
                        Text { anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; text: "Random offset"; color: root.muted; font.pixelSize: 13 }
                        FlintButton { id: offsetToggle; anchors.right: parent.right; width: 56; height: 30; label: backend.randomOffset ? "On" : "Off"; selected: backend.randomOffset; dark: backend.dark; enabled: !backend.running; onClicked: backend.randomOffset = !backend.randomOffset }
                        Text { id: offsetUnit; anchors.right: offsetToggle.left; anchors.rightMargin: 6; anchors.verticalCenter: parent.verticalCenter; width: 38; text: "± ms"; color: root.muted; font.pixelSize: 13 }
                        NumberField { anchors.right: offsetUnit.left; anchors.rightMargin: 6; width: 96; height: 30; from: 0; to: Math.max(0, backend.intervalMs - 1); value: backend.offsetMs; dark: backend.dark; enabled: backend.randomOffset && !backend.running; onValueModified: backend.offsetMs = value }
                    }
                }
            }

            FlintCard {
                width: parent.width
                height: 84
                dark: backend.dark
                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 6
                    Text { text: "Repeat"; color: root.ink; font.pixelSize: 14; font.weight: Font.Bold }
                    Item {
                        width: parent.width
                        height: 32
                        SegmentedControl {
                            anchors.left: parent.left; width: parent.width - 116; height: 32
                            options: ["Until stopped", "Set count"]
                            currentIndex: backend.repeatForever ? 0 : 1
                            dark: backend.dark; enabled: !backend.running
                            onActivated: function(index) { backend.repeatForever = index === 0 }
                        }
                        NumberField { anchors.right: parent.right; width: 108; height: 32; from: 1; to: 100000000; value: backend.repeatCount; dark: backend.dark; enabled: !backend.repeatForever && !backend.running; onValueModified: backend.repeatCount = value }
                    }
                }
            }

            FlintCard {
                width: parent.width
                height: 114
                dark: backend.dark
                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 5
                    Item {
                        width: parent.width
                        height: 30
                        Text { anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; text: "Fixed Click Location"; color: root.ink; font.pixelSize: 14; font.weight: Font.Bold }
                        FlintButton { anchors.right: parent.right; width: 56; height: 30; label: backend.fixedLocation ? "On" : "Off"; selected: backend.fixedLocation; dark: backend.dark; enabled: !backend.running && root.mouseAction; onClicked: backend.fixedLocation = !backend.fixedLocation }
                    }
                    Text { text: root.mouseAction ? "Use the cursor or choose a fixed screen position." : "Keyboard input goes to the focused app."; color: root.muted; font.pixelSize: 12 }
                    Item {
                        width: parent.width
                        height: 30
                        Text { anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; text: "X  " + backend.locationX + "     Y  " + backend.locationY; color: backend.fixedLocation && root.mouseAction ? root.ink : root.muted; opacity: backend.fixedLocation && root.mouseAction ? 1 : 0.6; font.pixelSize: 13 }
                        FlintButton {
                            anchors.right: parent.right
                            height: 30
                            label: "Pick location"
                            dark: backend.dark
                            enabled: !backend.running && root.mouseAction
                            onClicked: {
                                root.hide()
                                picker.show()
                                picker.requestActivate()
                            }
                        }
                    }
                }
            }

            FlintCard {
                width: parent.width
                height: 110
                dark: backend.dark
                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 6
                    Text { text: "Ready to run"; color: root.ink; font.pixelSize: 14; font.weight: Font.Bold }
                    FlintButton { width: parent.width; height: 38; label: backend.running ? "Stop clicking" : "Start clicking"; primary: !backend.running; danger: backend.running; dark: backend.dark; onClicked: backend.toggleFromButton() }
                    Item {
                        width: parent.width
                        height: 14
                        Text { anchors.left: parent.left; text: "Actions completed  " + backend.completed; color: root.muted; font.pixelSize: 12 }
                        Text { anchors.right: parent.right; text: (backend.holdToClick ? "Hold  " : "Global hotkey  ") + backend.hotkeyShortcut; color: root.muted; font.pixelSize: 12 }
                    }
                }
            }

        }
    }

    MacroPanel {
        id: macroPanel
        anchors.top: topbar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 12
        visible: root.activeTab === 1
        dark: backend.dark
        onShowRequested: root.activeTab = 1
        onPickRequested: {
            picker.forMacro = true
            root.hide()
            picker.show()
            picker.requestActivate()
        }
    }

    Window {
        id: picker
        property bool forMacro: false
        visible: false
        x: backend.virtualScreenX
        y: backend.virtualScreenY
        width: backend.virtualScreenWidth
        height: backend.virtualScreenHeight
        flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
        color: "#44000000"
        Item {
            anchors.fill: parent
            focus: picker.visible
            Keys.onEscapePressed: {
                picker.hide()
                picker.forMacro = false
                root.show()
                root.requestActivate()
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.CrossCursor
                onClicked: {
                    if (picker.forMacro) macro.pickLocation()
                    else backend.pickLocation()
                    picker.hide()
                    picker.forMacro = false
                    root.show()
                    root.requestActivate()
                }
            }
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 28
                width: hint.implicitWidth + 28
                height: 36
                radius: 8
                color: "#202123"
                Text { id: hint; anchors.centerIn: parent; text: "Click anywhere to choose a location · Esc to cancel"; color: "#ffffff"; font.pixelSize: 13 }
            }
        }
    }
}
