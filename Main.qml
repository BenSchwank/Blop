import QtQuick
import QtQuick.Controls
import Qt.labs.folderlistmodel

Rectangle {
    color: "transparent"

    // WICHTIG: Diese Property erlaubt Updates von C++ aus
    property url currentFolder: notesPath

    // Signal, um Notizen zu öffnen
    signal openNote(string path)

    ListView {
        id: noteListView
        anchors.fill: parent
        anchors.leftMargin: 30
        anchors.rightMargin: 30
        anchors.topMargin: 10
        spacing: 15
        clip: true

        model: FolderListModel {
            // Wir binden uns an die dynamische Property 'currentFolder'
            folder: currentFolder
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
                        openNote("file:///" + filePath)
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
