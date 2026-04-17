import QtQuick 2.0
import QtQuick.Controls 2.0
import QtWebView 1.1

Rectangle {
    id: studyRoot
    color: "#0B0B1A"
    // Tracks whether we're currently waiting for the OAuth flow to complete in Chrome
    property bool oauthPending: false
    property string studyUrl: "https://blop-study.com"
    property string studyUrlFallback: "https://blop-study.com"
    property bool firstLoadDone: false
    // When false, embedded page is a user bookmark — disable Study SSO polling / Google bridge.
    property bool ssoPollingEnabled: true
    // Slight zoom-out so auth forms fit better without vertical scrolling.
    property real authUiScale: 0.94
    property bool cacheMissRecoveryArmed: true
    property int cacheMissRecoveryCount: 0
    readonly property int cacheMissRecoveryLimit: 2
    property bool nativeResetPending: false
    property double lastNativeWebViewResetMs: 0
    readonly property int nativeWebViewResetMinIntervalMs: 25000
    property double lastCacheMissRecoveryMs: 0
    property int webViewRecreateCount: 0
    readonly property int webViewRecreateLimit: 1
    property bool webviewRecreatePending: false
    readonly property real uiScale: Math.max(0.9, Math.min(width / 411, 1.35))
    readonly property int topBarHeight: Math.round(48 * uiScale)
    readonly property int topBarTopMargin: Math.round(8 * uiScale)

    function studyWeb() {
        return studyWebLoader.item
    }

    function buildFreshStudyEntryUrl() {
        var base = studyUrl
        if (base.length > 0 && base.charAt(base.length - 1) === "/")
            base = base.slice(0, -1)
        // Native entry: StudyFlow treats ?native=1 as embedded-app mode (headers, redirects).
        return base + "/?native=1"
    }

    // Called from C++ (MainWindow::invokeAndroidWebDestination) — must match invokeMethod name.
    function setWebDestination(kind, urlStr) {
        var k = Number(kind)
        if (k === 0) {
            ssoPollingEnabled = true
            oauthPending = false
            firstLoadDone = true
            cacheMissRecoveryArmed = true
            cacheMissRecoveryCount = 0
            webViewRecreateCount = 0
            webviewRecreatePending = false
            lastNativeWebViewResetMs = 0
            var w0 = studyWeb()
            if (w0)
                w0.url = buildFreshStudyEntryUrl()
            applyAuthUiScale()
        } else {
            ssoPollingEnabled = false
            oauthPending = false
            if (urlStr && String(urlStr).length > 0) {
                var w1 = studyWeb()
                if (w1)
                    w1.url = urlStr
            }
        }
    }

    function applyAuthUiScale() {
        if (!ssoPollingEnabled)
            return
        var w = studyWeb()
        if (!w)
            return
        var jsCode = "(function() {" +
                     "  try {" +
                     "    document.documentElement.style.backgroundColor = '#0B0B1A';" +
                     "    if (document.body) document.body.style.backgroundColor = '#0B0B1A';" +
                     "    var bodyText = (document.body && document.body.innerText ? document.body.innerText : '').toLowerCase();" +
                     "    var path = (location.pathname || '').toLowerCase();" +
                     "    var authLike = path.indexOf('login') !== -1 || path.indexOf('register') !== -1 || bodyText.indexOf('passwort') !== -1 || bodyText.indexOf('benutzername') !== -1;" +
                     "    if (authLike) {" +
                     "      var viewport = document.querySelector('meta[name=\"viewport\"]');" +
                     "      if (!viewport && document.head) {" +
                     "        viewport = document.createElement('meta');" +
                     "        viewport.name = 'viewport';" +
                     "        document.head.appendChild(viewport);" +
                     "      }" +
                     "      if (viewport) viewport.content = 'width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no';" +
                     "      document.documentElement.style.zoom = '" + authUiScale + "';" +
                     "      if (document.body) document.body.style.zoom = '" + authUiScale + "';" +
                    "    } else {" +
                    "      document.documentElement.style.zoom = '1.0';" +
                    "      if (document.body) document.body.style.zoom = '1.0';" +
                    "    }" +
                     "  } catch(e) {}" +
                     "  return true;" +
                     "})();"
        w.runJavaScript(jsCode)
    }

    function ensureStudyLoaded() {
        if (!ssoPollingEnabled)
            return
        var w = studyWeb()
        if (!w)
            return
        if (!firstLoadDone || w.url.toString() === "" || w.url.toString() === "about:blank") {
            w.url = buildFreshStudyEntryUrl()
            firstLoadDone = true
            cacheMissRecoveryArmed = true
            cacheMissRecoveryCount = 0
        }
    }

    function scheduleStudyWebViewRecreate(reason) {
        if (webviewRecreatePending)
            return
        if (webViewRecreateCount >= webViewRecreateLimit)
            return
        webviewRecreatePending = true
        webViewRecreateCount += 1
        cacheMissRecoveryCount = 0
        cacheMissRecoveryArmed = false
        nativeResetPending = false
        lastNativeWebViewResetMs = 0
        console.log("BlopStudy: webview recreate start", reason, "n=", webViewRecreateCount)
        webLoaderDeactivateTimer.start()
    }

    function recoverFromCacheMiss(reason) {
        if (!ssoPollingEnabled)
            return
        if (webviewRecreatePending)
            return
        if (cacheMissRecoveryCount >= cacheMissRecoveryLimit) {
            if (webViewRecreateCount < webViewRecreateLimit)
                scheduleStudyWebViewRecreate("limitReached:" + reason)
            return
        }
        var nowMs = Date.now()
        if (nowMs - lastCacheMissRecoveryMs < 1200)
            return
        lastCacheMissRecoveryMs = nowMs
        cacheMissRecoveryCount += 1
        // Keep the guard down until we actively retry a fresh URL, otherwise
        // the same already-visible error page can re-trigger recovery loops.
        cacheMissRecoveryArmed = false
        oauthPending = false
        var w = studyWeb()
        if (w)
            w.stop()

        // After several soft recoveries, destroy and recreate the WebView instance in-app.
        if (cacheMissRecoveryCount >= 3 && webViewRecreateCount < webViewRecreateLimit) {
            scheduleStudyWebViewRecreate("cacheMissCount:" + reason)
            return
        }

        if (cacheMissRecoveryCount >= 2 && !nativeResetPending &&
                (Date.now() - lastNativeWebViewResetMs >= nativeWebViewResetMinIntervalMs) &&
                typeof blopAppBridge !== "undefined" &&
                blopAppBridge.resetAndroidWebViewStorage) {
            lastNativeWebViewResetMs = Date.now()
            nativeResetPending = true
            blopAppBridge.resetAndroidWebViewStorage()
            nativeResetTimer.start()
            return
        }
        cacheMissRetryTimer.start()
    }

    Rectangle {
        id: qmlTopBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: topBarHeight + topBarTopMargin
        color: "#0F111A"
        z: 20

        Row {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: Math.round(10 * uiScale)
            spacing: Math.round(8 * uiScale)

            Rectangle {
                width: Math.round(74 * uiScale)
                height: Math.round(30 * uiScale)
                radius: Math.round(14 * uiScale)
                color: "transparent"
                border.color: "#2D3145"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "Notizen"
                    color: "#A7ACBB"
                    font.pixelSize: Math.round(12 * uiScale)
                    font.bold: true
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (typeof blopAppBridge !== "undefined")
                            blopAppBridge.switchToNotesFromWebQmlBar()
                    }
                }
            }

            Rectangle {
                width: Math.round(64 * uiScale)
                height: Math.round(30 * uiScale)
                radius: Math.round(14 * uiScale)
                color: "#5E5CE6"
                border.color: "#7D7AFF"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "Study"
                    color: "#F4F5FB"
                    font.pixelSize: Math.round(12 * uiScale)
                    font.bold: true
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: setWebDestination(0, "")
                }
            }

            Rectangle {
                width: Math.round(30 * uiScale)
                height: Math.round(30 * uiScale)
                radius: Math.round(14 * uiScale)
                color: "transparent"
                border.color: "#2D3145"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "+"
                    color: "#A7ACBB"
                    font.pixelSize: Math.round(14 * uiScale)
                    font.bold: true
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (typeof blopAppBridge !== "undefined")
                            blopAppBridge.openWebBookmarkMenuFromWebQmlBar()
                    }
                }
            }
        }
    }

    Component {
        id: studyWebViewComponent
        WebView {
            id: embeddedStudyWebView
            anchors.fill: parent
            url: "about:blank"

            // Qt 6: declare loadRequest explicitly (injected implicit parameter is deprecated)
            onLoadingChanged: function(loadRequest) {
                var isFailed = false
                var errorText = ""
                var urlText = ""
                try {
                    isFailed = (loadRequest.status === WebView.LoadFailedStatus)
                    errorText = String(loadRequest.errorString || "")
                    urlText = String(loadRequest.url || "")
                } catch (e) {
                    isFailed = false
                    errorText = ""
                    urlText = ""
                }

                console.log("BlopStudy: WebView loadingChanged",
                            "status=", loadRequest.status,
                            "url=", urlText,
                            "error=", errorText)

                if (isFailed && errorText.indexOf("ERR_CACHE_MISS") !== -1 && studyRoot.cacheMissRecoveryArmed) {
                    studyRoot.recoverFromCacheMiss("errorString")
                    return
                }

                // Android WebView can land on chrome error pages without a classic
                // LoadFailedStatus callback. Recover from that URL explicitly.
                if (urlText.toLowerCase().indexOf("chrome-error://chromewebdata") === 0 && studyRoot.cacheMissRecoveryArmed) {
                    studyRoot.recoverFromCacheMiss("chromeErrorPage")
                    return
                }

                if (studyRoot.ssoPollingEnabled && loadRequest.url.toString().indexOf("blop://google-login") === 0) {
                    embeddedStudyWebView.stop()
                    if (typeof blopAppBridge !== "undefined") {
                        blopAppBridge.requestGoogleLogin()
                    }
                }
                // Defensive reset: if an OAuth overlay stayed visible accidentally, clear it
                // when normal website navigation happens.
                if (loadRequest.url.toString().indexOf("https://") === 0 ||
                    loadRequest.url.toString().indexOf("http://") === 0) {
                    studyRoot.oauthPending = false
                    studyRoot.applyAuthUiScale()
                }
            }
        }
    }

    Loader {
        id: studyWebLoader
        anchors.top: qmlTopBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        active: true
        asynchronous: false
        sourceComponent: studyWebViewComponent
    }

    Timer {
        id: webLoaderDeactivateTimer
        interval: 50
        running: false
        repeat: false
        onTriggered: {
            studyWebLoader.active = false
            webLoaderReactivateTimer.start()
        }
    }

    Timer {
        id: webLoaderReactivateTimer
        interval: 80
        running: false
        repeat: false
        onTriggered: {
            studyWebLoader.active = true
            webviewRecreatePending = false
            console.log("BlopStudy: webview recreate done")
            cacheMissRecoveryArmed = true
            postRecreateLoadTimer.start()
        }
    }

    Timer {
        id: postRecreateLoadTimer
        interval: 0
        running: false
        repeat: false
        onTriggered: {
            var w = studyWeb()
            if (w && ssoPollingEnabled)
                w.url = buildFreshStudyEntryUrl()
            applyAuthUiScale()
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

    Timer {
        id: cacheMissRetryTimer
        interval: 420
        running: false
        repeat: false
        onTriggered: {
            cacheMissRecoveryArmed = true
            var w = studyWeb()
            if (w)
                w.url = buildFreshStudyEntryUrl()
            applyAuthUiScale()
        }
    }

    Timer {
        id: nativeResetTimer
        interval: 1200
        running: false
        repeat: false
        onTriggered: {
            nativeResetPending = false
            cacheMissRetryTimer.start()
        }
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
            var w = studyWeb()
            if (w)
                w.runJavaScript(jsCode)
        }
    }

    Timer {
        interval: 1000
        running: ssoPollingEnabled && !webviewRecreatePending
        repeat: true
        onTriggered: {
            if (!ssoPollingEnabled)
                return
            var w = studyWeb()
            if (!w)
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

            w.runJavaScript(jsCode, function(result) {
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

            // Fallback detection for Android WebView local error pages that still
            // report as loaded content.
            w.runJavaScript(
                "(function(){ try { " +
                "var t=((document.body&&document.body.innerText)?document.body.innerText:'').toLowerCase();" +
                "return t.indexOf('err_cache_miss')!==-1 ? 'ERR_CACHE_MISS' : ''; " +
                "} catch(e) { return ''; } })();",
                function(flag) {
                    if (!ssoPollingEnabled || !cacheMissRecoveryArmed)
                        return
                    if (flag && flag.toString && flag.toString() === "ERR_CACHE_MISS") {
                        recoverFromCacheMiss("domText")
                    }
                }
            )
        }
    }
}
