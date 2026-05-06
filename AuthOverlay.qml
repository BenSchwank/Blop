import QtQuick 2.15
import QtQuick.Controls 2.15
import QtWebView 1.1

Rectangle {
    id: root
    visible: true
    color: "#0F111A"
    readonly property real widthScale: width / 411
    readonly property real heightScale: height / 780
    readonly property real uiScale: Math.max(0.86, Math.min(Math.min(widthScale, heightScale), 1.35))
    readonly property int safeTopInset: Math.max(0, Math.round((height - Math.min(height, parent ? parent.height : height)) * 0.5))
    readonly property int headerTopPadding: Math.max(Math.round(8 * uiScale), safeTopInset)
    
    // We expect the C++ side to provide "authUrl" as a context property
    
    // Header to allow the user to go back if something is wrong
    Rectangle {
        id: header
        height: Math.round(50 * uiScale) + headerTopPadding
        width: parent.width
        color: "#1F2335"
        
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: Math.round(headerTopPadding * 0.3)
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
