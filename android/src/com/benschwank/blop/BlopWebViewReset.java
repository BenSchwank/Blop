package com.benschwank.blop;

import android.app.Activity;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.CookieManager;
import android.webkit.WebSettings;
import android.webkit.WebStorage;
import android.webkit.WebView;

public final class BlopWebViewReset {
    private static final String TAG = "BlopWebViewReset";

    private BlopWebViewReset() {
    }

    /**
     * Minimal in-process recovery for embedded WebView load failures (e.g. ERR_CACHE_MISS).
     * Does not clear disk cache, cookies, or DOM storage — those can worsen cache-only
     * navigation errors when retried immediately.
     */
    /**
     * Prefer network over HTTP cache for the embedded Study WebView. Helps avoid
     * {@code net::ERR_CACHE_MISS} when Chromium would satisfy a navigation only from cache.
     * Safe to call repeatedly (e.g. after QML Loader recreates the WebView).
     */
    public static void applyStudyWebViewNetworkCacheMode(final Activity activity) {
        if (activity == null) {
            Log.w(TAG, "applyStudyWebViewNetworkCacheMode called with null activity");
            return;
        }
        activity.runOnUiThread(() -> {
            try {
                WebView w = findWebView(activity.getWindow() != null
                        ? activity.getWindow().getDecorView() : null);
                if (w == null) {
                    Log.i(TAG, "applyStudyWebViewNetworkCacheMode: no WebView found yet");
                    return;
                }
                WebSettings s = w.getSettings();
                if (s != null) {
                    s.setDomStorageEnabled(true);
                    s.setCacheMode(WebSettings.LOAD_NO_CACHE);
                    Log.i(TAG, "applyStudyWebViewNetworkCacheMode: LOAD_NO_CACHE + domStorage ok");
                }
            } catch (Throwable t) {
                Log.e(TAG, "applyStudyWebViewNetworkCacheMode failed", t);
            }
        });
    }

    public static void clearWebViewStateLight(final Activity activity) {
        if (activity == null) {
            Log.w(TAG, "clearWebViewStateLight called with null activity");
            return;
        }
        activity.runOnUiThread(() -> {
            try {
                nudgeActiveWebViewOnly(activity);
            } catch (Throwable t) {
                Log.e(TAG, "WebView light reset failed", t);
            }
        });
    }

    /**
     * Full session reset: cookies, {@link WebStorage}, and active WebView history/forms.
     * Avoid calling from automated retry loops; prefer {@link #clearWebViewStateLight(Activity)}
     * for ERR_CACHE_MISS recovery. Disk cache eviction is intentionally omitted here too
     * ({@code clearCache(true)}) because it can provoke {@code net::ERR_CACHE_MISS} on retry.
     */
    public static void clearWebViewState(final Activity activity) {
        if (activity == null) {
            Log.w(TAG, "clearWebViewState called with null activity");
            return;
        }

        activity.runOnUiThread(() -> {
            try {
                CookieManager cookieManager = CookieManager.getInstance();
                if (cookieManager != null) {
                    cookieManager.removeAllCookies(value -> {
                        try {
                            cookieManager.flush();
                        } catch (Throwable flushError) {
                            Log.w(TAG, "Cookie flush failed after removeAllCookies", flushError);
                        }
                        clearStorageAndActiveWebView(activity);
                    });
                    return;
                }
                clearStorageAndActiveWebView(activity);
            } catch (Throwable t) {
                Log.e(TAG, "WebView cookie/storage reset failed", t);
            }
        });
    }

    private static void nudgeActiveWebViewOnly(Activity activity) {
        WebView activeWebView = findWebView(activity.getWindow() != null
                ? activity.getWindow().getDecorView() : null);
        if (activeWebView != null) {
            activeWebView.stopLoading();
            activeWebView.clearHistory();
            activeWebView.clearFormData();
            Log.i(TAG, "WebView light reset completed (stop/history/forms only)");
        } else {
            Log.i(TAG, "WebView light reset no-op (no active WebView found)");
        }
    }

    private static void clearStorageAndActiveWebView(Activity activity) {
        WebStorage.getInstance().deleteAllData();
        WebView activeWebView = findWebView(activity.getWindow() != null
                ? activity.getWindow().getDecorView() : null);
        if (activeWebView != null) {
            activeWebView.stopLoading();
            activeWebView.clearHistory();
            activeWebView.clearFormData();
            Log.i(TAG, "WebView cookie/storage reset completed");
        } else {
            Log.i(TAG, "WebView cookie/storage reset completed (no active WebView found)");
        }
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
