/*
   Quick connect history gateway

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.services;

import java.util.ArrayList;

import com.freerdp.freerdpcore.domain.BookmarkBase;
import com.freerdp.freerdpcore.domain.QuickConnectBookmark;

import android.database.Cursor;
import android.database.SQLException;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteException;
import android.database.sqlite.SQLiteOpenHelper;
import android.util.Log;


public class QuickConnectHistoryGateway {
	private final static String TAG = "QuickConnectHistoryGateway";
	private SQLiteOpenHelper historyDB;


	public QuickConnectHistoryGateway(SQLiteOpenHelper historyDB)
	{
		this.historyDB = historyDB;
	}

	public ArrayList<BookmarkBase> findHistory(String filter)
	{
		String[] column = { HistoryDB.QUICK_CONNECT_TABLE_COL_ITEM };

		SQLiteDatabase db = getReadableDatabase();
		String selection = (filter.length() > 0) ? (HistoryDB.QUICK_CONNECT_TABLE_COL_ITEM + " LIKE '%" + filter + "%'") : null;
		Cursor cursor = db.query(HistoryDB.QUICK_CONNECT_TABLE_NAME, column, selection, null, null, null, HistoryDB.QUICK_CONNECT_TABLE_COL_TIMESTAMP);
		
		ArrayList<BookmarkBase> result = new ArrayList<BookmarkBase>(cursor.getCount());
		if(cursor.moveToFirst())
		{
			do				
			{
				String hostname = cursor.getString(cursor.getColumnIndex(HistoryDB.QUICK_CONNECT_TABLE_COL_ITEM));
				QuickConnectBookmark bookmark = new QuickConnectBookmark();
				bookmark.setLabel(hostname);
				bookmark.setHostname(hostname);
				result.add(bookmark);
			} while(cursor.moveToNext());
		}
		cursor.close();
		return result;
	}
	
	public void addHistoryItem(String item) {
		String insertHistoryItem = "INSERT OR REPLACE INTO " + HistoryDB.QUICK_CONNECT_TABLE_NAME + " (" +
									HistoryDB.QUICK_CONNECT_TABLE_COL_ITEM + ", " + HistoryDB.QUICK_CONNECT_TABLE_COL_TIMESTAMP + ") VALUES('" + item + "', datetime('now'))"; 
		SQLiteDatabase db = getWritableDatabase();
		try
		{
			db.execSQL(insertHistoryItem);			
		}
		catch(SQLException e)
		{
			Log.v(TAG, e.toString());
		}
	}
	
	public boolean historyItemExists(String item) {
		String[] column = { HistoryDB.QUICK_CONNECT_TABLE_COL_ITEM };
		SQLiteDatabase db = getReadableDatabase();
		Cursor cursor = db.query(HistoryDB.QUICK_CONNECT_TABLE_NAME, column, HistoryDB.QUICK_CONNECT_TABLE_COL_ITEM + " = '" + item + "'", null, null, null, null);
		boolean exists = (cursor.getCount() == 1);
		cursor.close();
		return exists;
	}

	public void removeHistoryItem(String hostname) {
		SQLiteDatabase db = getWritableDatabase();
		db.delete(HistoryDB.QUICK_CONNECT_TABLE_NAME, HistoryDB.QUICK_CONNECT_TABLE_COL_ITEM + " = '" + hostname + "'", null);		
	}

	// safety wrappers
	// in case of getReadableDatabase it could happen that upgradeDB gets called which is 
	// a problem if the DB is only readable
	private SQLiteDatabase getWritableDatabase()
	{
		return historyDB.getWritableDatabase();		
	}

	private SQLiteDatabase getReadableDatabase()
	{
		SQLiteDatabase db;
		try {
			db = historyDB.getReadableDatabase();			
		}
		catch(SQLiteException e) {
			db = historyDB.getWritableDatabase();
		}
		return db;		
	}	
}
