import QtQuick
import QtQuick.Shapes

// Phosphor Icons, MIT. See README.md for the license.
Item {
    id: glyph
    property bool save: false
    property color tint: "#e7e7e8"
    implicitWidth: 20
    implicitHeight: 20
    Shape {
        id: iconShape
        anchors.centerIn: parent
        width: Math.min(glyph.width, glyph.height)
        height: width
        antialiasing: true
        Component.onCompleted: {
            if ("preferredRendererType" in iconShape && Shape.CurveRenderer !== undefined)
                iconShape.preferredRendererType = Shape.CurveRenderer
        }
        layer.enabled: !("preferredRendererType" in iconShape)
        layer.samples: 4
        ShapePath {
            fillColor: glyph.tint
            strokeColor: "transparent"
            scale: Qt.size(iconShape.width / 256, iconShape.height / 256)
            PathSvg {
                path: glyph.save
                    ? "M219.31,72,184,36.69A15.86,15.86,0,0,0,172.69,32H48A16,16,0,0,0,32,48V208a16,16,0,0,0,16,16H208a16,16,0,0,0,16-16V83.31A15.86,15.86,0,0,0,219.31,72ZM168,208H88V152h80Zm40,0H184V152a16,16,0,0,0-16-16H88a16,16,0,0,0-16,16v56H48V48H172.69L208,83.31ZM160,72a8,8,0,0,1-8,8H96a8,8,0,0,1,0-16h56A8,8,0,0,1,160,72Z"
                    : "M245,110.64A16,16,0,0,0,232,104H216V88a16,16,0,0,0-16-16H130.67L102.94,51.2a16.14,16.14,0,0,0-9.6-3.2H40A16,16,0,0,0,24,64V208h0a8,8,0,0,0,8,8H211.1a8,8,0,0,0,7.59-5.47l28.49-85.47A16.05,16.05,0,0,0,245,110.64ZM93.34,64,123.2,86.4A8,8,0,0,0,128,88h72v16H69.77a16,16,0,0,0-15.18,10.94L40,158.7V64Zm112,136H43.1l26.67-80H232Z"
            }
        }
    }
}
