package com.benschwank.blop;

import android.content.ContentResolver;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.util.Log;

import androidx.annotation.Keep;
import androidx.browser.customtabs.CustomTabsIntent;

import org.qtproject.qt.android.bindings.QtActivity;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Custom QtActivity subclass that forwards Google OAuth deep links
 * (custom scheme {@code com.benschwank.blop://oauth2redirect} OR
 * {@code com.benschwank.blop:/oauth2redirect}; v3.18.9 accepts both via
 * paired manifest <data> elements) into {@link BlopOAuthBridge}, which
 * in turn calls back into the C++ {@code GoogleAuthManager} via JNI.
 *
 * Also hosts SAF document open/create (ACTION_OPEN_DOCUMENT /
 * ACTION_CREATE_DOCUMENT) for Android-safe file pick without QFileDialog.
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
    private static final int REQ_OPEN_DOCUMENT = 0xB10B;
    private static final int REQ_CREATE_DOCUMENT = 0xB10C;
    private static volatile BlopActivity sInstance;
    private static volatile Uri sLastCreateUri;
    private static volatile String sCreateCachePath;

    /**
     * Opens the given OAuth URL in a Chrome Custom Tab. Called from C++ via JNI.
     * Falls back to a regular browser intent if no Custom Tabs provider is available.
     */
    @Keep
    public static void openCustomTab(String url) {
        Log.i(TAG, "=== openCustomTab called ===");
        Log.i(TAG, "openCustomTab: url=" + url);
        final BlopActivity activity = sInstance;
        if (activity == null) {
            Log.w(TAG, "openCustomTab: activity is null");
            return;
        }
        if (url == null || url.isEmpty()) {
            Log.w(TAG, "openCustomTab: url is null or empty");
            return;
        }
        try {
            Log.i(TAG, "openCustomTab: launching Chrome Custom Tab");
            CustomTabsIntent.Builder builder = new CustomTabsIntent.Builder();
            CustomTabsIntent intent = builder.build();
            intent.launchUrl(activity, Uri.parse(url));
            Log.i(TAG, "openCustomTab: Chrome Custom Tab launched successfully");
        } catch (Exception e) {
            Log.w(TAG, "openCustomTab failed, falling back to ACTION_VIEW", e);
            Intent fallback = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
            activity.startActivity(fallback);
            Log.i(TAG, "openCustomTab: fallback ACTION_VIEW launched");
        }
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        sInstance = this;
        Log.i(TAG, "=== BlopActivity onCreate ===");
        Log.i(TAG, "Intent: " + getIntent());
        Log.i(TAG, "Intent data: " + getIntent().getData());
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
        Log.i(TAG, "=== BlopActivity onNewIntent ===");
        Log.i(TAG, "Intent: " + intent);
        Log.i(TAG, "Intent data: " + intent.getData());
        forwardOAuthIntent(intent, "onNewIntent");
    }

    private void forwardOAuthIntent(Intent intent, String origin) {
        Log.i(TAG, origin + " forwardOAuthIntent called");
        if (intent == null) {
            Log.w(TAG, origin + " intent is null");
            return;
        }
        final Uri data = intent.getData();
        if (data == null) {
            Log.w(TAG, origin + " intent data is null");
            return;
        }
        Log.i(TAG, origin + " received URI: " + data.toString());
        final String scheme = data.getScheme();
        if (scheme == null || !scheme.equalsIgnoreCase("com.benschwank.blop")) {
            Log.w(TAG, origin + " scheme mismatch: " + scheme + " (expected com.benschwank.blop)");
            return;
        }
        final String hostStr = data.getHost() == null ? "(null)" : data.getHost();
        final String pathStr = data.getPath() == null ? "(null)" : data.getPath();
        Log.i(TAG, origin + " forwarding OAuth deep link host=" + hostStr
                + " path=" + pathStr + " query=" + data.getQuery());

        // Do NOT show a diagnostic toast here — host=(null) is the *expected*
        // shape for com.benschwank.blop:/oauth2redirect and looked like a
        // failure to users. Errors are surfaced by C++ via BlopDialogs.

        BlopOAuthBridge.deliverIntentUri(data);
    }

    /**
     * Launch SAF picker for opening a document. mimeCsv is semicolon-separated
     * MIME types (e.g. "image/*;image/png" or "application/pdf").
     */
    @Keep
    public static void openDocument(String mimeCsv) {
        final BlopActivity activity = sInstance;
        if (activity == null) {
            Log.w(TAG, "openDocument: activity null");
            BlopContentPickerBridge.deliverPath("");
            return;
        }
        activity.runOnUiThread(() -> {
            try {
                Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                intent.addCategory(Intent.CATEGORY_OPENABLE);
                intent.setType("*/*");
                if (mimeCsv != null && !mimeCsv.isEmpty()) {
                    String[] types = mimeCsv.split(";");
                    if (types.length == 1) {
                        intent.setType(types[0]);
                    } else {
                        intent.putExtra(Intent.EXTRA_MIME_TYPES, types);
                    }
                }
                intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
                activity.startActivityForResult(intent, REQ_OPEN_DOCUMENT);
            } catch (Exception e) {
                Log.e(TAG, "openDocument failed", e);
                BlopContentPickerBridge.deliverPath("");
            }
        });
    }

    /**
     * Launch SAF create document. After the user picks a destination, we copy
     * cachePath into the URI when {@link #publishCacheToLastUri(String)} runs.
     * Immediately delivers cachePath to C++ so the caller can write bytes.
     */
    @Keep
    public static void createDocument(String mimeType, String suggestedName,
                                      String cachePath) {
        final BlopActivity activity = sInstance;
        if (activity == null) {
            Log.w(TAG, "createDocument: activity null");
            BlopContentPickerBridge.deliverPath("");
            return;
        }
        sCreateCachePath = cachePath;
        activity.runOnUiThread(() -> {
            try {
                Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
                intent.addCategory(Intent.CATEGORY_OPENABLE);
                intent.setType(mimeType != null && !mimeType.isEmpty()
                                   ? mimeType : "application/octet-stream");
                if (suggestedName != null && !suggestedName.isEmpty())
                    intent.putExtra(Intent.EXTRA_TITLE, suggestedName);
                activity.startActivityForResult(intent, REQ_CREATE_DOCUMENT);
            } catch (Exception e) {
                Log.e(TAG, "createDocument failed", e);
                sCreateCachePath = null;
                BlopContentPickerBridge.deliverPath("");
            }
        });
    }

    /** Copy a local cache file into the last CREATE_DOCUMENT URI. */
    @Keep
    public static void publishCacheToLastUri(String cachePath) {
        final BlopActivity activity = sInstance;
        final Uri uri = sLastCreateUri;
        if (activity == null || uri == null || cachePath == null ||
            cachePath.isEmpty()) {
            Log.w(TAG, "publishCacheToLastUri: missing activity/uri/path");
            return;
        }
        activity.runOnUiThread(() -> {
            try {
                copyFileToUri(activity, new File(cachePath), uri);
                Log.i(TAG, "publishCacheToLastUri: ok");
            } catch (Exception e) {
                Log.e(TAG, "publishCacheToLastUri failed", e);
            }
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode,
                                    Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQ_OPEN_DOCUMENT) {
            if (resultCode == RESULT_OK && data != null && data.getData() != null) {
                final Uri uri = data.getData();
                try {
                    getContentResolver().takePersistableUriPermission(
                        uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
                } catch (SecurityException ignored) {
                }
                final String path = copyUriToCache(uri);
                BlopContentPickerBridge.deliverPath(path != null ? path : "");
            } else {
                BlopContentPickerBridge.deliverPath("");
            }
            return;
        }
        if (requestCode == REQ_CREATE_DOCUMENT) {
            if (resultCode == RESULT_OK && data != null && data.getData() != null) {
                sLastCreateUri = data.getData();
                final String cache =
                    sCreateCachePath != null ? sCreateCachePath : "";
                BlopContentPickerBridge.deliverPath(cache);
            } else {
                sLastCreateUri = null;
                sCreateCachePath = null;
                BlopContentPickerBridge.deliverPath("");
            }
        }
    }

    private String copyUriToCache(Uri uri) {
        try {
            File dir = new File(getCacheDir(), "blop_picker");
            if (!dir.exists() && !dir.mkdirs()) {
                Log.w(TAG, "copyUriToCache: mkdir failed");
            }
            String name = queryDisplayName(uri);
            if (name == null || name.isEmpty())
                name = "picked_" + System.currentTimeMillis();
            // Sanitize filename
            name = name.replaceAll("[\\\\/:*?\"<>|]", "_");
            File out = new File(dir, name);
            // Avoid collisions
            int n = 1;
            while (out.exists()) {
                out = new File(dir, n + "_" + name);
                n++;
            }
            try (InputStream in = getContentResolver().openInputStream(uri);
                 OutputStream os = new FileOutputStream(out)) {
                if (in == null)
                    return null;
                byte[] buf = new byte[8192];
                int r;
                while ((r = in.read(buf)) >= 0)
                    os.write(buf, 0, r);
            }
            Log.i(TAG, "copyUriToCache: " + out.getAbsolutePath());
            return out.getAbsolutePath();
        } catch (Exception e) {
            Log.e(TAG, "copyUriToCache failed", e);
            return null;
        }
    }

    private String queryDisplayName(Uri uri) {
        try (Cursor c = getContentResolver().query(
                 uri, new String[] {OpenableColumns.DISPLAY_NAME}, null, null,
                 null)) {
            if (c != null && c.moveToFirst()) {
                int idx = c.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if (idx >= 0)
                    return c.getString(idx);
            }
        } catch (Exception e) {
            Log.w(TAG, "queryDisplayName failed", e);
        }
        return null;
    }

    private static void copyFileToUri(BlopActivity activity, File src, Uri uri)
        throws Exception {
        try (InputStream in = new FileInputStream(src);
             OutputStream out =
                 activity.getContentResolver().openOutputStream(uri, "w")) {
            if (out == null)
                throw new IllegalStateException("null output stream");
            byte[] buf = new byte[8192];
            int r;
            while ((r = in.read(buf)) >= 0)
                out.write(buf, 0, r);
            out.flush();
        }
    }
}
