import QtQuick 2.15
import QtWebEngine 1.10

Item {
    id: root
    readonly property real uiScale: Math.max(0.9, Math.min(width / 411, 1.35))
    readonly property int toolbarHeight: Math.round(50 * uiScale)
    
    WebEngineView {
        id: webView
        anchors.top: toolbar.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        
        // Load Blop Study (Production)
        url: "https://blop-study.vercel.app"
        
        // Development URL (only if running locally)
        // url: "http://localhost:3000"
        
        // Settings
        settings.javascriptEnabled: true
        settings.localStorageEnabled: true
        settings.pluginsEnabled: true
        
        // Loading indicator
        Rectangle {
            anchors.centerIn: parent
            width: Math.round(100 * uiScale)
            height: Math.round(100 * uiScale)
            color: "#1e1e1e"
            radius: Math.round(12 * uiScale)
            visible: webView.loading
            
            Text {
                anchors.centerIn: parent
                text: "Lädt..."
                color: "#DDD"
                font.pixelSize: Math.round(14 * uiScale)
            }
        }
        
        // Error handling
        onLoadingChanged: {
            if (loadRequest.status === WebEngineView.LoadFailedStatus) {
                console.error("Failed to load Blop Study:", loadRequest.errorString)
            }
        }
    }
    
    // Optional: Add a toolbar
    Rectangle {
        id: toolbar
        anchors.top: parent.top
        width: parent.width
        height: root.toolbarHeight
        color: "#1e1e1e"
        border.color: "#333"
        border.width: 1
        
        Row {
            anchors.centerIn: parent
            spacing: Math.round(10 * uiScale)
            
            // Back button
            Rectangle {
                width: Math.round(80 * uiScale)
                height: Math.round(30 * uiScale)
                radius: Math.round(6 * uiScale)
                color: webView.canGoBack ? "#5E5CE6" : "#333"
                
                Text {
                    anchors.centerIn: parent
                    text: "← Zurück"
                    color: "white"
                    font.pixelSize: Math.round(12 * uiScale)
                }
                
                MouseArea {
                    anchors.fill: parent
                    enabled: webView.canGoBack
                    onClicked: webView.goBack()
                }
            }
            
            // Forward button
            Rectangle {
                width: Math.round(80 * uiScale)
                height: Math.round(30 * uiScale)
                radius: Math.round(6 * uiScale)
                color: webView.canGoForward ? "#5E5CE6" : "#333"
                
                Text {
                    anchors.centerIn: parent
                    text: "Vor →"
                    color: "white"
                    font.pixelSize: Math.round(12 * uiScale)
                }
                
                MouseArea {
                    anchors.fill: parent
                    enabled: webView.canGoForward
                    onClicked: webView.goForward()
                }
            }
            
            // Reload button
            Rectangle {
                width: Math.round(80 * uiScale)
                height: Math.round(30 * uiScale)
                radius: Math.round(6 * uiScale)
                color: "#5E5CE6"
                
                Text {
                    anchors.centerIn: parent
                    text: "↻ Neu laden"
                    color: "white"
                    font.pixelSize: Math.round(12 * uiScale)
                }
                
                MouseArea {
                    anchors.fill: parent
                    onClicked: webView.reload()
                }
            }
        }
    }
    
}
