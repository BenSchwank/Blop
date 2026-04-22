import QtQuick 2.0
import QtQuick.Controls 2.0
import QtWebView 1.1

Rectangle {
    id: studyRoot
    color: "#0B0B1A"
    // Native Android header in MainWindow is the single source of truth for Notes/Study/+.
    // Keep this disabled to avoid duplicated top bars.
    readonly property bool showQmlTopBar: false
    // Tracks whether we're currently waiting for the OAuth flow to complete in Chrome
    property bool oauthPending: false
    property double oauthPendingSinceMs: 0
    property string studyUrl: "https://blop-study.com"
    property string studyUrlFallback: "https://blop-study.com"
    property bool firstLoadDone: false
    // When false, embedded page is a user bookmark — disable Study SSO polling / Google bridge.
    property bool ssoPollingEnabled: true
    // Keep native login at natural scale to avoid visible top/bottom letterboxing.
    property real authUiScale: 1.0
    property bool cacheMissRecoveryArmed: true
    property int cacheMissRecoveryCount: 0
    readonly property int cacheMissRecoveryLimit: 7
    property int freshLoadSerial: 0
    property bool nativeResetPending: false
    property double lastNativeWebViewResetMs: 0
    readonly property int nativeWebViewResetMinIntervalMs: 25000
    property bool nativeFullResetPending: false
    property double lastNativeFullWebViewResetMs: 0
    readonly property int nativeFullWebViewResetMinIntervalMs: 70000
    property double lastCacheMissRecoveryMs: 0
    property int webViewRecreateCount: 0
    readonly property int webViewRecreateLimit: 3
    property bool webviewRecreatePending: false
    readonly property int cacheMissEarlyStopTierCount: 2
    readonly property int cacheMissNativeResetTierCount: 3
    readonly property int cacheMissFullResetTierCount: 4
    readonly property int cacheMissRecreateTierCount: 6
    readonly property int cacheMissRetryCooldownMs: 1200
    readonly property real uiScale: Math.max(0.9, Math.min(width / 411, 1.35))
    readonly property int topBarHeight: Math.round(48 * uiScale)
    readonly property int topBarTopMargin: Math.round(8 * uiScale)
    property bool bookmarkSheetOpen: false
    property bool bookmarkManageMode: false
    property string bookmarkDraftUrl: ""
    property string bookmarkDraftTitle: ""
    property string bookmarkAddError: ""

    ListModel {
        id: bookmarkModel
    }

    function studyWeb() {
        return studyWebLoader.item
    }

    function buildFreshStudyEntryUrl(addCacheBypass) {
        var base = studyUrl
        if (base.length > 0 && base.charAt(base.length - 1) === "/")
            base = base.slice(0, -1)
        // Native entry: StudyFlow treats ?native=1 as embedded-app mode (headers, redirects).
        var u = base + "/?native=1"
        if (addCacheBypass) {
            u += "&_src=android_webview"
            u += "&_ts=" + Date.now()
            u += "&_try=" + freshLoadSerial
        }
        return u
    }

    function loadStudyEntryFresh(reason, addCacheBypass) {
        if (!ssoPollingEnabled)
            return
        var w = studyWeb()
        if (!w)
            return
        freshLoadSerial += 1
        var target = buildFreshStudyEntryUrl(addCacheBypass)
        cacheMissRecoveryArmed = false
        console.log("BlopStudy: fresh load", "reason=", reason, "try=", freshLoadSerial, "url=", target)
        if (typeof blopAppBridge !== "undefined") {
            if (blopAppBridge.scheduleAndroidStudyWebViewNetworkCache)
                blopAppBridge.scheduleAndroidStudyWebViewNetworkCache()
            else if (blopAppBridge.applyAndroidStudyWebViewNetworkCache)
                blopAppBridge.applyAndroidStudyWebViewNetworkCache()
        }
        if (Qt.platform.os === "android") {
            pivotLoadTimer.stop()
            cacheMissRecoveryArmed = true
            w.url = target
        } else {
            w.url = "about:blank"
            pivotLoadTimer.targetUrl = target
            pivotLoadTimer.start()
        }
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
            lastNativeFullWebViewResetMs = 0
            nativeFullResetPending = false
            loadStudyEntryFresh("setWebDestination", true)
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

    function refreshBookmarkModel() {
        bookmarkModel.clear()
        if (typeof blopAppBridge === "undefined" || !blopAppBridge.webBookmarksForQml)
            return
        var rows = blopAppBridge.webBookmarksForQml()
        if (!rows || rows.length === undefined)
            return
        for (var i = 0; i < rows.length; ++i) {
            var row = rows[i]
            bookmarkModel.append({
                "title": String(row.title || ""),
                "url": String(row.url || "")
            })
        }
    }

    function openBookmarkSheet() {
        if (typeof blopAppBridge !== "undefined" && blopAppBridge.webBookmarksForQml)
            refreshBookmarkModel()
        bookmarkManageMode = false
        bookmarkAddError = ""
        bookmarkDraftUrl = ""
        bookmarkDraftTitle = ""
        bookmarkSheetOpen = true
    }

    function closeBookmarkSheet() {
        bookmarkSheetOpen = false
        bookmarkManageMode = false
        bookmarkAddError = ""
    }

    function onOAuthFailed(reason) {
        console.warn("BlopStudy: oauth failed", reason)
        oauthPending = false
        oauthPendingSinceMs = 0
    }

    function submitBookmarkFromSheet() {
        bookmarkAddError = ""
        if (typeof blopAppBridge === "undefined" || !blopAppBridge.addWebBookmarkFromQml) {
            bookmarkAddError = "Lesezeichen nicht verfuegbar."
            return
        }
        var ok = blopAppBridge.addWebBookmarkFromQml(bookmarkDraftUrl, bookmarkDraftTitle)
        if (!ok) {
            bookmarkAddError = "Bitte eine gueltige http/https-Adresse eingeben."
            return
        }
        closeBookmarkSheet()
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
            loadStudyEntryFresh("ensureStudyLoaded", true)
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
        nativeFullResetPending = false
        lastNativeWebViewResetMs = 0
        lastNativeFullWebViewResetMs = 0
        console.log("BlopStudy: webview recreate start", reason, "n=", webViewRecreateCount)
        webLoaderDeactivateTimer.start()
    }

    function recoverFromCacheMiss(reason) {
        if (!ssoPollingEnabled)
            return
        if (webviewRecreatePending)
            return
        if (cacheMissRecoveryCount >= cacheMissRecoveryLimit) {
            if (webViewRecreateCount < webViewRecreateLimit) {
                console.warn("BlopStudy: cache-miss escalate", "stage=limitRecreate",
                             "reason=", reason, "count=", cacheMissRecoveryCount)
                scheduleStudyWebViewRecreate("limitReached:" + reason)
            } else {
                console.warn("BlopStudy: cache-miss exhausted",
                             "reason=", reason,
                             "count=", cacheMissRecoveryCount,
                             "recreateCount=", webViewRecreateCount)
            }
            return
        }
        var nowMs = Date.now()
        if (nowMs - lastCacheMissRecoveryMs < cacheMissRetryCooldownMs)
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

        if (cacheMissRecoveryCount >= cacheMissRecreateTierCount &&
                webViewRecreateCount < webViewRecreateLimit) {
            console.warn("BlopStudy: cache-miss escalate",
                         "stage=recreate",
                         "reason=", reason,
                         "count=", cacheMissRecoveryCount,
                         "recreateCount=", webViewRecreateCount)
            scheduleStudyWebViewRecreate("tierRecreate:" + reason)
            return
        }

        if (cacheMissRecoveryCount >= cacheMissFullResetTierCount &&
                !nativeFullResetPending &&
                (Date.now() - lastNativeFullWebViewResetMs >= nativeFullWebViewResetMinIntervalMs) &&
                typeof blopAppBridge !== "undefined" &&
                blopAppBridge.resetAndroidWebViewStorageFull) {
            console.warn("BlopStudy: cache-miss escalate",
                         "stage=nativeFullReset",
                         "reason=", reason,
                         "count=", cacheMissRecoveryCount)
            lastNativeFullWebViewResetMs = Date.now()
            nativeFullResetPending = true
            blopAppBridge.resetAndroidWebViewStorageFull()
            nativeResetTimer.start()
            return
        }

        if (cacheMissRecoveryCount >= cacheMissNativeResetTierCount && !nativeResetPending &&
                (Date.now() - lastNativeWebViewResetMs >= nativeWebViewResetMinIntervalMs) &&
                typeof blopAppBridge !== "undefined" &&
                blopAppBridge.resetAndroidWebViewStorage) {
            console.warn("BlopStudy: cache-miss escalate",
                         "stage=nativeLightReset",
                         "reason=", reason,
                         "count=", cacheMissRecoveryCount)
            lastNativeWebViewResetMs = Date.now()
            nativeResetPending = true
            blopAppBridge.resetAndroidWebViewStorage()
            nativeResetTimer.start()
            return
        }
        if (cacheMissRecoveryCount >= cacheMissEarlyStopTierCount &&
                typeof blopAppBridge !== "undefined" &&
                blopAppBridge.nudgeAndroidWebViewStopOnly) {
            console.warn("BlopStudy: cache-miss escalate",
                         "stage=nativeStopOnly",
                         "reason=", reason,
                         "count=", cacheMissRecoveryCount)
            blopAppBridge.nudgeAndroidWebViewStopOnly()
        }
        console.log("BlopStudy: cache-miss stage",
                    "stage=retryLoad",
                    "reason=", reason,
                    "count=", cacheMissRecoveryCount)
        cacheMissRetryTimer.start()
    }

    Rectangle {
        id: qmlTopBar
        visible: showQmlTopBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: showQmlTopBar ? (topBarHeight + topBarTopMargin) : 0
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
                    onClicked: openBookmarkSheet()
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
        anchors.top: showQmlTopBar ? qmlTopBar.bottom : parent.top
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
            if (typeof blopAppBridge !== "undefined") {
                if (blopAppBridge.scheduleAndroidStudyWebViewNetworkCache)
                    blopAppBridge.scheduleAndroidStudyWebViewNetworkCache()
                else if (blopAppBridge.applyAndroidStudyWebViewNetworkCache)
                    blopAppBridge.applyAndroidStudyWebViewNetworkCache()
            }
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
                loadStudyEntryFresh("postRecreate", true)
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
            loadStudyEntryFresh("cacheMissRetry", true)
            applyAuthUiScale()
        }
    }

    Timer {
        id: pivotLoadTimer
        interval: 140
        running: false
        repeat: false
        property string targetUrl: ""
        onTriggered: {
            var w = studyWeb()
            if (!w)
                return
            if (typeof blopAppBridge !== "undefined") {
                if (blopAppBridge.scheduleAndroidStudyWebViewNetworkCache)
                    blopAppBridge.scheduleAndroidStudyWebViewNetworkCache()
                else if (blopAppBridge.applyAndroidStudyWebViewNetworkCache)
                    blopAppBridge.applyAndroidStudyWebViewNetworkCache()
            }
            cacheMissRecoveryArmed = true
            w.url = targetUrl
        }
    }

    Timer {
        id: nativeResetTimer
        interval: 1200
        running: false
        repeat: false
        onTriggered: {
            nativeResetPending = false
            nativeFullResetPending = false
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

    Rectangle {
        id: bookmarkBackdrop
        anchors.fill: parent
        color: "#B3000000"
        visible: bookmarkSheetOpen
        z: 30

        MouseArea {
            anchors.fill: parent
            onClicked: closeBookmarkSheet()
        }

        Rectangle {
            id: bookmarkSheet
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            radius: Math.round(18 * uiScale)
            color: "#14121F"
            border.color: "#4F4A70"
            border.width: 1
            height: Math.min(parent.height * 0.82, Math.round((bookmarkManageMode ? 500 : 420) * uiScale))

            MouseArea {
                anchors.fill: parent
                onClicked: {}
            }

            Column {
                anchors.fill: parent
                anchors.margins: Math.round(14 * uiScale)
                spacing: Math.round(10 * uiScale)

                Rectangle {
                    width: Math.round(42 * uiScale)
                    height: Math.round(4 * uiScale)
                    radius: height / 2
                    color: "#5A5578"
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Row {
                    width: parent.width
                    spacing: Math.round(8 * uiScale)

                    Text {
                        text: "Web-Lesezeichen"
                        color: "#F4F2FF"
                        font.pixelSize: Math.round(18 * uiScale)
                        font.bold: true
                    }

                    Item { width: 1; height: 1 }

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.round(96 * uiScale)
                        height: Math.round(30 * uiScale)
                        radius: height / 2
                        color: bookmarkManageMode ? "#5E5CE6" : "#252335"
                        border.color: bookmarkManageMode ? "#7D7AFF" : "#3A3651"
                        border.width: 1
                        anchors.right: parent.right

                        Text {
                            anchors.centerIn: parent
                            text: bookmarkManageMode ? "Fertig" : "Verwalten"
                            color: "#F0EEFF"
                            font.pixelSize: Math.round(12 * uiScale)
                            font.bold: true
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: bookmarkManageMode = !bookmarkManageMode
                        }
                    }
                }

                Rectangle {
                    width: parent.width
                    height: Math.round(1 * uiScale)
                    color: "#2B2840"
                }

                Flickable {
                    width: parent.width
                    height: Math.round(170 * uiScale)
                    clip: true
                    contentWidth: width
                    contentHeight: bookmarkColumn.implicitHeight

                    Column {
                        id: bookmarkColumn
                        width: parent.width
                        spacing: Math.round(8 * uiScale)

                        Repeater {
                            model: bookmarkModel

                            delegate: Rectangle {
                                width: bookmarkColumn.width
                                height: Math.round(44 * uiScale)
                                radius: Math.round(10 * uiScale)
                                color: "#1B1930"
                                border.color: "#2F2C46"
                                border.width: 1

                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.left: parent.left
                                    anchors.leftMargin: Math.round(12 * uiScale)
                                    width: bookmarkManageMode ? parent.width - Math.round(70 * uiScale) : parent.width - Math.round(24 * uiScale)
                                    text: model.title
                                    color: "#E9E6FF"
                                    elide: Text.ElideRight
                                    font.pixelSize: Math.round(13 * uiScale)
                                }

                                Rectangle {
                                    visible: bookmarkManageMode
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.right: parent.right
                                    anchors.rightMargin: Math.round(8 * uiScale)
                                    width: Math.round(28 * uiScale)
                                    height: Math.round(28 * uiScale)
                                    radius: width / 2
                                    color: "#3B2331"
                                    border.color: "#8D4B67"
                                    border.width: 1

                                    Text {
                                        anchors.centerIn: parent
                                        text: "x"
                                        color: "#FFC1D4"
                                        font.pixelSize: Math.round(13 * uiScale)
                                        font.bold: true
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: {
                                            if (typeof blopAppBridge !== "undefined" && blopAppBridge.removeWebBookmarkFromQml) {
                                                blopAppBridge.removeWebBookmarkFromQml(index)
                                                refreshBookmarkModel()
                                            }
                                        }
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    enabled: !bookmarkManageMode
                                    onClicked: {
                                        if (typeof blopAppBridge !== "undefined" && blopAppBridge.openWebBookmarkFromQml) {
                                            blopAppBridge.openWebBookmarkFromQml(index)
                                            closeBookmarkSheet()
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Text {
                    text: "URL hinzufuegen"
                    color: "#D8D4F5"
                    font.pixelSize: Math.round(13 * uiScale)
                    font.bold: true
                }

                TextField {
                    id: addUrlField
                    width: parent.width
                    placeholderText: "https://..."
                    text: bookmarkDraftUrl
                    onTextChanged: bookmarkDraftUrl = text
                }

                TextField {
                    width: parent.width
                    placeholderText: "Titel (optional)"
                    text: bookmarkDraftTitle
                    onTextChanged: bookmarkDraftTitle = text
                }

                Text {
                    visible: bookmarkAddError.length > 0
                    text: bookmarkAddError
                    color: "#FF909D"
                    font.pixelSize: Math.round(12 * uiScale)
                }

                Row {
                    width: parent.width
                    spacing: Math.round(8 * uiScale)

                    Rectangle {
                        width: (parent.width - spacing) / 2
                        height: Math.round(40 * uiScale)
                        radius: Math.round(10 * uiScale)
                        color: "#262237"
                        border.color: "#3A3550"
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: "Abbrechen"
                            color: "#E0DBFF"
                            font.pixelSize: Math.round(13 * uiScale)
                            font.bold: true
                        }
                        MouseArea { anchors.fill: parent; onClicked: closeBookmarkSheet() }
                    }

                    Rectangle {
                        width: (parent.width - spacing) / 2
                        height: Math.round(40 * uiScale)
                        radius: Math.round(10 * uiScale)
                        color: "#5E5CE6"
                        border.color: "#7D7AFF"
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: "Speichern"
                            color: "#F9F8FF"
                            font.pixelSize: Math.round(13 * uiScale)
                            font.bold: true
                        }
                        MouseArea { anchors.fill: parent; onClicked: submitBookmarkFromSheet() }
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
            oauthPendingSinceMs = 0
            var w = studyWeb()
            if (w)
                w.runJavaScript(jsCode)
        }

        function onOauthFailed(reason) {
            onOAuthFailed(reason)
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
                        if (oauthPending && (Date.now() - oauthPendingSinceMs) < 5000)
                            return
                        oauthPending = true;  // Show overlay while Chrome is open
                        oauthPendingSinceMs = Date.now()
                        blopAppBridge.requestGoogleLogin();
                    } else if (resStr !== "") {
                        oauthPending = false;
                        oauthPendingSinceMs = 0
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

    // If OAuth browser return leaves the embedded page stale, recover automatically.
    Timer {
        interval: 1200
        running: ssoPollingEnabled
        repeat: true
        onTriggered: {
            if (!oauthPending)
                return
            if (Date.now() - oauthPendingSinceMs < 20000)
                return
            console.warn("BlopStudy: oauth watchdog timeout, forcing fresh load")
            oauthPending = false
            oauthPendingSinceMs = 0
            loadStudyEntryFresh("oauthWatchdog", true)
        }
    }
}
