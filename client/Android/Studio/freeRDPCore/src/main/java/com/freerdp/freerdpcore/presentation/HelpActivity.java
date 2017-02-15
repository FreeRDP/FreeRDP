/*
   Activity that displays the help pages

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.presentation;

import android.app.Activity;
import android.content.res.Configuration;
import android.os.Bundle;
import android.util.Log;
import android.webkit.WebView;

import java.io.IOException;
import java.io.InputStream;
import java.util.Locale;
public class HelpActivity extends Activity {

	private static final String TAG = "FreeRDPCore.HelpActivity";
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		
		WebView webview = new WebView(this);
		setContentView(webview);

		String filename;
		if ((getResources().getConfiguration().screenLayout & Configuration.SCREENLAYOUT_SIZE_MASK) >= Configuration.SCREENLAYOUT_SIZE_LARGE)
			filename = "gestures.html";
		else
			filename = "gestures_phone.html";

		webview.getSettings().setJavaScriptEnabled(true);
		Locale def = Locale.getDefault();
		String prefix = def.getLanguage().toLowerCase(def);

		String base = "file:///android_asset/"; 
		String dir = prefix + "_help_page/"
				+ filename;
		try {
			InputStream is = getAssets().open(dir);
			is.close();
			dir = base + dir;
		} catch (IOException e) {
			Log.e(TAG, "Missing localized asset " + dir, e);
			dir = "file:///android_asset/help_page/" + filename;
		} 

		webview.loadUrl(dir);
	}
}
