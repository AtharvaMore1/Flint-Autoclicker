import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Dialogs

Basic.Pane {
    id: panel
    objectName: "macroPanel"
    property bool dark: true
    property var selectedIndices: []
    readonly property var stepModel: macro.steps
    readonly property int stepCount: stepModel.length
    readonly property int selectedIndex: selectedIndices.length ? selectedIndices[0] : -1
    property int selectionAnchor: -1
    readonly property bool shortcutsEnabled: visible && !macro.busy && !macro.editorOpen && !editingText()
    property int editingIndex: -1
    property var draft: ({})
    signal pickRequested()
    signal showRequested()
    readonly property color ink: dark ? "#e9eaeb" : "#222427"
    readonly property color muted: dark ? "#a0a2a5" : "#676b70"
    readonly property var actionTypes: ["click", "press", "hold", "scroll", "wait"]
    readonly property var actionNames: ["Click", "Press key", "Hold input", "Scroll", "Wait"]
    readonly property bool keyboardAction: draft.type === "press" || (draft.type === "hold" && draft.button === 0)
    readonly property bool fixedPosition: keyboardAction ? !!draft.keyLocation : !!draft.fixed
    padding: 0
    background: Item {}
    TapHandler {
        onTapped: function(point) {
            const window = panel.Window.window
            const focused = window ? window.activeFocusItem : null
            if (!focused || focused.selectByMouse === undefined) return
            const local = focused.mapFromItem(panel, point.position.x, point.position.y)
            if (local.x < 0 || local.y < 0 || local.x >= focused.width || local.y >= focused.height)
                panel.parent.forceActiveFocus(Qt.OtherFocusReason)
        }
    }
    palette.window: dark ? "#202123" : "#ffffff"
    palette.base: dark ? "#2b2c2f" : "#f1f2f4"
    palette.button: dark ? "#303236" : "#e9eaed"
    palette.text: ink
    palette.windowText: ink
    palette.buttonText: ink
    palette.highlight: "#e99458"
    palette.highlightedText: "#1b1410"
    Binding { target: macro; property: "editorOpen"; value: editor.visible || openDialog.visible || saveDialog.visible }

    function edit(index, type) {
        editingIndex = index
        draft = index < 0 ? macro.defaultStep(type) : macro.stepAt(index)
        editor.open()
    }
    function editingText() {
        const window = panel.Window.window
        const item = window ? window.activeFocusItem : null
        return item && item.selectByMouse !== undefined
    }
    function selectRow(index, modifiers) {
        if ((modifiers & Qt.ShiftModifier) && selectionAnchor >= 0) {
            let next = (modifiers & Qt.ControlModifier) ? selectedIndices.slice() : []
            for (let i = Math.min(selectionAnchor, index); i <= Math.max(selectionAnchor, index); ++i)
                if (next.indexOf(i) < 0) next.push(i)
            selectedIndices = next.sort(function(a,b) { return a-b })
        } else if (modifiers & Qt.ControlModifier) {
            let next = selectedIndices.slice()
            const found = next.indexOf(index)
            if (found >= 0) next.splice(found, 1); else next.push(index)
            selectedIndices = next.sort(function(a,b) { return a-b })
            selectionAnchor = index
        } else {
            selectedIndices = [index]
            selectionAnchor = index
        }
        sequence.forceActiveFocus()
    }
    function selectAll() {
        let all = []
        for (let i = 0; i < stepCount; ++i) all.push(i)
        selectedIndices = all
        selectionAnchor = all.length ? 0 : -1
        sequence.forceActiveFocus()
    }
    function moveSelected(delta) {
        selectedIndices = macro.moveSteps(selectedIndices, delta)
        selectionAnchor = selectedIndex
        sequence.forceActiveFocus()
    }
    function pasteSelected() {
        const after = selectedIndices.length ? selectedIndices[selectedIndices.length-1] : stepCount-1
        const inserted = macro.pasteSteps(after)
        if (inserted.length) selectedIndices = inserted
        sequence.forceActiveFocus()
    }
    function cutSelected() { macro.cutSteps(selectedIndices); selectedIndices = []; sequence.forceActiveFocus() }
    function deleteSelected() { macro.removeSteps(selectedIndices); selectedIndices = []; sequence.forceActiveFocus() }
    function duplicateSelected() { selectedIndices = macro.duplicateSteps(selectedIndices); sequence.forceActiveFocus() }
    function saveFile() { if (macro.fileName.length) macro.save(); else saveDialog.open() }
    function openEditMenu(x, y) {
        editMenu.x = Math.max(0, Math.min(x, panel.width - editMenu.width))
        editMenu.y = Math.max(0, Math.min(y, panel.height - editMenu.height))
        editMenu.open()
    }
    function update(key, value) {
        let next = Object.assign({}, draft)
        next[key] = value
        draft = next
    }
    function requestRecording() { macro.startRecording() }

    Connections {
        target: macro
        function onRecordRequested() { panel.showRequested(); panel.requestRecording() }
        function onLocationPicked(x, y) { panel.update("x", x); panel.update("y", y); panel.update(panel.keyboardAction ? "keyLocation" : "fixed", true) }
        function onDocumentChanged() { panel.selectedIndices = panel.selectedIndices.filter(function(i) { return i < panel.stepCount }) }
        function onProgressChanged() {
            if (macro.playing && macro.currentStep >= 0)
                sequence.positionViewAtIndex(macro.currentStep, ListView.Contain)
        }
    }

    Shortcut { sequence: "Ctrl+C"; enabled: panel.shortcutsEnabled; onActivated: macro.copySteps(panel.selectedIndices) }
    Shortcut { sequence: "Ctrl+X"; enabled: panel.shortcutsEnabled; onActivated: panel.cutSelected() }
    Shortcut { sequences: ["Ctrl+V", "Ctrl+P"]; enabled: panel.shortcutsEnabled; onActivated: panel.pasteSelected() }
    Shortcut { sequence: "Ctrl+A"; enabled: panel.shortcutsEnabled; onActivated: panel.selectAll() }
    Shortcut { sequence: "Ctrl+Z"; enabled: panel.shortcutsEnabled; onActivated: macro.undo() }
    Shortcut { sequences: ["Ctrl+Y", "Ctrl+Shift+Z"]; enabled: panel.shortcutsEnabled; onActivated: macro.redo() }
    Shortcut { sequence: "Delete"; enabled: panel.shortcutsEnabled; onActivated: panel.deleteSelected() }
    Shortcut { sequence: "Ctrl+D"; enabled: panel.shortcutsEnabled; onActivated: panel.duplicateSelected() }
    Shortcut { sequence: "Alt+Up"; enabled: panel.shortcutsEnabled; onActivated: panel.moveSelected(-1) }
    Shortcut { sequence: "Alt+Down"; enabled: panel.shortcutsEnabled; onActivated: panel.moveSelected(1) }
    Shortcut { sequence: "Ctrl+S"; enabled: panel.shortcutsEnabled; onActivated: panel.saveFile() }
    Shortcut { sequence: "Ctrl+Shift+S"; enabled: panel.shortcutsEnabled; onActivated: saveDialog.open() }
    Shortcut { sequence: "Ctrl+O"; enabled: panel.shortcutsEnabled; onActivated: openDialog.open() }

    Item {
        id: header
        width: parent.width
        height: 34
        Row {
            spacing: 6
            FlintButton { width: 52; height: 32; label: "Edit"; dark: panel.dark; enabled: !macro.busy; onClicked: panel.openEditMenu(0, header.height) }
            FlintButton { width: 34; height: 32; label: "↑"; dark: panel.dark; enabled: !macro.busy && panel.selectedIndex > 0; onClicked: panel.moveSelected(-1) }
            FlintButton { width: 34; height: 32; label: "↓"; dark: panel.dark; enabled: !macro.busy && panel.selectedIndices.length > 0 && panel.selectedIndices[panel.selectedIndices.length-1] < panel.stepCount-1; onClicked: panel.moveSelected(1) }
            Text { anchors.verticalCenter: parent.verticalCenter; text: panel.selectedIndices.length > 1 ? panel.selectedIndices.length + " selected" : panel.stepCount + " steps"; color: panel.muted; font.pixelSize: 12 }
        }
        Row {
            anchors.right: parent.right
            spacing: 6
            FlintButton {
                width: 36; height: 32; dark: panel.dark; enabled: !macro.busy
                Accessible.name: "Open macro"
                FileGlyph { anchors.centerIn: parent; tint: panel.dark ? "#e9eaeb" : "#222427"; opacity: parent.enabled ? 1 : 0.4 }
                Basic.ToolTip.visible: hovered; Basic.ToolTip.text: "Open macro (Ctrl+O)"
                onClicked: openDialog.open()
            }
            FlintButton {
                width: 36; height: 32; dark: panel.dark; enabled: !macro.busy
                Accessible.name: "Save macro"
                FileGlyph { anchors.centerIn: parent; save: true; tint: panel.dark ? "#e9eaeb" : "#222427"; opacity: parent.enabled ? 1 : 0.4 }
                Basic.ToolTip.visible: hovered; Basic.ToolTip.text: "Save macro (Ctrl+S)"
                onClicked: panel.saveFile()
            }
        }
    }
    component ActionMenu: Basic.Menu {
        width: 232
        padding: 4
        background: Rectangle { radius: 8; color: panel.dark ? "#242528" : "#ffffff"; border.color: panel.dark ? "#45474b" : "#d8dae0" }
    }
    component ActionItem: Basic.MenuItem {
        id: menuItem
        property string keyHint: ""
        implicitHeight: 32
        contentItem: Item {
            implicitHeight: 18
            opacity: menuItem.enabled ? 1 : 0.45
            Text { anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; text: menuItem.text; font.pixelSize: 12; color: panel.ink }
            Text { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; text: menuItem.keyHint; font.pixelSize: 11; color: panel.muted }
        }
        background: Rectangle { radius: 6; color: menuItem.highlighted ? (panel.dark ? "#38322f" : "#f8e5d6") : "transparent" }
    }
    ActionMenu {
        id: addMenu
        y: header.height + 58
        Repeater {
            model: panel.actionNames.slice(0, 5)
            ActionItem {
                required property int index
                required property string modelData
                text: modelData
                onTriggered: panel.edit(-1, panel.actionTypes[index])
            }
        }
    }
    ActionMenu {
        id: editMenu
        objectName: "editMenu"
        ActionItem { text: "Edit step"; enabled: panel.selectedIndices.length === 1; onTriggered: panel.edit(panel.selectedIndex, "") }
        ActionItem { text: "Copy"; keyHint: "Ctrl+C"; enabled: panel.selectedIndices.length > 0; onTriggered: macro.copySteps(panel.selectedIndices) }
        ActionItem { text: "Cut"; keyHint: "Ctrl+X"; enabled: panel.selectedIndices.length > 0; onTriggered: panel.cutSelected() }
        ActionItem { text: "Paste"; keyHint: "Ctrl+V / Ctrl+P"; enabled: macro.canPaste; onTriggered: panel.pasteSelected() }
        ActionItem { text: "Duplicate"; keyHint: "Ctrl+D"; enabled: panel.selectedIndices.length > 0; onTriggered: panel.duplicateSelected() }
        ActionItem { text: "Delete"; keyHint: "Del"; enabled: panel.selectedIndices.length > 0; onTriggered: panel.deleteSelected() }
        Basic.MenuSeparator {}
        ActionItem { text: "Undo"; keyHint: "Ctrl+Z"; enabled: macro.canUndo; onTriggered: macro.undo() }
        ActionItem { text: "Redo"; keyHint: "Ctrl+Y"; enabled: macro.canRedo; onTriggered: macro.redo() }
        ActionItem { text: "Select all"; keyHint: "Ctrl+A"; enabled: panel.stepCount > 0; onTriggered: panel.selectAll() }
        Basic.MenuSeparator {}
        ActionItem { text: "New sequence"; onTriggered: { macro.newMacro(); panel.selectedIndices = [] } }
        ActionItem { text: "Save as…"; keyHint: "Ctrl+Shift+S"; onTriggered: saveDialog.open() }
    }

    FlintCard {
        anchors.top: header.bottom; anchors.topMargin: 8
        anchors.bottom: playback.top; anchors.bottomMargin: 8
        width: parent.width
        dark: panel.dark
        ListView {
            id: sequence
            objectName: "sequence"
            anchors.fill: parent; anchors.margins: 6
            clip: true
            spacing: 4
            model: panel.stepModel
            focus: true
            header: Item {
                width: sequence.width; height: 54
                FlintButton {
                    anchors.fill: parent; anchors.bottomMargin: 6
                    label: "+"; labelSize: 30; dark: panel.dark; enabled: !macro.busy
                    Accessible.name: "Add action"
                    Basic.ToolTip.visible: hovered; Basic.ToolTip.text: "Add action"
                    onClicked: addMenu.open()
                }
            }
            Basic.ScrollBar.vertical: Basic.ScrollBar { policy: Basic.ScrollBar.AsNeeded }
            delegate: Rectangle {
                required property int index
                required property var modelData
                objectName: "step_" + index
                width: sequence.width; height: 42
                radius: 6
                color: (macro.playing ? macro.currentStep === index : panel.selectedIndices.indexOf(index) >= 0)
                       ? (panel.dark ? "#38322f" : "#f8e5d6") : (rowMouse.containsMouse ? (panel.dark ? "#2b2d30" : "#eceef1") : "transparent")
                Text {
                    anchors.left: parent.left; anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: index + 1; color: panel.muted; font.pixelSize: 11
                }
                Text {
                    anchors.left: parent.left; anchors.leftMargin: 37
                    anchors.right: parent.right; anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.summary; color: panel.ink; font.pixelSize: 12
                    elide: Text.ElideRight
                }
                MouseArea {
                    id: rowMouse
                    anchors.fill: parent; hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: function(mouse) {
                        if (macro.busy) return
                        if (mouse.button === Qt.RightButton) {
                            if (panel.selectedIndices.indexOf(index) < 0) panel.selectRow(index, 0)
                            const point = mapToItem(panel, mouse.x, mouse.y)
                            panel.openEditMenu(point.x, point.y)
                        } else panel.selectRow(index, mouse.modifiers)
                    }
                    onDoubleClicked: function(mouse) { if (!macro.busy && mouse.button === Qt.LeftButton) panel.edit(index, "") }
                }
            }
        }
        Text {
            anchors.centerIn: parent
            width: parent.width - 24
            horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
            visible: macro.recording || panel.stepCount === 0
            text: macro.recording ? "Recording…\n" + macro.recordedCount + " input events\nF8 or F10 to stop"
                                  : "Record your actions or add a step to begin."
            color: panel.muted; font.pixelSize: 13
        }
        // Hide the previous sequence until the new recording is complete.
        Binding { target: sequence; property: "visible"; value: !macro.recording }
    }

    FlintCard {
        id: playback
        anchors.bottom: parent.bottom
        width: parent.width; height: 222
        dark: panel.dark
        Column {
            anchors.fill: parent; anchors.margins: 10
            spacing: 7
            Row {
                width: parent.width; height: 30; spacing: 6
                SegmentedControl {
                    width: parent.width - 70; height: 30
                    options: ["Continuous", "Set loops"]
                    currentIndex: macro.loops === 0 ? 0 : 1
                    dark: panel.dark; enabled: !macro.busy
                    onActivated: function(index) { macro.loops = index === 0 ? 0 : Math.max(1, loopCount.value) }
                }
                NumberField { id: loopCount; width: 64; height: 30; from: 1; to: 1000000; value: Math.max(1, macro.loops); dark: panel.dark; enabled: !macro.busy && macro.loops > 0; onValueModified: macro.loops = value }
            }
            Row {
                width: parent.width; height: 26; spacing: 8
                Text { anchors.verticalCenter: parent.verticalCenter; text: "Loop gap"; color: panel.muted; font.pixelSize: 12; width: 80 }
                NumberField {
                    objectName: "loopGapField"
                    width: 90; height: 26; from: 0; to: 3600000
                    value: macro.loopGapMs; dark: panel.dark; enabled: !macro.busy && macro.loops !== 1
                    onValueModified: macro.loopGapMs = value
                    Basic.ToolTip.visible: gapHover.hovered
                    Basic.ToolTip.text: "Pause between loops. Independent of playback speed."
                    HoverHandler { id: gapHover }
                }
                Text { anchors.verticalCenter: parent.verticalCenter; text: "ms between loops"; color: panel.muted; font.pixelSize: 11 }
            }
            Row {
                height: 28; width: parent.width; spacing: 8
                Text { anchors.verticalCenter: parent.verticalCenter; text: "Speed"; color: panel.muted; font.pixelSize: 12; width: 50 }
                SegmentedControl {
                    objectName: "speedPresets"
                    width: 150; height: 28; options: ["0.5×", "1×", "2×"]
                    currentIndex: macro.customSpeed ? -1 : macro.speed === 0.5 ? 0 : macro.speed === 1 ? 1 : macro.speed === 2 ? 2 : -1
                    dark: panel.dark; enabled: !macro.busy
                    onActivated: function(index) { macro.setPresetSpeed([0.5, 1, 2][index]) }
                }
                Basic.TextField {
                    id: speedField
                    objectName: "speedField"
                    property bool userEdited: false
                    width: parent.width - 50 - 150 - 24 - 14; height: 28
                    text: macro.speed.toString()
                    placeholderText: "Custom"
                    enabled: !macro.busy
                    selectByMouse: true
                    horizontalAlignment: Text.AlignHCenter
                    color: enabled && (macro.customSpeed || activeFocus) ? panel.ink : panel.muted
                    font.pixelSize: 12
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    background: Rectangle { radius: 7; color: macro.customSpeed ? (panel.dark ? "#38322f" : "#f8e5d6") : (panel.dark ? "#242528" : "#e8e9ec"); border.width: macro.customSpeed ? 1 : 0; border.color: panel.dark ? "#a66a45" : "#dba47d" }
                    Basic.ToolTip.visible: hovered; Basic.ToolTip.text: "Click to enter a custom speed (0–10×)"
                    onTextEdited: userEdited = true
                    onEditingFinished: {
                        if (userEdited) {
                            userEdited = false
                            if (text.trim().length && isFinite(Number(text))) macro.speed = Number(text)
                        }
                        text = Qt.binding(function() { return macro.speed.toString() })
                    }
                    onAccepted: sequence.forceActiveFocus()
                }
                Text { width: 14; anchors.verticalCenter: parent.verticalCenter; text: "×"; color: panel.muted; font.pixelSize: 12 }
            }
            Row {
                width: parent.width; height: 36; spacing: 6
                FlintButton { width: (parent.width - 12) / 3; height: 36; label: "Record"; dark: panel.dark; enabled: !macro.busy; onClicked: panel.requestRecording() }
                FlintButton { width: (parent.width - 12) / 3; height: 36; label: "Play"; primary: true; dark: panel.dark; enabled: !macro.busy && panel.stepCount > 0; onClicked: macro.play() }
                FlintButton { width: (parent.width - 12) / 3; height: 36; label: "Stop"; danger: macro.busy; dark: panel.dark; enabled: macro.busy; onClicked: macro.stop(true) }
            }
            Text { width: parent.width; text: macro.playing ? "Loop " + macro.currentLoop + (macro.loops > 0 ? " / " + macro.loops : "") + " · " + macro.status : macro.status; color: panel.muted; font.pixelSize: 11; elide: Text.ElideRight; Basic.ToolTip.visible: statusHover.hovered; Basic.ToolTip.text: text; HoverHandler { id: statusHover } }
            Text { text: "F8 Record     F9 Play / stop     F10 Stop all"; color: panel.muted; font.pixelSize: 11 }
        }
    }

    FileDialog { id: openDialog; title: "Open macro"; nameFilters: ["Flint macros (*.flintmacro)"]; onAccepted: { if (macro.open(selectedFile)) panel.selectedIndices = [] } }
    FileDialog { id: saveDialog; title: "Save macro"; fileMode: FileDialog.SaveFile; defaultSuffix: "flintmacro"; nameFilters: ["Flint macros (*.flintmacro)"]; onAccepted: macro.save(selectedFile) }
    Basic.Dialog {
        id: editor
        objectName: "stepEditor"
        palette: panel.palette
        background: Rectangle { radius: 10; color: panel.dark ? "#202123" : "#ffffff"; border.color: panel.dark ? "#45474b" : "#d8dae0" }
        parent: Basic.Overlay.overlay
        x: (parent.width - width) / 2; y: (parent.height - height) / 2
        width: panel.width
        modal: true; focus: true
        title: panel.editingIndex < 0 ? "Add action" : "Edit step " + (panel.editingIndex + 1)
        closePolicy: Basic.Popup.CloseOnEscape
        contentItem: Column {
            spacing: 10
            FlintComboBox {
                id: actionType
                objectName: "actionType"
                dark: panel.dark
                width: parent.width; height: 32
                model: panel.actionNames
                currentIndex: panel.actionTypes.indexOf(panel.draft.type)
                onActivated: function(index) {
                    panel.draft = macro.defaultStep(panel.actionTypes[index])
                }
            }
            Row {
                width: parent.width; height: 30
                visible: panel.draft.type === "click" || panel.draft.type === "hold"
                Text { width: 100; anchors.verticalCenter: parent.verticalCenter; text: "Input"; color: panel.ink }
                FlintComboBox {
                    dark: panel.dark
                    width: parent.width - 100; height: 30
                    model: panel.draft.type === "hold" ? ["Keyboard", "Left", "Middle", "Right", "Mouse 4", "Mouse 5"] : ["Left", "Middle", "Right", "Mouse 4", "Mouse 5"]
                    currentIndex: (panel.draft.button || 0) - (panel.draft.type === "hold" ? 0 : 1)
                    onActivated: function(index) { panel.update("button", index + (panel.draft.type === "hold" ? 0 : 1)) }
                }
            }
            Row {
                width: parent.width; height: 30
                visible: panel.draft.type === "press" || (panel.draft.type === "hold" && panel.draft.button === 0)
                Text { width: 100; anchors.verticalCenter: parent.verticalCenter; text: "Key / shortcut"; color: panel.ink }
                Basic.TextField { width: parent.width - 100; height: 30; text: panel.draft.key || ""; placeholderText: "E or Ctrl+C"; onTextEdited: panel.update("key", text) }
            }
            Row {
                width: parent.width; height: 30
                visible: panel.draft.type !== "wait"
                Text { width: 100; anchors.verticalCenter: parent.verticalCenter; text: "Position"; color: panel.ink }
                SegmentedControl { width: parent.width - 100; height: 30; options: ["Cursor", "Fixed"]; currentIndex: panel.fixedPosition ? 1 : 0; dark: panel.dark; onActivated: function(index) { panel.update(panel.keyboardAction ? "keyLocation" : "fixed", index === 1) } }
            }
            Row {
                width: parent.width; height: 30; spacing: 6
                visible: panel.draft.type !== "wait" && panel.fixedPosition
                Text { anchors.verticalCenter: parent.verticalCenter; text: "X"; color: panel.ink }
                NumberField { width: 76; height: 30; from: -100000; to: 100000; value: panel.draft.x || 0; dark: panel.dark; onValueModified: panel.update("x", value) }
                Text { anchors.verticalCenter: parent.verticalCenter; text: "Y"; color: panel.ink }
                NumberField { width: 76; height: 30; from: -100000; to: 100000; value: panel.draft.y || 0; dark: panel.dark; onValueModified: panel.update("y", value) }
                FlintButton { width: 82; height: 30; label: "Pick location"; dark: panel.dark; onClicked: panel.pickRequested() }
            }
            Row {
                width: parent.width; height: 30
                visible: panel.draft.type === "wait" || panel.draft.type === "hold"
                Text { width: 124; anchors.verticalCenter: parent.verticalCenter; text: "Duration (ms)"; color: panel.ink }
                NumberField { width: parent.width - 124; height: 30; from: 0; to: panel.draft.type === "wait" ? 7200000 : 3600000; value: panel.draft.durationMs || 0; dark: panel.dark; onValueModified: panel.update("durationMs", value) }
            }
            Row {
                width: parent.width; height: 30
                visible: panel.draft.type === "click" || panel.draft.type === "press"
                Text { width: 124; anchors.verticalCenter: parent.verticalCenter; text: panel.draft.type === "click" ? "Click count" : "Press count"; color: panel.ink }
                NumberField { width: parent.width - 124; height: 30; from: 1; to: 2147483647; value: panel.draft.count || 1; dark: panel.dark; onValueModified: panel.update("count", value) }
            }
            Row {
                width: parent.width; height: 30
                visible: (panel.draft.type === "click" || panel.draft.type === "press") && panel.draft.count > 1
                Text { width: 124; anchors.verticalCenter: parent.verticalCenter; text: "Repeat gap (ms)"; color: panel.ink }
                NumberField { width: parent.width - 124; height: 30; from: 0; to: 3600000; value: panel.draft.intervalMs || 0; dark: panel.dark; onValueModified: panel.update("intervalMs", value) }
            }
            Row {
                width: parent.width; height: 30
                visible: panel.draft.type === "scroll"
                Text { width: 124; anchors.verticalCenter: parent.verticalCenter; text: "Scroll (120 = notch)"; color: panel.ink; font.pixelSize: 11 }
                NumberField { width: parent.width - 124; height: 30; from: -120000; to: 120000; value: panel.draft.amount || 0; dark: panel.dark; onValueModified: panel.update("amount", value) }
            }
            Basic.CheckBox { visible: panel.draft.type === "scroll"; text: "Horizontal scrolling"; checked: panel.draft.horizontal || false; onClicked: panel.update("horizontal", checked) }
            Text { width: parent.width; visible: panel.keyboardAction; text: panel.fixedPosition ? "Moves the pointer before pressing. Keyboard input goes to the focused app." : "F8–F10 are reserved for macro controls."; color: panel.muted; font.pixelSize: 11; wrapMode: Text.WordWrap }
            Text { width: parent.width; text: editorError.text; visible: text.length > 0; color: panel.dark ? "#ef958b" : "#a92318"; font.pixelSize: 11; wrapMode: Text.WordWrap }
            Row {
                width: parent.width; spacing: 8
                FlintButton { width: (parent.width - 8) / 2; label: "Cancel"; dark: panel.dark; onClicked: editor.close() }
                FlintButton { width: (parent.width - 8) / 2; label: "Apply"; primary: true; dark: panel.dark; onClicked: { if (macro.putStep(panel.editingIndex, panel.draft)) { panel.selectedIndices = [panel.editingIndex < 0 ? panel.stepCount - 1 : panel.editingIndex]; editor.close(); sequence.forceActiveFocus() } else editorError.text = macro.status } }
            }
        }
        onOpened: editorError.text = ""
    }
    QtObject { id: editorError; property string text: "" }
}
