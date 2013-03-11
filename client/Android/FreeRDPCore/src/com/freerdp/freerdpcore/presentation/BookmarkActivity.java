/*
   Bookmark editing activity

   Copyright 2013 Thinstuff Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.presentation;

import java.io.File;
import java.io.IOException;

import com.freerdp.freerdpcore.R;
import com.freerdp.freerdpcore.application.GlobalApp;
import com.freerdp.freerdpcore.domain.BookmarkBase;
import com.freerdp.freerdpcore.domain.ConnectionReference;
import com.freerdp.freerdpcore.domain.ManualBookmark;
import com.freerdp.freerdpcore.services.BookmarkBaseGateway;
import com.freerdp.freerdpcore.utils.RDPFileParser;

import android.app.AlertDialog;
import android.content.ComponentName;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.os.Bundle;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceActivity;
import android.util.Log;

public class BookmarkActivity extends PreferenceActivity implements OnSharedPreferenceChangeListener
{
	public static final String PARAM_CONNECTION_REFERENCE = "conRef"; 		

	private static final String TAG = "BookmarkActivity";

	private int current_preferences;
	private static final int PREFERENCES_BOOKMARK = 1;
	private static final int PREFERENCES_CREDENTIALS = 2;
	private static final int PREFERENCES_SCREEN = 3;
	private static final int PREFERENCES_PERFORMANCE = 4;
	private static final int PREFERENCES_ADVANCED = 5;
	private static final int PREFERENCES_SCREEN3G = 6;
	private static final int PREFERENCES_PERFORMANCE3G = 7;

	// bookmark needs to be static because the activity is started for each subview 
	// (we have to do this because Android has a bug where the style for Preferences 
	// is only applied to the first PreferenceScreen but not to subsequent ones)
	private static BookmarkBase bookmark = null;
	
	private static boolean settings_changed = false;
	private static boolean new_bookmark = false;
	
	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		// init shared preferences for activity
		getPreferenceManager().setSharedPreferencesName("TEMP");
		getPreferenceManager().setSharedPreferencesMode(MODE_PRIVATE);

		if (bookmark == null)
		{
			// if we have a bookmark id set in the extras we are in edit mode
			Bundle bundle = getIntent().getExtras(); 
			if(bundle != null)
			{
				// See if we got a connection reference to a bookmark
				if(bundle.containsKey(PARAM_CONNECTION_REFERENCE))
				{
					String refStr = bundle.getString(PARAM_CONNECTION_REFERENCE);
					if(ConnectionReference.isManualBookmarkReference(refStr))
					{
						bookmark = GlobalApp.getManualBookmarkGateway().findById(ConnectionReference.getManualBookmarkId(refStr));
						new_bookmark = false;
					}
					else if(ConnectionReference.isHostnameReference(refStr))
					{
						bookmark = new ManualBookmark();
						bookmark.<ManualBookmark>get().setLabel(ConnectionReference.getHostname(refStr));
						bookmark.<ManualBookmark>get().setHostname(ConnectionReference.getHostname(refStr));
						new_bookmark = true;
					}
					else if (ConnectionReference.isFileReference(refStr))
					{
						String file = ConnectionReference.getFile(refStr);

						bookmark = new ManualBookmark();
						bookmark.setLabel(file);

						try
						{
							RDPFileParser rdpFile = new RDPFileParser(file);
							updateBookmarkFromFile((ManualBookmark)bookmark, rdpFile);
							
							bookmark.setLabel(new File(file).getName());
							new_bookmark = true;
						}
						catch (IOException e)
						{
							Log.e(TAG, "Failed reading RDP file", e);
						}
					}
				}					
			}			

			// last chance - ensure we really have a valid bookmark
			if(bookmark == null)
				bookmark = new ManualBookmark();				

			// update preferences from bookmark
			bookmark.writeToSharedPreferences(getPreferenceManager().getSharedPreferences());
			
			// no settings changed yet
			settings_changed = false;
		}
				
		// load the requested settings resource
		if (getIntent() == null || getIntent().getData() == null)
		{
			addPreferencesFromResource(R.xml.bookmark_settings);
			current_preferences = PREFERENCES_BOOKMARK;			
		}
		else if (getIntent().getData().toString().equals("preferences://screen_settings"))
		{
			addPreferencesFromResource(R.xml.screen_settings);
			current_preferences = PREFERENCES_SCREEN;
		}
		else if (getIntent().getData().toString().equals("preferences://performance_flags"))
		{
			addPreferencesFromResource(R.xml.performance_flags);
			current_preferences = PREFERENCES_PERFORMANCE;
		}
		else if (getIntent().getData().toString().equals("preferences://screen_settings_3g"))
		{
			addPreferencesFromResource(R.xml.screen_settings_3g);
			current_preferences = PREFERENCES_SCREEN3G;
		}
		else if (getIntent().getData().toString().equals("preferences://performance_flags_3g"))
		{
			addPreferencesFromResource(R.xml.performance_flags_3g);
			current_preferences = PREFERENCES_PERFORMANCE3G;
		}
		else if (getIntent().getData().toString().equals("preferences://advanced_settings"))
		{
			addPreferencesFromResource(R.xml.advanced_settings);
			current_preferences = PREFERENCES_ADVANCED;
		}
		else if (getIntent().getData().toString().equals("preferences://credentials_settings"))
		{
			addPreferencesFromResource(R.xml.credentials_settings);
			current_preferences = PREFERENCES_CREDENTIALS;
		}
		else
		{
			addPreferencesFromResource(R.xml.bookmark_settings);
			current_preferences = PREFERENCES_BOOKMARK;
		}
		
		// update UI with bookmark data
		initSettings(getPreferenceManager().getSharedPreferences());

		// register for preferences changed notification
		getPreferenceManager().getSharedPreferences().registerOnSharedPreferenceChangeListener(this);
		
		// set the correct component names in our preferencescreen settings 
		setIntentComponentNames();
	}

	private void updateBookmarkFromFile(ManualBookmark bookmark, RDPFileParser rdpFile)
	{
		String s;
		Integer i;
		
		s = rdpFile.getString("full address");
		if (s != null)
		{
			// this gets complicated as it can include port
			if (s.lastIndexOf(":") > s.lastIndexOf("]"))
			{
				try
				{
					String port = s.substring(s.lastIndexOf(":") + 1);
					bookmark.setPort(Integer.parseInt(port));
				}
				catch (NumberFormatException e)
				{
					Log.e(TAG, "Malformed address");
				}
				
				s = s.substring(0, s.lastIndexOf(":"));
			}
			
			// or even be an ipv6 address
			if (s.startsWith("[") && s.endsWith("]"))
				s = s.substring(1, s.length() - 1);
			
			bookmark.setHostname(s);
		}

		i = rdpFile.getInteger("server port");
		if (i != null)
			bookmark.setPort(i);

		s = rdpFile.getString("username");
		if (s != null)
			bookmark.setUsername(s);

		s = rdpFile.getString("domain");
		if (s != null)
			bookmark.setDomain(s);
		
		i = rdpFile.getInteger("connect to console");
		if (i != null)
			bookmark.getAdvancedSettings().setConsoleMode(i == 1);
	}

	private void setIntentComponentNames()
	{
		// we set the component name for our sub-activity calls here because we don't know the package
		// name of the main app in our library project. 
		ComponentName compName = new ComponentName(getPackageName(), BookmarkActivity.class.getName());		
		String[] prefKeys = {
			"bookmark.credentials",
			"bookmark.screen",
			"bookmark.performance",
			"bookmark.advanced",
			"bookmark.screen_3g",
			"bookmark.performance_3g"
		};
		
		for (int i = 0; i < prefKeys.length; ++i)
		{
			Preference pref = findPreference(prefKeys[i]);
			if (pref != null)
				pref.getIntent().setComponent(compName);			
		}
		
		
	}
	
	@Override
	public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {
		settings_changed = true;
		switch (current_preferences)
		{
			case PREFERENCES_BOOKMARK:
				bookmarkSettingsChanged(sharedPreferences, key);
				break;
				
			case PREFERENCES_ADVANCED:
				advancedSettingsChanged(sharedPreferences, key);
				break;
				
			case PREFERENCES_CREDENTIALS:
				credentialsSettingsChanged(sharedPreferences, key);
				break;
				
			case PREFERENCES_SCREEN:
			case PREFERENCES_SCREEN3G:
				screenSettingsChanged(sharedPreferences, key);
				break;
		}
					
	}

	private void initSettings(SharedPreferences sharedPreferences)
	{
		switch (current_preferences)
		{
			case PREFERENCES_BOOKMARK:
				initBookmarkSettings(sharedPreferences);
				break;
				
			case PREFERENCES_ADVANCED:
				initAdvancedSettings(sharedPreferences);
				break;
				
			case PREFERENCES_CREDENTIALS:
				initCredentialsSettings(sharedPreferences);
				break;
				
			case PREFERENCES_SCREEN:
				initScreenSettings(sharedPreferences);
				break;

			case PREFERENCES_SCREEN3G:
				initScreenSettings3G(sharedPreferences);
				break;
		}		
	}
	
	private void initBookmarkSettings(SharedPreferences sharedPreferences)
	{
		bookmarkSettingsChanged(sharedPreferences, "bookmark.label");		
		bookmarkSettingsChanged(sharedPreferences, "bookmark.hostname");		
		bookmarkSettingsChanged(sharedPreferences, "bookmark.port");		
		bookmarkSettingsChanged(sharedPreferences, "bookmark.username");		
		bookmarkSettingsChanged(sharedPreferences, "bookmark.resolution");
	}
		
	private void bookmarkSettingsChanged(SharedPreferences sharedPreferences, String key)
	{	
		if (key.equals("bookmark.label") && findPreference(key) != null)
			findPreference(key).setSummary(sharedPreferences.getString(key, ""));		
		else if (key.equals("bookmark.hostname") && findPreference(key) != null)
			findPreference(key).setSummary(sharedPreferences.getString(key, ""));
		else if (key.equals("bookmark.port") && findPreference(key) != null)
			findPreference(key).setSummary(String.valueOf(sharedPreferences.getInt(key, -1)));			
		else if (key.equals("bookmark.username"))
		{
			String username = sharedPreferences.getString(key, "<none>");
			if (username.length() == 0)
				username = "<none>";
			findPreference("bookmark.credentials").setSummary(username);			
		}
		else if (key.equals("bookmark.resolution") || key.equals("bookmark.colors") || key.equals("bookmark.width") || key.equals("bookmark.height"))
		{
			String resolution = sharedPreferences.getString("bookmark.resolution", "800x600");
			if (resolution.equals("automatic"))
				resolution = getResources().getString(R.string.resolution_automatic);
			else if (resolution.equals("custom"))
				resolution = getResources().getString(R.string.resolution_custom);
			resolution += "@" + sharedPreferences.getInt("bookmark.colors", 16);
			findPreference("bookmark.screen").setSummary(resolution);						
		}
	}

	private void initAdvancedSettings(SharedPreferences sharedPreferences)
	{
		advancedSettingsChanged(sharedPreferences, "bookmark.enable_3g_settings");		
		advancedSettingsChanged(sharedPreferences, "bookmark.security");		
		advancedSettingsChanged(sharedPreferences, "bookmark.resolution_3g");		
		advancedSettingsChanged(sharedPreferences, "bookmark.remote_program");		
		advancedSettingsChanged(sharedPreferences, "bookmark.work_dir");		
	}

	private void advancedSettingsChanged(SharedPreferences sharedPreferences, String key)
	{		
		if (key.equals("bookmark.enable_3g_settings"))
		{
			boolean enabled = sharedPreferences.getBoolean(key, false);
			findPreference("bookmark.screen_3g").setEnabled(enabled);
			findPreference("bookmark.performance_3g").setEnabled(enabled);
		}
		else if (key.equals("bookmark.security"))
		{
			ListPreference listPreference = (ListPreference)findPreference(key);			
			CharSequence security = listPreference.getEntries()[sharedPreferences.getInt(key, 0)];
			listPreference.setSummary(security);			
		}
		else if (key.equals("bookmark.resolution_3g") || key.equals("bookmark.colors_3g") || key.equals("bookmark.width_3g") || key.equals("bookmark.height_3g"))
		{
			String resolution = sharedPreferences.getString("bookmark.resolution_3g", "800x600");			
			if (resolution.equals("automatic"))
				resolution = getResources().getString(R.string.resolution_automatic);
			else if (resolution.equals("custom"))
				resolution = getResources().getString(R.string.resolution_custom);
			resolution += "@" + sharedPreferences.getInt("bookmark.colors_3g", 16);
			findPreference("bookmark.screen_3g").setSummary(resolution);						
		}
		else if (key.equals("bookmark.remote_program"))
			findPreference(key).setSummary(sharedPreferences.getString(key, ""));		
		else if (key.equals("bookmark.work_dir"))
			findPreference(key).setSummary(sharedPreferences.getString(key, ""));
	}

	private void initCredentialsSettings(SharedPreferences sharedPreferences)
	{
		credentialsSettingsChanged(sharedPreferences, "bookmark.username");		
		credentialsSettingsChanged(sharedPreferences, "bookmark.password");		
		credentialsSettingsChanged(sharedPreferences, "bookmark.domain");		
	}

	private void credentialsSettingsChanged(SharedPreferences sharedPreferences, String key)
	{		
		if (key.equals("bookmark.username"))
			findPreference(key).setSummary(sharedPreferences.getString(key, ""));		
		else if (key.equals("bookmark.password"))
		{
			if (sharedPreferences.getString(key, "").length() == 0)
				findPreference(key).setSummary(getResources().getString(R.string.settings_password_empty));
			else
				findPreference(key).setSummary(getResources().getString(R.string.settings_password_present));
		}
		else if (key.equals("bookmark.domain"))
			findPreference(key).setSummary(sharedPreferences.getString(key, ""));		
	}

	private void initScreenSettings(SharedPreferences sharedPreferences)
	{
		screenSettingsChanged(sharedPreferences, "bookmark.colors");		
		screenSettingsChanged(sharedPreferences, "bookmark.resolution");		
		screenSettingsChanged(sharedPreferences, "bookmark.width");		
		screenSettingsChanged(sharedPreferences, "bookmark.height");		
	}

	private void initScreenSettings3G(SharedPreferences sharedPreferences)
	{
		screenSettingsChanged(sharedPreferences, "bookmark.colors_3g");		
		screenSettingsChanged(sharedPreferences, "bookmark.resolution_3g");		
		screenSettingsChanged(sharedPreferences, "bookmark.width_3g");		
		screenSettingsChanged(sharedPreferences, "bookmark.height_3g");		
	}

	private void screenSettingsChanged(SharedPreferences sharedPreferences, String key)
	{	
		// could happen during initialization because 3g and non-3g settings share this routine - just skip	
		if (findPreference(key) == null)
			return;
		
		if (key.equals("bookmark.colors") || key.equals("bookmark.colors_3g"))
		{
			ListPreference listPreference = (ListPreference)findPreference(key);
			listPreference.setSummary(listPreference.getEntry());						
		}
		else if (key.equals("bookmark.resolution") || key.equals("bookmark.resolution_3g"))
		{
			ListPreference listPreference = (ListPreference)findPreference(key);
			listPreference.setSummary(listPreference.getEntry());		
			
			boolean enabled = listPreference.getValue().equals("custom");
			if (key.equals("bookmark.resolution"))
			{
				findPreference("bookmark.width").setEnabled(enabled);				
				findPreference("bookmark.height").setEnabled(enabled);				
			}
			else
			{
				findPreference("bookmark.width_3g").setEnabled(enabled);				
				findPreference("bookmark.height_3g").setEnabled(enabled);				
			}
		}
		else if (key.equals("bookmark.width") || key.equals("bookmark.width_3g"))
			findPreference(key).setSummary(String.valueOf(sharedPreferences.getInt(key, 800)));		
		else if (key.equals("bookmark.height") || key.equals("bookmark.height_3g"))
			findPreference(key).setSummary(String.valueOf(sharedPreferences.getInt(key, 600)));		
	}

	private boolean verifySettings(SharedPreferences sharedPreferences) {
	
		boolean verifyFailed = false;
		// perform sanity checks on settings		
		//  Label set
		if (sharedPreferences.getString("bookmark.label", "").length() == 0)
			verifyFailed = true; 		

		//  Server and port specified		
		if (!verifyFailed && sharedPreferences.getString("bookmark.hostname", "").length() == 0)
			verifyFailed = true; 		

		//  Server and port specified		
		if (!verifyFailed && sharedPreferences.getInt("bookmark.port", -1) <= 0)
			verifyFailed = true; 		

		// if an error occured - display toast and return false	
		return (!verifyFailed);
	}
	
	private void finishAndResetBookmark()
	{
		bookmark = null;
		finish();
	}
	
	@Override
	public void onBackPressed()
	{
		// only proceed if we are in the main preferences screen
		if (current_preferences != PREFERENCES_BOOKMARK)
		{
			super.onBackPressed();
			return;			
		}
		
		SharedPreferences sharedPreferences = getPreferenceManager().getSharedPreferences();
		if(!verifySettings(sharedPreferences))
		{
			// ask the user if he wants to cancel or continue editing 
			AlertDialog.Builder builder = new AlertDialog.Builder(this);
			builder.setTitle(R.string.error_bookmark_incomplete_title)
				.setMessage(R.string.error_bookmark_incomplete)			
				.setPositiveButton(R.string.cancel, new DialogInterface.OnClickListener() {					
					@Override
					public void onClick(DialogInterface dialog, int which) {
						finishAndResetBookmark();
					}
				})
				.setNegativeButton(R.string.cont, new DialogInterface.OnClickListener() {					
					@Override
					public void onClick(DialogInterface dialog, int which) {
						dialog.cancel();						
					}
				})
				.show();

			return;
		}
		else
		{
			// ask the user if he wants to save or cancel editing if a setting has changed
			if (new_bookmark || settings_changed)
			{
				AlertDialog.Builder builder = new AlertDialog.Builder(this);
				builder.setTitle(R.string.dlg_title_save_bookmark)
					.setMessage(R.string.dlg_save_bookmark)			
					.setPositiveButton(R.string.yes, new DialogInterface.OnClickListener() {					
						@Override
						public void onClick(DialogInterface dialog, int which) {
							// read shared prefs back to bookmark
							bookmark.readFromSharedPreferences(getPreferenceManager().getSharedPreferences());
							
							BookmarkBaseGateway bookmarkGateway;
							if(bookmark.getType() == BookmarkBase.TYPE_MANUAL)
							{
								bookmarkGateway = GlobalApp.getManualBookmarkGateway();
								// remove any history entry for this bookmark
								GlobalApp.getQuickConnectHistoryGateway().removeHistoryItem(bookmark.<ManualBookmark>get().getHostname());
							}
							else
							{
								assert false;
								return;
							}
								
							// insert or update bookmark and leave activity
							if(bookmark.getId() > 0)
								bookmarkGateway.update(bookmark);
							else
								bookmarkGateway.insert(bookmark);

							finishAndResetBookmark();
						}
					})
					.setNegativeButton(R.string.no, new DialogInterface.OnClickListener() {					
						@Override
						public void onClick(DialogInterface dialog, int which) {
							finishAndResetBookmark();
						}
					})
					.show();				
			}
			else
			{
				finishAndResetBookmark();				
			}
		}		
	}

}
