import QtQuick
import QtQuick.Controls.Basic as Basic

Basic.SpinBox {
    id: field
    property bool dark: true
    editable: true
    implicitWidth: 112
    implicitHeight: 36
    font.family: "Segoe UI"
    font.pixelSize: 13
    font.weight: Font.DemiBold

    function commitInput() {
        const parsed = valueFromText(input.text, locale)
        if (isFinite(parsed)) {
            const next = Math.max(from, Math.min(to, parsed))
            if (value !== next) { value = next; valueModified() }
        }
        input.text = Qt.binding(function() { return field.textFromValue(field.value, field.locale) })
    }

    contentItem: TextInput {
        id: input
        text: field.textFromValue(field.value, field.locale)
        font: field.font
        color: !field.enabled ? (field.dark ? "#77797d" : "#a0a2a8") : (field.dark ? "#ededee" : "#25272a")
        selectionColor: "#e99458"
        selectedTextColor: "#1b1410"
        horizontalAlignment: TextInput.AlignHCenter
        verticalAlignment: TextInput.AlignVCenter
        readOnly: !field.editable
        validator: field.validator
        inputMethodHints: Qt.ImhDigitsOnly
        selectByMouse: true
        onEditingFinished: field.commitInput()
        onAccepted: { field.commitInput(); field.parent.forceActiveFocus(Qt.OtherFocusReason) }
        onActiveFocusChanged: if (!activeFocus) deselect()
    }
    up.indicator: Item { implicitWidth: 0; implicitHeight: 0 }
    down.indicator: Item { implicitWidth: 0; implicitHeight: 0 }
    background: Rectangle {
        radius: 8
        color: field.dark ? "#2b2c2f" : "#f1f2f4"
        border.width: input.activeFocus ? 1 : 0
        border.color: "#e99458"
    }
}
