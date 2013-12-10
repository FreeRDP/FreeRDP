/*
   Quick Connect History Database

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.services;

import android.content.Context;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;

public class HistoryDB extends SQLiteOpenHelper {

	private static final int DB_VERSION = 1;
	private static final String DB_NAME = "history.db";
	
	public static final String QUICK_CONNECT_TABLE_NAME = "quick_connect_history";
	public static final String QUICK_CONNECT_TABLE_COL_ITEM = "item";
	public static final String QUICK_CONNECT_TABLE_COL_TIMESTAMP = "timestamp";

	public HistoryDB(Context context)
	{
		super(context, DB_NAME, null, DB_VERSION);
	}

	@Override
	public void onCreate(SQLiteDatabase db) {
		
		String sqlQuickConnectHistory =
			"CREATE TABLE " + QUICK_CONNECT_TABLE_NAME + " ("
			+ QUICK_CONNECT_TABLE_COL_ITEM + " TEXT PRIMARY KEY, "
			+ QUICK_CONNECT_TABLE_COL_TIMESTAMP + " INTEGER"
			+ ");";
		
		db.execSQL(sqlQuickConnectHistory);
	}

	@Override
	public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
		// TODO Auto-generated method stub

	}

}
