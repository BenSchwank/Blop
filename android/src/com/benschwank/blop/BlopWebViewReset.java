package com.benschwank.blop;

import android.app.Activity;
import android.util.Log;
import android.webkit.CookieManager;
import android.webkit.WebStorage;

public final class BlopWebViewReset {
    private static final String TAG = "BlopWebViewReset";

    private BlopWebViewReset() {
    }

    public static void clearWebViewState(final Activity activity) {
        if (activity == null) {
            Log.w(TAG, "clearWebViewState called with null activity");
            return;
        }

        activity.runOnUiThread(() -> {
            try {
                CookieManager cookieManager = CookieManager.getInstance();
                if (cookieManager != null) {
                    cookieManager.removeAllCookies(null);
                    cookieManager.flush();
                }
                WebStorage.getInstance().deleteAllData();
                Log.i(TAG, "WebView cookie/storage reset completed");
            } catch (Throwable t) {
                Log.e(TAG, "WebView cookie/storage reset failed", t);
            }
        });
    }
}
