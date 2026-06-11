/*
   SessionViewModel — exposes RDP connection state as LiveData

   Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.presentation;

import android.app.Application;
import android.os.Handler;
import android.os.Looper;

import androidx.annotation.NonNull;
import androidx.lifecycle.AndroidViewModel;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;

import com.freerdp.freerdpcore.application.GlobalApp;
import com.freerdp.freerdpcore.data.AppDatabase;
import com.freerdp.freerdpcore.data.HistoryDatabase;
import com.freerdp.freerdpcore.domain.BookmarkBase;
import com.freerdp.freerdpcore.services.ManualBookmarkGateway;
import com.freerdp.freerdpcore.services.QuickConnectHistoryGateway;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.function.Consumer;

public class SessionViewModel extends AndroidViewModel
{
	public enum ConnectionState
	{
		IDLE,
		CONNECTING,
		CONNECTED,
		FAILED,
		DISCONNECTED
	}

	private final MutableLiveData<ConnectionState> state =
	    new MutableLiveData<>(ConnectionState.IDLE);
	private long registeredInstance = 0;

	private final ExecutorService executor = Executors.newSingleThreadExecutor();
	private final Handler mainHandler = new Handler(Looper.getMainLooper());
	private final ManualBookmarkGateway manualBookmarkGateway;
	private final QuickConnectHistoryGateway quickConnectHistoryGateway;

	public SessionViewModel(@NonNull Application application)
	{
		super(application);
		manualBookmarkGateway =
		    new ManualBookmarkGateway(AppDatabase.getInstance(application).bookmarkDao());
		quickConnectHistoryGateway =
		    new QuickConnectHistoryGateway(HistoryDatabase.getInstance(application).historyDao());
	}

	public LiveData<ConnectionState> getState()
	{
		return state;
	}

	public void register(long instance)
	{
		unregister();
		registeredInstance = instance;
		state.setValue(ConnectionState.CONNECTING);
		GlobalApp.registerSessionListener(instance, new GlobalApp.SessionEventListener() {
			@Override public void onConnectionSuccess()
			{
				state.setValue(ConnectionState.CONNECTED);
			}

			@Override public void onConnectionFailure()
			{
				state.setValue(ConnectionState.FAILED);
			}

			@Override public void onDisconnected()
			{
				state.setValue(ConnectionState.DISCONNECTED);
			}
		});
	}

	public void unregister()
	{
		if (registeredInstance != 0)
		{
			GlobalApp.unregisterSessionListener(registeredInstance);
			registeredInstance = 0;
		}
	}

	/**
	 * Loads a bookmark by id off the UI thread and delivers the result on the main thread.
	 */
	public void loadBookmarkById(long bookmarkId, @NonNull Consumer<BookmarkBase> callback)
	{
		executor.execute(() -> {
			final BookmarkBase bookmark = manualBookmarkGateway.findById(bookmarkId);
			mainHandler.post(() -> callback.accept(bookmark));
		});
	}

	/**
	 * Inserts the given hostname into quick-connect history off the UI thread (no-op on duplicate).
	 */
	public void recordQuickConnectHistory(@NonNull String hostname)
	{
		executor.execute(() -> {
			if (!quickConnectHistoryGateway.historyItemExists(hostname))
				quickConnectHistoryGateway.addHistoryItem(hostname);
		});
	}

	@Override protected void onCleared()
	{
		unregister();
		executor.shutdown();
		super.onCleared();
	}
}
