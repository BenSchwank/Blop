import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects // Für FastBlur/Glassmorphism

Item {
    id: root
    width: 240
    height: 380
    visible: Bridge.settingsVisible // Gesteuert vom C++ Code
    x: Bridge.overlayPosition.x
    y: Bridge.overlayPosition.y

    // Animation beim Einblenden
    Behavior on opacity { NumberAnimation { duration: 200 } }
    opacity: visible ? 1.0 : 0.0

    // --- Glassmorphism Background ---
    Rectangle {
        id: bg
        anchors.fill: parent
        radius: 20
        color: Qt.rgba(0.2, 0.2, 0.2, 0.70) // Halbtransparenter Hintergrund
        border.color: Qt.rgba(1, 1, 1, 0.2)
        border.width: 1

        layer.enabled: true
        layer.effect: DropShadow {
            transparentBorder: true
            horizontalOffset: 0
            verticalOffset: 8
            radius: 16
            samples: 32
            color: "#80000000"
        }
    }

    // Blur Effekt hinter dem Rechteck (optional, benötigt ShaderEffectSource des Parents)
    // Hier simuliert durch die Farbe oben.

    // --- Inhalt ---
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 15

        // Header
        Text {
            text: getToolName(Bridge.activeToolMode)
            color: "white"
            font.bold: true
            font.pixelSize: 18
            Layout.alignment: Qt.AlignHCenter
        }

        Rectangle { height: 1; Layout.fillWidth: true; color: Qt.rgba(1,1,1,0.1) }

        // --- Dynamische Controls je nach Tool ---

        // 1. STIFTDICKE (Für Pen, Pencil, Eraser)
        ColumnLayout {
            visible: isStrokeTool(Bridge.activeToolMode)
            Layout.fillWidth: true
            spacing: 5

            Text { text: "Stärke: " + Bridge.penWidth + "px"; color: "#ddd"; font.pixelSize: 12 }
            Slider {
                from: 1; to: 50
                value: Bridge.penWidth
                Layout.fillWidth: true
                onMoved: Bridge.penWidth = value
            }
        }

        // 2. TRANSPARENZ (Nur Highlighter)
        ColumnLayout {
            visible: Bridge.activeToolMode === 2 // Highlighter
            Layout.fillWidth: true
            spacing: 5

            Text { text: "Deckkraft"; color: "#ddd"; font.pixelSize: 12 }
            Slider {
                from: 0.1; to: 1.0
                value: Bridge.highlighterOpacity
                Layout.fillWidth: true
                onMoved: Bridge.highlighterOpacity = value
            }
        }

        // 3. FARBPALETTE (Nicht für Eraser/Lasso)
        GridLayout {
            visible: Bridge.activeToolMode !== 3 && Bridge.activeToolMode !== 4
            columns: 4
            rowSpacing: 10
            columnSpacing: 10
            Layout.alignment: Qt.AlignHCenter

            Repeater {
                model: ["#000000", "#FFFFFF", "#FF3B30", "#4CD964", "#007AFF", "#FFCC00", "#5856D6", "#FF9500"]
                delegate: Rectangle {
                    width: 32; height: 32; radius: 16
                    color: modelData
                    border.width: Bridge.penColor == modelData ? 3 : 0
                    border.color: "white"

                    MouseArea {
                        anchors.fill: parent
                        onClicked: Bridge.penColor = modelData
                    }
                }
            }
        }

        Item { Layout.fillHeight: true } // Spacer nach unten
    }

    // --- Hilfsfunktionen ---
    function getToolName(mode) {
        switch(mode) {
            case 0: return "Füller";
            case 1: return "Bleistift";
            case 2: return "Marker";
            case 3: return "Radierer";
            case 4: return "Lasso";
            case 5: return "Bild";
            case 6: return "Lineal";
            case 7: return "Formen";
            default: return "Werkzeug";
        }
    }

    function isStrokeTool(mode) {
        return mode <= 3; // Pen, Pencil, Highlighter, Eraser
    }
}
