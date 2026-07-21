package com.benschwank.blop;

import android.net.Uri;
import android.util.Log;

/**
 * Java side of the Google OAuth deep-link callback. The custom-scheme intent
 * declared in {@code AndroidManifest.xml} is delivered to {@link BlopActivity},
 * which forwards the URI here. The native (C++) {@code GoogleAuthManager}
 * registers native methods via JNI on startup.
 *
 * Because Android can deliver the deep link before the C++ singleton has
 * finished registering the JNI method (cold launch through the launcher
 * activity), we keep a single-shot pending URI cache that the native side
 * drains from {@code consumePendingUri()} when it initialises.
 *
 * Also notifies native when the activity resumes after the OAuth Custom Tab
 * so a missing redirect cannot leave the login UI hung forever.
 */
public final class BlopOAuthBridge {
    private static final String TAG = "BlopOAuthBridge";
    private static volatile boolean sNativeReady = false;
    private static volatile String sPendingUri;
    private static volatile boolean sExternalAuthStarted = false;
    private static volatile boolean sPendingResumeNotify = false;
    private static volatile String sPendingAbandonReason;

    private BlopOAuthBridge() {
    }

    /** Called from {@link BlopActivity} for both onCreate and onNewIntent. */
    public static synchronized void deliverIntentUri(Uri uri) {
        Log.i(TAG, "=== deliverIntentUri called ===");
        if (uri == null) {
            Log.w(TAG, "deliverIntentUri: uri is null");
            return;
        }
        final String s = uri.toString();
        if (s.isEmpty()) {
            Log.w(TAG, "deliverIntentUri: uri string is empty");
            return;
        }
        sExternalAuthStarted = false;
        sPendingResumeNotify = false;
        BlopActivity.clearOAuthBrowserFlag();
        Log.i(TAG, "deliverIntentUri: uri=" + s + " scheme=" + uri.getScheme()
                + " host=" + uri.getHost());
        if (sNativeReady) {
            Log.i(TAG, "deliverIntentUri: native ready, calling nativeNotifyAuthCallback");
            try {
                nativeNotifyAuthCallback(s);
                Log.i(TAG, "deliverIntentUri: nativeNotifyAuthCallback succeeded");
            } catch (UnsatisfiedLinkError e) {
                Log.w(TAG, "native bridge gone; cache for next attach", e);
                sPendingUri = s;
                sNativeReady = false;
            }
        } else {
            Log.i(TAG, "deliverIntentUri: native not ready, caching URI");
            sPendingUri = s;
        }
    }

    /** Called just before launching the OAuth Custom Tab / browser. */
    public static synchronized void markExternalAuthStarted() {
        sExternalAuthStarted = true;
        sPendingResumeNotify = false;
        sPendingAbandonReason = null;
    }

    /**
     * Activity resumed while an OAuth browser session may still be open.
     * Native waits a short grace period for a late deep link, then unlocks.
     */
    public static synchronized void notifyActivityResumedAfterAuth() {
        if (!sExternalAuthStarted) {
            return;
        }
        Log.i(TAG, "notifyActivityResumedAfterAuth: external auth still pending");
        if (sNativeReady) {
            try {
                nativeNotifyAuthResume();
                return;
            } catch (UnsatisfiedLinkError e) {
                Log.w(TAG, "nativeNotifyAuthResume unavailable; defer", e);
                sNativeReady = false;
            }
        }
        sPendingResumeNotify = true;
    }

    /** Browser could not be opened at all. */
    public static synchronized void notifyExternalAuthAbandoned(String reason) {
        sExternalAuthStarted = false;
        BlopActivity.clearOAuthBrowserFlag();
        final String r = (reason == null || reason.isEmpty())
                ? "browser_open_failed" : reason;
        if (sNativeReady) {
            try {
                nativeNotifyAuthAbandoned(r);
                return;
            } catch (UnsatisfiedLinkError e) {
                Log.w(TAG, "nativeNotifyAuthAbandoned unavailable; defer", e);
                sNativeReady = false;
            }
        }
        sPendingAbandonReason = r;
    }

    /** Called from C++ after {@code registerNativeMethods} succeeds. */
    public static synchronized String consumePendingUri() {
        Log.i(TAG, "=== consumePendingUri called ===");
        sNativeReady = true;
        final String s = sPendingUri;
        sPendingUri = null;
        if (s != null && !s.isEmpty()) {
            Log.i(TAG, "consumePendingUri: returning pending URI: " + s);
            sExternalAuthStarted = false;
            sPendingResumeNotify = false;
            return s;
        }
        Log.i(TAG, "consumePendingUri: no pending URI");
        // Flush deferred resume / abandon once native is ready.
        if (sPendingAbandonReason != null) {
            final String reason = sPendingAbandonReason;
            sPendingAbandonReason = null;
            try {
                nativeNotifyAuthAbandoned(reason);
            } catch (UnsatisfiedLinkError e) {
                Log.w(TAG, "deferred abandon failed", e);
            }
        } else if (sPendingResumeNotify && sExternalAuthStarted) {
            sPendingResumeNotify = false;
            try {
                nativeNotifyAuthResume();
            } catch (UnsatisfiedLinkError e) {
                Log.w(TAG, "deferred resume failed", e);
            }
        }
        return "";
    }

    /** Called from C++ after a successful callback or cancel. */
    public static synchronized void clearExternalAuthPending() {
        sExternalAuthStarted = false;
        sPendingResumeNotify = false;
        BlopActivity.clearOAuthBrowserFlag();
    }

    /** Called from C++ to open the OAuth URL in a Chrome Custom Tab. */
    public static synchronized void openAuthUrl(String url) {
        BlopActivity.openCustomTab(url);
    }

    /** Implemented in C++ {@code googleauthmanager.cpp}. */
    public static native void nativeNotifyAuthCallback(String uri);

    /** User returned to the app; deep link may still arrive shortly. */
    public static native void nativeNotifyAuthResume();

    /** OAuth browser handoff failed before any redirect. */
    public static native void nativeNotifyAuthAbandoned(String reason);
}
