package com.benschwank.blop;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;

import androidx.annotation.Keep;

/**
 * AppAuth-style OAuth redirect catcher.
 *
 * Chrome Custom Tabs hand the custom-scheme callback to a dedicated,
 * noHistory activity more reliably than routing it through the heavy
 * Qt {@link BlopActivity}. We forward the URI into {@link BlopOAuthBridge}
 * and then re-focus the main activity.
 */
@Keep
public class BlopOAuthRedirectActivity extends Activity {
    private static final String TAG = "BlopOAuthRedirect";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        handleIntent(getIntent(), "onCreate");
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        handleIntent(intent, "onNewIntent");
    }

    private void handleIntent(Intent intent, String origin) {
        final Uri data = intent != null ? intent.getData() : null;
        Log.i(TAG, origin + " data=" + data);
        if (data != null) {
            BlopOAuthBridge.deliverIntentUri(data);
        } else {
            Log.w(TAG, origin + ": missing intent data");
        }

        // Stay in the app task (no empty taskAffinity) so CLEAR_TOP pops the
        // Chrome Custom Tab that was launched on top of BlopActivity. An empty
        // affinity started this trampoline in a different task, so BLOP came
        // back behind a blank Custom Tab — the "Redirect zu BLOP → Standbild"
        // hang. Do not attach the URI to this Intent: BlopOAuthBridge already
        // delivered it, and a second onNewIntent would double-exchange.
        try {
            Intent main = new Intent(this, BlopActivity.class);
            main.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP
                    | Intent.FLAG_ACTIVITY_SINGLE_TOP
                    | Intent.FLAG_ACTIVITY_REORDER_TO_FRONT
                    | Intent.FLAG_ACTIVITY_NEW_TASK);
            startActivity(main);
        } catch (Exception e) {
            Log.e(TAG, "failed to re-focus BlopActivity", e);
        }
        finish();
    }
}
