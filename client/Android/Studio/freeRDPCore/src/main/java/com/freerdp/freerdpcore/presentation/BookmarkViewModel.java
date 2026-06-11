/*
 * ViewModel for BookmarkActivity — handles asynchronous data loading and saving, keeping database
 * and file I/O off the main thread.
 *
 * Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */

package com.freerdp.freerdpcore.presentation;

import android.app.Application;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.lifecycle.AndroidViewModel;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;

import com.freerdp.freerdpcore.data.AppDatabase;
import com.freerdp.freerdpcore.data.HistoryDatabase;
import com.freerdp.freerdpcore.domain.BookmarkBase;
import com.freerdp.freerdpcore.domain.ConnectionReference;
import com.freerdp.freerdpcore.services.ManualBookmarkGateway;
import com.freerdp.freerdpcore.services.QuickConnectHistoryGateway;
import com.freerdp.freerdpcore.utils.RDPFileHelper;

import java.io.File;
import java.io.IOException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class BookmarkViewModel extends AndroidViewModel
{
	private static final String TAG = "BookmarkViewModel";

	// LiveData for observing state changes from the Activity
	private final MutableLiveData<BookmarkBase> bookmarkLiveData = new MutableLiveData<>();
	private final MutableLiveData<Boolean> saveCompleteEvent = new MutableLiveData<>();

	private boolean settingsChanged = false;
	private boolean newBookmark = false;

	// Single-thread executor handles Room DB and File parsing off the UI thread
	private final ExecutorService executor = Executors.newSingleThreadExecutor();

	private final ManualBookmarkGateway manualBookmarkGateway;
	private final QuickConnectHistoryGateway quickConnectHistoryGateway;

	public BookmarkViewModel(@NonNull Application application)
	{
		super(application);
		manualBookmarkGateway =
		    new ManualBookmarkGateway(AppDatabase.getInstance(application).bookmarkDao());
		quickConnectHistoryGateway =
		    new QuickConnectHistoryGateway(HistoryDatabase.getInstance(application).historyDao());
	}

	public LiveData<BookmarkBase> getBookmarkLiveData()
	{
		return bookmarkLiveData;
	}

	public LiveData<Boolean> getSaveCompleteEvent()
	{
		return saveCompleteEvent;
	}

	public BookmarkBase getBookmark()
	{
		return bookmarkLiveData.getValue();
	}

	public boolean isSettingsChanged()
	{
		return settingsChanged;
	}

	public void setSettingsChanged(boolean changed)
	{
		settingsChanged = changed;
	}

	public boolean isNewBookmark()
	{
		return newBookmark;
	}

	public void setNewBookmark(boolean isNew)
	{
		newBookmark = isNew;
	}

	// -------------------------------------------------------------------------
	// Async Operations
	// -------------------------------------------------------------------------

	public void loadBookmark(Bundle bundle, SharedPreferences prefs)
	{
		if (bookmarkLiveData.getValue() != null)
			return;

		executor.execute(() -> {
			BookmarkBase bookmark = null;
			boolean isNew = false;

			if (bundle != null && bundle.containsKey(BookmarkActivity.PARAM_CONNECTION_REFERENCE))
			{
				String refStr = bundle.getString(BookmarkActivity.PARAM_CONNECTION_REFERENCE);

				if (ConnectionReference.isBookmarkReference(refStr))
				{
					bookmark =
					    manualBookmarkGateway.findById(ConnectionReference.getBookmarkId(refStr));
					isNew = false;
				}
				else if (ConnectionReference.isHostnameReference(refStr))
				{
					bookmark = new BookmarkBase();
					bookmark.setLabel(ConnectionReference.getHostname(refStr));
					bookmark.setHostname(ConnectionReference.getHostname(refStr));
					isNew = true;
				}
				else if (ConnectionReference.isFileReference(refStr))
				{
					String fileRef = ConnectionReference.getFile(refStr);
					bookmark = new BookmarkBase();
					String name = Uri.decode(new File(fileRef).getName());
					String lower = name.toLowerCase(java.util.Locale.ROOT);
					if (lower.endsWith(".rdp"))
						name = name.substring(0, name.length() - 4);
					else if (lower.endsWith(".rdpw"))
						name = name.substring(0, name.length() - 5);
					bookmark.setLabel(name);
					try
					{
						Uri uri = Uri.parse(fileRef);
						RDPFileHelper.importFrom(
						    getApplication().getContentResolver().openInputStream(uri), bookmark);
					}
					catch (IOException e)
					{
						Log.e(TAG, "Failed reading RDP file", e);
					}
					isNew = true;
				}
			}

			if (bookmark == null)
			{
				bookmark = new BookmarkBase();
			}

			this.newBookmark = isNew;
			this.settingsChanged = false;

			// Clear TEMP SharedPreferences and write fresh data BEFORE posting to UI
			prefs.edit().clear().apply();
			bookmark.writeToSharedPreferences(prefs);

			// Notify Activity that loading is complete on the Main Thread
			bookmarkLiveData.postValue(bookmark);
		});
	}

	public void saveBookmark(SharedPreferences prefs)
	{
		BookmarkBase bookmark = getBookmark();
		if (bookmark == null)
			return;

		executor.execute(() -> {
			bookmark.readFromSharedPreferences(prefs);

			if (bookmark.getType() == BookmarkBase.TYPE_MANUAL)
			{
				quickConnectHistoryGateway.removeHistoryItem(bookmark.getHostname());

				if (bookmark.getId() > 0)
				{
					manualBookmarkGateway.update(bookmark);
				}
				else
				{
					manualBookmarkGateway.insert(bookmark);
				}
			}

			// Notify Activity to close
			saveCompleteEvent.postValue(true);
		});
	}

	@Override protected void onCleared()
	{
		super.onCleared();
		executor.shutdown(); // Clean up threads when ViewModel dies
	}
}
