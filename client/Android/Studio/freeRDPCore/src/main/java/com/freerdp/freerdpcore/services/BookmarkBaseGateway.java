/*
   Helper class to access bookmark database

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.services;

import android.content.ContentValues;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteException;
import android.database.sqlite.SQLiteOpenHelper;
import android.database.sqlite.SQLiteQueryBuilder;
import android.util.Log;

import com.freerdp.freerdpcore.domain.BookmarkBase;

import java.util.ArrayList;

public abstract class BookmarkBaseGateway
{
	private final static String TAG = "BookmarkBaseGateway";
	private SQLiteOpenHelper bookmarkDB;

	private static final String JOIN_PREFIX = "join_";
	private static final String KEY_BOOKMARK_ID = "bookmarkId";
	private static final String KEY_SCREEN_COLORS = "screenColors";
	private static final String KEY_SCREEN_COLORS_3G = "screenColors3G";
	private static final String KEY_SCREEN_RESOLUTION = "screenResolution";
	private static final String KEY_SCREEN_RESOLUTION_3G = "screenResolution3G";
	private static final String KEY_SCREEN_WIDTH = "screenWidth";
	private static final String KEY_SCREEN_WIDTH_3G = "screenWidth3G";
	private static final String KEY_SCREEN_HEIGHT = "screenHeight";
	private static final String KEY_SCREEN_HEIGHT_3G = "screenHeight3G";

	private static final String KEY_PERFORMANCE_RFX = "performanceRemoteFX";
	private static final String KEY_PERFORMANCE_RFX_3G = "performanceRemoteFX3G";
	private static final String KEY_PERFORMANCE_GFX = "performanceGfx";
	private static final String KEY_PERFORMANCE_GFX_3G = "performanceGfx3G";
	private static final String KEY_PERFORMANCE_H264 = "performanceGfxH264";
	private static final String KEY_PERFORMANCE_H264_3G = "performanceGfxH2643G";
	private static final String KEY_PERFORMANCE_WALLPAPER = "performanceWallpaper";
	private static final String KEY_PERFORMANCE_WALLPAPER_3G = "performanceWallpaper3G";
	private static final String KEY_PERFORMANCE_THEME = "performanceTheming";
	private static final String KEY_PERFORMANCE_THEME_3G = "performanceTheming3G";

	private static final String KEY_PERFORMANCE_DRAG = "performanceFullWindowDrag";
	private static final String KEY_PERFORMANCE_DRAG_3G = "performanceFullWindowDrag3G";
	private static final String KEY_PERFORMANCE_MENU_ANIMATIONS = "performanceMenuAnimations";
	private static final String KEY_PERFORMANCE_MENU_ANIMATIONS_3G = "performanceMenuAnimations3G";
	private static final String KEY_PERFORMANCE_FONTS = "performanceFontSmoothing";
	private static final String KEY_PERFORMANCE_FONTS_3G = "performanceFontSmoothing3G";
	private static final String KEY_PERFORMANCE_COMPOSITION = "performanceDesktopComposition";
	private static final String KEY_PERFORMANCE_COMPOSITION_3G = "performanceDesktopComposition3G";

	public BookmarkBaseGateway(SQLiteOpenHelper bookmarkDB)
	{
		this.bookmarkDB = bookmarkDB;
	}

	protected abstract BookmarkBase createBookmark();

	protected abstract String getBookmarkTableName();

	protected abstract void addBookmarkSpecificColumns(ArrayList<String> columns);

	protected abstract void addBookmarkSpecificColumns(BookmarkBase bookmark,
	                                                   ContentValues columns);

	protected abstract void readBookmarkSpecificColumns(BookmarkBase bookmark, Cursor cursor);

	public void insert(BookmarkBase bookmark)
	{
		// begin transaction
		SQLiteDatabase db = getWritableDatabase();
		db.beginTransaction();

		long rowid;
		ContentValues values = new ContentValues();
		values.put(BookmarkDB.DB_KEY_BOOKMARK_LABEL, bookmark.getLabel());
		values.put(BookmarkDB.DB_KEY_BOOKMARK_USERNAME, bookmark.getUsername());
		values.put(BookmarkDB.DB_KEY_BOOKMARK_PASSWORD, bookmark.getPassword());
		values.put(BookmarkDB.DB_KEY_BOOKMARK_DOMAIN, bookmark.getDomain());
		// insert screen and performance settings
		rowid = insertScreenSettings(db, bookmark.getScreenSettings());
		values.put(BookmarkDB.DB_KEY_SCREEN_SETTINGS, rowid);
		rowid = insertPerformanceFlags(db, bookmark.getPerformanceFlags());
		values.put(BookmarkDB.DB_KEY_PERFORMANCE_FLAGS, rowid);

		// advanced settings
		values.put(BookmarkDB.DB_KEY_BOOKMARK_3G_ENABLE,
		           bookmark.getAdvancedSettings().getEnable3GSettings());
		// insert 3G screen and 3G performance settings
		rowid = insertScreenSettings(db, bookmark.getAdvancedSettings().getScreen3G());
		values.put(BookmarkDB.DB_KEY_SCREEN_SETTINGS_3G, rowid);
		rowid = insertPerformanceFlags(db, bookmark.getAdvancedSettings().getPerformance3G());
		values.put(BookmarkDB.DB_KEY_PERFORMANCE_FLAGS_3G, rowid);
		values.put(BookmarkDB.DB_KEY_BOOKMARK_REDIRECT_SDCARD,
		           bookmark.getAdvancedSettings().getRedirectSDCard());
		values.put(BookmarkDB.DB_KEY_BOOKMARK_REDIRECT_SOUND,
		           bookmark.getAdvancedSettings().getRedirectSound());
		values.put(BookmarkDB.DB_KEY_BOOKMARK_REDIRECT_MICROPHONE,
		           bookmark.getAdvancedSettings().getRedirectMicrophone());
		values.put(BookmarkDB.DB_KEY_BOOKMARK_SECURITY,
		           bookmark.getAdvancedSettings().getSecurity());
		values.put(BookmarkDB.DB_KEY_BOOKMARK_CONSOLE_MODE,
		           bookmark.getAdvancedSettings().getConsoleMode());
		values.put(BookmarkDB.DB_KEY_BOOKMARK_REMOTE_PROGRAM,
		           bookmark.getAdvancedSettings().getRemoteProgram());
		values.put(BookmarkDB.DB_KEY_BOOKMARK_WORK_DIR,
		           bookmark.getAdvancedSettings().getWorkDir());

		values.put(BookmarkDB.DB_KEY_BOOKMARK_ASYNC_CHANNEL,
		           bookmark.getDebugSettings().getAsyncChannel());
		values.put(BookmarkDB.DB_KEY_BOOKMARK_ASYNC_UPDATE,
		           bookmark.getDebugSettings().getAsyncUpdate());
		values.put(BookmarkDB.DB_KEY_BOOKMARK_DEBUG_LEVEL,
		           bookmark.getDebugSettings().getDebugLevel());

		// add any special columns
		addBookmarkSpecificColumns(bookmark, values);

		// insert bookmark and end transaction
		db.insertOrThrow(getBookmarkTableName(), null, values);
		db.setTransactionSuccessful();
		db.endTransaction();
	}

	public boolean update(BookmarkBase bookmark)
	{
		// start a transaction
		SQLiteDatabase db = getWritableDatabase();
		db.beginTransaction();

		// bookmark settings
		ContentValues values = new ContentValues();
		values.put(BookmarkDB.DB_KEY_BOOKMARK_LABEL, bookmark.getLabel());
		values.put(BookmarkDB.DB_KEY_BOOKMARK_USERNAME, bookmark.getUsername());
		values.put(BookmarkDB.DB_KEY_BOOKMARK_PASSWORD, bookmark.getPassword());
		values.put(BookmarkDB.DB_KEY_BOOKMARK_DOMAIN, bookmark.getDomain());
		// update screen and performance settings settings
		updateScreenSettings(db, bookmark);
		updatePerformanceFlags(db, bookmark);

		// advanced settings
		values.put(BookmarkDB.DB_KEY_BOOKMARK_3G_ENABLE,
		           bookmark.getAdvancedSettings().getEnable3GSettings());
		// update 3G screen and 3G performance settings settings
		updateScreenSettings3G(db, bookmark);
		updatePerformanceFlags3G(db, bookmark);
		values.put(BookmarkDB.DB_KEY_BOOKMARK_REDIRECT_SDCARD,
		           bookmark.getAdvancedSettings().getRedirectSDCard());
		values.put(BookmarkDB.DB_KEY_BOOKMARK_REDIRECT_SOUND,
		           bookmark.getAdvancedSettings().getRedirectSound());
		values.put(BookmarkDB.DB_KEY_BOOKMARK_REDIRECT_MICROPHONE,
		           bookmark.getAdvancedSettings().getRedirectMicrophone());
		values.put(BookmarkDB.DB_KEY_BOOKMARK_SECURITY,
		           bookmark.getAdvancedSettings().getSecurity());
		values.put(BookmarkDB.DB_KEY_BOOKMARK_CONSOLE_MODE,
		           bookmark.getAdvancedSettings().getConsoleMode());
		values.put(BookmarkDB.DB_KEY_BOOKMARK_REMOTE_PROGRAM,
		           bookmark.getAdvancedSettings().getRemoteProgram());
		values.put(BookmarkDB.DB_KEY_BOOKMARK_WORK_DIR,
		           bookmark.getAdvancedSettings().getWorkDir());

		values.put(BookmarkDB.DB_KEY_BOOKMARK_ASYNC_CHANNEL,
		           bookmark.getDebugSettings().getAsyncChannel());
		values.put(BookmarkDB.DB_KEY_BOOKMARK_ASYNC_UPDATE,
		           bookmark.getDebugSettings().getAsyncUpdate());
		values.put(BookmarkDB.DB_KEY_BOOKMARK_DEBUG_LEVEL,
		           bookmark.getDebugSettings().getDebugLevel());

		addBookmarkSpecificColumns(bookmark, values);

		// update bookmark
		boolean res = (db.update(getBookmarkTableName(), values,
		                         BookmarkDB.ID + " = " + bookmark.getId(), null) == 1);

		// commit
		db.setTransactionSuccessful();
		db.endTransaction();

		return res;
	}

	public void delete(long id)
	{
		SQLiteDatabase db = getWritableDatabase();
		db.delete(getBookmarkTableName(), BookmarkDB.ID + " = " + id, null);
	}

	public BookmarkBase findById(long id)
	{
		Cursor cursor =
		    queryBookmarks(getBookmarkTableName() + "." + BookmarkDB.ID + " = " + id, null);
		if (cursor.getCount() == 0)
		{
			cursor.close();
			return null;
		}

		cursor.moveToFirst();
		BookmarkBase bookmark = getBookmarkFromCursor(cursor);
		cursor.close();
		return bookmark;
	}

	public BookmarkBase findByLabel(String label)
	{
		Cursor cursor = queryBookmarks(BookmarkDB.DB_KEY_BOOKMARK_LABEL + " = '" + label + "'",
		                               BookmarkDB.DB_KEY_BOOKMARK_LABEL);
		if (cursor.getCount() > 1)
			Log.e(TAG, "More than one bookmark with the same label found!");

		BookmarkBase bookmark = null;
		if (cursor.moveToFirst() && (cursor.getCount() > 0))
			bookmark = getBookmarkFromCursor(cursor);

		cursor.close();
		return bookmark;
	}

	public ArrayList<BookmarkBase> findByLabelLike(String pattern)
	{
		Cursor cursor =
		    queryBookmarks(BookmarkDB.DB_KEY_BOOKMARK_LABEL + " LIKE '%" + pattern + "%'",
		                   BookmarkDB.DB_KEY_BOOKMARK_LABEL);
		ArrayList<BookmarkBase> bookmarks = new ArrayList<>(cursor.getCount());

		if (cursor.moveToFirst() && (cursor.getCount() > 0))
		{
			do
			{
				bookmarks.add(getBookmarkFromCursor(cursor));
			} while (cursor.moveToNext());
		}

		cursor.close();
		return bookmarks;
	}

	public ArrayList<BookmarkBase> findAll()
	{
		Cursor cursor = queryBookmarks(null, BookmarkDB.DB_KEY_BOOKMARK_LABEL);
		final int count = cursor.getCount();
		ArrayList<BookmarkBase> bookmarks = new ArrayList<>(count);

		if (cursor.moveToFirst() && (count > 0))
		{
			do
			{
				bookmarks.add(getBookmarkFromCursor(cursor));
			} while (cursor.moveToNext());
		}

		cursor.close();
		return bookmarks;
	}

	protected Cursor queryBookmarks(String whereClause, String orderBy)
	{
		// create tables string
		final String ID = BookmarkDB.ID;
		final String tables =
		    BookmarkDB.DB_TABLE_BOOKMARK + " INNER JOIN " + BookmarkDB.DB_TABLE_SCREEN + " AS " +
		    JOIN_PREFIX + BookmarkDB.DB_KEY_SCREEN_SETTINGS + " ON " + JOIN_PREFIX +
		    BookmarkDB.DB_KEY_SCREEN_SETTINGS + "." + ID + " = " + BookmarkDB.DB_TABLE_BOOKMARK +
		    "." + BookmarkDB.DB_KEY_SCREEN_SETTINGS + " INNER JOIN " +
		    BookmarkDB.DB_TABLE_PERFORMANCE + " AS " + JOIN_PREFIX +
		    BookmarkDB.DB_KEY_PERFORMANCE_FLAGS + " ON " + JOIN_PREFIX +
		    BookmarkDB.DB_KEY_PERFORMANCE_FLAGS + "." + ID + " = " + BookmarkDB.DB_TABLE_BOOKMARK +
		    "." + BookmarkDB.DB_KEY_PERFORMANCE_FLAGS + " INNER JOIN " +
		    BookmarkDB.DB_TABLE_SCREEN + " AS " + JOIN_PREFIX +
		    BookmarkDB.DB_KEY_SCREEN_SETTINGS_3G + " ON " + JOIN_PREFIX +
		    BookmarkDB.DB_KEY_SCREEN_SETTINGS_3G + "." + ID + " = " + BookmarkDB.DB_TABLE_BOOKMARK +
		    "." + BookmarkDB.DB_KEY_SCREEN_SETTINGS_3G + " INNER JOIN " +
		    BookmarkDB.DB_TABLE_PERFORMANCE + " AS " + JOIN_PREFIX +
		    BookmarkDB.DB_KEY_PERFORMANCE_FLAGS_3G + " ON " + JOIN_PREFIX +
		    BookmarkDB.DB_KEY_PERFORMANCE_FLAGS_3G + "." + ID + " = " +
		    BookmarkDB.DB_TABLE_BOOKMARK + "." + BookmarkDB.DB_KEY_PERFORMANCE_FLAGS_3G;

		// create columns list
		ArrayList<String> columns = new ArrayList<>();
		addBookmarkColumns(columns);
		addScreenSettingsColumns(columns);
		addPerformanceFlagsColumns(columns);
		addScreenSettings3GColumns(columns);
		addPerformanceFlags3GColumns(columns);

		String[] cols = new String[columns.size()];
		columns.toArray(cols);

		SQLiteDatabase db = getReadableDatabase();
		final String query = SQLiteQueryBuilder.buildQueryString(false, tables, cols, whereClause,
		                                                         null, null, orderBy, null);
		return db.rawQuery(query, null);
	}

	private void addBookmarkColumns(ArrayList<String> columns)
	{
		columns.add(getBookmarkTableName() + "." + BookmarkDB.ID + " " + KEY_BOOKMARK_ID);
		columns.add(BookmarkDB.DB_KEY_BOOKMARK_LABEL);
		columns.add(BookmarkDB.DB_KEY_BOOKMARK_USERNAME);
		columns.add(BookmarkDB.DB_KEY_BOOKMARK_PASSWORD);
		columns.add(BookmarkDB.DB_KEY_BOOKMARK_DOMAIN);

		// advanced settings
		columns.add(BookmarkDB.DB_KEY_BOOKMARK_3G_ENABLE);
		columns.add(BookmarkDB.DB_KEY_BOOKMARK_REDIRECT_SDCARD);
		columns.add(BookmarkDB.DB_KEY_BOOKMARK_REDIRECT_SOUND);
		columns.add(BookmarkDB.DB_KEY_BOOKMARK_REDIRECT_MICROPHONE);
		columns.add(BookmarkDB.DB_KEY_BOOKMARK_SECURITY);
		columns.add(BookmarkDB.DB_KEY_BOOKMARK_CONSOLE_MODE);
		columns.add(BookmarkDB.DB_KEY_BOOKMARK_REMOTE_PROGRAM);
		columns.add(BookmarkDB.DB_KEY_BOOKMARK_WORK_DIR);

		// debug settings
		columns.add(BookmarkDB.DB_KEY_BOOKMARK_DEBUG_LEVEL);
		columns.add(BookmarkDB.DB_KEY_BOOKMARK_ASYNC_CHANNEL);
		columns.add(BookmarkDB.DB_KEY_BOOKMARK_ASYNC_UPDATE);

		addBookmarkSpecificColumns(columns);
	}

	private void addScreenSettingsColumns(ArrayList<String> columns)
	{
		columns.add(JOIN_PREFIX + BookmarkDB.DB_KEY_SCREEN_SETTINGS + "." +
		            BookmarkDB.DB_KEY_SCREEN_COLORS + " as " + KEY_SCREEN_COLORS);
		columns.add(JOIN_PREFIX + BookmarkDB.DB_KEY_SCREEN_SETTINGS + "." +
		            BookmarkDB.DB_KEY_SCREEN_RESOLUTION + " as " + KEY_SCREEN_RESOLUTION);
		columns.add(JOIN_PREFIX + BookmarkDB.DB_KEY_SCREEN_SETTINGS + "." +
		            BookmarkDB.DB_KEY_SCREEN_WIDTH + " as " + KEY_SCREEN_WIDTH);
		columns.add(JOIN_PREFIX + BookmarkDB.DB_KEY_SCREEN_SETTINGS + "." +
		            BookmarkDB.DB_KEY_SCREEN_HEIGHT + " as " + KEY_SCREEN_HEIGHT);
	}

	private void addPerformanceFlagsColumns(ArrayList<String> columns)
	{
		columns.add(JOIN_PREFIX + BookmarkDB.DB_KEY_PERFORMANCE_FLAGS + "." +
		            BookmarkDB.DB_KEY_PERFORMANCE_RFX + " as " + KEY_PERFORMANCE_RFX);
		columns.add(JOIN_PREFIX + BookmarkDB.DB_KEY_PERFORMANCE_FLAGS + "." +
		            BookmarkDB.DB_KEY_PERFORMANCE_GFX + " as " + KEY_PERFORMANCE_GFX);
		columns.add(JOIN_PREFIX + BookmarkDB.DB_KEY_PERFORMANCE_FLAGS + "." +
		            BookmarkDB.DB_KEY_PERFORMANCE_H264 + " as " + KEY_PERFORMANCE_H264);
		columns.add(JOIN_PREFIX + BookmarkDB.DB_KEY_PERFORMANCE_FLAGS + "." +
		            BookmarkDB.DB_KEY_PERFORMANCE_WALLPAPER + " as " + KEY_PERFORMANCE_WALLPAPER);
		columns.add(JOIN_PREFIX + BookmarkDB.DB_KEY_PERFORMANCE_FLAGS + "." +
		            BookmarkDB.DB_KEY_PERFORMANCE_THEME + " as " + KEY_PERFORMANCE_THEME);
		columns.add(JOIN_PREFIX + BookmarkDB.DB_KEY_PERFORMANCE_FLAGS + "." +
		            BookmarkDB.DB_KEY_PERFORMANCE_DRAG + " as " + KEY_PERFORMANCE_DRAG);
		columns.add(JOIN_PREFIX + BookmarkDB.DB_KEY_PERFORMANCE_FLAGS + "." +
		            BookmarkDB.DB_KEY_PERFORMANCE_MENU_ANIMATIONS + " as " +
		            KEY_PERFORMANCE_MENU_ANIMATIONS);
		columns.add(JOIN_PREFIX + BookmarkDB.DB_KEY_PERFORMANCE_FLAGS + "." +
		            BookmarkDB.DB_KEY_PERFORMANCE_FONTS + " as " + KEY_PERFORMANCE_FONTS);
		columns.add(JOIN_PREFIX + BookmarkDB.DB_KEY_PERFORMANCE_FLAGS + "." +
		            BookmarkDB.DB_KEY_PERFORMANCE_COMPOSITION + " " + KEY_PERFORMANCE_COMPOSITION);
	}

	private void addScreenSettings3GColumns(ArrayList<String> columns)
	{
		columns.add(JOIN_PREFIX + BookmarkDB.DB_KEY_SCREEN_SETTINGS_3G + "." +
		            BookmarkDB.DB_KEY_SCREEN_COLORS + " as " + KEY_SCREEN_COLORS_3G);
		columns.add(JOIN_PREFIX + BookmarkDB.DB_KEY_SCREEN_SETTINGS_3G + "." +
		            BookmarkDB.DB_KEY_SCREEN_RESOLUTION + " as " + KEY_SCREEN_RESOLUTION_3G);
		columns.add(JOIN_PREFIX + BookmarkDB.DB_KEY_SCREEN_SETTINGS_3G + "." +
		            BookmarkDB.DB_KEY_SCREEN_WIDTH + " as " + KEY_SCREEN_WIDTH_3G);
		columns.add(JOIN_PREFIX + BookmarkDB.DB_KEY_SCREEN_SETTINGS_3G + "." +
		            BookmarkDB.DB_KEY_SCREEN_HEIGHT + " as " + KEY_SCREEN_HEIGHT_3G);
	}

	private void addPerformanceFlags3GColumns(ArrayList<String> columns)
	{
		columns.add(JOIN_PREFIX + BookmarkDB.DB_KEY_PERFORMANCE_FLAGS_3G + "." +
		            BookmarkDB.DB_KEY_PERFORMANCE_RFX + " as " + KEY_PERFORMANCE_RFX_3G);
		columns.add(JOIN_PREFIX + BookmarkDB.DB_KEY_PERFORMANCE_FLAGS_3G + "." +
		            BookmarkDB.DB_KEY_PERFORMANCE_GFX + " as " + KEY_PERFORMANCE_GFX_3G);
		columns.add(JOIN_PREFIX + BookmarkDB.DB_KEY_PERFORMANCE_FLAGS_3G + "." +
		            BookmarkDB.DB_KEY_PERFORMANCE_H264 + " as " + KEY_PERFORMANCE_H264_3G);
		columns.add(JOIN_PREFIX + BookmarkDB.DB_KEY_PERFORMANCE_FLAGS_3G + "." +
		            BookmarkDB.DB_KEY_PERFORMANCE_WALLPAPER + " as " +
		            KEY_PERFORMANCE_WALLPAPER_3G);
		columns.add(JOIN_PREFIX + BookmarkDB.DB_KEY_PERFORMANCE_FLAGS_3G + "." +
		            BookmarkDB.DB_KEY_PERFORMANCE_THEME + " as " + KEY_PERFORMANCE_THEME_3G);
		columns.add(JOIN_PREFIX + BookmarkDB.DB_KEY_PERFORMANCE_FLAGS_3G + "." +
		            BookmarkDB.DB_KEY_PERFORMANCE_DRAG + " as " + KEY_PERFORMANCE_DRAG_3G);
		columns.add(JOIN_PREFIX + BookmarkDB.DB_KEY_PERFORMANCE_FLAGS_3G + "." +
		            BookmarkDB.DB_KEY_PERFORMANCE_MENU_ANIMATIONS + " as " +
		            KEY_PERFORMANCE_MENU_ANIMATIONS_3G);
		columns.add(JOIN_PREFIX + BookmarkDB.DB_KEY_PERFORMANCE_FLAGS_3G + "." +
		            BookmarkDB.DB_KEY_PERFORMANCE_FONTS + " as " + KEY_PERFORMANCE_FONTS_3G);
		columns.add(JOIN_PREFIX + BookmarkDB.DB_KEY_PERFORMANCE_FLAGS_3G + "." +
		            BookmarkDB.DB_KEY_PERFORMANCE_COMPOSITION + " " +
		            KEY_PERFORMANCE_COMPOSITION_3G);
	}

	protected BookmarkBase getBookmarkFromCursor(Cursor cursor)
	{
		BookmarkBase bookmark = createBookmark();
		bookmark.setId(cursor.getLong(cursor.getColumnIndex(KEY_BOOKMARK_ID)));
		bookmark.setLabel(
		    cursor.getString(cursor.getColumnIndex(BookmarkDB.DB_KEY_BOOKMARK_LABEL)));
		bookmark.setUsername(
		    cursor.getString(cursor.getColumnIndex(BookmarkDB.DB_KEY_BOOKMARK_USERNAME)));
		bookmark.setPassword(
		    cursor.getString(cursor.getColumnIndex(BookmarkDB.DB_KEY_BOOKMARK_PASSWORD)));
		bookmark.setDomain(
		    cursor.getString(cursor.getColumnIndex(BookmarkDB.DB_KEY_BOOKMARK_DOMAIN)));
		readScreenSettings(bookmark, cursor);
		readPerformanceFlags(bookmark, cursor);

		// advanced settings
		bookmark.getAdvancedSettings().setEnable3GSettings(
		    cursor.getInt(cursor.getColumnIndex(BookmarkDB.DB_KEY_BOOKMARK_3G_ENABLE)) != 0);
		readScreenSettings3G(bookmark, cursor);
		readPerformanceFlags3G(bookmark, cursor);
		bookmark.getAdvancedSettings().setRedirectSDCard(
		    cursor.getInt(cursor.getColumnIndex(BookmarkDB.DB_KEY_BOOKMARK_REDIRECT_SDCARD)) != 0);
		bookmark.getAdvancedSettings().setRedirectSound(
		    cursor.getInt(cursor.getColumnIndex(BookmarkDB.DB_KEY_BOOKMARK_REDIRECT_SOUND)));
		bookmark.getAdvancedSettings().setRedirectMicrophone(
		    cursor.getInt(cursor.getColumnIndex(BookmarkDB.DB_KEY_BOOKMARK_REDIRECT_MICROPHONE)) !=
		    0);
		bookmark.getAdvancedSettings().setSecurity(
		    cursor.getInt(cursor.getColumnIndex(BookmarkDB.DB_KEY_BOOKMARK_SECURITY)));
		bookmark.getAdvancedSettings().setConsoleMode(
		    cursor.getInt(cursor.getColumnIndex(BookmarkDB.DB_KEY_BOOKMARK_CONSOLE_MODE)) != 0);
		bookmark.getAdvancedSettings().setRemoteProgram(
		    cursor.getString(cursor.getColumnIndex(BookmarkDB.DB_KEY_BOOKMARK_REMOTE_PROGRAM)));
		bookmark.getAdvancedSettings().setWorkDir(
		    cursor.getString(cursor.getColumnIndex(BookmarkDB.DB_KEY_BOOKMARK_WORK_DIR)));

		bookmark.getDebugSettings().setAsyncChannel(
		    cursor.getInt(cursor.getColumnIndex(BookmarkDB.DB_KEY_BOOKMARK_ASYNC_CHANNEL)) == 1);
		bookmark.getDebugSettings().setAsyncUpdate(
		    cursor.getInt(cursor.getColumnIndex(BookmarkDB.DB_KEY_BOOKMARK_ASYNC_UPDATE)) == 1);
		bookmark.getDebugSettings().setDebugLevel(
		    cursor.getString(cursor.getColumnIndex(BookmarkDB.DB_KEY_BOOKMARK_DEBUG_LEVEL)));

		readBookmarkSpecificColumns(bookmark, cursor);

		return bookmark;
	}

	private void readScreenSettings(BookmarkBase bookmark, Cursor cursor)
	{
		BookmarkBase.ScreenSettings screenSettings = bookmark.getScreenSettings();
		screenSettings.setColors(cursor.getInt(cursor.getColumnIndex(KEY_SCREEN_COLORS)));
		screenSettings.setResolution(cursor.getInt(cursor.getColumnIndex(KEY_SCREEN_RESOLUTION)));
		screenSettings.setWidth(cursor.getInt(cursor.getColumnIndex(KEY_SCREEN_WIDTH)));
		screenSettings.setHeight(cursor.getInt(cursor.getColumnIndex(KEY_SCREEN_HEIGHT)));
	}

	private void readPerformanceFlags(BookmarkBase bookmark, Cursor cursor)
	{
		BookmarkBase.PerformanceFlags perfFlags = bookmark.getPerformanceFlags();
		perfFlags.setRemoteFX(cursor.getInt(cursor.getColumnIndex(KEY_PERFORMANCE_RFX)) != 0);
		perfFlags.setGfx(cursor.getInt(cursor.getColumnIndex(KEY_PERFORMANCE_GFX)) != 0);
		perfFlags.setH264(cursor.getInt(cursor.getColumnIndex(KEY_PERFORMANCE_H264)) != 0);
		perfFlags.setWallpaper(cursor.getInt(cursor.getColumnIndex(KEY_PERFORMANCE_WALLPAPER)) !=
		                       0);
		perfFlags.setTheming(cursor.getInt(cursor.getColumnIndex(KEY_PERFORMANCE_THEME)) != 0);
		perfFlags.setFullWindowDrag(cursor.getInt(cursor.getColumnIndex(KEY_PERFORMANCE_DRAG)) !=
		                            0);
		perfFlags.setMenuAnimations(
		    cursor.getInt(cursor.getColumnIndex(KEY_PERFORMANCE_MENU_ANIMATIONS)) != 0);
		perfFlags.setFontSmoothing(cursor.getInt(cursor.getColumnIndex(KEY_PERFORMANCE_FONTS)) !=
		                           0);
		perfFlags.setDesktopComposition(
		    cursor.getInt(cursor.getColumnIndex(KEY_PERFORMANCE_COMPOSITION)) != 0);
	}

	private void readScreenSettings3G(BookmarkBase bookmark, Cursor cursor)
	{
		BookmarkBase.ScreenSettings screenSettings = bookmark.getAdvancedSettings().getScreen3G();
		screenSettings.setColors(cursor.getInt(cursor.getColumnIndex(KEY_SCREEN_COLORS_3G)));
		screenSettings.setResolution(
		    cursor.getInt(cursor.getColumnIndex(KEY_SCREEN_RESOLUTION_3G)));
		screenSettings.setWidth(cursor.getInt(cursor.getColumnIndex(KEY_SCREEN_WIDTH_3G)));
		screenSettings.setHeight(cursor.getInt(cursor.getColumnIndex(KEY_SCREEN_HEIGHT_3G)));
	}

	private void readPerformanceFlags3G(BookmarkBase bookmark, Cursor cursor)
	{
		BookmarkBase.PerformanceFlags perfFlags = bookmark.getAdvancedSettings().getPerformance3G();
		perfFlags.setRemoteFX(cursor.getInt(cursor.getColumnIndex(KEY_PERFORMANCE_RFX_3G)) != 0);
		perfFlags.setGfx(cursor.getInt(cursor.getColumnIndex(KEY_PERFORMANCE_GFX_3G)) != 0);
		perfFlags.setH264(cursor.getInt(cursor.getColumnIndex(KEY_PERFORMANCE_H264_3G)) != 0);
		perfFlags.setWallpaper(cursor.getInt(cursor.getColumnIndex(KEY_PERFORMANCE_WALLPAPER_3G)) !=
		                       0);
		perfFlags.setTheming(cursor.getInt(cursor.getColumnIndex(KEY_PERFORMANCE_THEME_3G)) != 0);
		perfFlags.setFullWindowDrag(cursor.getInt(cursor.getColumnIndex(KEY_PERFORMANCE_DRAG_3G)) !=
		                            0);
		perfFlags.setMenuAnimations(
		    cursor.getInt(cursor.getColumnIndex(KEY_PERFORMANCE_MENU_ANIMATIONS_3G)) != 0);
		perfFlags.setFontSmoothing(cursor.getInt(cursor.getColumnIndex(KEY_PERFORMANCE_FONTS_3G)) !=
		                           0);
		perfFlags.setDesktopComposition(
		    cursor.getInt(cursor.getColumnIndex(KEY_PERFORMANCE_COMPOSITION_3G)) != 0);
	}

	private void fillScreenSettingsContentValues(BookmarkBase.ScreenSettings settings,
	                                             ContentValues values)
	{
		values.put(BookmarkDB.DB_KEY_SCREEN_COLORS, settings.getColors());
		values.put(BookmarkDB.DB_KEY_SCREEN_RESOLUTION, settings.getResolution());
		values.put(BookmarkDB.DB_KEY_SCREEN_WIDTH, settings.getWidth());
		values.put(BookmarkDB.DB_KEY_SCREEN_HEIGHT, settings.getHeight());
	}

	private void fillPerformanceFlagsContentValues(BookmarkBase.PerformanceFlags perfFlags,
	                                               ContentValues values)
	{
		values.put(BookmarkDB.DB_KEY_PERFORMANCE_RFX, perfFlags.getRemoteFX());
		values.put(BookmarkDB.DB_KEY_PERFORMANCE_GFX, perfFlags.getGfx());
		values.put(BookmarkDB.DB_KEY_PERFORMANCE_H264, perfFlags.getH264());
		values.put(BookmarkDB.DB_KEY_PERFORMANCE_WALLPAPER, perfFlags.getWallpaper());
		values.put(BookmarkDB.DB_KEY_PERFORMANCE_THEME, perfFlags.getTheming());
		values.put(BookmarkDB.DB_KEY_PERFORMANCE_DRAG, perfFlags.getFullWindowDrag());
		values.put(BookmarkDB.DB_KEY_PERFORMANCE_MENU_ANIMATIONS, perfFlags.getMenuAnimations());
		values.put(BookmarkDB.DB_KEY_PERFORMANCE_FONTS, perfFlags.getFontSmoothing());
		values.put(BookmarkDB.DB_KEY_PERFORMANCE_COMPOSITION, perfFlags.getDesktopComposition());
	}

	private long insertScreenSettings(SQLiteDatabase db, BookmarkBase.ScreenSettings settings)
	{
		ContentValues values = new ContentValues();
		fillScreenSettingsContentValues(settings, values);
		return db.insertOrThrow(BookmarkDB.DB_TABLE_SCREEN, null, values);
	}

	private boolean updateScreenSettings(SQLiteDatabase db, BookmarkBase bookmark)
	{
		ContentValues values = new ContentValues();
		fillScreenSettingsContentValues(bookmark.getScreenSettings(), values);
		String whereClause = BookmarkDB.ID + " IN "
		                     + "(SELECT " + BookmarkDB.DB_KEY_SCREEN_SETTINGS + " FROM " +
		                     getBookmarkTableName() + " WHERE " + BookmarkDB.ID + " =  " +
		                     bookmark.getId() + ");";
		return (db.update(BookmarkDB.DB_TABLE_SCREEN, values, whereClause, null) == 1);
	}

	private boolean updateScreenSettings3G(SQLiteDatabase db, BookmarkBase bookmark)
	{
		ContentValues values = new ContentValues();
		fillScreenSettingsContentValues(bookmark.getAdvancedSettings().getScreen3G(), values);
		String whereClause = BookmarkDB.ID + " IN "
		                     + "(SELECT " + BookmarkDB.DB_KEY_SCREEN_SETTINGS_3G + " FROM " +
		                     getBookmarkTableName() + " WHERE " + BookmarkDB.ID + " =  " +
		                     bookmark.getId() + ");";
		return (db.update(BookmarkDB.DB_TABLE_SCREEN, values, whereClause, null) == 1);
	}

	private long insertPerformanceFlags(SQLiteDatabase db, BookmarkBase.PerformanceFlags perfFlags)
	{
		ContentValues values = new ContentValues();
		fillPerformanceFlagsContentValues(perfFlags, values);
		return db.insertOrThrow(BookmarkDB.DB_TABLE_PERFORMANCE, null, values);
	}

	private boolean updatePerformanceFlags(SQLiteDatabase db, BookmarkBase bookmark)
	{
		ContentValues values = new ContentValues();
		fillPerformanceFlagsContentValues(bookmark.getPerformanceFlags(), values);
		String whereClause = BookmarkDB.ID + " IN "
		                     + "(SELECT " + BookmarkDB.DB_KEY_PERFORMANCE_FLAGS + " FROM " +
		                     getBookmarkTableName() + " WHERE " + BookmarkDB.ID + " =  " +
		                     bookmark.getId() + ");";
		return (db.update(BookmarkDB.DB_TABLE_PERFORMANCE, values, whereClause, null) == 1);
	}

	private boolean updatePerformanceFlags3G(SQLiteDatabase db, BookmarkBase bookmark)
	{
		ContentValues values = new ContentValues();
		fillPerformanceFlagsContentValues(bookmark.getAdvancedSettings().getPerformance3G(),
		                                  values);
		String whereClause = BookmarkDB.ID + " IN "
		                     + "(SELECT " + BookmarkDB.DB_KEY_PERFORMANCE_FLAGS_3G + " FROM " +
		                     getBookmarkTableName() + " WHERE " + BookmarkDB.ID + " =  " +
		                     bookmark.getId() + ");";
		return (db.update(BookmarkDB.DB_TABLE_PERFORMANCE, values, whereClause, null) == 1);
	}

	// safety wrappers
	// in case of getReadableDatabase it could happen that upgradeDB gets called which is
	// a problem if the DB is only readable
	private SQLiteDatabase getWritableDatabase()
	{
		return bookmarkDB.getWritableDatabase();
	}

	private SQLiteDatabase getReadableDatabase()
	{
		SQLiteDatabase db;
		try
		{
			db = bookmarkDB.getReadableDatabase();
		}
		catch (SQLiteException e)
		{
			db = bookmarkDB.getWritableDatabase();
		}
		return db;
	}
}
