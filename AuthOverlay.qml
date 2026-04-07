import QtQuick 2.15
import QtQuick.Controls 2.15
import QtWebView 1.1

Rectangle {
    id: root
    visible: true
    color: "#0F111A"
    readonly property real uiScale: Math.max(0.9, Math.min(width / 411, 1.35))
    
    // We expect the C++ side to provide "authUrl" as a context property
    
    // Header to allow the user to go back if something is wrong
    Rectangle {
        id: header
        height: Math.round(50 * uiScale)
        width: parent.width
        color: "#1F2335"
        
        Text {
            anchors.centerIn: parent
            text: "Google Login"
            color: "white"
            font.bold: true
            font.pixelSize: Math.round(16 * uiScale)
        }
    }
    
    WebView {
        id: webView
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        url: authUrl
        
        onLoadingChanged: {
            // If the login successfully finishes and redirects to the localhost callback
            if (loadRequest.url.toString().indexOf("http://127.0.0.1:8080") === 0) {
                // Let it load so the internal QOAuthHttpServerReplyHandler catches the token!
                // The C++ side will close this overlay automatically upon the granted() signal.
            }
        }
    }
}
