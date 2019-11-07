/*
   Activity that displays the help pages

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.presentation;

import android.content.res.Configuration;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.webkit.WebSettings;
import android.webkit.WebView;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.Locale;

public class HelpActivity extends AppCompatActivity
{

	private static final String TAG = HelpActivity.class.toString();

	@Override public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		WebView webview = new WebView(this);
		setContentView(webview);

		String filename;
		if ((getResources().getConfiguration().screenLayout &
		     Configuration.SCREENLAYOUT_SIZE_MASK) >= Configuration.SCREENLAYOUT_SIZE_LARGE)
			filename = "gestures.html";
		else
			filename = "gestures_phone.html";

		WebSettings settings = webview.getSettings();
		settings.setDomStorageEnabled(true);
		settings.setUseWideViewPort(true);
		settings.setLoadWithOverviewMode(true);
		settings.setSupportZoom(true);
		settings.setJavaScriptEnabled(true);

		settings.setAllowContentAccess(true);
		settings.setAllowFileAccess(true);

		final Locale def = Locale.getDefault();
		final String prefix = def.getLanguage().toLowerCase(def);

		final String base = "file:///android_asset/";
		final String baseName = "help_page";
		String dir = prefix + "_" + baseName + "/";
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
			dir = baseName + "/";
			file = dir + filename;
		}

		webview.loadUrl(base + file);
	}
}
