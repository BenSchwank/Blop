package com.benschwank.blop;

import android.app.Activity;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.CookieManager;
import android.webkit.WebStorage;
import android.webkit.WebView;

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
                WebView activeWebView = findWebView(activity.getWindow() != null
                        ? activity.getWindow().getDecorView() : null);
                if (activeWebView != null) {
                    activeWebView.stopLoading();
                    activeWebView.clearHistory();
                    activeWebView.clearFormData();
                    activeWebView.clearCache(true);
                    Log.i(TAG, "WebView cookie/storage/cache reset completed");
                } else {
                    Log.i(TAG, "WebView cookie/storage reset completed (no active WebView found)");
                }
            } catch (Throwable t) {
                Log.e(TAG, "WebView cookie/storage reset failed", t);
            }
        });
    }

    private static WebView findWebView(View root) {
        if (root == null) {
            return null;
        }
        if (root instanceof WebView) {
            return (WebView) root;
        }
        if (!(root instanceof ViewGroup)) {
            return null;
        }
        ViewGroup group = (ViewGroup) root;
        for (int i = 0; i < group.getChildCount(); i++) {
            WebView found = findWebView(group.getChildAt(i));
            if (found != null) {
                return found;
            }
        }
        return null;
    }
}
