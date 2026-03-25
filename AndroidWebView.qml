import QtQuick 2.0
import QtQuick.Controls 2.0
import QtWebView 1.1

Item {
    // Tracks whether we're currently waiting for the OAuth flow to complete in Chrome
    property bool oauthPending: false
    property string studyUrl: "https://blop-six.vercel.app"
    property bool firstLoadDone: false

    function ensureStudyLoaded() {
        if (!firstLoadDone || webView.url.toString() === "" || webView.url.toString() === "about:blank") {
            webView.url = studyUrl
            firstLoadDone = true
        }
    }

    WebView {
        id: webView
        anchors.fill: parent
        url: "about:blank"

        // Qt 6: declare loadRequest explicitly (injected implicit parameter is deprecated)
        onLoadingChanged: function(loadRequest) {
            if (loadRequest.url.toString().indexOf("blop://google-login") === 0) {
                webView.stop()
                if (typeof blopAppBridge !== "undefined") {
                    blopAppBridge.requestGoogleLogin()
                }
            }
            // Defensive reset: if an OAuth overlay stayed visible accidentally, clear it
            // when normal website navigation happens.
            if (loadRequest.url.toString().indexOf("https://") === 0 ||
                loadRequest.url.toString().indexOf("http://") === 0) {
                oauthPending = false
            }
        }
    }

    // Delay initial URL assignment until the view is fully attached to avoid
    // occasional gray/blank starts on Android.
    Timer {
        interval: 120
        running: true
        repeat: false
        onTriggered: ensureStudyLoaded()
    }

    onVisibleChanged: {
        if (visible) {
            ensureStudyLoaded()
        }
    }

    // OAuth Waiting Overlay — shown while Chrome is open
    Rectangle {
        id: oauthOverlay
        anchors.fill: parent
        color: "#CC0B0B1A"
        visible: oauthPending
        z: 10

        Column {
            anchors.centerIn: parent
            spacing: 20

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                running: oauthPending
                width: 60; height: 60
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Google-Anmeldung läuft..."
                color: "white"
                font.pixelSize: 20
                font.bold: true
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 300
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                text: "Bitte alle Schritte im Browser abschließen\n(Account wählen & Berechtigungen bestätigen).\n\nDanach wirst du automatisch angemeldet."
                color: "#AAAACC"
                font.pixelSize: 15
            }

            // Cancel button
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 160; height: 44
                radius: 22
                color: "#2A2A44"
                border.color: "#5E5CE6"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "Abbrechen"
                    color: "#888"
                    font.pixelSize: 14
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: oauthPending = false
                }
            }
        }
    }

    Connections {
        target: typeof blopAppBridge !== "undefined" ? blopAppBridge : null

        function onInjectToken(jsCode) {
            // jsCode is pre-built by C++: sets session_id + username in localStorage, then navigates to /
            console.log("QML: Injecting session from C++ backend verification...");
            oauthPending = false;  // Hide the overlay
            webView.runJavaScript(jsCode);
        }
    }

    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: {
            var jsCode = "(function() { \n" +
                         "  window.isBlopNativeApp = true;\n" +
                         "  if (localStorage.getItem('trigger_google_login') === '1') {\n" +
                         "      localStorage.removeItem('trigger_google_login');\n" +
                         "      return 'TRIGGER_GOOGLE_LOGIN';\n" +
                         "  }\n" +
                         "  var u = localStorage.getItem('username');\n" +
                         "  var s = localStorage.getItem('session_id');\n" +
                         "  return (u && s) ? u : '';\n" +
                         "})();"

            webView.runJavaScript(jsCode, function(result) {
                if (typeof blopAppBridge !== "undefined" && result !== undefined) {
                    var resStr = result.toString().trim();
                    if (resStr === "TRIGGER_GOOGLE_LOGIN") {
                        oauthPending = true;  // Show overlay while Chrome is open
                        blopAppBridge.requestGoogleLogin();
                    } else if (resStr !== "") {
                        oauthPending = false;
                        blopAppBridge.onSessionCheck(resStr);
                    }
                }
            })
        }
    }
}
