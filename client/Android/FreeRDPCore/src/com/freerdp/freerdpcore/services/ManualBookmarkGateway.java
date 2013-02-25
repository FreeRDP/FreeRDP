/*
   Manual bookmarks database gateway

   Copyright 2013 Thinstuff Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.services;

import java.util.ArrayList;

import android.content.ContentValues;
import android.database.Cursor;
import android.database.sqlite.SQLiteOpenHelper;

import com.freerdp.freerdpcore.domain.BookmarkBase;
import com.freerdp.freerdpcore.domain.ManualBookmark;

public class ManualBookmarkGateway extends BookmarkBaseGateway {

	public ManualBookmarkGateway(SQLiteOpenHelper bookmarkDB) {
		super(bookmarkDB);
	}
	
	@Override
	protected BookmarkBase createBookmark() {
		return new ManualBookmark();
	}
	
	@Override
	protected String getBookmarkTableName() {
		return "tbl_manual_bookmarks";
	}

	@Override
	protected void addBookmarkSpecificColumns(BookmarkBase bookmark, ContentValues columns) {
		ManualBookmark bm = (ManualBookmark)bookmark;
		columns.put("hostname", bm.getHostname());
		columns.put("port", bm.getPort());
	}

	@Override
	protected void addBookmarkSpecificColumns(ArrayList<String> columns) {
		columns.add("hostname");
		columns.add("port");
	}
	
	@Override
	protected void readBookmarkSpecificColumns(BookmarkBase bookmark, Cursor cursor) {
		ManualBookmark bm = (ManualBookmark)bookmark;
		bm.setHostname(cursor.getString(cursor.getColumnIndex("hostname")));
		bm.setPort(cursor.getInt(cursor.getColumnIndex("port")));
	}

	public BookmarkBase findByLabelOrHostname(String pattern) 
	{	
		if(pattern.length() == 0)
			return null;
		
		Cursor cursor = queryBookmarks("label = '" + pattern + "' OR hostname = '" + pattern + "'", "label");				
		BookmarkBase bookmark = null;
		if(cursor.moveToFirst())
			bookmark = getBookmarkFromCursor(cursor);

		cursor.close();	
		return bookmark;
	}	
	
	public ArrayList<BookmarkBase> findByLabelOrHostnameLike(String pattern) 
	{	
		Cursor cursor = queryBookmarks("label LIKE '%" + pattern + "%' OR hostname LIKE '%" + pattern + "%'", "label");
		ArrayList<BookmarkBase> bookmarks = new ArrayList<BookmarkBase>(cursor.getCount());
		
		if(cursor.moveToFirst())
		{
			do
			{
				bookmarks.add(getBookmarkFromCursor(cursor));
			}while(cursor.moveToNext());
		}

		cursor.close();	
		return bookmarks;
	}	
}
