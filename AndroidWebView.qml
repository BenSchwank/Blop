import QtQuick 2.0
import QtQuick.Controls 2.0
import QtWebView 1.1

Item {
    // Tracks whether we're currently waiting for the OAuth flow to complete in Chrome
    property bool oauthPending: false
    property string studyUrl: "https://blop-six.vercel.app"
    property bool firstLoadDone: false
    // When false, embedded page is a user bookmark — disable Study SSO polling / Google bridge.
    property bool ssoPollingEnabled: true
    // Slight zoom-out so auth forms fit better without vertical scrolling.
    property real authUiScale: 0.86
    readonly property real uiScale: Math.max(0.9, Math.min(width / 411, 1.35))

    // Called from C++ (MainWindow::invokeAndroidWebDestination) — must match invokeMethod name.
    function setWebDestination(kind, urlStr) {
        var k = Number(kind)
        if (k === 0) {
            ssoPollingEnabled = true
            oauthPending = false
            firstLoadDone = true
            webView.url = studyUrl
            applyAuthUiScale()
        } else {
            ssoPollingEnabled = false
            oauthPending = false
            if (urlStr && String(urlStr).length > 0)
                webView.url = urlStr
        }
    }

    function applyAuthUiScale() {
        if (!ssoPollingEnabled)
            return
        var jsCode = "(function() {" +
                     "  try {" +
                     "    var bodyText = (document.body && document.body.innerText ? document.body.innerText : '').toLowerCase();" +
                     "    var path = (location.pathname || '').toLowerCase();" +
                     "    var authLike = path.indexOf('login') !== -1 || path.indexOf('register') !== -1 || bodyText.indexOf('passwort') !== -1 || bodyText.indexOf('benutzername') !== -1;" +
                     "    if (authLike) {" +
                     "      document.documentElement.style.zoom = '" + authUiScale + "';" +
                     "      if (document.body) document.body.style.zoom = '" + authUiScale + "';" +
                     "    } else {" +
                     "      document.documentElement.style.zoom = '0.94';" +
                     "      if (document.body) document.body.style.zoom = '0.94';" +
                     "    }" +
                     "  } catch(e) {}" +
                     "  return true;" +
                     "})();";
        webView.runJavaScript(jsCode);
    }

    function ensureStudyLoaded() {
        if (!ssoPollingEnabled)
            return
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
            if (ssoPollingEnabled && loadRequest.url.toString().indexOf("blop://google-login") === 0) {
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
                applyAuthUiScale()
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
            applyAuthUiScale()
        }
    }

    // OAuth Waiting Overlay — shown while Chrome is open
    Rectangle {
        id: oauthOverlay
        anchors.fill: parent
        color: "#CC0B0B1A"
        visible: oauthPending && ssoPollingEnabled
        z: 10

        Flickable {
            anchors.fill: parent
            contentWidth: width
            contentHeight: waitingColumn.implicitHeight + Math.round(48 * uiScale)
            clip: true

            Column {
                id: waitingColumn
                width: Math.min(parent.width - Math.round(24 * uiScale), Math.round(420 * uiScale))
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.height > implicitHeight ? parent.verticalCenter : undefined
                anchors.top: parent.height > implicitHeight ? undefined : parent.top
                anchors.topMargin: Math.round(24 * uiScale)
                spacing: Math.round(20 * uiScale)

                BusyIndicator {
                    anchors.horizontalCenter: parent.horizontalCenter
                    running: oauthPending
                    width: Math.round(60 * uiScale)
                    height: Math.round(60 * uiScale)
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Google-Anmeldung läuft..."
                    color: "white"
                    font.pixelSize: Math.round(20 * uiScale)
                    font.bold: true
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    text: "Bitte alle Schritte im Browser abschließen\n(Account wählen & Berechtigungen bestätigen).\n\nDanach wirst du automatisch angemeldet."
                    color: "#AAAACC"
                    font.pixelSize: Math.round(15 * uiScale)
                }

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(parent.width, Math.round(200 * uiScale))
                    height: Math.round(44 * uiScale)
                    radius: height / 2
                    color: "#2A2A44"
                    border.color: "#5E5CE6"
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "Abbrechen"
                        color: "#888"
                        font.pixelSize: Math.round(14 * uiScale)
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: oauthPending = false
                    }
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
        running: ssoPollingEnabled
        repeat: true
        onTriggered: {
            if (!ssoPollingEnabled)
                return
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
                if (!ssoPollingEnabled)
                    return
                if (typeof blopAppBridge !== "undefined" && result !== undefined) {
                    var resStr = result.toString().trim();
                    if (resStr === "TRIGGER_GOOGLE_LOGIN") {
                        oauthPending = true;  // Show overlay while Chrome is open
                        blopAppBridge.requestGoogleLogin();
                    } else if (resStr !== "") {
                        oauthPending = false;
                        blopAppBridge.onSessionCheck(resStr);
                    } else {
                        // Keep the login/register page slightly zoomed out.
                        applyAuthUiScale();
                    }
                }
            })
        }
    }
}
