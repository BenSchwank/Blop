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
        if (uri == null) {
            return;
        }
        final String s = uri.toString();
        if (s.isEmpty()) {
            return;
        }
        Log.i(TAG, "deliverIntentUri scheme=" + uri.getScheme() + " host=" + uri.getHost());
        if (sNativeReady) {
            try {
                nativeNotifyAuthCallback(s);
            } catch (UnsatisfiedLinkError e) {
                Log.w(TAG, "native bridge gone; cache for next attach", e);
                sPendingUri = s;
                sNativeReady = false;
            }
        } else {
            sPendingUri = s;
        }
    }

    /** Called from C++ after {@code registerNativeMethods} succeeds. */
    public static synchronized String consumePendingUri() {
        sNativeReady = true;
        final String s = sPendingUri;
        sPendingUri = null;
        return s == null ? "" : s;
    }

    /** Implemented in C++ {@code googleauthmanager.cpp}. */
    public static native void nativeNotifyAuthCallback(String uri);
}
