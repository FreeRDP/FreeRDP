/*
   Android Shortcut activity

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.presentation;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.widget.EditText;

import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.pm.ShortcutInfoCompat;
import androidx.core.content.pm.ShortcutManagerCompat;
import androidx.core.graphics.drawable.IconCompat;
import androidx.lifecycle.ViewModelProvider;
import androidx.recyclerview.widget.LinearLayoutManager;

import com.freerdp.freerdpcore.R;
import com.freerdp.freerdpcore.databinding.ActivityShortcutsBinding;
import com.freerdp.freerdpcore.domain.BookmarkBase;
import com.freerdp.freerdpcore.domain.ConnectionReference;
import com.freerdp.freerdpcore.services.SessionRequestHandlerActivity;
import com.freerdp.freerdpcore.utils.BookmarkListAdapter;

public class ShortcutsActivity extends AppCompatActivity
{
	@Override public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);

		if (!Intent.ACTION_CREATE_SHORTCUT.equals(getIntent().getAction()))
		{
			finish();
			return;
		}

		ActivityShortcutsBinding binding = ActivityShortcutsBinding.inflate(getLayoutInflater());
		setContentView(binding.getRoot());

		if (getSupportActionBar() != null)
			getSupportActionBar().setDisplayHomeAsUpEnabled(true);

		ShortcutsViewModel viewModel = new ViewModelProvider(this).get(ShortcutsViewModel.class);

		BookmarkListAdapter adapter = new BookmarkListAdapter();
		adapter.setActionsEnabled(false);
		adapter.setCallbacks(new BookmarkListAdapter.Callbacks() {
			@Override public void onItemClick(String refStr)
			{
				if (!ConnectionReference.isBookmarkReference(refStr))
					return;
				BookmarkBase bookmark = findBookmark(adapter, refStr);
				String label = bookmark != null ? bookmark.getLabel() : refStr;
				setupShortcut(refStr, label);
			}

			@Override public void onDelete(long id)
			{
			}

			@Override public void onExport(BookmarkBase bookmark)
			{
			}
		});

		binding.recyclerViewShortcuts.setLayoutManager(new LinearLayoutManager(this));
		binding.recyclerViewShortcuts.setAdapter(adapter);

		viewModel.getBookmarks().observe(this, adapter::setItems);

		viewModel.loadBookmarks();
	}

	@Override public boolean onSupportNavigateUp()
	{
		finish();
		return true;
	}

	private static BookmarkBase findBookmark(BookmarkListAdapter adapter, String refStr)
	{
		for (BookmarkBase b : adapter.getItems())
		{
			if (ConnectionReference.getBookmarkReference(b.getId()).equals(refStr))
				return b;
		}
		return null;
	}

	private void setupShortcut(String strRef, String defaultLabel)
	{
		final EditText input = new EditText(this);
		input.setText(defaultLabel);

		new AlertDialog.Builder(this)
		    .setTitle(R.string.dlg_title_create_shortcut)
		    .setMessage(R.string.dlg_msg_create_shortcut)
		    .setView(input)
		    .setPositiveButton(
		        android.R.string.ok,
		        (dialog, which) -> {
			        String label = input.getText().toString();
			        if (label.isEmpty())
				        label = defaultLabel;

			        Intent shortcutIntent = new Intent(Intent.ACTION_VIEW);
			        shortcutIntent.setClassName(this,
			                                    SessionRequestHandlerActivity.class.getName());
			        shortcutIntent.setData(Uri.parse(strRef));

			        ShortcutInfoCompat shortcutInfo =
			            new ShortcutInfoCompat.Builder(this, "shortcut_" + strRef.hashCode())
			                .setShortLabel(label)
			                .setIcon(IconCompat.createWithResource(this, R.mipmap.ic_launcher))
			                .setIntent(shortcutIntent)
			                .build();

			        setResult(RESULT_OK,
			                  ShortcutManagerCompat.createShortcutResultIntent(this, shortcutInfo));
			        finish();
		        })
		    .setNegativeButton(android.R.string.cancel, (dialog, which) -> dialog.dismiss())
		    .show();
	}
}
