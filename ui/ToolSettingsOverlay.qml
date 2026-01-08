import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects
import Blop 1.0

Popup {
    id: root
    width: 300
    height: Math.min(contentLayout.implicitHeight + 40, 680)

    x: Bridge.overlayPosition.x
    y: Bridge.overlayPosition.y
    visible: Bridge.settingsVisible

    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onClosed: Bridge.settingsVisible = false

    background: Item {
        Rectangle {
            anchors.fill: parent; radius: 16
            color: Qt.rgba(0.15, 0.15, 0.16, 0.96)
            border.color: Qt.rgba(1, 1, 1, 0.15); border.width: 1
        }
        layer.enabled: true
        layer.effect: DropShadow {
            transparentBorder: true; horizontalOffset: 0; verticalOffset: 8
            radius: 24; samples: 17; color: "#80000000"
        }
    }

    contentItem: ColumnLayout {
        id: contentLayout
        spacing: 0
        anchors.fill: parent; anchors.margins: 16

        Label {
            text: getToolName(Bridge.activeToolMode)
            color: "white"; font.pixelSize: 16; font.bold: true
            Layout.alignment: Qt.AlignHCenter; Layout.bottomMargin: 16
        }

        Loader {
            Layout.fillWidth: true; Layout.fillHeight: true
            sourceComponent: {
                switch(Bridge.activeToolMode) {
                    case 0: return penComponent
                    case 1: return pencilComponent
                    case 2: return highlighterComponent
                    case 3: return eraserComponent
                    case 4: return lassoComponent
                    case 5: return imageComponent
                    case 6: return rulerComponent
                    case 7: return shapeComponent
                    case 8: return stickyNoteComponent
                    case 9: return textToolComponent
                    default: return placeholderComponent
                }
            }
        }
    }

    // --- Komponenten ---
    component SectionLabel: Label { color: "#B0B0B0"; font.pixelSize: 12; font.weight: Font.Medium; Layout.topMargin: 8 }
    component CustomSlider: Slider {
        Layout.fillWidth: true
        background: Rectangle {
            x: parent.leftPadding; y: parent.topPadding + parent.availableHeight/2 - height/2
            implicitWidth: 200; implicitHeight: 4; width: parent.availableWidth; height: implicitHeight
            radius: 2; color: "#3A3A3E"
            Rectangle { width: parent.visualPosition * parent.width; height: parent.height; color: "#5E5CE6"; radius: 2 }
        }
        handle: Rectangle {
            x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
            y: parent.topPadding + parent.availableHeight/2 - height/2
            implicitWidth: 16; implicitHeight: 16; radius: 8; color: "#FFFFFF"; border.color: "#5E5CE6"
        }
    }
    component CustomSwitch: Switch {
        Layout.fillWidth: true
        indicator: Rectangle {
            implicitWidth: 40; implicitHeight: 20; x: parent.leftPadding; y: parent.height/2 - height/2
            radius: 10; color: parent.checked ? "#5E5CE6" : "#3A3A3E"; border.color: "#555"
            Rectangle { x: parent.checked ? parent.width - width - 2 : 2; y: 2; width: 16; height: 16; radius: 8; color: "#ffffff"
            Behavior on x { NumberAnimation { duration: 150 } } }
        }
        contentItem: Text { text: parent.text; color: "#E0E0E0"; verticalAlignment: Text.AlignVCenter; leftPadding: parent.indicator.width + parent.spacing }
    }
    component OptionButton: Button {
        Layout.fillWidth: true; checkable: true; autoExclusive: true
        background: Rectangle { color: parent.checked ? "#5E5CE6" : "#3A3A3E"; radius: 6; border.color: "#555" }
        contentItem: Text { text: parent.text; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
    }
    component IconButton: Button {
        Layout.preferredWidth: 45; Layout.preferredHeight: 45; checkable: true; autoExclusive: true
        background: Rectangle { color: parent.checked ? "#5E5CE6" : "#3A3A3E"; radius: 8; border.color: "#555" }
        contentItem: Text { text: parent.text; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 20 }
    }

    // --- 1. PEN ---
    Component {
        id: penComponent
        ColumnLayout {
            spacing: 12
            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 60; color: "#252526"; radius: 8
                Canvas {
                    anchors.centerIn: parent; width: parent.width-20; height: parent.height-10
                    onPaint: {
                        var ctx = getContext("2d"); ctx.reset(); ctx.beginPath();
                        ctx.moveTo(10, height/2); ctx.bezierCurveTo(width/3, 0, width/3*2, height, width-10, height/2);
                        ctx.lineWidth = Bridge.penWidth; ctx.lineCap = "round";
                        ctx.strokeStyle = Bridge.penColor; ctx.globalAlpha = Bridge.opacity; ctx.stroke();
                    }
                    Connections { target: Bridge; function onConfigChanged() { parent.requestPaint() } }
                }
            }
            SectionLabel { text: "Dicke (" + Bridge.penWidth + " px)" }
            CustomSlider { from: 1; to: 50; value: Bridge.penWidth; onMoved: Bridge.penWidth = value }
            SectionLabel { text: "Deckkraft" }
            CustomSlider { from: 0.1; to: 1.0; value: Bridge.opacity; onMoved: Bridge.opacity = value }
            SectionLabel { text: "Glättung" }
            CustomSlider { from: 0; to: 100; value: Bridge.smoothing; onMoved: Bridge.smoothing = value }
            CustomSwitch { text: "Drucksensitivität"; checked: Bridge.pressureSensitivity; onToggled: Bridge.pressureSensitivity = checked }
            SectionLabel { text: "Farbe" }
            Flow {
                Layout.fillWidth: true; spacing: 10
                Repeater {
                    model: ["#FFFFFF", "#000000", "#FF3B30", "#007AFF", "#34C759"]
                    delegate: Rectangle {
                        width: 32; height: 32; radius: 16; color: modelData
                        border.width: 2; border.color: (Bridge.penColor == modelData) ? "white" : "#444"
                        MouseArea { anchors.fill: parent; onClicked: Bridge.penColor = modelData }
                    }
                }
            }
        }
    }

    // --- 2. PENCIL ---
    Component {
        id: pencilComponent
        ColumnLayout {
            spacing: 12
            SectionLabel { text: "Dicke" }
            CustomSlider { from: 1; to: 30; value: Bridge.penWidth; onMoved: Bridge.penWidth = value }
            SectionLabel { text: "Härte" }
            CustomSlider { from: 0; to: 100; value: Bridge.hardness; onMoved: Bridge.hardness = value }
            SectionLabel { text: "Deckkraft" }
            CustomSlider { from: 0.1; to: 1.0; value: Bridge.opacity; onMoved: Bridge.opacity = value }
            CustomSwitch { text: "Neigungssensitivität"; checked: Bridge.tiltShading; onToggled: Bridge.tiltShading = checked }
            SectionLabel { text: "Textur" }
            RowLayout {
                spacing: 10
                OptionButton { text: "Fein"; checked: Bridge.texture === "Fein"; onClicked: Bridge.texture = "Fein" }
                OptionButton { text: "Grob"; checked: Bridge.texture === "Grob"; onClicked: Bridge.texture = "Grob" }
                OptionButton { text: "Kohle"; checked: Bridge.texture === "Kohle"; onClicked: Bridge.texture = "Kohle" }
            }
        }
    }

    // --- 3. HIGHLIGHTER ---
    Component {
        id: highlighterComponent
        ColumnLayout {
            spacing: 12
            SectionLabel { text: "Dicke" }
            CustomSlider { from: 10; to: 80; value: Bridge.penWidth; onMoved: Bridge.penWidth = value }
            SectionLabel { text: "Deckkraft" }
            CustomSlider { from: 0.1; to: 1.0; value: Bridge.opacity; onMoved: Bridge.opacity = value }
            CustomSwitch { text: "Gerade Linie"; checked: Bridge.smartLine; onToggled: Bridge.smartLine = checked }
            CustomSwitch { text: "Hinter Text"; checked: Bridge.drawBehind; onToggled: Bridge.drawBehind = checked }
            SectionLabel { text: "Spitze" }
            RowLayout {
                spacing: 10
                OptionButton { text: "Rund"; checked: Bridge.tipType === 0; onClicked: Bridge.tipType = 0 }
                OptionButton { text: "Abgeschrägt"; checked: Bridge.tipType === 1; onClicked: Bridge.tipType = 1 }
            }
        }
    }

    // --- 4. ERASER ---
    Component {
        id: eraserComponent
        ColumnLayout {
            spacing: 15
            RowLayout {
                Layout.fillWidth: true; spacing: 0
                OptionButton { text: "Pixel"; radius: 4; checked: Bridge.eraserMode === 0; onClicked: Bridge.eraserMode = 0 }
                OptionButton { text: "Objekt"; radius: 4; checked: Bridge.eraserMode === 1; onClicked: Bridge.eraserMode = 1 }
            }
            SectionLabel { text: "Größe" }
            CustomSlider { from: 5; to: 100; value: Bridge.penWidth; onMoved: Bridge.penWidth = value }
            CustomSwitch { text: "Nur Textmarker löschen"; checked: Bridge.eraserKeepInk; onToggled: Bridge.eraserKeepInk = checked }
            Button {
                text: "Ganze Seite leeren"; Layout.fillWidth: true
                background: Rectangle { color: "#3A2020"; border.color: "#FF3B30"; radius: 6 }
                contentItem: Text { text: parent.text; color: "#FF3B30"; horizontalAlignment: Text.AlignHCenter }
                onClicked: console.log("Clear")
            }
        }
    }

    // --- 5. LASSO ---
    Component {
        id: lassoComponent
        ColumnLayout {
            spacing: 12
            SectionLabel { text: "Modus" }
            RowLayout {
                OptionButton { text: "Freihand"; checked: Bridge.lassoMode === 0; onClicked: Bridge.lassoMode = 0 }
                OptionButton { text: "Rechteck"; checked: Bridge.lassoMode === 1; onClicked: Bridge.lassoMode = 1 }
            }
            SectionLabel { text: "Aktionen" }
            GridLayout {
                columns: 2
                OptionButton { text: "✂️ Ausschneiden"; checkable: false }
                OptionButton { text: "📋 Kopieren"; checkable: false }
                OptionButton { text: "🗑️ Löschen"; checkable: false; palette.buttonText: "#FF3B30" }
                OptionButton { text: "🎨 Farbe"; checkable: false }
            }
            CustomSwitch { text: "Seitenverhältnis sperren"; checked: Bridge.aspectLock; onToggled: Bridge.aspectLock = checked }
        }
    }

    // --- 6. IMAGE ---
    Component {
        id: imageComponent
        ColumnLayout {
            spacing: 12
            SectionLabel { text: "Deckkraft" }
            CustomSlider { from: 0.1; to: 1.0; value: Bridge.imageOpacity; onMoved: Bridge.imageOpacity = value }
            SectionLabel { text: "Filter" }
            RowLayout {
                OptionButton { text: "Original"; checked: Bridge.imageFilter === 0; onClicked: Bridge.imageFilter = 0 }
                OptionButton { text: "Grau"; checked: Bridge.imageFilter === 1; onClicked: Bridge.imageFilter = 1 }
                OptionButton { text: "Kontrast"; checked: Bridge.imageFilter === 2; onClicked: Bridge.imageFilter = 2 }
            }
            SectionLabel { text: "Rahmen" }
            RowLayout {
                spacing: 10
                CustomSlider { Layout.fillWidth: true; from: 0; to: 10; value: Bridge.borderWidth; onMoved: Bridge.borderWidth = value }
                Rectangle {
                    width: 30; height: 30; color: Bridge.borderColor; radius: 4; border.color: "#888"
                    MouseArea { anchors.fill: parent; onClicked: console.log("Border Color Picker") }
                }
            }
            SectionLabel { text: "Anordnung" }
            RowLayout {
                OptionButton { text: "⬆️ Vorne"; checkable: false }
                OptionButton { text: "⬇️ Hinten"; checkable: false }
            }
        }
    }

    // --- 7. RULER ---
    Component {
        id: rulerComponent
        ColumnLayout {
            spacing: 15
            SectionLabel { text: "Einheit" }
            RowLayout {
                OptionButton { text: "cm"; checked: Bridge.rulerUnit === 0; onClicked: Bridge.rulerUnit = 0 }
                OptionButton { text: "inch"; checked: Bridge.rulerUnit === 1; onClicked: Bridge.rulerUnit = 1 }
                OptionButton { text: "px"; checked: Bridge.rulerUnit === 2; onClicked: Bridge.rulerUnit = 2 }
            }
            CustomSwitch { text: "Einrasten (Snap)"; checked: Bridge.rulerSnap; onToggled: Bridge.rulerSnap = checked }
            CustomSwitch { text: "Kompass-Modus"; checked: Bridge.compassMode; onToggled: Bridge.compassMode = checked }
        }
    }

    // --- 8. SHAPES ---
    Component {
        id: shapeComponent
        ColumnLayout {
            spacing: 12
            SectionLabel { text: "Form" }
            GridLayout {
                columns: 3; Layout.alignment: Qt.AlignHCenter
                IconButton { text: "⬜"; checked: Bridge.shapeType === 0; onClicked: Bridge.shapeType = 0 }
                IconButton { text: "⚪"; checked: Bridge.shapeType === 1; onClicked: Bridge.shapeType = 1 }
                IconButton { text: "△"; checked: Bridge.shapeType === 2; onClicked: Bridge.shapeType = 2 }
                IconButton { text: "➜"; checked: Bridge.shapeType === 3; onClicked: Bridge.shapeType = 3 }
                IconButton { text: "★"; checked: Bridge.shapeType === 4; onClicked: Bridge.shapeType = 4 }
                IconButton { text: "📉"; checked: Bridge.shapeType === 5; onClicked: Bridge.shapeType = 5 }
            }
            SectionLabel { text: "Konturdicke" }
            CustomSlider { from: 1; to: 20; value: Bridge.penWidth; onMoved: Bridge.penWidth = value }
            SectionLabel { text: "Füllung" }
            RowLayout {
                IconButton { text: "🚫"; checked: Bridge.shapeFill === 0; onClicked: Bridge.shapeFill = 0 }
                IconButton { text: "⬛"; checked: Bridge.shapeFill === 1; onClicked: Bridge.shapeFill = 1 }
                IconButton { text: "▒"; checked: Bridge.shapeFill === 2; onClicked: Bridge.shapeFill = 2 }
            }
            ColumnLayout {
                visible: Bridge.shapeType === 5
                SectionLabel { text: "Koordinaten-Optionen" }
                CustomSwitch { text: "X-Achse"; checked: Bridge.showXAxis; onToggled: Bridge.showXAxis = checked }
                CustomSwitch { text: "Y-Achse"; checked: Bridge.showYAxis; onToggled: Bridge.showYAxis = checked }
                CustomSwitch { text: "Gitter"; checked: Bridge.showGrid; onToggled: Bridge.showGrid = checked }
            }
        }
    }

    // --- 9. STICKY NOTES ---
    Component {
        id: stickyNoteComponent
        ColumnLayout {
            spacing: 15
            SectionLabel { text: "Farbe" }
            Flow {
                Layout.fillWidth: true; spacing: 12
                Repeater {
                    model: ["#FFEB3B", "#4FC3F7", "#FF8A80", "#81C784", "#FFFFFF"]
                    delegate: Rectangle {
                        width: 36; height: 36; radius: 4; color: modelData
                        border.width: Bridge.paperColor == modelData ? 2 : 1; border.color: "#666"
                        MouseArea { anchors.fill: parent; onClicked: Bridge.paperColor = modelData }
                    }
                }
            }
            Button {
                Layout.fillWidth: true
                background: Rectangle { color: parent.down ? "#3E3E42" : "#2D2D30"; radius: 6; border.color: "#444" }
                contentItem: RowLayout {
                    spacing: 8
                    Text { text: "🔒"; color: "white" }
                    Text { text: "PIN festlegen / ändern"; color: "white"; font.pixelSize: 13 }
                    Item { Layout.fillWidth: true }
                }
                onClicked: console.log("PIN Dialog")
            }
            CustomSwitch { text: "Eingeklappt anzeigen"; checked: Bridge.collapsed; onToggled: Bridge.collapsed = checked }
        }
    }

    // --- 10. TEXT ---
    Component {
        id: textToolComponent
        ColumnLayout {
            spacing: 12
            SectionLabel { text: "Schriftart" }
            ComboBox {
                model: ["Segoe UI", "Arial", "Courier New", "Verdana"]
                Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 10
                OptionButton { text: "B"; font.bold: true; checked: Bridge.bold; onClicked: Bridge.bold = checked }
                OptionButton { text: "I"; font.italic: true; checked: Bridge.italic; onClicked: Bridge.italic = checked }
                OptionButton { text: "U"; font.underline: true; checked: Bridge.underline; onClicked: Bridge.underline = checked }
            }
            SectionLabel { text: "Größe" }
            CustomSlider { from: 8; to: 72; value: Bridge.fontSize; onMoved: Bridge.fontSize = value }
            SectionLabel { text: "Ausrichtung" }
            RowLayout {
                OptionButton { text: "L"; checked: Bridge.textAlignment === 0; onClicked: Bridge.textAlignment = 0 }
                OptionButton { text: "M"; checked: Bridge.textAlignment === 1; onClicked: Bridge.textAlignment = 1 }
                OptionButton { text: "R"; checked: Bridge.textAlignment === 2; onClicked: Bridge.textAlignment = 2 }
            }
            SectionLabel { text: "Hintergrund" }
            RowLayout {
                CustomSwitch { text: "Textfeld füllen"; checked: Bridge.fillTextBackground; onToggled: Bridge.fillTextBackground = checked; Layout.fillWidth: true }
                Rectangle {
                    width: 30; height: 30; radius: 4; color: Bridge.textBackgroundColor; border.color: "#888"
                    MouseArea { anchors.fill: parent; onClicked: console.log("Text BG Color") }
                }
            }
        }
    }

    Component { id: placeholderComponent; Item{} }

    function getToolName(mode) {
        var names = ["Füller", "Bleistift", "Textmarker", "Radierer", "Lasso", "Bild", "Lineal", "Formen", "Notiz", "Text"];
        return names[mode] || "Werkzeug";
    }
}
