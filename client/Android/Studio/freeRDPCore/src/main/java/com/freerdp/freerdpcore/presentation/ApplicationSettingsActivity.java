/*
   Application Settings Activity

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.presentation;

import java.io.File;

import com.freerdp.freerdpcore.R;

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceClickListener;
import android.preference.PreferenceActivity;
import android.util.Log;
import android.widget.Toast;

public class ApplicationSettingsActivity extends PreferenceActivity implements OnSharedPreferenceChangeListener {

	private Preference prefEraseAllCertificates;	

	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		Log.v("SettingsActivity", "onCreate");
		super.onCreate(savedInstanceState);
		addPreferencesFromResource(R.xml.application_settings);

		prefEraseAllCertificates = (Preference) findPreference("security.clear_certificate_cache");
		
		// erase certificate cache button
		prefEraseAllCertificates.setOnPreferenceClickListener(new OnPreferenceClickListener() {
			@Override
			public boolean onPreferenceClick(Preference preference) {
				AlertDialog.Builder builder = new AlertDialog.Builder(preference.getContext());
				builder.setTitle(R.string.dlg_title_clear_cert_cache)
				.setMessage(R.string.dlg_msg_clear_cert_cache)
				.setPositiveButton(R.string.yes, new DialogInterface.OnClickListener() {
				    public void onClick(DialogInterface dialog, int which) 
				    {
						clearCertificateCache();
				    }
				})
				.setNegativeButton(R.string.no, new DialogInterface.OnClickListener() {
				    public void onClick(DialogInterface dialog, int which) 
				    {
				    	dialog.dismiss();
				    }
				})
				.create()
				.show();							
				return true;
			}			
		});

		// register for preferences changed notification
		getPreferenceManager().getSharedPreferences().registerOnSharedPreferenceChangeListener(this);
		onSharedPreferenceChanged(getPreferenceManager().getSharedPreferences(), "power.disconnect_timeout");
	}
	
	@Override
	public void onSharedPreferenceChanged(SharedPreferences sharedPreferences, String key) {	
		if (key.equals("power.disconnect_timeout"))
		{
			int val = sharedPreferences.getInt(key, 5);
			Preference pref = findPreference(key);
			if (pref != null)
			{
				if (val == 0)
					pref.setSummary(getResources().getString(R.string.settings_description_disabled));
				else
					pref.setSummary(String.format(getResources().getString(R.string.settings_description_after_minutes), val));					
			}
		}
	}
		
	private boolean deleteDirectory(File dir)
	{
		if (dir.isDirectory()) 
		{
	        String[] children = dir.list();
	        for(String file : children) 
	        {
	            if(!deleteDirectory(new File(dir, file)))
	            		return false;
	        }
	    }	
		return dir.delete();
	}

	private void clearCertificateCache()
	{
		if((new File(getFilesDir() +  "/.freerdp")).exists())
		{
			if(deleteDirectory(new File(getFilesDir() +  "/.freerdp")))
				Toast.makeText(this, R.string.info_reset_success, Toast.LENGTH_LONG).show();
			else
				Toast.makeText(this, R.string.info_reset_failed, Toast.LENGTH_LONG).show();
		}
		else
			Toast.makeText(this, R.string.info_reset_success, Toast.LENGTH_LONG).show();		
	}	
}
