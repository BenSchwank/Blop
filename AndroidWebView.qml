import QtQuick 2.0
import QtWebView 1.1

Item {
    WebView {
        id: webView
        anchors.fill: parent
        url: "https://blop-six.vercel.app"
        
        onLoadingChanged: {
            if (loadRequest.url.toString().indexOf("blop://google-login") === 0) {
                webView.stop()
                if (typeof blopAppBridge !== "undefined") {
                    blopAppBridge.requestGoogleLogin()
                }
            }
        }
    }

    Connections {
        target: typeof blopAppBridge !== "undefined" ? blopAppBridge : null
        
        function onInjectToken(token) {
            console.log("QML: Token received from bridge! Injecting into React app...");
            var jsCode = "if (window.handleGoogleLoginSuccess) { window.handleGoogleLoginSuccess({credential: '" + token + "'}); }";
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
                        blopAppBridge.requestGoogleLogin();
                    } else if (resStr !== "") {
                        blopAppBridge.onSessionCheck(resStr);
                    }
                }
            })
        }
    }
}
