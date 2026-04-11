import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

Rectangle {
    id: root
    readonly property real uiScale: Math.max(0.9, Math.min((parent ? parent.width : 411) / 411, 1.35))
    width: Math.min(Math.round((parent ? parent.width : 411) * 0.86), Math.round(340 * uiScale))
    height: (parent ? parent.height : 900) - Math.round(20 * uiScale)
    color: "#1E1E1E"
    radius: Math.round(12 * uiScale)
    border.color: "#333"
    border.width: 1
    visible: false

    function toggle() { visible = !visible }

    // --- Helper Components ---
    component SliderRow: ColumnLayout {
        property string text
        property alias value: slider.value
        property alias from: slider.from
        property alias to: slider.to
        spacing: 2
        RowLayout {
            Layout.fillWidth: true
            Text { text: parent.text; color: "#DDD"; font.pixelSize: Math.round(12 * root.uiScale) }
            Item { Layout.fillWidth: true }
            Text { text: slider.value.toFixed(0); color: "#888"; font.pixelSize: Math.round(12 * root.uiScale) }
        }
        Slider {
            id: slider
            Layout.fillWidth: true
            handle: Rectangle {
                x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                width: Math.round(16 * root.uiScale); height: Math.round(16 * root.uiScale); radius: width / 2; color: "white"
            }
            background: Rectangle {
                x: slider.leftPadding; y: slider.topPadding + slider.availableHeight / 2 - height / 2
                width: slider.availableWidth; height: Math.round(4 * root.uiScale); radius: height / 2; color: "#444"
                Rectangle { width: slider.visualPosition * parent.width; height: parent.height; radius: parent.radius; color: "#5E5CE6" }
            }
        }
    }

    component CustomSwitch: RowLayout {
        property alias text: lbl.text
        property alias checked: sw.checked
        Layout.fillWidth: true
        Text { id: lbl; color: "#DDD"; font.pixelSize: Math.round(14 * root.uiScale); Layout.fillWidth: true }
        Switch {
            id: sw
            indicator: Rectangle {
                width: Math.round(40 * root.uiScale); height: Math.round(20 * root.uiScale); radius: height / 2
                color: sw.checked ? "#5E5CE6" : "#444"
                border.color: "#333"
                Rectangle {
                    x: sw.checked ? parent.width - width - 2 : 2
                    y: Math.round(2 * root.uiScale); width: Math.round(16 * root.uiScale); height: Math.round(16 * root.uiScale); radius: width / 2; color: "white"
                    Behavior on x { NumberAnimation { duration: 150 } }
                }
            }
        }
    }

    component SectionLabel: Text {
        color: "#888"; font.bold: true; font.pixelSize: Math.round(11 * root.uiScale); font.capitalization: Font.AllUppercase
        Layout.topMargin: Math.round(10 * root.uiScale)
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: Math.round(15 * root.uiScale)
        contentWidth: parent.width - 30

        ColumnLayout {
            width: parent.width
            spacing: 20

            Text {
                text: "Werkzeug Optionen"
                color: "white"
                font.pixelSize: Math.round(18 * root.uiScale)
                font.bold: true
            }

            Loader {
                Layout.fillWidth: true
                sourceComponent: {
                    var __refresh = Bridge.settingsRevision
                    switch(Bridge.activeToolMode) {
                        case 0: return penComponent
                        case 1: return pencilComponent
                        case 2: return highlighterComponent
                        case 3: return eraserComponent
                        case 4: return lassoComponent
                        case 5: return imageComponent
                        case 6: return rulerComponent
                        case 7: return shapeComponent
                        default: return null
                    }
                }
            }
        }
    }

    // --- Components ---
    Component {
        id: penComponent
        ColumnLayout { spacing: 12
            SliderRow { text: "Stiftgröße"; from: 1; to: 50; value: Bridge.penWidth; onValueChanged: Bridge.penWidth = value }
            SliderRow { text: "Deckkraft"; from: 1; to: 100; value: Bridge.opacity * 100; onValueChanged: Bridge.opacity = value/100 }
            SliderRow { text: "Glättung"; from: 0; to: 100; value: Bridge.smoothing; onValueChanged: Bridge.smoothing = value }
            CustomSwitch { text: "Druckempfindlichkeit"; checked: Bridge.pressure; onToggled: Bridge.pressure = checked }
            SectionLabel { text: "Halten → Form" }
            SliderRow { text: "Empfindlichkeit"; from: 0; to: 100; value: Bridge.holdShapeSensitivity; onValueChanged: Bridge.holdShapeSensitivity = value }
            SliderRow { text: "Haltezeit (ms)"; from: 200; to: 900; value: Bridge.holdStillDelayMs; onValueChanged: Bridge.holdStillDelayMs = value }
            CustomSwitch { text: "Kreis-Erkennung"; checked: Bridge.holdEnableCircle; onToggled: Bridge.holdEnableCircle = checked }
            CustomSwitch { text: "Dreieck-Erkennung"; checked: Bridge.holdEnableTriangle; onToggled: Bridge.holdEnableTriangle = checked }
        }
    }

    Component {
        id: pencilComponent
        ColumnLayout { spacing: 12
            SliderRow { text: "Größe"; from: 1; to: 30; value: Bridge.penWidth; onValueChanged: Bridge.penWidth = value }
            SliderRow { text: "Härte"; from: 0; to: 100; value: Bridge.hardness; onValueChanged: Bridge.hardness = value }
            CustomSwitch { text: "Neigungsschattierung"; checked: Bridge.tiltShading; onToggled: Bridge.tiltShading = checked }
            SectionLabel { text: "Halten → Form" }
            SliderRow { text: "Empfindlichkeit"; from: 0; to: 100; value: Bridge.holdShapeSensitivity; onValueChanged: Bridge.holdShapeSensitivity = value }
            SliderRow { text: "Haltezeit (ms)"; from: 200; to: 900; value: Bridge.holdStillDelayMs; onValueChanged: Bridge.holdStillDelayMs = value }
            CustomSwitch { text: "Kreis-Erkennung"; checked: Bridge.holdEnableCircle; onToggled: Bridge.holdEnableCircle = checked }
            CustomSwitch { text: "Dreieck-Erkennung"; checked: Bridge.holdEnableTriangle; onToggled: Bridge.holdEnableTriangle = checked }
        }
    }

    Component {
        id: highlighterComponent
        ColumnLayout { spacing: 12
            SliderRow { text: "Größe"; from: 5; to: 80; value: Bridge.penWidth; onValueChanged: Bridge.penWidth = value }
            CustomSwitch { text: "Gerade Linien"; checked: Bridge.smartLine; onToggled: Bridge.smartLine = checked }
            CustomSwitch { text: "Hinter Zeichnung malen"; checked: Bridge.drawBehind; onToggled: Bridge.drawBehind = checked }
            SectionLabel { text: "Spitze" }
            RowLayout { spacing: 10
                Repeater { model: ["Rund", "Keil"]
                    delegate: Rectangle { width: Math.round(80 * root.uiScale); height: Math.round(30 * root.uiScale); radius: Math.round(6 * root.uiScale); color: (Bridge.highlighterTip === index) ? "#5E5CE6" : "#333"
                        Text { anchors.centerIn: parent; text: modelData; color: "white" }
                        MouseArea { anchors.fill: parent; onClicked: Bridge.highlighterTip = index }
                    }
                }
            }
        }
    }

    Component {
        id: eraserComponent
        ColumnLayout { spacing: 12
            SectionLabel { text: "Modus" }
            RowLayout { spacing: 10
                Repeater { model: ["Pixel", "Objekt"]
                    delegate: Rectangle { width: Math.round(80 * root.uiScale); height: Math.round(30 * root.uiScale); radius: Math.round(6 * root.uiScale); color: (Bridge.eraserMode === index) ? "#5E5CE6" : "#333"
                        Text { anchors.centerIn: parent; text: modelData; color: "white" }
                        MouseArea { anchors.fill: parent; onClicked: Bridge.eraserMode = index }
                    }
                }
            }
            SliderRow { text: "Größe"; from: 5; to: 100; value: Bridge.penWidth; visible: Bridge.eraserMode === 0; onValueChanged: Bridge.penWidth = value }
            CustomSwitch { text: "Nur Tinte löschen"; checked: Bridge.eraserKeepInk; onToggled: Bridge.eraserKeepInk = checked }
        }
    }

    Component {
        id: lassoComponent
        ColumnLayout { spacing: 12
            SectionLabel { text: "Auswahl-Modus" }
            RowLayout { spacing: 10
                Repeater { model: ["Freihand", "Rechteck"]
                    delegate: Rectangle { width: Math.round(80 * root.uiScale); height: Math.round(30 * root.uiScale); radius: Math.round(6 * root.uiScale); color: (Bridge.lassoMode === index) ? "#5E5CE6" : "#333"
                        Text { anchors.centerIn: parent; text: modelData; color: "white" }
                        MouseArea { anchors.fill: parent; onClicked: Bridge.lassoMode = index }
                    }
                }
            }
            CustomSwitch { text: "Seitenverhältnis sperren"; checked: Bridge.aspectLock; onToggled: Bridge.aspectLock = checked }
        }
    }

    Component { id: imageComponent; Item {} }

    Component {
        id: shapeComponent
        ColumnLayout { spacing: 12
            SliderRow { text: "Dicke"; from: 1; to: 50; value: Bridge.penWidth; onValueChanged: Bridge.penWidth = value }
            SliderRow { text: "Deckkraft"; from: 1; to: 100; value: Bridge.opacity * 100; onValueChanged: Bridge.opacity = value/100 }
            SectionLabel { text: "Form" }
            RowLayout { spacing: 6
                Repeater { model: ["Rechteck", "Kreis", "Achsen", "Sinus", "Graph"]
                    delegate: Rectangle { implicitWidth: Math.round(72 * root.uiScale); implicitHeight: Math.round(28 * root.uiScale); radius: Math.round(6 * root.uiScale); color: (Bridge.shapeToolKind === index) ? "#5E5CE6" : "#333"
                        Text { anchors.centerIn: parent; text: modelData; color: "white"; font.pixelSize: Math.round(10 * root.uiScale) }
                        MouseArea { anchors.fill: parent; onClicked: Bridge.shapeToolKind = index }
                    }
                }
            }
            ColumnLayout {
                visible: Bridge.shapeToolKind === 2
                Layout.fillWidth: true
                SliderRow { Layout.fillWidth: true; text: "Achsen: Teilstriche"; from: 2; to: 12; value: Bridge.shapeAxisTicks; onValueChanged: Bridge.shapeAxisTicks = value }
            }
            ColumnLayout {
                visible: Bridge.shapeToolKind === 3
                Layout.fillWidth: true
                spacing: 8
                CustomSwitch {
                    text: "Feste Parameter verwenden"
                    checked: Bridge.shapeSineFixedParams
                    onToggled: Bridge.shapeSineFixedParams = checked
                }
                Text {
                    visible: !Bridge.shapeSineFixedParams
                    text: "Flexibel: Aufziehen skaliert Amplitude und Perioden mit dem Rechteck."
                    color: "#888"
                    font.pixelSize: Math.round(11 * root.uiScale)
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Text {
                    visible: Bridge.shapeSineFixedParams
                    text: "Fest: Breiter ziehen zeigt mehr Perioden (b je 100 px); Amplitude skaliert nicht mit der Höhe."
                    color: "#888"
                    font.pixelSize: Math.round(11 * root.uiScale)
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                ColumnLayout {
                    visible: Bridge.shapeSineFixedParams
                    Layout.fillWidth: true
                    spacing: 8
                Text {
                    text: "y = Mitte − (a·45·sin(b·2π·(x−x links)/100 + c) + d·45); b = Perioden je 100 px"
                    color: "#888"
                    font.pixelSize: Math.round(11 * root.uiScale)
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Connections {
                    target: Bridge
                    function onConfigChanged() {
                        if (!tfShapeA.activeFocus) tfShapeA.text = Bridge.shapeMathA.toFixed(4)
                        if (!tfShapeB.activeFocus) tfShapeB.text = Bridge.shapeMathB.toFixed(4)
                        if (!tfShapeC.activeFocus) tfShapeC.text = Bridge.shapeMathC.toFixed(4)
                        if (!tfShapeD.activeFocus) tfShapeD.text = Bridge.shapeMathD.toFixed(4)
                    }
                    function onSettingsRevisionChanged() {
                        if (!tfShapeA.activeFocus) tfShapeA.text = Bridge.shapeMathA.toFixed(4)
                        if (!tfShapeB.activeFocus) tfShapeB.text = Bridge.shapeMathB.toFixed(4)
                        if (!tfShapeC.activeFocus) tfShapeC.text = Bridge.shapeMathC.toFixed(4)
                        if (!tfShapeD.activeFocus) tfShapeD.text = Bridge.shapeMathD.toFixed(4)
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "a"; color: "#DDD"; font.pixelSize: Math.round(12 * root.uiScale); Layout.preferredWidth: Math.round(22 * root.uiScale) }
                    TextField {
                        id: tfShapeA
                        Layout.fillWidth: true
                        horizontalAlignment: TextInput.AlignRight
                        padding: Math.round(6 * root.uiScale)
                        color: "#EEE"
                        font.pixelSize: Math.round(13 * root.uiScale)
                        background: Rectangle { radius: Math.round(4 * root.uiScale); color: "#333"; border.color: "#555"; border.width: 1 }
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        validator: DoubleValidator { bottom: 0.01; top: 2.0; decimals: 4; notation: DoubleValidator.StandardNotation }
                        onEditingFinished: {
                            var v = parseFloat(text.replace(",", "."))
                            if (!isNaN(v)) Bridge.shapeMathA = v
                            text = Bridge.shapeMathA.toFixed(4)
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "b"; color: "#DDD"; font.pixelSize: Math.round(12 * root.uiScale); Layout.preferredWidth: Math.round(22 * root.uiScale) }
                    TextField {
                        id: tfShapeB
                        Layout.fillWidth: true
                        horizontalAlignment: TextInput.AlignRight
                        padding: Math.round(6 * root.uiScale)
                        color: "#EEE"
                        font.pixelSize: Math.round(13 * root.uiScale)
                        background: Rectangle { radius: Math.round(4 * root.uiScale); color: "#333"; border.color: "#555"; border.width: 1 }
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        validator: DoubleValidator { bottom: 0.05; top: 12.0; decimals: 4; notation: DoubleValidator.StandardNotation }
                        onEditingFinished: {
                            var v = parseFloat(text.replace(",", "."))
                            if (!isNaN(v)) Bridge.shapeMathB = v
                            text = Bridge.shapeMathB.toFixed(4)
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "c"; color: "#DDD"; font.pixelSize: Math.round(12 * root.uiScale); Layout.preferredWidth: Math.round(22 * root.uiScale) }
                    TextField {
                        id: tfShapeC
                        Layout.fillWidth: true
                        horizontalAlignment: TextInput.AlignRight
                        padding: Math.round(6 * root.uiScale)
                        color: "#EEE"
                        font.pixelSize: Math.round(13 * root.uiScale)
                        background: Rectangle { radius: Math.round(4 * root.uiScale); color: "#333"; border.color: "#555"; border.width: 1 }
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        validator: DoubleValidator { bottom: -12.57; top: 12.57; decimals: 4; notation: DoubleValidator.StandardNotation }
                        onEditingFinished: {
                            var v = parseFloat(text.replace(",", "."))
                            if (!isNaN(v)) Bridge.shapeMathC = v
                            text = Bridge.shapeMathC.toFixed(4)
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "d"; color: "#DDD"; font.pixelSize: Math.round(12 * root.uiScale); Layout.preferredWidth: Math.round(22 * root.uiScale) }
                    TextField {
                        id: tfShapeD
                        Layout.fillWidth: true
                        horizontalAlignment: TextInput.AlignRight
                        padding: Math.round(6 * root.uiScale)
                        color: "#EEE"
                        font.pixelSize: Math.round(13 * root.uiScale)
                        background: Rectangle { radius: Math.round(4 * root.uiScale); color: "#333"; border.color: "#555"; border.width: 1 }
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        validator: DoubleValidator { bottom: -1.5; top: 1.5; decimals: 4; notation: DoubleValidator.StandardNotation }
                        onEditingFinished: {
                            var v = parseFloat(text.replace(",", "."))
                            if (!isNaN(v)) Bridge.shapeMathD = v
                            text = Bridge.shapeMathD.toFixed(4)
                        }
                    }
                }
                Component.onCompleted: {
                    tfShapeA.text = Bridge.shapeMathA.toFixed(4)
                    tfShapeB.text = Bridge.shapeMathB.toFixed(4)
                    tfShapeC.text = Bridge.shapeMathC.toFixed(4)
                    tfShapeD.text = Bridge.shapeMathD.toFixed(4)
                }
                }
            }
            ColumnLayout {
                visible: false
                Layout.fillWidth: true
                spacing: 8
                Text { text: "Koordinatensystem"; color: "#DDD"; font.bold: true }
                RowLayout {
                    Layout.fillWidth: true
                    TextField { id: graphExpr; Layout.fillWidth: true; placeholderText: "f(x) oder y=..."; color: "#EEE" }
                    Rectangle {
                        width: Math.round(30 * root.uiScale); height: Math.round(30 * root.uiScale); radius: 6; color: "#5E5CE6"
                        Text { anchors.centerIn: parent; text: "+"; color: "white"; font.bold: true }
                        MouseArea { anchors.fill: parent; onClicked: {
                            Bridge.addGraphFunction(graphExpr.text)
                            graphExpr.text = ""
                        } }
                    }
                }
                Repeater {
                    model: Bridge.graphFunctionExpressions
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: Math.round(34 * root.uiScale)
                        radius: 6
                        color: Bridge.graphSelectedFunction === index ? "#3E3A7A" : "#2a2a2a"
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 6
                            Text { text: modelData; color: "white"; Layout.fillWidth: true; elide: Text.ElideRight }
                            MouseArea { anchors.fill: parent; onClicked: Bridge.graphSelectedFunction = index }
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Rectangle { Layout.fillWidth: true; implicitHeight: 28; radius: 6; color: "#333"
                        Text { anchors.centerIn: parent; text: "Ableitung"; color: "white" }
                        MouseArea { anchors.fill: parent; onClicked: Bridge.setGraphFunctionFlag(Bridge.graphSelectedFunction, "derivative", !Bridge.graphFunctionFlag(Bridge.graphSelectedFunction, "derivative")) }
                    }
                    Rectangle { Layout.fillWidth: true; implicitHeight: 28; radius: 6; color: "#333"
                        Text { anchors.centerIn: parent; text: "Nullstellen"; color: "white" }
                        MouseArea { anchors.fill: parent; onClicked: Bridge.setGraphFunctionFlag(Bridge.graphSelectedFunction, "roots", !Bridge.graphFunctionFlag(Bridge.graphSelectedFunction, "roots")) }
                    }
                    Rectangle { Layout.fillWidth: true; implicitHeight: 28; radius: 6; color: "#333"
                        Text { anchors.centerIn: parent; text: "Extrema"; color: "white" }
                        MouseArea { anchors.fill: parent; onClicked: Bridge.setGraphFunctionFlag(Bridge.graphSelectedFunction, "extrema", !Bridge.graphFunctionFlag(Bridge.graphSelectedFunction, "extrema")) }
                    }
                }
            }
            SectionLabel { text: "Halten → Form" }
            SliderRow { text: "Empfindlichkeit"; from: 0; to: 100; value: Bridge.holdShapeSensitivity; onValueChanged: Bridge.holdShapeSensitivity = value }
            SliderRow { text: "Haltezeit (ms)"; from: 200; to: 900; value: Bridge.holdStillDelayMs; onValueChanged: Bridge.holdStillDelayMs = value }
            CustomSwitch { text: "Kreis-Erkennung"; checked: Bridge.holdEnableCircle; onToggled: Bridge.holdEnableCircle = checked }
            CustomSwitch { text: "Dreieck-Erkennung"; checked: Bridge.holdEnableTriangle; onToggled: Bridge.holdEnableTriangle = checked }
        }
    }

    Component {
        id: rulerComponent
        ColumnLayout { spacing: 12
            SectionLabel { text: "Lineal Optionen" }
            CustomSwitch { text: "Am Lineal einrasten"; checked: Bridge.rulerSnap; onToggled: Bridge.rulerSnap = checked }
            CustomSwitch { text: "Rundes Lineal"; checked: Bridge.compassMode; onToggled: Bridge.compassMode = checked }
            CustomSwitch { text: "Unendliches Lineal"; checked: Bridge.infiniteRuler; onToggled: Bridge.infiniteRuler = checked }
            SectionLabel { text: "Einheiten" }
            RowLayout { spacing: 10
                Repeater { model: ["px", "cm", "in"]
                    delegate: Rectangle { width: Math.round(60 * root.uiScale); height: Math.round(35 * root.uiScale); radius: Math.round(6 * root.uiScale); color: (Bridge.rulerUnit === index) ? "#5E5CE6" : "#333"
                        Text { anchors.centerIn: parent; text: modelData; color: "white"; font.bold: true }
                        MouseArea { anchors.fill: parent; onClicked: Bridge.rulerUnit = index }
                    }
                }
            }
            Label {
                text: "Info: Wähle das Lineal-Werkzeug, um es zu bewegen. Wähle einen Stift, um daran zu zeichnen."
                color: "#888"; font.pixelSize: Math.round(12 * root.uiScale); wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.topMargin: Math.round(10 * root.uiScale)
            }
        }
    }
}
