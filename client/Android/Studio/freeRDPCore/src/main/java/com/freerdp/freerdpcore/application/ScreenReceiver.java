/*
   Helper class to receive notifications when the screen is turned on/off

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.application;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

public class ScreenReceiver extends BroadcastReceiver
{

	@Override public void onReceive(Context context, Intent intent)
	{
		GlobalApp app = (GlobalApp)context.getApplicationContext();
		Log.v("ScreenReceiver", "Received action: " + intent.getAction());
		if (intent.getAction().equals(Intent.ACTION_SCREEN_OFF))
			app.startDisconnectTimer();
		else if (intent.getAction().equals(Intent.ACTION_SCREEN_ON))
			app.cancelDisconnectTimer();
	}
}
