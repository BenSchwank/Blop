package com.benschwank.blop;

import android.util.Log;

/**
 * JNI bridge for SAF document open/create results. Mirrors
 * {@link BlopOAuthBridge}: C++ registers {@link #nativeNotifyPicked(String)}
 * and drains any pending path that arrived before registration.
 */
public final class BlopContentPickerBridge {
    private static final String TAG = "BlopContentPicker";
    private static volatile boolean sNativeReady = false;
    private static volatile String sPendingPath;

    private BlopContentPickerBridge() {}

    /** Deliver a local filesystem path (or empty string on cancel/error). */
    public static synchronized void deliverPath(String path) {
        final String s = path == null ? "" : path;
        Log.i(TAG, "deliverPath len=" + s.length() + " ready=" + sNativeReady);
        if (sNativeReady) {
            try {
                nativeNotifyPicked(s);
            } catch (UnsatisfiedLinkError e) {
                Log.w(TAG, "native bridge gone; cache path", e);
                sPendingPath = s;
                sNativeReady = false;
            }
        } else {
            sPendingPath = s;
        }
    }

    /** Called from C++ after JNI registration. */
    public static synchronized String consumePendingPath() {
        sNativeReady = true;
        final String s = sPendingPath;
        sPendingPath = null;
        return s == null ? "" : s;
    }

    public static native void nativeNotifyPicked(String path);
}
