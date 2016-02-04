/*
   Manual bookmarks database gateway

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

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
		
		// gateway settings
		columns.put("enable_gateway_settings", bm.getEnableGatewaySettings());
		columns.put("gateway_hostname", bm.getGatewaySettings().getHostname());
		columns.put("gateway_port", bm.getGatewaySettings().getPort());
		columns.put("gateway_username", bm.getGatewaySettings().getUsername());
		columns.put("gateway_password", bm.getGatewaySettings().getPassword());
		columns.put("gateway_domain", bm.getGatewaySettings().getDomain());		
	}

	@Override
	protected void addBookmarkSpecificColumns(ArrayList<String> columns) {
		columns.add("hostname");
		columns.add("port");
		columns.add("enable_gateway_settings");
		columns.add("gateway_hostname");
		columns.add("gateway_port");
		columns.add("gateway_username");
		columns.add("gateway_password");
		columns.add("gateway_domain");		
	}
	
	@Override
	protected void readBookmarkSpecificColumns(BookmarkBase bookmark, Cursor cursor) {
		ManualBookmark bm = (ManualBookmark)bookmark;
		bm.setHostname(cursor.getString(cursor.getColumnIndex("hostname")));
		bm.setPort(cursor.getInt(cursor.getColumnIndex("port")));

		bm.setEnableGatewaySettings(cursor.getInt(cursor.getColumnIndex("enable_gateway_settings")) == 0 ? false : true);
        readGatewaySettings(bm, cursor);
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
	
	private void readGatewaySettings(ManualBookmark bookmark, Cursor cursor) 
	{
        ManualBookmark.GatewaySettings gatewaySettings = bookmark.getGatewaySettings();
        gatewaySettings.setHostname(cursor.getString(cursor.getColumnIndex("gateway_hostname")));
        gatewaySettings.setPort(cursor.getInt(cursor.getColumnIndex("gateway_port")));
        gatewaySettings.setUsername(cursor.getString(cursor.getColumnIndex("gateway_username")));
        gatewaySettings.setPassword(cursor.getString(cursor.getColumnIndex("gateway_password")));
        gatewaySettings.setDomain(cursor.getString(cursor.getColumnIndex("gateway_domain")));
	}
}
