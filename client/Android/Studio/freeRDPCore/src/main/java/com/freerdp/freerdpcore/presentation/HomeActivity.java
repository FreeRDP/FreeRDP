/*
   Main/Home Activity

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.presentation;

import androidx.appcompat.app.AlertDialog;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import androidx.appcompat.app.AppCompatActivity;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.activity.OnBackPressedCallback;
import androidx.appcompat.widget.SearchView;
import androidx.lifecycle.ViewModelProvider;
import androidx.recyclerview.widget.LinearLayoutManager;

import com.freerdp.freerdpcore.R;
import com.freerdp.freerdpcore.databinding.HomeBinding;
import com.freerdp.freerdpcore.domain.ConnectionReference;
import com.freerdp.freerdpcore.utils.BookmarkListAdapter;

public class HomeActivity extends AppCompatActivity
{
	private static final String TAG = "HomeActivity";
	private static final String PARAM_SEARCH_QUERY = "search_query";

	private HomeBinding binding;
	private HomeViewModel viewModel;
	private BookmarkListAdapter bookmarkListAdapter;
	private MenuItem searchMenuItem;
	private SearchView searchView;

	@Override public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);
		binding = HomeBinding.inflate(getLayoutInflater());
		setContentView(binding.getRoot());

		long heapSize = Runtime.getRuntime().maxMemory();
		Log.i(TAG, "Max HeapSize: " + heapSize);
		Log.i(TAG, "App data folder: " + getFilesDir().toString());

		// check for passed .rdp file and open it in a new bookmark
		Intent caller = getIntent();
		Uri callParameter = caller.getData();

		if (Intent.ACTION_VIEW.equals(caller.getAction()) && callParameter != null)
		{
			String refStr = ConnectionReference.getFileReference(callParameter.getPath());
			Bundle bundle = new Bundle();
			bundle.putString(BookmarkActivity.PARAM_CONNECTION_REFERENCE, refStr);

			Intent bookmarkIntent =
			    new Intent(this.getApplicationContext(), BookmarkActivity.class);
			bookmarkIntent.putExtras(bundle);
			startActivity(bookmarkIntent);
		}

		viewModel = new ViewModelProvider(this).get(HomeViewModel.class);

		bookmarkListAdapter = new BookmarkListAdapter();
		bookmarkListAdapter.setCallbacks(new BookmarkListAdapter.Callbacks() {
			@Override public void onItemClick(String refStr)
			{
				if (ConnectionReference.isBookmarkReference(refStr) ||
				    ConnectionReference.isHostnameReference(refStr))
				{
					Bundle bundle = new Bundle();
					bundle.putString(SessionActivity.PARAM_CONNECTION_REFERENCE, refStr);

					Intent sessionIntent = new Intent(HomeActivity.this, SessionActivity.class);
					sessionIntent.putExtras(bundle);
					sessionIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK |
					                       Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
					startActivity(sessionIntent);

					if (searchView != null)
					{
						searchView.setQuery("", false);
						searchView.setIconified(true);
					}
					if (searchMenuItem != null)
					{
						searchMenuItem.collapseActionView();
					}
					viewModel.loadBookmarks("");
				}
			}

			@Override public void onDelete(long id)
			{
				viewModel.deleteBookmark(id);
			}
		});

		binding.recyclerViewBookmarks.setLayoutManager(new LinearLayoutManager(this));
		binding.recyclerViewBookmarks.setAdapter(bookmarkListAdapter);

		viewModel.getBookmarks().observe(this,
		                                 bookmarks -> bookmarkListAdapter.setItems(bookmarks));

		// restore search query after process death
		if (savedInstanceState != null)
		{
			String query = savedInstanceState.getString(PARAM_SEARCH_QUERY);
			if (query != null && !query.isEmpty() && viewModel.getCurrentQuery().isEmpty())
			{
				viewModel.loadBookmarks(query);
			}
		}

		getOnBackPressedDispatcher().addCallback(this, new OnBackPressedCallback(true) {
			@Override public void handleOnBackPressed()
			{
				if (ApplicationSettingsActivity.getAskOnExit(HomeActivity.this))
				{
					new AlertDialog.Builder(HomeActivity.this)
					    .setTitle(R.string.dlg_title_exit)
					    .setMessage(R.string.dlg_msg_exit)
					    .setPositiveButton(R.string.yes, (dialog, which) -> finish())
					    .setNegativeButton(R.string.no, (dialog, which) -> dialog.dismiss())
					    .show();
				}
				else
				{
					finish();
				}
			}
		});
	}

	@Override protected void onResume()
	{
		super.onResume();
		Log.v(TAG, "HomeActivity.onResume");
		viewModel.loadBookmarks(viewModel.getCurrentQuery());
	}

	@Override protected void onSaveInstanceState(Bundle outState)
	{
		super.onSaveInstanceState(outState);
		outState.putString(PARAM_SEARCH_QUERY, viewModel.getCurrentQuery());
	}

	@Override public boolean onCreateOptionsMenu(Menu menu)
	{
		MenuInflater inflater = getMenuInflater();
		inflater.inflate(R.menu.home_menu, menu);
		setupSearchView(menu);
		return true;
	}

	@Override public boolean onOptionsItemSelected(MenuItem item)
	{
		// refer to http://tools.android.com/tips/non-constant-fields why we can't use switch/case
		// here ..
		int itemId = item.getItemId();
		if (itemId == R.id.newBookmark)
		{
			Intent bookmarkIntent = new Intent(this, BookmarkActivity.class);
			startActivity(bookmarkIntent);
		}
		else if (itemId == R.id.appSettings)
		{
			Intent settingsIntent = new Intent(this, ApplicationSettingsActivity.class);
			startActivity(settingsIntent);
		}
		else if (itemId == R.id.help)
		{
			Intent helpIntent = new Intent(this, HelpActivity.class);
			startActivity(helpIntent);
		}
		else if (itemId == R.id.about)
		{
			Intent aboutIntent = new Intent(this, AboutActivity.class);
			startActivity(aboutIntent);
		}

		return true;
	}

	private void setupSearchView(Menu menu)
	{
		searchMenuItem = menu.findItem(R.id.action_search);
		if (searchMenuItem != null)
		{
			searchView = (SearchView)searchMenuItem.getActionView();

			String currentQuery = viewModel.getCurrentQuery();
			if (currentQuery != null && !currentQuery.isEmpty())
			{
				searchMenuItem.expandActionView();
				searchView.setQuery(currentQuery, false);
				searchView.clearFocus();
			}

			searchView.setOnQueryTextListener(new SearchView.OnQueryTextListener() {
				@Override public boolean onQueryTextSubmit(String query)
				{
					return true;
				}

				@Override public boolean onQueryTextChange(String s)
				{
					viewModel.loadBookmarks(s);
					return true;
				}
			});
		}
	}
}
