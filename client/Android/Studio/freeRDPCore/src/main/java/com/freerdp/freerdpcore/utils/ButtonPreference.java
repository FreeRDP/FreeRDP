/*
   Custom preference item showing a button on the right side

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.utils;

import com.freerdp.freerdpcore.R;

import android.content.Context;
import android.preference.Preference;
import android.util.AttributeSet;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;

public class ButtonPreference extends Preference {
	
	private OnClickListener buttonOnClickListener;
	private String buttonText;
	private Button button;
	
	public ButtonPreference(Context context) {
		super(context);
		init();
	}

	public ButtonPreference(Context context, AttributeSet attrs) {
		super(context, attrs);
		init();
	}

	public ButtonPreference(Context context, AttributeSet attrs, int defStyle) {
		super(context, attrs, defStyle);
		init();
	}
	
	private void init()
	{
		setLayoutResource(R.layout.button_preference);
		button = null;
		buttonText = null;
		buttonOnClickListener = null;
	}
	
	@Override
	public View getView(View convertView, ViewGroup parent) 
	{
		View v = super.getView(convertView, parent);
		button = (Button)v.findViewById(R.id.preference_button);		
		if (buttonText != null)
			button.setText(buttonText);
		if (buttonOnClickListener != null)
			button.setOnClickListener(buttonOnClickListener);
		
		// additional init for ICS - make widget frame visible
		// refer to http://stackoverflow.com/questions/8762984/custom-preference-broken-in-honeycomb-ics
		LinearLayout widgetFrameView = ((LinearLayout)v.findViewById(android.R.id.widget_frame));
		widgetFrameView.setVisibility(View.VISIBLE);

		return v;
	}

	public void setButtonText(int resId)
	{
		buttonText = getContext().getResources().getString(resId); 
		if (button != null)
			button.setText(buttonText);
	}

	public void setButtonText(String text)
	{
		buttonText = text; 
		if (button != null)
			button.setText(text);
	}
	
	public void setButtonOnClickListener(OnClickListener listener)
	{
		if (button != null)
			button.setOnClickListener(listener);
		else
			buttonOnClickListener = listener; 
	}
}
