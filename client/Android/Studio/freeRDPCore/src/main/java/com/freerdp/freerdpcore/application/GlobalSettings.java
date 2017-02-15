/*
   Global Settings helper class

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.application;

import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;

public class GlobalSettings {
	
	private static final String PREF_UI_ASKONEXIT = "ui.ask_on_exit";
	private static final String PREF_UI_HIDESTATUSBAR = "ui.hide_status_bar";
	private static final String PREF_UI_INVERTSCROLLING = "ui.invert_scrolling";
	private static final String PREF_UI_SWAPMOUSEBUTTONS = "ui.swap_mouse_buttons";
	private static final String PREF_UI_HIDEZOOMCONTROLS = "ui.hide_zoom_controls";
	private static final String PREF_UI_AUTOSCROLLTOUCHPOINTER = "ui.auto_scroll_touchpointer";

	private static final String PREF_POWER_DISCONNECTTIMEOUT = "power.disconnect_timeout";
	
	private static final String PREF_SECURITY_ACCEPTALLCERTIFICATES = "security.accept_certificates";

	private static SharedPreferences settings;
	
	public static void init(Context context)
	{		
		settings = PreferenceManager.getDefaultSharedPreferences(context);
		initValues();
	}
	
	private static void initValues()
	{
		SharedPreferences.Editor editor = settings.edit();

		if (!settings.contains(PREF_UI_HIDESTATUSBAR))
			editor.putBoolean(PREF_UI_HIDESTATUSBAR, false);
		if (!settings.contains(PREF_UI_HIDEZOOMCONTROLS))
			editor.putBoolean(PREF_UI_HIDEZOOMCONTROLS, true);
		if (!settings.contains(PREF_UI_SWAPMOUSEBUTTONS))
			editor.putBoolean(PREF_UI_SWAPMOUSEBUTTONS, false);
		if (!settings.contains(PREF_UI_INVERTSCROLLING))
			editor.putBoolean(PREF_UI_INVERTSCROLLING, false);
		if (!settings.contains(PREF_UI_ASKONEXIT))
			editor.putBoolean(PREF_UI_ASKONEXIT, true);
		if (!settings.contains(PREF_UI_AUTOSCROLLTOUCHPOINTER))
			editor.putBoolean(PREF_UI_AUTOSCROLLTOUCHPOINTER, true);		
		if (!settings.contains(PREF_POWER_DISCONNECTTIMEOUT))
			editor.putInt(PREF_POWER_DISCONNECTTIMEOUT, 5);
		if (!settings.contains(PREF_SECURITY_ACCEPTALLCERTIFICATES))
			editor.putBoolean(PREF_SECURITY_ACCEPTALLCERTIFICATES, false);
		
		editor.commit();
	}

	public static void setHideStatusBar(boolean hide)
	{
		settings.edit().putBoolean(PREF_UI_HIDESTATUSBAR, hide).commit();
	}
	
	public static boolean getHideStatusBar()
	{
		return settings.getBoolean(PREF_UI_HIDESTATUSBAR, false);
	}	

	public static void setHideZoomControls(boolean hide)
	{
		settings.edit().putBoolean(PREF_UI_HIDEZOOMCONTROLS, hide).commit();
	}
	
	public static boolean getHideZoomControls()
	{
		return settings.getBoolean(PREF_UI_HIDEZOOMCONTROLS, true);
	}	
	
	public static void setSwapMouseButtons(boolean swap)
	{
		settings.edit().putBoolean(PREF_UI_SWAPMOUSEBUTTONS, swap).commit();
	}
	
	public static boolean getSwapMouseButtons()
	{
		return settings.getBoolean(PREF_UI_SWAPMOUSEBUTTONS, false);
	}	

	public static void setInvertScrolling(boolean invert)
	{
		settings.edit().putBoolean(PREF_UI_INVERTSCROLLING, invert).commit();
	}
	
	public static boolean getInvertScrolling()
	{
		return settings.getBoolean(PREF_UI_INVERTSCROLLING, false);
	}	

	public static void setAskOnExit(boolean ask)
	{
		settings.edit().putBoolean(PREF_UI_ASKONEXIT, ask).commit();
	}
	
	public static boolean getAskOnExit()
	{
		return settings.getBoolean(PREF_UI_ASKONEXIT, true);
	}	

	public static void setAutoScrollTouchPointer(boolean scroll)
	{
		settings.edit().putBoolean(PREF_UI_AUTOSCROLLTOUCHPOINTER, scroll).commit();
	}
	
	public static boolean getAutoScrollTouchPointer()
	{
		return settings.getBoolean(PREF_UI_AUTOSCROLLTOUCHPOINTER, true);
	}		
	
	public static void setAcceptAllCertificates(boolean accept)
	{
		settings.edit().putBoolean(PREF_SECURITY_ACCEPTALLCERTIFICATES, accept).commit();
	}
	
	public static boolean getAcceptAllCertificates()
	{
		return settings.getBoolean(PREF_SECURITY_ACCEPTALLCERTIFICATES, false);
	}
	
	public static void setDisconnectTimeout(int timeoutMinutes)
	{
		settings.edit().putInt(PREF_POWER_DISCONNECTTIMEOUT, timeoutMinutes).commit();
	}
	
	public static int getDisconnectTimeout()
	{
		return settings.getInt(PREF_POWER_DISCONNECTTIMEOUT, 5);
	}
	
}
