import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.folderlistmodel
// WICHTIG: Importiert den Ordner "ui", wo ToolSettingsOverlay.qml liegt
import "ui"

ApplicationWindow {
    id: window
    visible: true
    width: 480
    height: 800
    title: "Blop"
    readonly property bool isAndroid: Qt.platform.os === "android"
    readonly property real uiScale: isAndroid ? Math.max(0.9, Math.min(width / 411, 1.35)) : 1.0
    readonly property int pad: Math.round(12 * uiScale)
    readonly property int cardRadius: Math.round(12 * uiScale)

    // --- AUTOMATIK: Android = Vollbild, Windows = Fenster ---
    visibility: Qt.platform.os === "android" ? Window.FullScreen : Window.Windowed

    // Hintergrundfarbe der App
    color: "#1e1e1e"

    // --- HEADER: Nur auf Windows sichtbar ---
    header: ToolBar {
        visible: Qt.platform.os !== "android"
        height: visible ? implicitHeight : 0
        background: Rectangle { color: "#333333" }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            Label {
                text: "Blop Desktop"
                color: "white"
                font.bold: true
                Layout.fillWidth: true
            }
            ToolButton {
                text: "Exit"
                onClicked: Qt.quit()
            }
        }
    }

    // --- INHALT: Deine Dateiliste ---
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: window.isAndroid ? 0 : window.pad

        // Überschrift (Nur Android, weil PC hat ja den Header oben)
        Text {
            visible: window.isAndroid
            text: "Meine Notizen"
            color: "white"
            font.pixelSize: Math.round(28 * window.uiScale)
            font.bold: true
            Layout.leftMargin: Math.round(20 * window.uiScale)
            Layout.topMargin: Math.round(20 * window.uiScale)
        }

        // DEINE LIST VIEW (Original-Code integriert)
        ListView {
            id: noteListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Math.round(12 * window.uiScale)

            // Wichtig: Padding für bessere Touch-Bedienung
            topMargin: Math.round(10 * window.uiScale)
            leftMargin: window.isAndroid ? Math.round(10 * window.uiScale) : 30
            rightMargin: window.isAndroid ? Math.round(10 * window.uiScale) : 30

            model: FolderListModel {
                folder: notesPath // Kommt aus main.cpp (Context Property)
                nameFilters: ["*.blop"]
                showDirs: false
                sortField: FolderListModel.Name
            }

            delegate: Item {
                width: ListView.view.width
                height: Math.max(Math.round(88 * window.uiScale), width * 0.18)

                Rectangle {
                    anchors.fill: parent
                    color: "#252526"
                    radius: window.cardRadius

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            console.log("Öffne Notiz: " + fileName)
                            // Hier Logik zum Öffnen einbauen (z.B. Signal an C++)
                        }
                    }

                    Row {
                        spacing: Math.round(16 * window.uiScale)
                        anchors.fill: parent
                        anchors.margins: Math.round(12 * window.uiScale)

                        Image {
                            id: thumbnail
                            width: Math.max(56, Math.round(70 * window.uiScale))
                            height: Math.max(72, Math.round(90 * window.uiScale))
                            fillMode: Image.PreserveAspectFit
                            // Image Provider Aufruf
                            source: "image://blop/" + filePath
                            asynchronous: true
                            cache: true

                            Rectangle {
                                anchors.fill: parent
                                color: "#333"
                                visible: thumbnail.status === Image.Loading
                                radius: 4
                            }
                        }

                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 8

                            Text {
                                text: fileName
                                color: "white"
                                font.bold: true
                                font.pixelSize: Math.round(18 * window.uiScale)
                                font.family: "Segoe UI"
                            }
                            Text {
                                text: "Notiz • " + fileModified
                                color: "#888"
                                font.pixelSize: Math.round(13 * window.uiScale)
                                font.family: "Segoe UI"
                            }
                        }
                    }
                }
            }
        }
    }

    // --- NEU: Das Einstellungs-Overlay ---
    // Es ist unsichtbar, bis "Bridge.settingsVisible" true wird.
    // Da es ein Popup ist, legt es sich automatisch über alles andere.
    ToolSettingsOverlay {
        id: settingsUI
    }
}
