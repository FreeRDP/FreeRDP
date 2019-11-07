/*
   Android Bookmark Database

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.services;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.provider.BaseColumns;
import android.util.Log;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class BookmarkDB extends SQLiteOpenHelper
{
	public static final String ID = BaseColumns._ID;
	private static final int DB_VERSION = 9;
	private static final String DB_BACKUP_PREFIX = "temp_";
	private static final String DB_NAME = "bookmarks.db";
	static final String DB_TABLE_BOOKMARK = "tbl_manual_bookmarks";
	static final String DB_TABLE_SCREEN = "tbl_screen_settings";
	static final String DB_TABLE_PERFORMANCE = "tbl_performance_flags";
	private static final String[] DB_TABLES = { DB_TABLE_BOOKMARK, DB_TABLE_SCREEN,
		                                        DB_TABLE_PERFORMANCE };

	static final String DB_KEY_SCREEN_COLORS = "colors";
	static final String DB_KEY_SCREEN_RESOLUTION = "resolution";
	static final String DB_KEY_SCREEN_WIDTH = "width";
	static final String DB_KEY_SCREEN_HEIGHT = "height";

	static final String DB_KEY_SCREEN_SETTINGS = "screen_settings";
	static final String DB_KEY_SCREEN_SETTINGS_3G = "screen_3g";
	static final String DB_KEY_PERFORMANCE_FLAGS = "performance_flags";
	static final String DB_KEY_PERFORMANCE_FLAGS_3G = "performance_3g";

	static final String DB_KEY_PERFORMANCE_RFX = "perf_remotefx";
	static final String DB_KEY_PERFORMANCE_GFX = "perf_gfx";
	static final String DB_KEY_PERFORMANCE_H264 = "perf_gfx_h264";
	static final String DB_KEY_PERFORMANCE_WALLPAPER = "perf_wallpaper";
	static final String DB_KEY_PERFORMANCE_THEME = "perf_theming";
	static final String DB_KEY_PERFORMANCE_DRAG = "perf_full_window_drag";
	static final String DB_KEY_PERFORMANCE_MENU_ANIMATIONS = "perf_menu_animations";
	static final String DB_KEY_PERFORMANCE_FONTS = "perf_font_smoothing";
	static final String DB_KEY_PERFORMANCE_COMPOSITION = "perf_desktop_composition";

	static final String DB_KEY_BOOKMARK_LABEL = "label";
	static final String DB_KEY_BOOKMARK_HOSTNAME = "hostname";
	static final String DB_KEY_BOOKMARK_USERNAME = "username";
	static final String DB_KEY_BOOKMARK_PASSWORD = "password";
	static final String DB_KEY_BOOKMARK_DOMAIN = "domain";
	static final String DB_KEY_BOOKMARK_PORT = "port";

	static final String DB_KEY_BOOKMARK_REDIRECT_SDCARD = "redirect_sdcard";
	static final String DB_KEY_BOOKMARK_REDIRECT_SOUND = "redirect_sound";
	static final String DB_KEY_BOOKMARK_REDIRECT_MICROPHONE = "redirect_microphone";
	static final String DB_KEY_BOOKMARK_SECURITY = "security";
	static final String DB_KEY_BOOKMARK_REMOTE_PROGRAM = "remote_program";
	static final String DB_KEY_BOOKMARK_WORK_DIR = "work_dir";
	static final String DB_KEY_BOOKMARK_ASYNC_CHANNEL = "async_channel";
	static final String DB_KEY_BOOKMARK_ASYNC_INPUT = "async_input";
	static final String DB_KEY_BOOKMARK_ASYNC_UPDATE = "async_update";
	static final String DB_KEY_BOOKMARK_CONSOLE_MODE = "console_mode";
	static final String DB_KEY_BOOKMARK_DEBUG_LEVEL = "debug_level";

	static final String DB_KEY_BOOKMARK_GW_ENABLE = "enable_gateway_settings";
	static final String DB_KEY_BOOKMARK_GW_HOSTNAME = "gateway_hostname";
	static final String DB_KEY_BOOKMARK_GW_PORT = "gateway_port";
	static final String DB_KEY_BOOKMARK_GW_USERNAME = "gateway_username";
	static final String DB_KEY_BOOKMARK_GW_PASSWORD = "gateway_password";
	static final String DB_KEY_BOOKMARK_GW_DOMAIN = "gateway_domain";
	static final String DB_KEY_BOOKMARK_3G_ENABLE = "enable_3g_settings";

	public BookmarkDB(Context context)
	{
		super(context, DB_NAME, null, DB_VERSION);
	}

	private static List<String> GetColumns(SQLiteDatabase db, String tableName)
	{
		List<String> ar = null;
		Cursor c = null;
		try
		{
			c = db.rawQuery("SELECT * FROM " + tableName + " LIMIT 1", null);
			if (c != null)
			{
				ar = new ArrayList<>(Arrays.asList(c.getColumnNames()));
			}
		}
		catch (Exception e)
		{
			Log.v(tableName, e.getMessage(), e);
			e.printStackTrace();
		}
		finally
		{
			if (c != null)
				c.close();
		}
		return ar;
	}

	private static String joinStrings(List<String> list, String delim)
	{
		StringBuilder buf = new StringBuilder();
		int num = list.size();
		for (int i = 0; i < num; i++)
		{
			if (i != 0)
				buf.append(delim);
			buf.append((String)list.get(i));
		}
		return buf.toString();
	}

	private void backupTables(SQLiteDatabase db)
	{
		for (String table : DB_TABLES)
		{
			final String tmpTable = DB_BACKUP_PREFIX + table;
			final String query = "ALTER TABLE '" + table + "' RENAME TO '" + tmpTable + "'";
			try
			{
				db.execSQL(query);
			}
			catch (Exception e)
			{
				/* Ignore errors if table does not exist. */
			}
		}
	}

	private void dropOldTables(SQLiteDatabase db)
	{
		for (String table : DB_TABLES)
		{
			final String tmpTable = DB_BACKUP_PREFIX + table;
			final String query = "DROP TABLE IF EXISTS '" + tmpTable + "'";
			db.execSQL(query);
		}
	}

	private void createDB(SQLiteDatabase db)
	{
		final String sqlScreenSettings =
		    "CREATE TABLE IF NOT EXISTS " + DB_TABLE_SCREEN + " (" + ID + " INTEGER PRIMARY KEY, " +
		    DB_KEY_SCREEN_COLORS + " INTEGER DEFAULT 16, " + DB_KEY_SCREEN_RESOLUTION +
		    " INTEGER DEFAULT 0, " + DB_KEY_SCREEN_WIDTH + ", " + DB_KEY_SCREEN_HEIGHT + ");";

		db.execSQL(sqlScreenSettings);

		final String sqlPerformanceFlags =
		    "CREATE TABLE IF NOT EXISTS " + DB_TABLE_PERFORMANCE + " (" + ID +
		    " INTEGER PRIMARY KEY, " + DB_KEY_PERFORMANCE_RFX + " INTEGER, " +
		    DB_KEY_PERFORMANCE_GFX + " INTEGER, " + DB_KEY_PERFORMANCE_H264 + " INTEGER, " +
		    DB_KEY_PERFORMANCE_WALLPAPER + " INTEGER, " + DB_KEY_PERFORMANCE_THEME + " INTEGER, " +
		    DB_KEY_PERFORMANCE_DRAG + " INTEGER, " + DB_KEY_PERFORMANCE_MENU_ANIMATIONS +
		    " INTEGER, " + DB_KEY_PERFORMANCE_FONTS + " INTEGER, " +
		    DB_KEY_PERFORMANCE_COMPOSITION + " INTEGER);";

		db.execSQL(sqlPerformanceFlags);

		final String sqlManualBookmarks = getManualBookmarksCreationString();
		db.execSQL(sqlManualBookmarks);
	}

	private void upgradeTables(SQLiteDatabase db)
	{
		for (String table : DB_TABLES)
		{
			final String tmpTable = DB_BACKUP_PREFIX + table;

			final List<String> newColumns = GetColumns(db, table);
			List<String> columns = GetColumns(db, tmpTable);

			if (columns != null)
			{
				columns.retainAll(newColumns);

				// restore data
				final String cols = joinStrings(columns, ",");
				final String query = String.format("INSERT INTO %s (%s) SELECT %s from '%s'", table,
				                                   cols, cols, tmpTable);
				db.execSQL(query);
			}
		}
	}

	private void downgradeTables(SQLiteDatabase db)
	{
		for (String table : DB_TABLES)
		{
			final String tmpTable = DB_BACKUP_PREFIX + table;

			List<String> oldColumns = GetColumns(db, table);
			final List<String> columns = GetColumns(db, tmpTable);

			if (oldColumns != null)
			{
				oldColumns.retainAll(columns);

				// restore data
				final String cols = joinStrings(oldColumns, ",");
				final String query = String.format("INSERT INTO %s (%s) SELECT %s from '%s'", table,
				                                   cols, cols, tmpTable);
				db.execSQL(query);
			}
		}
	}

	private List<String> getTableNames(SQLiteDatabase db)
	{
		final String query = "SELECT name FROM sqlite_master WHERE type='table'";
		Cursor cursor = db.rawQuery(query, null);
		List<String> list = new ArrayList<>();
		try
		{
			if (cursor.moveToFirst() && (cursor.getCount() > 0))
			{
				while (!cursor.isAfterLast())
				{
					final String name = cursor.getString(cursor.getColumnIndex("name"));
					list.add(name);
					cursor.moveToNext();
				}
			}
		}
		finally
		{
			cursor.close();
		}

		return list;
	}

	private void insertDefault(SQLiteDatabase db)
	{
		ContentValues screenValues = new ContentValues();
		screenValues.put(DB_KEY_SCREEN_COLORS, 32);
		screenValues.put(DB_KEY_SCREEN_RESOLUTION, 1);
		screenValues.put(DB_KEY_SCREEN_WIDTH, 1024);
		screenValues.put(DB_KEY_SCREEN_HEIGHT, 768);

		final long idScreen = db.insert(DB_TABLE_SCREEN, null, screenValues);
		final long idScreen3g = db.insert(DB_TABLE_SCREEN, null, screenValues);

		ContentValues performanceValues = new ContentValues();
		performanceValues.put(DB_KEY_PERFORMANCE_RFX, 1);
		performanceValues.put(DB_KEY_PERFORMANCE_GFX, 1);
		performanceValues.put(DB_KEY_PERFORMANCE_H264, 0);
		performanceValues.put(DB_KEY_PERFORMANCE_WALLPAPER, 0);
		performanceValues.put(DB_KEY_PERFORMANCE_THEME, 0);
		performanceValues.put(DB_KEY_PERFORMANCE_DRAG, 0);
		performanceValues.put(DB_KEY_PERFORMANCE_MENU_ANIMATIONS, 0);
		performanceValues.put(DB_KEY_PERFORMANCE_FONTS, 0);
		performanceValues.put(DB_KEY_PERFORMANCE_COMPOSITION, 0);

		final long idPerformance = db.insert(DB_TABLE_PERFORMANCE, null, performanceValues);
		final long idPerformance3g = db.insert(DB_TABLE_PERFORMANCE, null, performanceValues);

		ContentValues bookmarkValues = new ContentValues();
		bookmarkValues.put(DB_KEY_BOOKMARK_LABEL, "Test Server");
		bookmarkValues.put(DB_KEY_BOOKMARK_HOSTNAME, "testservice.afreerdp.com");
		bookmarkValues.put(DB_KEY_BOOKMARK_USERNAME, "");
		bookmarkValues.put(DB_KEY_BOOKMARK_PASSWORD, "");
		bookmarkValues.put(DB_KEY_BOOKMARK_DOMAIN, "");
		bookmarkValues.put(DB_KEY_BOOKMARK_PORT, "3389");

		bookmarkValues.put(DB_KEY_SCREEN_SETTINGS, idScreen);
		bookmarkValues.put(DB_KEY_SCREEN_SETTINGS_3G, idScreen3g);
		bookmarkValues.put(DB_KEY_PERFORMANCE_FLAGS, idPerformance);
		bookmarkValues.put(DB_KEY_PERFORMANCE_FLAGS_3G, idPerformance3g);

		bookmarkValues.put(DB_KEY_BOOKMARK_REDIRECT_SDCARD, 0);
		bookmarkValues.put(DB_KEY_BOOKMARK_REDIRECT_SOUND, 0);
		bookmarkValues.put(DB_KEY_BOOKMARK_REDIRECT_MICROPHONE, 0);
		bookmarkValues.put(DB_KEY_BOOKMARK_SECURITY, 0);
		bookmarkValues.put(DB_KEY_BOOKMARK_REMOTE_PROGRAM, "");
		bookmarkValues.put(DB_KEY_BOOKMARK_WORK_DIR, "");
		bookmarkValues.put(DB_KEY_BOOKMARK_ASYNC_CHANNEL, 1);
		bookmarkValues.put(DB_KEY_BOOKMARK_ASYNC_INPUT, 1);
		bookmarkValues.put(DB_KEY_BOOKMARK_ASYNC_UPDATE, 1);
		bookmarkValues.put(DB_KEY_BOOKMARK_CONSOLE_MODE, 0);
		bookmarkValues.put(DB_KEY_BOOKMARK_DEBUG_LEVEL, "INFO");

		db.insert(DB_TABLE_BOOKMARK, null, bookmarkValues);
	}

	@Override public void onCreate(SQLiteDatabase db)
	{
		createDB(db);
		insertDefault(db);
	}

	private String getManualBookmarksCreationString()
	{
		return ("CREATE TABLE IF NOT EXISTS " + DB_TABLE_BOOKMARK + " (" + ID +
		        " INTEGER PRIMARY KEY, " + DB_KEY_BOOKMARK_LABEL + " TEXT NOT NULL, " +
		        DB_KEY_BOOKMARK_HOSTNAME + " TEXT NOT NULL, " + DB_KEY_BOOKMARK_USERNAME +
		        " TEXT NOT NULL, " + DB_KEY_BOOKMARK_PASSWORD + " TEXT, " + DB_KEY_BOOKMARK_DOMAIN +
		        " TEXT, " + DB_KEY_BOOKMARK_PORT + " TEXT, " + DB_KEY_SCREEN_SETTINGS +
		        " INTEGER NOT NULL, " + DB_KEY_PERFORMANCE_FLAGS + " INTEGER NOT NULL, "

		        + DB_KEY_BOOKMARK_GW_ENABLE + " INTEGER DEFAULT 0, " + DB_KEY_BOOKMARK_GW_HOSTNAME +
		        " TEXT, " + DB_KEY_BOOKMARK_GW_PORT + " INTEGER DEFAULT 443, " +
		        DB_KEY_BOOKMARK_GW_USERNAME + " TEXT, " + DB_KEY_BOOKMARK_GW_PASSWORD + " TEXT, " +
		        DB_KEY_BOOKMARK_GW_DOMAIN + " TEXT, "

		        + DB_KEY_BOOKMARK_3G_ENABLE + " INTEGER DEFAULT 0, " + DB_KEY_SCREEN_SETTINGS_3G +
		        " INTEGER NOT NULL, " + DB_KEY_PERFORMANCE_FLAGS_3G + " INTEGER NOT NULL, " +
		        DB_KEY_BOOKMARK_REDIRECT_SDCARD + " INTEGER DEFAULT 0, " +
		        DB_KEY_BOOKMARK_REDIRECT_SOUND + " INTEGER DEFAULT 0, " +
		        DB_KEY_BOOKMARK_REDIRECT_MICROPHONE + " INTEGER DEFAULT 0, " +
		        DB_KEY_BOOKMARK_SECURITY + " INTEGER, " + DB_KEY_BOOKMARK_REMOTE_PROGRAM +
		        " TEXT, " + DB_KEY_BOOKMARK_WORK_DIR + " TEXT, " + DB_KEY_BOOKMARK_ASYNC_CHANNEL +
		        " INTEGER DEFAULT 0, " + DB_KEY_BOOKMARK_ASYNC_INPUT + " INTEGER DEFAULT 0, " +
		        DB_KEY_BOOKMARK_ASYNC_UPDATE + " INTEGER DEFAULT 0, " +
		        DB_KEY_BOOKMARK_CONSOLE_MODE + " INTEGER, " + DB_KEY_BOOKMARK_DEBUG_LEVEL +
		        " TEXT DEFAULT 'INFO', "

		        + "FOREIGN KEY(" + DB_KEY_SCREEN_SETTINGS + ") REFERENCES " + DB_TABLE_SCREEN +
		        "(" + ID + "), "
		        + "FOREIGN KEY(" + DB_KEY_PERFORMANCE_FLAGS + ") REFERENCES " +
		        DB_TABLE_PERFORMANCE + "(" + ID + "), "
		        + "FOREIGN KEY(" + DB_KEY_SCREEN_SETTINGS_3G + ") REFERENCES " + DB_TABLE_SCREEN +
		        "(" + ID + "), "
		        + "FOREIGN KEY(" + DB_KEY_PERFORMANCE_FLAGS_3G + ") REFERENCES " +
		        DB_TABLE_PERFORMANCE + "(" + ID + ") "

		        + ");");
	}

	private void recreateDB(SQLiteDatabase db)
	{
		for (String table : DB_TABLES)
		{
			final String query = "DROP TABLE IF EXISTS '" + table + "'";
			db.execSQL(query);
		}
		onCreate(db);
	}

	private void upgradeDB(SQLiteDatabase db)
	{
		db.beginTransaction();
		try
		{
			/* Back up old tables. */
			dropOldTables(db);
			backupTables(db);
			createDB(db);
			upgradeTables(db);

			db.setTransactionSuccessful();
		}
		finally
		{
			db.endTransaction();
			dropOldTables(db);
		}
	}

	private void downgradeDB(SQLiteDatabase db)
	{
		db.beginTransaction();
		try
		{
			/* Back up old tables. */
			dropOldTables(db);
			backupTables(db);
			createDB(db);
			downgradeTables(db);

			db.setTransactionSuccessful();
		}
		finally
		{
			db.endTransaction();
			dropOldTables(db);
		}
	}

	// from
	// http://stackoverflow.com/questions/3424156/upgrade-sqlite-database-from-one-version-to-another
	@Override public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion)
	{
		switch (oldVersion)
		{
			case 0:
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 8:
			case 9:
				upgradeDB(db);
				break;
			default:
				recreateDB(db);
				break;
		}
	}

	@Override public void onDowngrade(SQLiteDatabase db, int oldVersion, int newVersion)
	{
		downgradeDB(db);
	}
}
