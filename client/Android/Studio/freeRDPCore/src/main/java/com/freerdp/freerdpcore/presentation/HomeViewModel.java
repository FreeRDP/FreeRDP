/*
   HomeViewModel for HomeActivity

   Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.presentation;

import android.app.Application;

import androidx.annotation.NonNull;
import androidx.lifecycle.AndroidViewModel;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;

import com.freerdp.freerdpcore.data.AppDatabase;
import com.freerdp.freerdpcore.data.HistoryDatabase;
import com.freerdp.freerdpcore.domain.BookmarkBase;
import com.freerdp.freerdpcore.services.ManualBookmarkGateway;
import com.freerdp.freerdpcore.services.QuickConnectHistoryGateway;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class HomeViewModel extends AndroidViewModel
{
	private final MutableLiveData<List<BookmarkBase>> bookmarks =
	    new MutableLiveData<>(Collections.emptyList());
	private String currentQuery = "";
	private final ExecutorService executor = Executors.newSingleThreadExecutor();

	private final ManualBookmarkGateway manualBookmarkGateway;
	private final QuickConnectHistoryGateway quickConnectHistoryGateway;

	public HomeViewModel(@NonNull Application application)
	{
		super(application);
		manualBookmarkGateway =
		    new ManualBookmarkGateway(AppDatabase.getInstance(application).bookmarkDao());
		quickConnectHistoryGateway =
		    new QuickConnectHistoryGateway(HistoryDatabase.getInstance(application).historyDao());
	}

	public LiveData<List<BookmarkBase>> getBookmarks()
	{
		return bookmarks;
	}

	public String getCurrentQuery()
	{
		return currentQuery;
	}

	public void loadBookmarks(String query)
	{
		currentQuery = query != null ? query : "";
		executor.execute(() -> {
			List<BookmarkBase> result = new ArrayList<>();
			if (!currentQuery.isEmpty())
			{
				BookmarkBase qcBm = new BookmarkBase();
				qcBm.setType(BookmarkBase.TYPE_QUICKCONNECT);
				qcBm.setLabel(currentQuery);
				qcBm.setHostname(currentQuery);
				qcBm.setDirectConnect(true);
				result.add(qcBm);
				result.addAll(quickConnectHistoryGateway.findHistory(currentQuery));
				result.addAll(manualBookmarkGateway.findByLabelOrHostnameLike(currentQuery));
			}
			else
			{
				result.addAll(manualBookmarkGateway.findAll());
			}
			bookmarks.postValue(result);
		});
	}

	public void deleteBookmark(long id)
	{
		executor.execute(() -> {
			manualBookmarkGateway.delete(id);
			loadBookmarks(currentQuery);
		});
	}

	@Override protected void onCleared()
	{
		super.onCleared();
		executor.shutdown();
	}
}
