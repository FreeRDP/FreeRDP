/*
   Application Settings Activity

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.presentation;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.os.Build;
import android.os.Bundle;
import android.preference.EditTextPreference;
import android.preference.Preference;
import android.preference.PreferenceFragment;
import android.preference.PreferenceManager;
import android.preference.PreferenceScreen;
import android.support.v7.app.AlertDialog;
import android.widget.Toast;

import com.freerdp.freerdpcore.R;
import com.freerdp.freerdpcore.utils.AppCompatPreferenceActivity;

import java.io.File;
import java.util.List;
import java.util.UUID;

public class ApplicationSettingsActivity extends AppCompatPreferenceActivity {
    private static boolean isXLargeTablet(Context context) {
        return (context.getResources().getConfiguration().screenLayout
                & Configuration.SCREENLAYOUT_SIZE_MASK) >= Configuration.SCREENLAYOUT_SIZE_XLARGE;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setupActionBar();
    }

    private void setupActionBar() {
        android.app.ActionBar actionBar = getActionBar();
        if (actionBar != null) {
            actionBar.setDisplayHomeAsUpEnabled(true);
        }
    }

    @Override
    public boolean onIsMultiPane() {
        return isXLargeTablet(this);
    }

    @Override
    @TargetApi(Build.VERSION_CODES.HONEYCOMB)
    public void onBuildHeaders(List<Header> target) {
        loadHeadersFromResource(R.xml.settings_app_headers, target);
    }

    protected boolean isValidFragment(String fragmentName) {
        return PreferenceFragment.class.getName().equals(fragmentName)
                || ClientPreferenceFragment.class.getName().equals(fragmentName)
                || UiPreferenceFragment.class.getName().equals(fragmentName)
                || PowerPreferenceFragment.class.getName().equals(fragmentName)
                || SecurityPreferenceFragment.class.getName().equals(fragmentName);
    }

    @TargetApi(Build.VERSION_CODES.HONEYCOMB)
    public static class ClientPreferenceFragment extends PreferenceFragment implements SharedPreferences.OnSharedPreferenceChangeListener {
        @Override
        public void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            addPreferencesFromResource(R.xml.settings_app_client);
            SharedPreferences preferences = get(getActivity());
            preferences.registerOnSharedPreferenceChangeListener(this);
        }

        @Override
        public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {
            if (isAdded()) {
                final String clientNameKey = getString(R.string.preference_key_client_name);

                get(getActivity());
                if (key.equals(clientNameKey)) {
                    final String clientNameValue = sharedPreferences.getString(clientNameKey, "");
                    EditTextPreference pref = (EditTextPreference) findPreference(clientNameKey);
                    pref.setText(clientNameValue);
                }
            }
        }
    }

    @TargetApi(Build.VERSION_CODES.HONEYCOMB)
    public static class UiPreferenceFragment extends PreferenceFragment {
        @Override
        public void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            addPreferencesFromResource(R.xml.settings_app_ui);
        }
    }

    @TargetApi(Build.VERSION_CODES.HONEYCOMB)
    public static class PowerPreferenceFragment extends PreferenceFragment {
        @Override
        public void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            addPreferencesFromResource(R.xml.settings_app_power);
        }
    }

    @TargetApi(Build.VERSION_CODES.HONEYCOMB)
    public static class SecurityPreferenceFragment extends PreferenceFragment {
        @Override
        public void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            addPreferencesFromResource(R.xml.settings_app_security);
        }

        @Override
        public boolean onPreferenceTreeClick(PreferenceScreen preferenceScreen, Preference preference) {
            final String clear = getString(R.string.preference_key_security_clear_certificate_cache);
            if (preference.getKey().equals(clear)) {
                showDialog();
                return true;
            } else {
                return super.onPreferenceTreeClick(preferenceScreen, preference);
            }
        }

        private void showDialog() {
            new AlertDialog.Builder(getActivity())
                    .setTitle(R.string.dlg_title_clear_cert_cache)
                    .setMessage(R.string.dlg_msg_clear_cert_cache)
                    .setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {
                        @Override
                        public void onClick(DialogInterface dialog, int which) {
                            clearCertificateCache();
                            dialog.dismiss();
                        }
                    })
                    .setNegativeButton(android.R.string.cancel, new DialogInterface.OnClickListener() {
                        @Override
                        public void onClick(DialogInterface dialog, int which) {
                            dialog.dismiss();
                        }
                    })
                    .setIcon(android.R.drawable.ic_delete)
                    .show();
        }

        private boolean deleteDirectory(File dir) {
            if (dir.isDirectory()) {
                String[] children = dir.list();
                for (String file : children) {
                    if (!deleteDirectory(new File(dir, file)))
                        return false;
                }
            }
            return dir.delete();
        }

        private void clearCertificateCache() {
            Context context = getActivity();
            if ((new File(context.getFilesDir() + "/.freerdp")).exists()) {
                if (deleteDirectory(new File(context.getFilesDir() + "/.freerdp")))
                    Toast.makeText(context, R.string.info_reset_success, Toast.LENGTH_LONG).show();
                else
                    Toast.makeText(context, R.string.info_reset_failed, Toast.LENGTH_LONG).show();
            } else
                Toast.makeText(context, R.string.info_reset_success, Toast.LENGTH_LONG).show();
        }
    }

    public static SharedPreferences get(Context context) {
        Context appContext = context.getApplicationContext();
        PreferenceManager.setDefaultValues(appContext, R.xml.settings_app_client, false);
        PreferenceManager.setDefaultValues(appContext, R.xml.settings_app_power, false);
        PreferenceManager.setDefaultValues(appContext, R.xml.settings_app_security, false);
        PreferenceManager.setDefaultValues(appContext, R.xml.settings_app_ui, false);
        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(appContext);

        final String key = context.getString(R.string.preference_key_client_name);
        final String value = preferences.getString(key, "");
        if (value.isEmpty()) {
            final String android_id = UUID.randomUUID().toString();
            final String defaultValue = context.getString(R.string.preference_default_client_name);
            final String name = defaultValue + "-" + android_id;
            preferences.edit().putString(key, name.substring(0, 31)).apply();
        }

        return preferences;
    }

    public static int getDisconnectTimeout(Context context) {
        SharedPreferences preferences = get(context);
        return preferences.getInt(context.getString(R.string.preference_key_power_disconnect_timeout), 0);
    }

    public static boolean getHideStatusBar(Context context) {
        SharedPreferences preferences = get(context);
        return preferences.getBoolean(context.getString(R.string.preference_key_ui_hide_status_bar), false);
    }

    public static boolean getHideActionBar(Context context) {
        SharedPreferences preferences = get(context);
        return preferences.getBoolean(context.getString(R.string.preference_key_ui_hide_action_bar), false);
    }

    public static boolean getAcceptAllCertificates(Context context) {
        SharedPreferences preferences = get(context);
        return preferences.getBoolean(context.getString(R.string.preference_key_accept_certificates), false);
    }

    public static boolean getHideZoomControls(Context context) {
        SharedPreferences preferences = get(context);
        return preferences.getBoolean(context.getString(R.string.preference_key_ui_hide_zoom_controls), false);
    }

    public static boolean getSwapMouseButtons(Context context) {
        SharedPreferences preferences = get(context);
        return preferences.getBoolean(context.getString(R.string.preference_key_ui_swap_mouse_buttons), false);
    }

    public static boolean getInvertScrolling(Context context) {
        SharedPreferences preferences = get(context);
        return preferences.getBoolean(context.getString(R.string.preference_key_ui_invert_scrolling), false);
    }

    public static boolean getAskOnExit(Context context) {
        SharedPreferences preferences = get(context);
        return preferences.getBoolean(context.getString(R.string.preference_key_ui_ask_on_exit), false);
    }

    public static boolean getAutoScrollTouchPointer(Context context) {
        SharedPreferences preferences = get(context);
        return preferences.getBoolean(context.getString(R.string.preference_key_ui_auto_scroll_touchpointer), false);
    }

    public static String getClientName(Context context) {
        SharedPreferences preferences = get(context);
        return preferences.getString(context.getString(R.string.preference_key_client_name), "");
    }
}
