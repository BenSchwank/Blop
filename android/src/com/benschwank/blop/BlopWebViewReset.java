package com.benschwank.blop;

import android.app.Activity;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.CookieManager;
import android.webkit.WebSettings;
import android.webkit.WebStorage;
import android.webkit.WebView;
import android.os.Build;

import java.util.ArrayList;
import java.util.List;

public final class BlopWebViewReset {
    private static final String TAG = "BlopWebViewReset";

    private BlopWebViewReset() {
    }

    /**
     * Prefer network over HTTP cache for embedded WebViews. Helps avoid
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
                int applied = applyAllWebViewsNow(activity);
                if (applied == 0) {
                    Log.i(TAG, "applyStudyWebViewNetworkCacheMode: no WebView found yet");
                }
            } catch (Throwable t) {
                Log.e(TAG, "applyStudyWebViewNetworkCacheMode failed", t);
            }
        });
    }

    /**
     * Retries {@link #applyAllWebViewsNow(Activity)} on the main thread until at least one
     * WebView receives settings or {@code maxAttempts} is reached (covers late-attached Qt WebView).
     */
    public static void scheduleApplyStudyWebViewNetworkCacheMode(final Activity activity,
            final int maxAttempts, final long stepMs) {
        if (activity == null) {
            Log.w(TAG, "scheduleApplyStudyWebViewNetworkCacheMode called with null activity");
            return;
        }
        final Handler h = new Handler(Looper.getMainLooper());
        final Runnable[] work = new Runnable[1];
        work[0] = new Runnable() {
            int attempt = 0;

            @Override
            public void run() {
                if (activity.isFinishing() || activity.isDestroyed()) {
                    Log.i(TAG, "scheduleApplyStudyWebViewNetworkCacheMode: activity gone, stop");
                    return;
                }
                attempt++;
                int applied = 0;
                try {
                    applied = applyAllWebViewsNow(activity);
                } catch (Throwable t) {
                    Log.e(TAG, "scheduleApplyStudyWebViewNetworkCacheMode attempt failed", t);
                }
                Log.i(TAG, "scheduleApplyStudyWebViewNetworkCacheMode attempt=" + attempt
                        + "/" + maxAttempts + " appliedSettings=" + applied);
                if (applied > 0) {
                    Log.i(TAG, "scheduleApplyStudyWebViewNetworkCacheMode: success");
                    return;
                }
                if (attempt >= maxAttempts) {
                    Log.w(TAG, "scheduleApplyStudyWebViewNetworkCacheMode: exhausted attempts");
                    return;
                }
                long adaptiveStepMs = Math.max(16L, stepMs + (attempt * 40L));
                if (adaptiveStepMs > 500L) {
                    adaptiveStepMs = 500L;
                }
                h.postDelayed(work[0], adaptiveStepMs);
            }
        };
        h.post(work[0]);
    }

    /**
     * Must run on the UI thread. Returns how many WebViews had non-null {@link WebSettings}.
     */
    private static int applyAllWebViewsNow(Activity activity) {
        List<WebView> views = new ArrayList<>();
        collectWebViews(activity.getWindow() != null
                ? activity.getWindow().getDecorView() : null, views);
        int applied = 0;
        for (WebView w : views) {
            if (w == null) {
                continue;
            }
            WebSettings s = w.getSettings();
            if (s != null) {
                s.setDomStorageEnabled(true);
                // LOAD_DEFAULT is safer with Chromium navigation stack on release/AAB builds.
                s.setCacheMode(WebSettings.LOAD_DEFAULT);
                s.setBlockNetworkLoads(false);
                s.setDatabaseEnabled(true);
                CookieManager cm = CookieManager.getInstance();
                if (cm != null) {
                    cm.setAcceptCookie(true);
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                        cm.setAcceptThirdPartyCookies(w, true);
                    }
                }
                applied++;
            }
        }
        Log.i(TAG, "applyAllWebViewsNow: totalFound=" + views.size() + " appliedSettings=" + applied);
        return applied;
    }

    public static void clearWebViewStateLight(final Activity activity) {
        if (activity == null) {
            Log.w(TAG, "clearWebViewStateLight called with null activity");
            return;
        }
        activity.runOnUiThread(() -> {
            try {
                nudgeActiveWebView(activity, true);
            } catch (Throwable t) {
                Log.e(TAG, "WebView light reset failed", t);
            }
        });
    }

    /**
     * Tier-1 recovery: only abort in-flight load, keep navigation/forms intact.
     */
    public static void stopLoadingOnly(final Activity activity) {
        if (activity == null) {
            Log.w(TAG, "stopLoadingOnly called with null activity");
            return;
        }
        activity.runOnUiThread(() -> {
            try {
                nudgeActiveWebView(activity, false);
            } catch (Throwable t) {
                Log.e(TAG, "WebView stop-only failed", t);
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

    private static void nudgeActiveWebView(Activity activity, boolean clearTransientState) {
        WebView activeWebView = findWebView(activity.getWindow() != null
                ? activity.getWindow().getDecorView() : null);
        if (activeWebView != null) {
            activeWebView.stopLoading();
            if (clearTransientState) {
                activeWebView.clearHistory();
                activeWebView.clearFormData();
                Log.i(TAG, "WebView light reset completed (stop/history/forms)");
            } else {
                Log.i(TAG, "WebView stop-only completed");
            }
        } else {
            if (clearTransientState) {
                Log.i(TAG, "WebView light reset no-op (no active WebView found)");
            } else {
                Log.i(TAG, "WebView stop-only no-op (no active WebView found)");
            }
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

    /** Depth-first: every {@link WebView} under {@code root} (order matches traversal). */
    private static void collectWebViews(View root, List<WebView> out) {
        if (root == null || out == null) {
            return;
        }
        if (root instanceof WebView) {
            out.add((WebView) root);
            return;
        }
        if (!(root instanceof ViewGroup)) {
            return;
        }
        ViewGroup group = (ViewGroup) root;
        for (int i = 0; i < group.getChildCount(); i++) {
            collectWebViews(group.getChildAt(i), out);
        }
    }
}
