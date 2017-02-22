package com.freerdp.freerdpcore.presentation;

import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.os.Build;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.webkit.WebView;

import com.freerdp.freerdpcore.R;
import com.freerdp.freerdpcore.services.LibFreeRDP;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.IllegalFormatException;
import java.util.Locale;

public class AboutActivity extends AppCompatActivity {
    private static final String TAG = AboutActivity.class.toString();
    private WebView mWebView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_about);
        mWebView = (WebView) findViewById(R.id.activity_about_webview);
    }

    @Override
    protected void onResume() {
        populate();
        super.onResume();
    }

    private void populate() {
        // get app version
        String version;
        try {
            version = getPackageManager().getPackageInfo(getPackageName(), 0).versionName;
        } catch (PackageManager.NameNotFoundException e) {
            version = "unknown";
        }

        StringBuilder total = new StringBuilder();
        try {
            String filename = ((getResources().getConfiguration().screenLayout & Configuration.SCREENLAYOUT_SIZE_MASK) >= Configuration.SCREENLAYOUT_SIZE_LARGE) ? "about.html" : "about_phone.html";
            Locale def = Locale.getDefault();
            String prefix = def.getLanguage().toLowerCase(def);

            String file = prefix + "_about_page/" + filename;
            InputStream is;
            try {
                is = getAssets().open(file);
                is.close();
            } catch (IOException e) {
                Log.e(TAG, "Missing localized asset " + file, e);
                file = "about_page/" + filename;
            }
            BufferedReader r = new BufferedReader(new InputStreamReader(getAssets().open(file)));
            String line;
            while ((line = r.readLine()) != null) {
                total.append(line);
            }
        } catch (IOException e) {
        }

        // append FreeRDP core version to app version
        version = version + " (" + LibFreeRDP.getVersion() + ")";

        String about_html = "no about ;(";
        try {
            about_html = String.format(total.toString(), version, Build.VERSION.RELEASE, Build.MODEL);
        } catch (IllegalFormatException e) {
            about_html = "Nothing here ;(";
        }

        Locale def = Locale.getDefault();
        String prefix = def.getLanguage().toLowerCase(def);

        String base = "file:///android_asset/";
        String dir = prefix + "_about_page/";
        try {
            InputStream is = getAssets().open(dir);
            is.close();
            dir = base + dir;
        } catch (IOException e) {
            Log.e(TAG, "Missing localized asset " + dir, e);
        }

        mWebView.loadDataWithBaseURL(dir, about_html, "text/html", null,
                "about:blank");
    }

}
