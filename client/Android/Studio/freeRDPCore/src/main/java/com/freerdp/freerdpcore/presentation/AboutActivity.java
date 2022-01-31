package com.freerdp.freerdpcore.presentation;

import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.nfc.FormatException;
import android.os.Build;
import android.os.Bundle;
import androidx.core.text.TextUtilsCompat;
import androidx.appcompat.app.AppCompatActivity;
import android.util.Log;
import android.webkit.WebSettings;
import android.webkit.WebView;

import com.freerdp.freerdpcore.R;
import com.freerdp.freerdpcore.services.LibFreeRDP;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.Formatter;
import java.util.IllegalFormatException;
import java.util.Locale;

public class AboutActivity extends AppCompatActivity
{
	private static final String TAG = AboutActivity.class.toString();
	private WebView mWebView;

	@Override protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_about);
		mWebView = (WebView)findViewById(R.id.activity_about_webview);
	}

	@Override protected void onResume()
	{
		populate();
		super.onResume();
	}

	private void populate()
	{
		StringBuilder total = new StringBuilder();

		String filename = "about_phone.html";
		if ((getResources().getConfiguration().screenLayout &
		     Configuration.SCREENLAYOUT_SIZE_MASK) >= Configuration.SCREENLAYOUT_SIZE_LARGE)
		{
			filename = "about.html";
		}
		Locale def = Locale.getDefault();
		String prefix = def.getLanguage().toLowerCase(def);

		String dir = prefix + "_about_page/";
		String file = dir + filename;
		InputStream is;
		try
		{
			is = getAssets().open(file);
			is.close();
		}
		catch (IOException e)
		{
			Log.e(TAG, "Missing localized asset " + file, e);
			dir = "about_page/";
			file = dir + filename;
		}

		try
		{
			try (BufferedReader r =
			         new BufferedReader(new InputStreamReader(getAssets().open(file))))
			{
				String line;
				while ((line = r.readLine()) != null)
				{
					total.append(line);
					total.append("\n");
				}
			}
		}
		catch (IOException e)
		{
			Log.e(TAG, "Could not read about page " + file, e);
		}

		// append FreeRDP core version to app version
		// get app version
		String version;
		try
		{
			version = getPackageManager().getPackageInfo(getPackageName(), 0).versionName;
		}
		catch (PackageManager.NameNotFoundException e)
		{
			version = "unknown";
		}
		version = version + " (" + LibFreeRDP.getVersion() + ")";

		WebSettings settings = mWebView.getSettings();
		settings.setDomStorageEnabled(true);
		settings.setUseWideViewPort(true);
		settings.setLoadWithOverviewMode(true);
		settings.setSupportZoom(true);

		final String base = "file:///android_asset/" + dir;

		final String rawHtml = total.toString();
		final String html = rawHtml.replaceAll("%AFREERDP_VERSION%", version)
		                        .replaceAll("%SYSTEM_VERSION%", Build.VERSION.RELEASE)
		                        .replaceAll("%DEVICE_MODEL%", Build.MODEL);

		mWebView.loadDataWithBaseURL(base, html, "text/html", null, "about:blank");
	}
}
