import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.folderlistmodel

ApplicationWindow {
    id: window
    visible: true
    width: 480
    height: 800
    title: "Blop"

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
        anchors.margins: Qt.platform.os === "android" ? 0 : 10 // Rand nur am PC

        // Überschrift (Nur Android, weil PC hat ja den Header oben)
        Text {
            visible: Qt.platform.os === "android"
            text: "Meine Notizen"
            color: "white"
            font.pixelSize: 28
            font.bold: true
            Layout.leftMargin: 20
            Layout.topMargin: 20
        }

        // DEINE LIST VIEW (Original-Code integriert)
        ListView {
            id: noteListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 15

            // Wichtig: Padding für bessere Touch-Bedienung
            topMargin: 10
            leftMargin: Qt.platform.os === "android" ? 10 : 30
            rightMargin: Qt.platform.os === "android" ? 10 : 30

            model: FolderListModel {
                folder: notesPath // Kommt jetzt aus main.cpp
                nameFilters: ["*.blop"]
                showDirs: false
                sortField: FolderListModel.Name
            }

            delegate: Item {
                width: ListView.view.width
                height: 100

                Rectangle {
                    anchors.fill: parent
                    color: "#252526"
                    radius: 12

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            console.log("Öffne Notiz: " + fileName)
                            // Hier später Logik zum Öffnen einbauen
                        }
                    }

                    Row {
                        spacing: 20
                        anchors.fill: parent
                        anchors.margins: 12

                        Image {
                            id: thumbnail
                            width: 70
                            height: 90
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
                                font.pixelSize: 18
                                font.family: "Segoe UI"
                            }
                            Text {
                                text: "Notiz • " + fileModified
                                color: "#888"
                                font.pixelSize: 13
                                font.family: "Segoe UI"
                            }
                        }
                    }
                }
            }
        }
    }
}
