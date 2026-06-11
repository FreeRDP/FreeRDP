/*
   Main/Home Activity

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz
   Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.presentation;

import androidx.appcompat.app.AlertDialog;
import android.Manifest;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.widget.Toast;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.FileProvider;
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
import com.freerdp.freerdpcore.domain.BookmarkBase;
import com.freerdp.freerdpcore.domain.ConnectionReference;
import com.freerdp.freerdpcore.utils.BookmarkListAdapter;
import com.freerdp.freerdpcore.utils.RDPFileHelper;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;

public class HomeActivity extends AppCompatActivity
{
	private static final String TAG = "HomeActivity";
	private static final String PARAM_SEARCH_QUERY = "search_query";

	private HomeBinding binding;
	private HomeViewModel viewModel;
	private BookmarkListAdapter bookmarkListAdapter;
	private MenuItem searchMenuItem;
	private SearchView searchView;

	private ExternalDisplayManager externalDisplayManager;

	@Override public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);
		binding = HomeBinding.inflate(getLayoutInflater());
		setContentView(binding.getRoot());

		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU &&
		    checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) !=
		        PackageManager.PERMISSION_GRANTED)
		{
			requestPermissions(new String[] { Manifest.permission.POST_NOTIFICATIONS }, 0);
		}

		externalDisplayManager = new ExternalDisplayManager(this);

		long heapSize = Runtime.getRuntime().maxMemory();
		Log.i(TAG, "Max HeapSize: " + heapSize);
		Log.i(TAG, "App data folder: " + getFilesDir().toString());

		// check for passed .rdp file and open it in a new bookmark
		Intent caller = getIntent();
		Uri callParameter = caller.getData();

		if (Intent.ACTION_VIEW.equals(caller.getAction()) && callParameter != null)
		{
			String refStr = ConnectionReference.getFileReference(callParameter.toString());
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
					externalDisplayManager.launchSessionWithDisplayPicker(refStr);

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

			@Override public void onExport(BookmarkBase bookmark)
			{
				shareRdpFile(bookmark);
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

	private void shareRdpFile(BookmarkBase bookmark)
	{
		String filename = bookmark.getLabel().replaceAll("[^\\w. -]", "_") + ".rdp";
		File file = new File(getCacheDir(), filename);
		try (FileOutputStream out = new FileOutputStream(file))
		{
			out.write(RDPFileHelper.toRdpString(bookmark).getBytes(StandardCharsets.UTF_8));
		}
		catch (IOException e)
		{
			Log.e(TAG, "Failed to write RDP file for sharing", e);
			Toast.makeText(this, R.string.export_failed, Toast.LENGTH_SHORT).show();
			return;
		}
		Uri uri = FileProvider.getUriForFile(this, "com.freerdp.afreerdp.fileprovider", file);
		Intent share = new Intent(Intent.ACTION_SEND);
		share.setType("application/x-rdp");
		share.putExtra(Intent.EXTRA_STREAM, uri);
		share.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
		startActivity(Intent.createChooser(share, bookmark.getLabel()));
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
