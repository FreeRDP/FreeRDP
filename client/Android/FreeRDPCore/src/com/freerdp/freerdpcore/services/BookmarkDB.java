/*
   Android Bookmark Database

   Copyright 2013 Thinstuff Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.services;

import android.content.Context;
import android.provider.BaseColumns;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;

public class BookmarkDB extends SQLiteOpenHelper
{
	private static final int DB_VERSION = 1;
	private static final String DB_NAME = "bookmarks.db";
	
	public static final String ID = BaseColumns._ID;
			
	public BookmarkDB(Context context)
	{
		super(context, DB_NAME, null, DB_VERSION);
	}
	
	@Override
	public void onCreate(SQLiteDatabase db)
	{
		String sqlScreenSettings =
			"CREATE TABLE tbl_screen_settings ("
			+ ID + " INTEGER PRIMARY KEY, "
			+ "colors INTEGER DEFAULT 16, "
			+ "resolution INTEGER DEFAULT 0, "
			+ "width, "
			+ "height);";
		
		db.execSQL(sqlScreenSettings);

		String sqlPerformanceFlags =
			"CREATE TABLE tbl_performance_flags ("
			+ ID + " INTEGER PRIMARY KEY, "
			+ "perf_remotefx INTEGER, "
			+ "perf_wallpaper INTEGER, "
			+ "perf_theming INTEGER, "
			+ "perf_full_window_drag INTEGER, "
			+ "perf_menu_animations INTEGER, "
			+ "perf_font_smoothing INTEGER, "
			+ "perf_desktop_composition INTEGER);";

		db.execSQL(sqlPerformanceFlags);
				
		String sqlManualBookmarks =
			"CREATE TABLE tbl_manual_bookmarks ("
			+ ID + " INTEGER PRIMARY KEY, "
			+ "label TEXT NOT NULL, "
			+ "hostname TEXT NOT NULL, "
			+ "username TEXT NOT NULL, "
			+ "password TEXT, "
			+ "domain TEXT, "
			+ "port TEXT, "
			+ "screen_settings INTEGER NOT NULL, "
			+ "performance_flags INTEGER NOT NULL, "
			
			+ "enable_3g_settings INTEGER DEFAULT 0, "
			+ "screen_3g INTEGER NOT NULL, "
			+ "performance_3g INTEGER NOT NULL, "
			+ "security INTEGER, "
			+ "remote_program TEXT, "
			+ "work_dir TEXT, "
			+ "console_mode INTEGER, "
		
			+ "FOREIGN KEY(screen_settings) REFERENCES tbl_screen_settings(" + ID + "), "
			+ "FOREIGN KEY(performance_flags) REFERENCES tbl_performance_flags(" + ID + "), "
			+ "FOREIGN KEY(screen_3g) REFERENCES tbl_screen_settings(" + ID + "), "
			+ "FOREIGN KEY(performance_3g) REFERENCES tbl_performance_flags(" + ID + ") "

			+ ");";
		
		db.execSQL(sqlManualBookmarks);

			
		// Insert a test entry
		String sqlInsertDefaultScreenEntry = 
			"INSERT INTO tbl_screen_settings ("
			+ "colors, "
			+ "resolution, "
			+ "width, "
			+ "height) "
			+ "VALUES ( "
			+ "32, 1, 1024, 768);";
		db.execSQL(sqlInsertDefaultScreenEntry);		
		db.execSQL(sqlInsertDefaultScreenEntry);		
		
		String sqlInsertDefaultPerfFlags = 
			"INSERT INTO tbl_performance_flags ("
			+ "perf_remotefx, "
			+ "perf_wallpaper, "
			+ "perf_theming, "
			+ "perf_full_window_drag, "
			+ "perf_menu_animations, "
			+ "perf_font_smoothing, "
			+ "perf_desktop_composition) "
			+ "VALUES ( "
			+ "1, 0, 0, 0, 0, 0, 0);";
		db.execSQL(sqlInsertDefaultPerfFlags);
		db.execSQL(sqlInsertDefaultPerfFlags);

		String sqlInsertDefaultSessionEntry = 
			"INSERT INTO tbl_manual_bookmarks ("
			+ "label, "
			+ "hostname, "
			+ "username, "
			+ "password, "
			+ "domain, "
			+ "port, "
			+ "screen_settings, "
			+ "performance_flags, "
			+ "screen_3g, "
			+ "performance_3g, "
			+ "security, "
			+ "remote_program, "
			+ "work_dir, "
			+ "console_mode) "			
			+ "VALUES ( "
			+ "'Test Server', "
			+ "'testservice.afreerdp.com', "
			+ "'', "
			+ "'', "
			+ "'', "
			+ "3389, "
			+ "1, 1, 2, 2, 0, '', '', 0);";
		db.execSQL(sqlInsertDefaultSessionEntry);
	}

	@Override
	public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion)
	{
	}
}
