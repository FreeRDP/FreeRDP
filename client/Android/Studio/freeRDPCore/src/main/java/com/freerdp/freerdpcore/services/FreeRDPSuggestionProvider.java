/*
   Suggestion Provider for RDP bookmarks 

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.services;

import java.util.ArrayList;

import com.freerdp.freerdpcore.R;
import com.freerdp.freerdpcore.application.GlobalApp;
import com.freerdp.freerdpcore.domain.BookmarkBase;
import com.freerdp.freerdpcore.domain.ConnectionReference;
import com.freerdp.freerdpcore.domain.ManualBookmark;

import android.app.SearchManager;
import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;

public class FreeRDPSuggestionProvider extends ContentProvider {

	public static final Uri CONTENT_URI = Uri.parse("content://com.freerdp.afreerdp.services.freerdpsuggestionprovider");	
	
	@Override
	public int delete(Uri uri, String selection, String[] selectionArgs) {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public String getType(Uri uri) {
		return "vnd.android.cursor.item/vnd.freerdp.remote";
	}

	@Override
	public Uri insert(Uri uri, ContentValues values) {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public boolean onCreate() {
		return true;
	}

	@Override
	public Cursor query(Uri uri, String[] projection, String selection,
			String[] selectionArgs, String sortOrder) {
		
		String query = (selectionArgs != null && selectionArgs.length > 0) ? selectionArgs[0] : "";

		// search history
		ArrayList<BookmarkBase> history = GlobalApp.getQuickConnectHistoryGateway().findHistory(query);

		// search bookmarks
		ArrayList<BookmarkBase> manualBookmarks;
		if(query.length() > 0)
			manualBookmarks = GlobalApp.getManualBookmarkGateway().findByLabelOrHostnameLike(query);
		else
			manualBookmarks = GlobalApp.getManualBookmarkGateway().findAll();

		return createResultCursor(history, manualBookmarks);
	}

	@Override
	public int update(Uri uri, ContentValues values, String selection,
			String[] selectionArgs) {
		// TODO Auto-generated method stub
		return 0;
	}

	private void addBookmarksToCursor(ArrayList<BookmarkBase> bookmarks, MatrixCursor resultCursor) {
		Object[] row = new Object[5];
		for(BookmarkBase bookmark : bookmarks)
		{
			row[0] = new Long(bookmark.getId());
			row[1] = bookmark.getLabel();
			row[2] = bookmark.<ManualBookmark>get().getHostname();
			row[3] = ConnectionReference.getManualBookmarkReference(bookmark.getId());
			row[4] = "android.resource://" + getContext().getPackageName() + "/" + R.drawable.icon_star_on;
			resultCursor.addRow(row);			
		}
	}
	
	private void addHistoryToCursor(ArrayList<BookmarkBase> history, MatrixCursor resultCursor) {		
		Object[] row = new Object[5];
		for(BookmarkBase bookmark : history)
		{
			row[0] = new Integer(1);
			row[1] = bookmark.getLabel();
			row[2] = bookmark.getLabel();
			row[3] = ConnectionReference.getHostnameReference(bookmark.getLabel());
			row[4] = "android.resource://" + getContext().getPackageName() + "/" + R.drawable.icon_star_off;
			resultCursor.addRow(row);			
		}
	}
	
	private Cursor createResultCursor(ArrayList<BookmarkBase> history, ArrayList<BookmarkBase> manualBookmarks) {
		
		// create result matrix cursor
		int totalCount = history.size() + manualBookmarks.size(); 
		String[] columns = { android.provider.BaseColumns._ID, SearchManager.SUGGEST_COLUMN_TEXT_1, 
				 SearchManager.SUGGEST_COLUMN_TEXT_2, SearchManager.SUGGEST_COLUMN_INTENT_DATA, 
				 SearchManager.SUGGEST_COLUMN_ICON_2 };
		MatrixCursor matrixCursor = new MatrixCursor(columns, totalCount);
		
		// populate result matrix
		if(totalCount > 0)
		{
			addHistoryToCursor(history, matrixCursor);		
			addBookmarksToCursor(manualBookmarks, matrixCursor);
		}
		return matrixCursor;
	}

}
