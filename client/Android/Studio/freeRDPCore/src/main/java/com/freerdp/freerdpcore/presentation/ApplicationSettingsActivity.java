/*
   Application Settings Activity

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.presentation;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;
import androidx.appcompat.app.AppCompatDelegate;
import androidx.preference.ListPreference;
import androidx.preference.PreferenceManager;
import android.widget.Toast;

import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import com.freerdp.freerdpcore.R;

import java.io.File;
import java.util.UUID;

public class ApplicationSettingsActivity
    extends AppCompatActivity implements PreferenceFragmentCompat.OnPreferenceStartFragmentCallback
{
	@Override protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_settings);

		// Ensure app setting defaults are initialised and the client name is set.
		get(this);

		if (getSupportActionBar() != null)
		{
			getSupportActionBar().setDisplayHomeAsUpEnabled(true);
		}

		if (savedInstanceState == null)
		{
			getSupportFragmentManager()
			    .beginTransaction()
			    .replace(R.id.settings_fragment_container, new MainFragment())
			    .commit();
		}
	}

	@Override public boolean onSupportNavigateUp()
	{
		if (getSupportFragmentManager().getBackStackEntryCount() > 0)
		{
			getSupportFragmentManager().popBackStack();
		}
		else
		{
			finish();
		}
		return true;
	}

	@Override
	public boolean onPreferenceStartFragment(PreferenceFragmentCompat caller, Preference pref)
	{
		PreferenceFragmentCompat fragment =
		    (PreferenceFragmentCompat)getSupportFragmentManager().getFragmentFactory().instantiate(
		        getClassLoader(), pref.getFragment());
		fragment.setArguments(pref.getExtras());

		getSupportFragmentManager()
		    .beginTransaction()
		    .replace(R.id.settings_fragment_container, fragment)
		    .addToBackStack(null)
		    .commit();
		return true;
	}

	// =========================================================================
	// Inner PreferenceFragmentCompat classes — one per settings category.
	// The app:fragment attribute in settings_app_headers.xml references these
	// using the $ notation, e.g. "...ApplicationSettingsActivity$ClientFragment".
	// =========================================================================

	/** Root screen listing all settings categories. */
	public static class MainFragment extends PreferenceFragmentCompat
	{
		@Override public void onCreatePreferences(Bundle savedInstanceState, String rootKey)
		{
			setPreferencesFromResource(R.xml.settings_app_headers, rootKey);
		}
	}

	// -------------------------------------------------------------------------

	public static class ClientFragment extends PreferenceFragmentCompat
	{
		@Override public void onCreatePreferences(Bundle savedInstanceState, String rootKey)
		{
			setPreferencesFromResource(R.xml.settings_app_client, rootKey);
		}
	}

	// -------------------------------------------------------------------------

	public static class UiFragment extends PreferenceFragmentCompat
	{
		@Override public void onCreatePreferences(Bundle savedInstanceState, String rootKey)
		{
			setPreferencesFromResource(R.xml.settings_app_ui, rootKey);
			ListPreference theme = findPreference(getString(R.string.pref_key_theme));
			if (theme != null)
				theme.setOnPreferenceChangeListener((p, v) -> {
					AppCompatDelegate.setDefaultNightMode(nightModeFor((String)v));
					requireActivity().recreate();
					return true;
				});
		}
	}

	// -------------------------------------------------------------------------

	public static class PowerFragment extends PreferenceFragmentCompat
	{
		@Override public void onCreatePreferences(Bundle savedInstanceState, String rootKey)
		{
			setPreferencesFromResource(R.xml.settings_app_power, rootKey);
		}
	}

	// -------------------------------------------------------------------------

	public static class SecurityFragment extends PreferenceFragmentCompat
	{
		@Override public void onCreatePreferences(Bundle savedInstanceState, String rootKey)
		{
			setPreferencesFromResource(R.xml.settings_app_security, rootKey);

			Preference clearCache =
			    findPreference(getString(R.string.preference_key_security_clear_certificate_cache));
			if (clearCache != null)
				clearCache.setOnPreferenceClickListener(pref -> {
					showClearCacheDialog();
					return true;
				});
		}

		private void showClearCacheDialog()
		{
			new AlertDialog
			    .Builder(requireContext()) // TODO: DialogFragment with ViewModel to survive config
			                               // changes
			    .setTitle(R.string.dlg_title_clear_cert_cache)
			    .setMessage(R.string.dlg_msg_clear_cert_cache)
			    .setPositiveButton(android.R.string.ok,
			                       (d, w) -> {
				                       clearCertificateCache();
				                       d.dismiss();
			                       })
			    .setNegativeButton(android.R.string.cancel, (d, w) -> d.dismiss())
			    .setIcon(android.R.drawable.ic_delete)
			    .show();
		}

		private boolean deleteDirectory(File dir)
		{
			if (dir.isDirectory())
			{
				String[] children = dir.list();
				for (String file : children)
				{
					if (!deleteDirectory(new File(dir, file)))
						return false;
				}
			}
			return dir.delete();
		}

		private void clearCertificateCache()
		{
			Context context = requireContext();
			File dir = new File(context.getFilesDir() + "/.freerdp");
			if (dir.exists())
			{
				if (deleteDirectory(dir))
					Toast.makeText(context, R.string.info_reset_success, Toast.LENGTH_LONG).show();
				else
					Toast.makeText(context, R.string.info_reset_failed, Toast.LENGTH_LONG).show();
			}
			else
			{
				Toast.makeText(context, R.string.info_reset_success, Toast.LENGTH_LONG).show();
			}
		}
	}

	public static class ExperimentalFragment extends PreferenceFragmentCompat
	{
		@Override public void onCreatePreferences(Bundle savedInstanceState, String rootKey)
		{
			setPreferencesFromResource(R.xml.settings_app_experimental, rootKey);
		}
	}

	// Preference key convention: "experimental.<feature>".
	public static boolean isExperimentalEnabled(Context context, String feature)
	{
		return get(context).getBoolean("experimental." + feature, false);
	}

	public static SharedPreferences get(Context context)
	{
		Context appContext = context.getApplicationContext();
		PreferenceManager.setDefaultValues(appContext, R.xml.settings_app_client, false);
		PreferenceManager.setDefaultValues(appContext, R.xml.settings_app_power, false);
		PreferenceManager.setDefaultValues(appContext, R.xml.settings_app_security, false);
		PreferenceManager.setDefaultValues(appContext, R.xml.settings_app_ui, false);
		PreferenceManager.setDefaultValues(appContext, R.xml.settings_app_experimental, false);
		SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(appContext);

		final String key = context.getString(R.string.preference_key_client_name);
		final String value = preferences.getString(key, "");
		if (value.isEmpty())
		{
			final String android_id = UUID.randomUUID().toString();
			final String defaultValue = context.getString(R.string.preference_default_client_name);
			final String name = defaultValue + "-" + android_id;
			preferences.edit().putString(key, name.substring(0, 31)).apply();
		}

		return preferences;
	}

	public static int getDisconnectTimeout(Context context)
	{
		SharedPreferences preferences = get(context);
		return preferences.getInt(
		    context.getString(R.string.preference_key_power_disconnect_timeout), 0);
	}

	public static boolean getKeepScreenOnWhenConnected(Context context)
	{
		SharedPreferences preferences = get(context);
		return preferences.getBoolean(
		    context.getString(R.string.preference_key_power_keep_screen_on_when_connected), false);
	}

	public static boolean getHideStatusBar(Context context)
	{
		SharedPreferences preferences = get(context);
		return preferences.getBoolean(context.getString(R.string.preference_key_ui_hide_status_bar),
		                              false);
	}

	public static boolean getHideNavigationBar(Context context)
	{
		SharedPreferences preferences = get(context);
		return preferences.getBoolean(
		    context.getString(R.string.preference_key_ui_hide_navigation_bar), false);
	}

	public static boolean getFitRoundedCorners(Context context)
	{
		SharedPreferences preferences = get(context);
		return preferences.getBoolean(
		    context.getString(R.string.preference_key_ui_fit_rounded_corners), false);
	}

	public static boolean getUseBackAsAltf4(Context context)
	{
		SharedPreferences preferences = get(context);
		return preferences.getBoolean(
		    context.getString(R.string.preference_key_ui_use_back_as_altf4), false);
	}

	public static boolean getAcceptAllCertificates(Context context)
	{
		SharedPreferences preferences = get(context);
		return preferences.getBoolean(
		    context.getString(R.string.preference_key_accept_certificates), false);
	}

	public static boolean getSwapMouseButtons(Context context)
	{
		SharedPreferences preferences = get(context);
		return preferences.getBoolean(
		    context.getString(R.string.preference_key_ui_swap_mouse_buttons), false);
	}

	public static boolean getInvertScrolling(Context context)
	{
		SharedPreferences preferences = get(context);
		return preferences.getBoolean(
		    context.getString(R.string.preference_key_ui_invert_scrolling), false);
	}

	public static boolean getAskOnExit(Context context)
	{
		SharedPreferences preferences = get(context);
		return preferences.getBoolean(context.getString(R.string.preference_key_ui_ask_on_exit),
		                              true);
	}

	public static boolean getAutoScrollTouchPointer(Context context)
	{
		SharedPreferences preferences = get(context);
		return preferences.getBoolean(
		    context.getString(R.string.preference_key_ui_auto_scroll_touchpointer), false);
	}

	public static String getClientName(Context context)
	{
		SharedPreferences preferences = get(context);
		return preferences.getString(context.getString(R.string.preference_key_client_name), "");
	}

	public static int getNightMode(Context context)
	{
		return nightModeFor(
		    get(context).getString(context.getString(R.string.pref_key_theme), "auto"));
	}

	private static int nightModeFor(String value)
	{
		switch (value)
		{
			case "light":
				return AppCompatDelegate.MODE_NIGHT_NO;
			case "dark":
				return AppCompatDelegate.MODE_NIGHT_YES;
			default:
				return AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM;
		}
	}
}
