package com.benschwank.blop;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;

import org.qtproject.qt.android.bindings.QtActivity;

/**
 * Custom QtActivity subclass that forwards Google OAuth deep links
 * (custom scheme {@code com.benschwank.blop://oauth2redirect}) into
 * {@link BlopOAuthBridge}, which in turn calls back into the C++
 * {@code GoogleAuthManager} via JNI.
 *
 * Cold-launch deep links arrive in {@link #onCreate(Bundle)} via the
 * launching {@link Intent}; warm/relaunch deep links are delivered through
 * {@link #onNewIntent(Intent)}. Both paths are handled identically.
 */
public class BlopActivity extends QtActivity {
    private static final String TAG = "BlopActivity";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        forwardOAuthIntent(getIntent(), "onCreate");
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        // Update the activity's intent so getIntent() returns the latest data.
        setIntent(intent);
        forwardOAuthIntent(intent, "onNewIntent");
    }

    private static void forwardOAuthIntent(Intent intent, String origin) {
        if (intent == null) {
            return;
        }
        final Uri data = intent.getData();
        if (data == null) {
            return;
        }
        final String scheme = data.getScheme();
        if (scheme == null || !scheme.equalsIgnoreCase("com.benschwank.blop")) {
            return;
        }
        Log.i(TAG, origin + " forwarding OAuth deep link host=" + data.getHost());
        BlopOAuthBridge.deliverIntentUri(data);
    }
}
