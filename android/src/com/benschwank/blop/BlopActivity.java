package com.benschwank.blop;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.widget.Toast;

import androidx.annotation.Keep;
import androidx.browser.customtabs.CustomTabsIntent;

import org.qtproject.qt.android.bindings.QtActivity;

/**
 * Custom QtActivity subclass that forwards Google OAuth deep links
 * (custom scheme {@code com.benschwank.blop://oauth2redirect} OR
 * {@code com.benschwank.blop:/oauth2redirect}; v3.18.9 accepts both via
 * paired manifest <data> elements) into {@link BlopOAuthBridge}, which
 * in turn calls back into the C++ {@code GoogleAuthManager} via JNI.
 *
 * Cold-launch deep links arrive in {@link #onCreate(Bundle)} via the
 * launching {@link Intent}; warm/relaunch deep links are delivered through
 * {@link #onNewIntent(Intent)}. Both paths are handled identically.
 *
 * v3.18.34: keeps a static activity reference so the C++ side can launch a
 * Chrome Custom Tab for the OAuth URL. Custom Tabs run in the same task and
 * reliably redirect back to the app via the registered custom scheme.
 */
@Keep
public class BlopActivity extends QtActivity {
    private static final String TAG = "BlopActivity";
    private static volatile BlopActivity sInstance;

    /**
     * Opens the given OAuth URL in a Chrome Custom Tab. Called from C++ via JNI.
     * Falls back to a regular browser intent if no Custom Tabs provider is available.
     */
    @Keep
    public static void openCustomTab(String url) {
        final BlopActivity activity = sInstance;
        if (activity == null || url == null || url.isEmpty()) {
            Log.w(TAG, "openCustomTab: no activity or no URL");
            return;
        }
        try {
            CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
            CustomTabsIntent intent = builder.build();
            intent.launchUrl(activity, Uri.parse(url));
        } catch (Exception e) {
            Log.w(TAG, "openCustomTab failed, falling back to ACTION_VIEW", e);
            Intent fallback = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
            activity.startActivity(fallback);
        }
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        sInstance = this;
        forwardOAuthIntent(getIntent(), "onCreate");
    }

    @Override
    protected void onDestroy() {
        if (sInstance == this) {
            sInstance = null;
        }
        super.onDestroy();
    }

    @Override
    public void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        // Update the activity's intent so getIntent() returns the latest data.
        setIntent(intent);
        forwardOAuthIntent(intent, "onNewIntent");
    }

    private void forwardOAuthIntent(Intent intent, String origin) {
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
        final String hostStr = data.getHost() == null ? "(null)" : data.getHost();
        final String pathStr = data.getPath() == null ? "(null)" : data.getPath();
        Log.i(TAG, origin + " forwarding OAuth deep link host=" + hostStr
                + " path=" + pathStr);

        // v3.18.9: brief on-screen confirmation so the user immediately sees
        // whether the OAuth deep-link reached the app. If the user never
        // sees this toast after returning from Chrome Custom Tab, the issue
        // is on the intent-filter / browser-redirect side (manifest match or
        // Cloud Console redirect_uri_mismatch). If the user DOES see the
        // toast but login still fails, the issue is in the C++
        // token-exchange path (handleDeepLinkCallback / exchangeCodeForToken).
        // Either way the next test produces a single clear datapoint without
        // requiring adb logcat.
        final String toastMsg = "OAuth callback (" + origin + "): host="
                + hostStr + " path=" + pathStr;
        runOnUiThread(() -> {
            try {
                Toast.makeText(BlopActivity.this, toastMsg, Toast.LENGTH_LONG).show();
            } catch (Throwable t) {
                Log.w(TAG, "diagnostic toast failed", t);
            }
        });

        BlopOAuthBridge.deliverIntentUri(data);
    }
}
