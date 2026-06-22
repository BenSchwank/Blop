package com.benschwank.blop;

import android.net.Uri;
import android.util.Log;

/**
 * Java side of the Google OAuth deep-link callback. The custom-scheme intent
 * declared in {@code AndroidManifest.xml} (scheme {@code com.benschwank.blop},
 * host {@code oauth2redirect}) is delivered to {@link BlopActivity}, which
 * forwards the URI here. The native (C++) {@code GoogleAuthManager} registers
 * {@link #nativeNotifyAuthCallback(String)} via JNI on first login.
 *
 * Because Android can deliver the deep link before the C++ singleton has
 * finished registering the JNI method (cold launch through the launcher
 * activity), we keep a single-shot pending URI cache that the native side
 * drains from {@code consumePendingUri()} when it initialises.
 */
public final class BlopOAuthBridge {
    private static final String TAG = "BlopOAuthBridge";
    private static volatile boolean sNativeReady = false;
    private static volatile String sPendingUri;

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
        Log.i(TAG, "deliverIntentUri: uri=" + s + " scheme=" + uri.getScheme() + " host=" + uri.getHost());
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

    /** Called from C++ after {@code registerNativeMethods} succeeds. */
    public static synchronized String consumePendingUri() {
        Log.i(TAG, "=== consumePendingUri called ===");
        sNativeReady = true;
        final String s = sPendingUri;
        sPendingUri = null;
        if (s != null && !s.isEmpty()) {
            Log.i(TAG, "consumePendingUri: returning pending URI: " + s);
        } else {
            Log.i(TAG, "consumePendingUri: no pending URI");
        }
        return s == null ? "" : s;
    }

    /** Called from C++ to open the OAuth URL in a Chrome Custom Tab. */
    public static synchronized void openAuthUrl(String url) {
        BlopActivity.openCustomTab(url);
    }

    /** Implemented in C++ {@code googleauthmanager.cpp}. */
    public static native void nativeNotifyAuthCallback(String uri);
}
