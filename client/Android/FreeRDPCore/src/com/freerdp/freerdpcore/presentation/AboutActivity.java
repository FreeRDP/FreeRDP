/*
   Activity showing information about aFreeRDP and FreeRDP build information

   Copyright 2013 Thinstuff Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.presentation;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.IllegalFormatException;

import com.freerdp.freerdpcore.services.LibFreeRDP;

import android.app.Activity;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Configuration;
import android.os.Build;
import android.os.Bundle;
import android.webkit.WebView;

public class AboutActivity extends Activity {
	
	@Override	
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		
		WebView webview = new WebView(this);
		setContentView(webview);

		// get app version
		String version;
		try
		{
			version = getPackageManager().getPackageInfo(getPackageName(), 0).versionName;			
		}
		catch(NameNotFoundException e)
		{
			version = "unknown";			
		}
		
		StringBuilder total = new StringBuilder();
		try
		{
			String filename = ((getResources().getConfiguration().screenLayout & Configuration.SCREENLAYOUT_SIZE_MASK) >= Configuration.SCREENLAYOUT_SIZE_LARGE) ? "about.html" : "about_phone.html";
			BufferedReader r = new BufferedReader(new InputStreamReader(getAssets().open("about_page/" + filename)));
			String line;
			while ((line = r.readLine()) != null) {
			    total.append(line);
			}			
		}
		catch(IOException e)
		{			
		}
		
		// append FreeRDP core version to app version
		version = version + " (" + LibFreeRDP.getVersion() + ")";
		
		String about_html = "no about ;(";
		try
		{
			about_html = String.format(total.toString(), version, Build.VERSION.RELEASE ,Build.MODEL);
		}
		catch(IllegalFormatException e)
		{
			about_html="Nothing here ;(";
		}
		webview.getSettings().setJavaScriptEnabled(true);
		webview.loadDataWithBaseURL("file:///android_asset/about_page/", about_html, "text/html", null, "about:blank");
	}
}
