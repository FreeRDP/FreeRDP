/*
   ListPreference to store/load integer values

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.utils;

import android.content.Context;
import android.preference.ListPreference;
import android.util.AttributeSet;

public class IntListPreference extends ListPreference {

	public IntListPreference(Context context) 
	{
		super(context);
	}

	public IntListPreference(Context context, AttributeSet attrs) 
	{
		super(context, attrs);
	}

	@Override
	protected String getPersistedString(String defaultReturnValue) {
		return String.valueOf(getPersistedInt(-1));
	}
	
	@Override
	protected boolean persistString(String value) {
		return persistInt(Integer.valueOf(value));
	}		
}
