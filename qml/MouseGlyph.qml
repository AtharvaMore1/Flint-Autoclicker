import QtQuick
import QtQuick.Shapes

// Mouse paths from Phosphor Icons (MIT). See README.md for the license.
Item {
    id: glyph
    property int buttonPart: 0 // 0 left, 1 middle, 2 right
    property color tint: "#e99458"
    implicitWidth: 18
    implicitHeight: 20

    readonly property string leftPath: "M144,16H112A64.07,64.07,0,0,0,48,80v96a64.07,64.07,0,0,0,64,64h32a64.07,64.07,0,0,0,64-64V80A64.07,64.07,0,0,0,144,16Zm48,64v24H136V32h8A48.05,48.05,0,0,1,192,80Zm-76.69,24-46-46A48.49,48.49,0,0,1,80.51,43.82L120,83.31V104ZM64,80c0-1.51.08-3,.22-4.47L92.69,104H64Zm56-48V60.69L94.59,35.28A47.73,47.73,0,0,1,112,32Zm24,192H112a48.05,48.05,0,0,1-48-48V120H192v56A48.05,48.05,0,0,1,144,224Z"
    readonly property string middlePath: "M144,16H112A64.07,64.07,0,0,0,48,80v96a64.07,64.07,0,0,0,64,64h32a64.07,64.07,0,0,0,64-64V80A64.07,64.07,0,0,0,144,16Zm48,64v24H152V88a16,16,0,0,0-16-16V32h8A48.05,48.05,0,0,1,192,80Zm-56,56H120V88h16v23.9a.51.51,0,0,0,0,.2ZM112,32h8V72a16,16,0,0,0-16,16v16H64V80A48.05,48.05,0,0,1,112,32Zm32,192H112a48.05,48.05,0,0,1-48-48V120h40v16a16,16,0,0,0,16,16h16a16,16,0,0,0,16-16V120h40v56A48.05,48.05,0,0,1,144,224Z"
    readonly property string rightPath: "M144,16H112A64.07,64.07,0,0,0,48,80v96a64.07,64.07,0,0,0,64,64h32a64.07,64.07,0,0,0,64-64V80A64.07,64.07,0,0,0,144,16Zm-8,67.31,39.49-39.49A48.49,48.49,0,0,1,186.66,58l-46,46H136Zm55.78-7.78c.14,1.47.22,3,.22,4.47v24H163.31ZM161.41,35.28,136,60.69V32h8A47.73,47.73,0,0,1,161.41,35.28ZM112,32h8v72H64V80A48.05,48.05,0,0,1,112,32Zm32,192H112a48.05,48.05,0,0,1-48-48V120H192v56A48.05,48.05,0,0,1,144,224Z"

    Shape {
        id: iconShape
        anchors.centerIn: parent
        width: Math.min(glyph.width, glyph.height)
        height: width
        antialiasing: true
        // Curve rendering smooths small SVG details without an extra texture.
        // Retain multisampling as a fallback for older supported Qt versions.
        Component.onCompleted: {
            if ("preferredRendererType" in iconShape && Shape.CurveRenderer !== undefined)
                iconShape.preferredRendererType = Shape.CurveRenderer
        }
        layer.enabled: !("preferredRendererType" in iconShape)
        layer.samples: 4
        ShapePath {
            fillColor: glyph.tint
            fillRule: ShapePath.WindingFill
            strokeColor: "transparent"
            scale: Qt.size(iconShape.width / 256, iconShape.height / 256)
            PathSvg { path: glyph.buttonPart === 0 ? glyph.leftPath : (glyph.buttonPart === 1 ? glyph.middlePath : glyph.rightPath) }
        }
    }
}
