/*
   Android Main Application

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.application;

import android.app.Application;
import androidx.appcompat.app.AppCompatDelegate;
import androidx.core.content.ContextCompat;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import com.freerdp.freerdpcore.domain.BookmarkBase;
import com.freerdp.freerdpcore.presentation.ApplicationSettingsActivity;
import com.freerdp.freerdpcore.presentation.PrintJobMonitor;
import com.freerdp.freerdpcore.presentation.PrintNotificationHelper;
import com.freerdp.freerdpcore.services.LibFreeRDP;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.Timer;
import java.util.TimerTask;
import java.util.concurrent.ConcurrentHashMap;
import java.util.function.Consumer;

public class GlobalApp extends Application implements LibFreeRDP.EventListener
{
	private static final String TAG = "GlobalApp";
	private static Map<Long, SessionState> sessionMap;
	private PrintJobMonitor printJobMonitor;

	/** Per-session connection event listener. Callbacks are invoked on the main thread. */
	public interface SessionEventListener
	{
		void onConnectionSuccess();
		void onConnectionFailure();
		void onDisconnected();
	}

	private static final Map<Long, SessionEventListener> sessionListeners =
	    new ConcurrentHashMap<>();
	private static final Handler mainHandler = new Handler(Looper.getMainLooper());

	public static void registerSessionListener(long instance, SessionEventListener listener)
	{
		sessionListeners.put(instance, listener);
	}

	public static void unregisterSessionListener(long instance)
	{
		sessionListeners.remove(instance);
	}

	private void dispatch(long instance, Consumer<SessionEventListener> action)
	{
		final SessionEventListener listener = sessionListeners.get(instance);
		if (listener == null)
			return;
		mainHandler.post(() -> action.accept(listener));
	}

	// timer for disconnecting sessions after the screen was turned off
	private static Timer disconnectTimer = null;

	// Disconnect handling for Screen on/off events
	public void startDisconnectTimer()
	{
		final int timeoutMinutes = ApplicationSettingsActivity.getDisconnectTimeout(this);
		if (timeoutMinutes > 0)
		{
			// start disconnect timeout...
			disconnectTimer = new Timer();
			disconnectTimer.schedule(new DisconnectTask(), (long)timeoutMinutes * 60 * 1000);
		}
	}

	static public void cancelDisconnectTimer()
	{
		// cancel any pending timer events
		if (disconnectTimer != null)
		{
			disconnectTimer.cancel();
			disconnectTimer.purge();
			disconnectTimer = null;
		}
	}

	// RDP session handling
	static public SessionState createSession(BookmarkBase bookmark, Context context)
	{
		SessionState session = new SessionState(LibFreeRDP.newInstance(context), bookmark);
		sessionMap.put(session.getInstance(), session);
		return session;
	}

	static public SessionState createSession(Uri openUri, Context context)
	{
		SessionState session = new SessionState(LibFreeRDP.newInstance(context), openUri);
		sessionMap.put(session.getInstance(), session);
		return session;
	}

	static public SessionState getSession(long instance)
	{
		return sessionMap.get(instance);
	}

	static public Collection<SessionState> getSessions()
	{
		// return a copy of the session items
		return new ArrayList<>(sessionMap.values());
	}

	static public void freeSession(long instance)
	{
		if (GlobalApp.sessionMap.containsKey(instance))
		{
			GlobalApp.sessionMap.remove(instance);
			LibFreeRDP.freeInstance(instance);
		}
	}

	@Override public void onCreate()
	{
		super.onCreate();

		/* Initialize preferences. */
		ApplicationSettingsActivity.get(this);
		AppCompatDelegate.setDefaultNightMode(ApplicationSettingsActivity.getNightMode(this));

		sessionMap = Collections.synchronizedMap(new HashMap<Long, SessionState>());

		LibFreeRDP.setEventListener(this);

		printJobMonitor = new PrintJobMonitor(file -> PrintNotificationHelper.notify(this, file));
		printJobMonitor.startWatching();

		// init screen receiver here (this can't be declared in AndroidManifest - refer to:
		// http://thinkandroid.wordpress.com/2010/01/24/handling-screen-off-and-screen-on-intents/
		IntentFilter filter = new IntentFilter(Intent.ACTION_SCREEN_ON);
		filter.addAction(Intent.ACTION_SCREEN_OFF);
		ContextCompat.registerReceiver(this, new ScreenReceiver(), filter,
		                               ContextCompat.RECEIVER_EXPORTED);
	}

	// helper to send FreeRDP notifications
	@Override public void OnPreConnect(long instance)
	{
		Log.v(TAG, "OnPreConnect");
	}

	// //////////////////////////////////////////////////////////////////////
	// Implementation of LibFreeRDP.EventListener
	public void OnConnectionSuccess(long instance)
	{
		Log.v(TAG, "OnConnectionSuccess");
		dispatch(instance, SessionEventListener::onConnectionSuccess);
	}

	public void OnConnectionFailure(long instance)
	{
		Log.v(TAG, "OnConnectionFailure");
		dispatch(instance, SessionEventListener::onConnectionFailure);
	}

	public void OnDisconnecting(long instance)
	{
		Log.v(TAG, "OnDisconnecting");
	}

	public void OnDisconnected(long instance)
	{
		Log.v(TAG, "OnDisconnected");
		dispatch(instance, SessionEventListener::onDisconnected);
	}

	// TimerTask for disconnecting sessions after screen was turned off
	private static class DisconnectTask extends TimerTask
	{
		@Override public void run()
		{
			Log.v("DisconnectTask", "Doing action");

			// disconnect any running rdp session
			Collection<SessionState> sessions = GlobalApp.getSessions();
			for (SessionState session : sessions)
			{
				LibFreeRDP.disconnect(session.getInstance());
			}
		}
	}
}
