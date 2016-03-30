/*
   Helper class to access bookmark database

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.services;


import java.util.ArrayList;

import com.freerdp.freerdpcore.domain.BookmarkBase;

import android.content.ContentValues;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteException;
import android.database.sqlite.SQLiteOpenHelper;
import android.database.sqlite.SQLiteQueryBuilder;
import android.util.Log;

public abstract class BookmarkBaseGateway
{
	private final static String TAG = "BookmarkBaseGateway";
	private SQLiteOpenHelper bookmarkDB;

	protected abstract BookmarkBase createBookmark();
	protected abstract String getBookmarkTableName();
	protected abstract void addBookmarkSpecificColumns(ArrayList<String> columns); 
	protected abstract void addBookmarkSpecificColumns(BookmarkBase bookmark, ContentValues columns);
	protected abstract void readBookmarkSpecificColumns(BookmarkBase bookmark, Cursor cursor);
	
	public BookmarkBaseGateway(SQLiteOpenHelper bookmarkDB)
	{
		this.bookmarkDB = bookmarkDB;
	}
	
	public void insert(BookmarkBase bookmark)
	{
		// begin transaction
		SQLiteDatabase db = getWritableDatabase();
		db.beginTransaction();

		long rowid;
		ContentValues values = new ContentValues();
		values.put("label", bookmark.getLabel());
		values.put("username", bookmark.getUsername());
		values.put("password", bookmark.getPassword());
		values.put("domain", bookmark.getDomain());
		// insert screen and performance settings
		rowid = insertScreenSettings(db, bookmark.getScreenSettings());
		values.put("screen_settings", rowid);
		rowid = insertPerformanceFlags(db, bookmark.getPerformanceFlags());
		values.put("performance_flags", rowid);

		// advanced settings
		values.put("enable_3g_settings", bookmark.getAdvancedSettings().getEnable3GSettings());
		// insert 3G screen and 3G performance settings
		rowid = insertScreenSettings(db, bookmark.getAdvancedSettings().getScreen3G());
		values.put("screen_3g", rowid);
		rowid = insertPerformanceFlags(db, bookmark.getAdvancedSettings().getPerformance3G());
		values.put("performance_3g", rowid);
		values.put("redirect_sdcard", bookmark.getAdvancedSettings().getRedirectSDCard());
		values.put("redirect_sound", bookmark.getAdvancedSettings().getRedirectSound());
		values.put("redirect_microphone", bookmark.getAdvancedSettings().getRedirectMicrophone());
		values.put("security", bookmark.getAdvancedSettings().getSecurity());
		values.put("console_mode", bookmark.getAdvancedSettings().getConsoleMode());
		values.put("remote_program", bookmark.getAdvancedSettings().getRemoteProgram());
		values.put("work_dir", bookmark.getAdvancedSettings().getWorkDir());
		
		values.put("async_channel", bookmark.getDebugSettings().getAsyncChannel());
		values.put("async_transport", bookmark.getDebugSettings().getAsyncTransport());
		values.put("async_input", bookmark.getDebugSettings().getAsyncInput());
		values.put("async_update", bookmark.getDebugSettings().getAsyncUpdate());
		values.put("debug_level", bookmark.getDebugSettings().getDebugLevel());

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
		values.put("label", bookmark.getLabel());
		values.put("username", bookmark.getUsername());
		values.put("password", bookmark.getPassword());
		values.put("domain", bookmark.getDomain());
		// update screen and performance settings settings
		updateScreenSettings(db, bookmark);
		updatePerformanceFlags(db, bookmark);
		
		// advanced settings
		values.put("enable_3g_settings", bookmark.getAdvancedSettings().getEnable3GSettings());
		// update 3G screen and 3G performance settings settings
		updateScreenSettings3G(db, bookmark);
		updatePerformanceFlags3G(db, bookmark);
		values.put("redirect_sdcard", bookmark.getAdvancedSettings().getRedirectSDCard());
		values.put("redirect_sound", bookmark.getAdvancedSettings().getRedirectSound());
		values.put("redirect_microphone", bookmark.getAdvancedSettings().getRedirectMicrophone());
		values.put("security", bookmark.getAdvancedSettings().getSecurity());
		values.put("console_mode", bookmark.getAdvancedSettings().getConsoleMode());
		values.put("remote_program", bookmark.getAdvancedSettings().getRemoteProgram());
		values.put("work_dir", bookmark.getAdvancedSettings().getWorkDir());
		
		values.put("async_channel", bookmark.getDebugSettings().getAsyncChannel());
		values.put("async_transport", bookmark.getDebugSettings().getAsyncTransport());
		values.put("async_input", bookmark.getDebugSettings().getAsyncInput());
		values.put("async_update", bookmark.getDebugSettings().getAsyncUpdate());
		values.put("debug_level", bookmark.getDebugSettings().getDebugLevel());
		
		addBookmarkSpecificColumns(bookmark, values);
				
		// update bookmark
		boolean res = (db.update(getBookmarkTableName(), values, BookmarkDB.ID + " = " + bookmark.getId(), null) == 1);
		
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
		Cursor cursor = queryBookmarks(getBookmarkTableName() + "." + BookmarkDB.ID + " = " + id, null);
		if(cursor.getCount() == 0)
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
		Cursor cursor = queryBookmarks("label = '" + label + "'", "label");
		if(cursor.getCount() > 1)
			Log.e(TAG, "More than one bookmark with the same label found!");

		BookmarkBase bookmark = null;
		if(cursor.moveToFirst())
			bookmark = getBookmarkFromCursor(cursor);						
		
		cursor.close();	
		return bookmark;
	}

	public ArrayList<BookmarkBase> findByLabelLike(String pattern) 
	{	
		Cursor cursor = queryBookmarks("label LIKE '%" + pattern + "%'", "label");
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
	
	public ArrayList<BookmarkBase> findAll()
	{
		Cursor cursor = queryBookmarks(null, "label");
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

	protected Cursor queryBookmarks(String whereClause, String orderBy)
	{
		// create tables string		
		String ID = BookmarkDB.ID;
		String bmTable = getBookmarkTableName();		
		String tables =  bmTable + " INNER JOIN tbl_screen_settings AS join_screen_settings ON join_screen_settings." + ID + " = " + bmTable + ".screen_settings" +
								   " INNER JOIN tbl_performance_flags AS join_performance_flags ON join_performance_flags." + ID + " = " + bmTable + ".performance_flags" +
								   " INNER JOIN tbl_screen_settings AS join_screen_3G ON join_screen_3G." + ID + " = " + bmTable + ".screen_3g" +
								   " INNER JOIN tbl_performance_flags AS join_performance_3G ON join_performance_3G." + ID + " = " + bmTable + ".performance_3g";
								   
		// create columns list
		ArrayList<String> columns = new ArrayList<String>(10);
		addBookmarkColumns(columns);
		addScreenSettingsColumns(columns);
		addPerformanceFlagsColumns(columns);
		addScreenSettings3GColumns(columns);
		addPerformanceFlags3GColumns(columns);

		String[] cols = new String[columns.size()];
		SQLiteDatabase db = getReadableDatabase();
		return db.rawQuery(SQLiteQueryBuilder.buildQueryString(false, tables, columns.toArray(cols), whereClause, null, null, orderBy, null), null);				
	}
	
	private void addBookmarkColumns(ArrayList<String> columns) {
		columns.add(getBookmarkTableName() + "." + BookmarkDB.ID + " bookmarkId");
		columns.add("label");
		columns.add("username");
		columns.add("password");
		columns.add("domain");
		
		// advanced settings
		columns.add("enable_3g_settings");
		columns.add("redirect_sdcard");
		columns.add("redirect_sound");
		columns.add("redirect_microphone");
		columns.add("security");
		columns.add("console_mode");
		columns.add("remote_program");
		columns.add("work_dir");
		
		// debug settings
		columns.add("debug_level");
		columns.add("async_channel");
		columns.add("async_transport");
		columns.add("async_update");
		columns.add("async_input");
		
		addBookmarkSpecificColumns(columns);		
	}

	private void addScreenSettingsColumns(ArrayList<String> columns) {
		columns.add("join_screen_settings.colors as screenColors");
		columns.add("join_screen_settings.resolution as screenResolution");
		columns.add("join_screen_settings.width as screenWidth");
		columns.add("join_screen_settings.height as screenHeight");
	}
	
	private void addPerformanceFlagsColumns(ArrayList<String> columns) {
		columns.add("join_performance_flags.perf_remotefx as performanceRemoteFX");
		columns.add("join_performance_flags.perf_gfx as performanceGfx");
		columns.add("join_performance_flags.perf_gfx_h264 as performanceGfxH264");
		columns.add("join_performance_flags.perf_wallpaper as performanceWallpaper");
		columns.add("join_performance_flags.perf_theming as performanceTheming");
		columns.add("join_performance_flags.perf_full_window_drag as performanceFullWindowDrag");
		columns.add("join_performance_flags.perf_menu_animations as performanceMenuAnimations");
		columns.add("join_performance_flags.perf_font_smoothing as performanceFontSmoothing");
		columns.add("join_performance_flags.perf_desktop_composition performanceDesktopComposition");
	}		

	private void addScreenSettings3GColumns(ArrayList<String> columns) {
		columns.add("join_screen_3G.colors as screenColors3G");
		columns.add("join_screen_3G.resolution as screenResolution3G");
		columns.add("join_screen_3G.width as screenWidth3G");
		columns.add("join_screen_3G.height as screenHeight3G");
	}
	
	private void addPerformanceFlags3GColumns(ArrayList<String> columns) {
		columns.add("join_performance_3G.perf_remotefx as performanceRemoteFX3G");
		columns.add("join_performance_3G.perf_gfx as performanceGfx3G");
		columns.add("join_performance_3G.perf_gfx_h264 as performanceGfxH2643G");
		columns.add("join_performance_3G.perf_wallpaper as performanceWallpaper3G");
		columns.add("join_performance_3G.perf_theming as performanceTheming3G");
		columns.add("join_performance_3G.perf_full_window_drag as performanceFullWindowDrag3G");
		columns.add("join_performance_3G.perf_menu_animations as performanceMenuAnimations3G");
		columns.add("join_performance_3G.perf_font_smoothing as performanceFontSmoothing3G");
		columns.add("join_performance_3G.perf_desktop_composition performanceDesktopComposition3G");
	}		
	
	protected BookmarkBase getBookmarkFromCursor(Cursor cursor)
	{
		BookmarkBase bookmark = createBookmark();		
		bookmark.setId(cursor.getLong(cursor.getColumnIndex("bookmarkId")));
		bookmark.setLabel(cursor.getString(cursor.getColumnIndex("label")));
		bookmark.setUsername(cursor.getString(cursor.getColumnIndex("username")));
		bookmark.setPassword(cursor.getString(cursor.getColumnIndex("password")));
		bookmark.setDomain(cursor.getString(cursor.getColumnIndex("domain")));
		readScreenSettings(bookmark, cursor);		
		readPerformanceFlags(bookmark, cursor);		

		// advanced settings
		bookmark.getAdvancedSettings().setEnable3GSettings(cursor.getInt(cursor.getColumnIndex("enable_3g_settings")) == 0 ? false : true);		
		readScreenSettings3G(bookmark, cursor);		
		readPerformanceFlags3G(bookmark, cursor);		
		bookmark.getAdvancedSettings().setRedirectSDCard(cursor.getInt(cursor.getColumnIndex("redirect_sdcard")) == 0 ? false : true);		
		bookmark.getAdvancedSettings().setRedirectSound(cursor.getInt(cursor.getColumnIndex("redirect_sound")));		
		bookmark.getAdvancedSettings().setRedirectMicrophone(cursor.getInt(cursor.getColumnIndex("redirect_microphone")) == 0 ? false : true);		
		bookmark.getAdvancedSettings().setSecurity(cursor.getInt(cursor.getColumnIndex("security")));		
		bookmark.getAdvancedSettings().setConsoleMode(cursor.getInt(cursor.getColumnIndex("console_mode")) == 0 ? false : true);		
		bookmark.getAdvancedSettings().setRemoteProgram(cursor.getString(cursor.getColumnIndex("remote_program")));		
		bookmark.getAdvancedSettings().setWorkDir(cursor.getString(cursor.getColumnIndex("work_dir")));		
		
		bookmark.getDebugSettings().setAsyncChannel(
				cursor.getInt(cursor.getColumnIndex("async_channel")) == 1 ? true : false);		
		bookmark.getDebugSettings().setAsyncTransport(
				cursor.getInt(cursor.getColumnIndex("async_transport")) == 1 ? true : false);		
		bookmark.getDebugSettings().setAsyncInput(
				cursor.getInt(cursor.getColumnIndex("async_input")) == 1 ? true : false);		
		bookmark.getDebugSettings().setAsyncUpdate(
				cursor.getInt(cursor.getColumnIndex("async_update")) == 1 ? true : false);		
		bookmark.getDebugSettings().setDebugLevel(cursor.getInt(cursor.getColumnIndex("debug_level")));	
	
		readBookmarkSpecificColumns(bookmark, cursor);

		return bookmark;
	}
		
	private void readScreenSettings(BookmarkBase bookmark, Cursor cursor) {
		BookmarkBase.ScreenSettings screenSettings = bookmark.getScreenSettings();
		screenSettings.setColors(cursor.getInt(cursor.getColumnIndex("screenColors")));
		screenSettings.setResolution(cursor.getInt(cursor.getColumnIndex("screenResolution")));
		screenSettings.setWidth(cursor.getInt(cursor.getColumnIndex("screenWidth")));
		screenSettings.setHeight(cursor.getInt(cursor.getColumnIndex("screenHeight")));		
	}
	
	private void readPerformanceFlags(BookmarkBase bookmark, Cursor cursor) {
		BookmarkBase.PerformanceFlags perfFlags = bookmark.getPerformanceFlags();
		perfFlags.setRemoteFX(cursor.getInt(cursor.getColumnIndex("performanceRemoteFX")) == 0 ? false : true);
		perfFlags.setGfx(cursor.getInt(cursor.getColumnIndex("performanceGfx")) == 0 ? false :
				true);
		perfFlags.setH264(cursor.getInt(cursor.getColumnIndex("performanceGfxH264")) == 0 ?
				false :
				true);
		perfFlags.setWallpaper(cursor.getInt(cursor.getColumnIndex("performanceWallpaper")) == 0 ? false : true);
		perfFlags.setTheming(cursor.getInt(cursor.getColumnIndex("performanceTheming")) == 0 ? false : true);
		perfFlags.setFullWindowDrag(cursor.getInt(cursor.getColumnIndex("performanceFullWindowDrag")) == 0 ? false : true);
		perfFlags.setMenuAnimations(cursor.getInt(cursor.getColumnIndex("performanceMenuAnimations")) == 0 ? false : true);
		perfFlags.setFontSmoothing(cursor.getInt(cursor.getColumnIndex("performanceFontSmoothing")) == 0 ? false : true);
		perfFlags.setDesktopComposition(cursor.getInt(cursor.getColumnIndex("performanceDesktopComposition")) == 0 ? false : true);
	}		

	private void readScreenSettings3G(BookmarkBase bookmark, Cursor cursor) {
		BookmarkBase.ScreenSettings screenSettings = bookmark.getAdvancedSettings().getScreen3G();
		screenSettings.setColors(cursor.getInt(cursor.getColumnIndex("screenColors3G")));
		screenSettings.setResolution(cursor.getInt(cursor.getColumnIndex("screenResolution3G")));
		screenSettings.setWidth(cursor.getInt(cursor.getColumnIndex("screenWidth3G")));
		screenSettings.setHeight(cursor.getInt(cursor.getColumnIndex("screenHeight3G")));		
	}
	
	private void readPerformanceFlags3G(BookmarkBase bookmark, Cursor cursor) {
		BookmarkBase.PerformanceFlags perfFlags = bookmark.getAdvancedSettings().getPerformance3G();
		perfFlags.setRemoteFX(cursor.getInt(cursor.getColumnIndex("performanceRemoteFX3G")) == 0 ? false : true);
		perfFlags.setGfx(cursor.getInt(cursor.getColumnIndex("performanceGfx3G")) == 0 ? false :
				true);
		perfFlags.setH264(cursor.getInt(cursor.getColumnIndex("performanceGfxH2643G")) == 0 ?
				false :
				true);
		perfFlags.setWallpaper(cursor.getInt(cursor.getColumnIndex("performanceWallpaper3G")) == 0 ? false : true);
		perfFlags.setTheming(cursor.getInt(cursor.getColumnIndex("performanceTheming3G")) == 0 ? false : true);
		perfFlags.setFullWindowDrag(cursor.getInt(cursor.getColumnIndex("performanceFullWindowDrag3G")) == 0 ? false : true);
		perfFlags.setMenuAnimations(cursor.getInt(cursor.getColumnIndex("performanceMenuAnimations3G")) == 0 ? false : true);
		perfFlags.setFontSmoothing(cursor.getInt(cursor.getColumnIndex("performanceFontSmoothing3G")) == 0 ? false : true);
		perfFlags.setDesktopComposition(cursor.getInt(cursor.getColumnIndex("performanceDesktopComposition3G")) == 0 ? false : true);
	}		

	private void fillScreenSettingsContentValues(BookmarkBase.ScreenSettings settings, ContentValues values)
	{
		values.put("colors", settings.getColors());
		values.put("resolution", settings.getResolution());
		values.put("width", settings.getWidth());
		values.put("height", settings.getHeight());
	}

	private void fillPerformanceFlagsContentValues(BookmarkBase.PerformanceFlags perfFlags, ContentValues values)
	{
		values.put("perf_remotefx", perfFlags.getRemoteFX());
		values.put("perf_gfx", perfFlags.getGfx());
		values.put("perf_gfx_h264", perfFlags.getH264());
		values.put("perf_wallpaper", perfFlags.getWallpaper());
		values.put("perf_theming", perfFlags.getTheming());
		values.put("perf_full_window_drag", perfFlags.getFullWindowDrag());
		values.put("perf_menu_animations", perfFlags.getMenuAnimations());
		values.put("perf_font_smoothing", perfFlags.getFontSmoothing());
		values.put("perf_desktop_composition", perfFlags.getDesktopComposition());		
	}
	
	private long insertScreenSettings(SQLiteDatabase db, BookmarkBase.ScreenSettings settings) 
	{		
		ContentValues values = new ContentValues();		
		fillScreenSettingsContentValues(settings, values);
		return db.insertOrThrow("tbl_screen_settings", null, values);
	}

	private boolean updateScreenSettings(SQLiteDatabase db, BookmarkBase bookmark) 
	{
		ContentValues values = new ContentValues();		
		fillScreenSettingsContentValues(bookmark.getScreenSettings(), values);
		String whereClause = BookmarkDB.ID + " IN " + "(SELECT screen_settings FROM " + getBookmarkTableName() + " WHERE " + BookmarkDB.ID + " =  " + bookmark.getId() + ");";
		return (db.update("tbl_screen_settings", values, whereClause, null) == 1);		
	}

	private boolean updateScreenSettings3G(SQLiteDatabase db, BookmarkBase bookmark) 
	{
		ContentValues values = new ContentValues();		
		fillScreenSettingsContentValues(bookmark.getAdvancedSettings().getScreen3G(), values);
		String whereClause = BookmarkDB.ID + " IN " + "(SELECT screen_3g FROM " + getBookmarkTableName() + " WHERE " + BookmarkDB.ID + " =  " + bookmark.getId() + ");";
		return (db.update("tbl_screen_settings", values, whereClause, null) == 1);		
	}

	private long insertPerformanceFlags(SQLiteDatabase db, BookmarkBase.PerformanceFlags perfFlags) 
	{		
		ContentValues values = new ContentValues();		
		fillPerformanceFlagsContentValues(perfFlags, values);
		return db.insertOrThrow("tbl_performance_flags", null, values);
	}

	private boolean updatePerformanceFlags(SQLiteDatabase db, BookmarkBase bookmark) 
	{
		ContentValues values = new ContentValues();		
		fillPerformanceFlagsContentValues(bookmark.getPerformanceFlags(), values);
		String whereClause = BookmarkDB.ID + " IN " + "(SELECT performance_flags FROM " + getBookmarkTableName() + " WHERE " + BookmarkDB.ID + " =  " + bookmark.getId() + ");";
		return (db.update("tbl_performance_flags", values, whereClause, null) == 1);		
	}

	private boolean updatePerformanceFlags3G(SQLiteDatabase db, BookmarkBase bookmark) 
	{
		ContentValues values = new ContentValues();		
		fillPerformanceFlagsContentValues(bookmark.getAdvancedSettings().getPerformance3G(), values);
		String whereClause = BookmarkDB.ID + " IN " + "(SELECT performance_3g FROM " + getBookmarkTableName() + " WHERE " + BookmarkDB.ID + " =  " + bookmark.getId() + ");";
		return (db.update("tbl_performance_flags", values, whereClause, null) == 1);		
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
		try {
			db = bookmarkDB.getReadableDatabase();			
		}
		catch(SQLiteException e) {
			db = bookmarkDB.getWritableDatabase();
		}
		return db;		
	}
}
