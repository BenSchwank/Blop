import QtQuick 2.15
import QtWebEngine 1.10

Item {
    id: root
    
    WebEngineView {
        id: webView
        anchors.fill: parent
        
        // Load Blop Study (Local Development)
        url: "http://localhost:3000"
        
        // Production URL (after deployment)
        // url: "https://blop-study.vercel.app"
        
        // Settings
        settings.javascriptEnabled: true
        settings.localStorageEnabled: true
        settings.pluginsEnabled: true
        
        // Loading indicator
        Rectangle {
            anchors.centerIn: parent
            width: 100
            height: 100
            color: "#1e1e1e"
            radius: 12
            visible: webView.loading
            
            Text {
                anchors.centerIn: parent
                text: "Lädt..."
                color: "#DDD"
                font.pixelSize: 14
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
        height: 50
        color: "#1e1e1e"
        border.color: "#333"
        border.width: 1
        
        Row {
            anchors.centerIn: parent
            spacing: 10
            
            // Back button
            Rectangle {
                width: 80
                height: 30
                radius: 6
                color: webView.canGoBack ? "#5E5CE6" : "#333"
                
                Text {
                    anchors.centerIn: parent
                    text: "← Zurück"
                    color: "white"
                    font.pixelSize: 12
                }
                
                MouseArea {
                    anchors.fill: parent
                    enabled: webView.canGoBack
                    onClicked: webView.goBack()
                }
            }
            
            // Forward button
            Rectangle {
                width: 80
                height: 30
                radius: 6
                color: webView.canGoForward ? "#5E5CE6" : "#333"
                
                Text {
                    anchors.centerIn: parent
                    text: "Vor →"
                    color: "white"
                    font.pixelSize: 12
                }
                
                MouseArea {
                    anchors.fill: parent
                    enabled: webView.canGoForward
                    onClicked: webView.goForward()
                }
            }
            
            // Reload button
            Rectangle {
                width: 80
                height: 30
                radius: 6
                color: "#5E5CE6"
                
                Text {
                    anchors.centerIn: parent
                    text: "↻ Neu laden"
                    color: "white"
                    font.pixelSize: 12
                }
                
                MouseArea {
                    anchors.fill: parent
                    onClicked: webView.reload()
                }
            }
        }
    }
    
    // Adjust WebView to account for toolbar
    Component.onCompleted: {
        webView.anchors.topMargin = toolbar.height
    }
}
