import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects
import Blop 1.0

Popup {
    id: root
    width: 320
    height: Math.min(contentLayout.implicitHeight + 40, 680)

    x: Bridge.overlayPosition.x
    y: Bridge.overlayPosition.y
    visible: Bridge.settingsVisible

    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onClosed: Bridge.settingsVisible = false

    background: Item {
        Rectangle {
            anchors.fill: parent
            radius: 16
            // Glassmorphism: Dunkel, leicht transparent
            color: Qt.rgba(0.12, 0.12, 0.14, 0.95)
            border.color: Qt.rgba(1, 1, 1, 0.15)
            border.width: 1
        }
        layer.enabled: true
        layer.effect: DropShadow {
            transparentBorder: true
            horizontalOffset: 0; verticalOffset: 8
            radius: 24; samples: 17
            color: "#80000000"
        }
    }

    contentItem: ColumnLayout {
        id: contentLayout
        spacing: 0
        anchors.fill: parent
        anchors.margins: 16

        Label {
            text: "Füller Einstellungen"
            color: "white"
            font.pixelSize: 16; font.bold: true
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 16
        }

        Loader {
            Layout.fillWidth: true
            Layout.fillHeight: true
            sourceComponent: {
                switch(Bridge.activeToolMode) {
                    case 0: return penComponent
                    // Andere Tools folgen in den nächsten Schritten
                    default: return placeholderComponent
                }
            }
        }
    }

    // --- Styles ---
    component SectionLabel: Label {
        color: "#B0B0B0"
        font.pixelSize: 12
        font.weight: Font.Medium
        Layout.topMargin: 8
    }

    component CustomSlider: Slider {
        Layout.fillWidth: true
        background: Rectangle {
            x: parent.leftPadding
            y: parent.topPadding + parent.availableHeight / 2 - height / 2
            implicitWidth: 200
            implicitHeight: 4
            width: parent.availableWidth
            height: implicitHeight
            radius: 2
            color: "#3A3A3E"
            Rectangle {
                width: parent.visualPosition * parent.width
                height: parent.height
                color: "#5E5CE6"
                radius: 2
            }
        }
        handle: Rectangle {
            x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
            y: parent.topPadding + parent.availableHeight / 2 - height / 2
            implicitWidth: 20
            implicitHeight: 20
            radius: 10
            color: "#FFFFFF"
            border.color: "#5E5CE6"
            border.width: 2
        }
    }

    component CustomSwitch: Switch {
        Layout.fillWidth: true
        indicator: Rectangle {
            implicitWidth: 44
            implicitHeight: 24
            x: parent.leftPadding
            y: parent.height / 2 - height / 2
            radius: 12
            color: parent.checked ? "#5E5CE6" : "#3A3A3E"
            border.color: "#555"
            Rectangle {
                x: parent.checked ? parent.width - width - 2 : 2
                y: 2
                width: 20
                height: 20
                radius: 10
                color: "#ffffff"
                Behavior on x { NumberAnimation { duration: 150 } }
            }
        }
        contentItem: Text {
            text: parent.text
            color: "#E0E0E0"
            verticalAlignment: Text.AlignVCenter
            leftPadding: parent.indicator.width + parent.spacing
        }
    }

    // --- 1. PEN TOOL UI ---
    Component {
        id: penComponent
        ColumnLayout {
            spacing: 12

            // 1. Vorschau (Dynamische Linie)
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 70
                color: "#252526"
                radius: 8
                border.color: "#333"

                Canvas {
                    anchors.centerIn: parent
                    width: parent.width - 20
                    height: parent.height - 20
                    onPaint: {
                        var ctx = getContext("2d");
                        ctx.reset();

                        // Zeichnet eine Sinus-ähnliche Kurve
                        ctx.beginPath();
                        ctx.moveTo(0, height/2);
                        ctx.bezierCurveTo(width/3, 0, width/3*2, height, width, height/2);

                        ctx.lineWidth = Bridge.penWidth;
                        ctx.lineCap = "round";
                        ctx.lineJoin = "round";
                        ctx.strokeStyle = Bridge.penColor;
                        ctx.globalAlpha = Bridge.opacity;

                        // Simulation von Smoothing visuell (nur Symbolik hier)
                        if (Bridge.smoothing > 50) {
                            ctx.shadowColor = Bridge.penColor;
                            ctx.shadowBlur = 2;
                        }

                        ctx.stroke();
                    }
                    // Repaint wenn sich Config ändert
                    Connections {
                        target: Bridge
                        function onConfigChanged() { parent.requestPaint() }
                    }
                }
            }

            // 2. Sliders
            SectionLabel { text: "Dicke (" + Bridge.penWidth + " px)" }
            CustomSlider {
                from: 1; to: 50
                value: Bridge.penWidth
                onMoved: Bridge.penWidth = value
            }

            SectionLabel { text: "Deckkraft (" + Math.round(Bridge.opacity * 100) + "%)" }
            CustomSlider {
                from: 0.1; to: 1.0
                value: Bridge.opacity
                onMoved: Bridge.opacity = value
            }

            SectionLabel { text: "Glättung (Streamline)" }
            CustomSlider {
                from: 0; to: 100
                value: Bridge.smoothing
                onMoved: Bridge.smoothing = value
            }

            // 3. Toggles
            CustomSwitch {
                text: "Drucksensitivität"
                checked: Bridge.pressureSensitivity
                onToggled: Bridge.pressureSensitivity = checked
            }

            // 4. Farben (Quick Colors + Wheel)
            SectionLabel { text: "Farbe" }
            Flow {
                Layout.fillWidth: true
                spacing: 12

                // Farbrad Button
                Rectangle {
                    width: 36; height: 36
                    radius: 18
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "red" }
                        GradientStop { position: 0.33; color: "green" }
                        GradientStop { position: 0.66; color: "blue" }
                        GradientStop { position: 1.0; color: "red" }
                    }
                    border.width: 1
                    border.color: "#888"

                    Text {
                        anchors.centerIn: parent
                        text: "+"
                        color: "white"
                        font.bold: true
                        style: Text.Outline; styleColor: "black"
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: console.log("Open Color Dialog needed") // Hier würde man ColorDialog öffnen
                    }
                }

                // Quick Colors
                Repeater {
                    model: ["#000000", "#FFFFFF", "#FF3B30", "#007AFF", "#34C759"]
                    delegate: Rectangle {
                        width: 36; height: 36
                        radius: 18
                        color: modelData
                        border.width: (Bridge.penColor == modelData) ? 3 : 1
                        border.color: (Bridge.penColor == modelData) ? "#5E5CE6" : "#444"

                        MouseArea {
                            anchors.fill: parent
                            onClicked: Bridge.penColor = modelData
                        }
                    }
                }
            }
        }
    }

    Component { id: placeholderComponent; Item{} }
}
