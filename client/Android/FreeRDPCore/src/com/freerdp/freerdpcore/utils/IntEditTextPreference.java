/*
   EditTextPreference to store/load integer values

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.utils;

import com.freerdp.freerdpcore.R;

import android.content.Context;
import android.content.res.TypedArray;
import android.preference.EditTextPreference;
import android.util.AttributeSet;

public class IntEditTextPreference extends EditTextPreference {

	private int bounds_min, bounds_max, bounds_default;
	
	public IntEditTextPreference(Context context) 
	{
		super(context);
		init(context, null);
	}

	public IntEditTextPreference(Context context, AttributeSet attrs) 
	{
		super(context, attrs);
		init(context, attrs);
	}

	public IntEditTextPreference(Context context, AttributeSet attrs, int defStyle) 
	{
		super(context, attrs, defStyle);
		init(context, attrs);
	}

	private void init(Context context, AttributeSet attrs)
	{
		if (attrs != null)
		{
	        TypedArray array = context.obtainStyledAttributes(attrs, R.styleable.IntEditTextPreference, 0, 0);
	        bounds_min = array.getInt(R.styleable.IntEditTextPreference_bounds_min, Integer.MIN_VALUE);
	        bounds_max = array.getInt(R.styleable.IntEditTextPreference_bounds_max, Integer.MAX_VALUE);
	        bounds_default = array.getInt(R.styleable.IntEditTextPreference_bounds_default, 0);
	        array.recycle(); 			
		}
		else
		{
			bounds_min = Integer.MIN_VALUE;
			bounds_max = Integer.MAX_VALUE;
			bounds_default = 0;		
		}        
	}

	public void setBounds(int min, int max, int defaultValue)
	{
		bounds_min = min;
		bounds_max = max;
		bounds_default = defaultValue;
	}
	
	@Override
	protected String getPersistedString(String defaultReturnValue) {
		int value = getPersistedInt(-1);
		if (value > bounds_max || value < bounds_min)
			value = bounds_default;		
		return String.valueOf(value);
	}
	
	@Override
	protected boolean persistString(String value) {
		return persistInt(Integer.valueOf(value));
	}
	
	@Override
	protected void onDialogClosed(boolean positiveResult) 
	{
		if (positiveResult)
		{
			// prevent exception when an empty value is persisted
			if (getEditText().getText().length() == 0)
				getEditText().setText("0");
			
			// check bounds
			int value = Integer.valueOf(getEditText().getText().toString());
			if (value > bounds_max || value < bounds_min)
				value = bounds_default;			
			getEditText().setText(String.valueOf(value));			
		}
		
		super.onDialogClosed(positiveResult);
	}
}
