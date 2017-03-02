/*
   Android Bookmark Database

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.services;

import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.provider.BaseColumns;
import android.util.Log;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class BookmarkDB extends SQLiteOpenHelper {
    public static final String ID = BaseColumns._ID;
    private static final int DB_VERSION = 8;
    private static final String DB_NAME = "bookmarks.db";
    private static final String DB_TABLE_BOOKMARK = "tbl_manual_bookmarks";
    private static final String DB_TABLE_SCREEN = "tbl_screen_settings";
    private static final String DB_TABLE_PERFORMANCE = "tbl_performance_flags";

    public BookmarkDB(Context context) {
        super(context, DB_NAME, null, DB_VERSION);
    }

    private static List<String> GetColumns(SQLiteDatabase db, String tableName) {
        List<String> ar = null;
        Cursor c = null;
        try {
            c = db.rawQuery("SELECT * FROM " + tableName + " LIMIT 1", null);
            if (c != null) {
                ar = new ArrayList<String>(Arrays.asList(c.getColumnNames()));
            }
        } catch (Exception e) {
            Log.v(tableName, e.getMessage(), e);
            e.printStackTrace();
        } finally {
            if (c != null)
                c.close();
        }
        return ar;
    }

    private static String joinStrings(List<String> list, String delim) {
        StringBuilder buf = new StringBuilder();
        int num = list.size();
        for (int i = 0; i < num; i++) {
            if (i != 0)
                buf.append(delim);
            buf.append((String) list.get(i));
        }
        return buf.toString();
    }

    @Override
    public void onCreate(SQLiteDatabase db) {
        final String sqlScreenSettings =
                "CREATE TABLE " + DB_TABLE_SCREEN + " ("
                        + ID + " INTEGER PRIMARY KEY, "
                        + "colors INTEGER DEFAULT 16, "
                        + "resolution INTEGER DEFAULT 0, "
                        + "width, "
                        + "height);";

        db.execSQL(sqlScreenSettings);

        final String sqlPerformanceFlags =
                "CREATE TABLE " + DB_TABLE_PERFORMANCE + " ("
                        + ID + " INTEGER PRIMARY KEY, "
                        + "perf_remotefx INTEGER, "
                        + "perf_gfx INTEGER, "
                        + "perf_gfx_h264 INTEGER, "
                        + "perf_wallpaper INTEGER, "
                        + "perf_theming INTEGER, "
                        + "perf_full_window_drag INTEGER, "
                        + "perf_menu_animations INTEGER, "
                        + "perf_font_smoothing INTEGER, "
                        + "perf_desktop_composition INTEGER);";

        db.execSQL(sqlPerformanceFlags);

        final String sqlManualBookmarks = getManualBookmarksCreationString();
        db.execSQL(sqlManualBookmarks);


        // Insert a test entry
        final String sqlInsertDefaultScreenEntry =
                "INSERT INTO " + DB_TABLE_SCREEN + " ("
                        + "colors, "
                        + "resolution, "
                        + "width, "
                        + "height) "
                        + "VALUES ( "
                        + "32, 1, 1024, 768);";
        db.execSQL(sqlInsertDefaultScreenEntry);

        final String sqlInsertDefaultPerfFlags =
                "INSERT INTO " + DB_TABLE_PERFORMANCE + " ("
                        + "perf_remotefx, "
                        + "perf_gfx, "
                        + "perf_gfx_h264, "
                        + "perf_wallpaper, "
                        + "perf_theming, "
                        + "perf_full_window_drag, "
                        + "perf_menu_animations, "
                        + "perf_font_smoothing, "
                        + "perf_desktop_composition) "
                        + "VALUES ( "
                        + "1, 1, 1, 0, 0, 0, 0, 0, 0);";
        db.execSQL(sqlInsertDefaultPerfFlags);

        final String sqlInsertDefaultSessionEntry =
                "INSERT INTO " + DB_TABLE_BOOKMARK + " ("
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
                        + "redirect_sdcard, "
                        + "redirect_sound, "
                        + "redirect_microphone, "
                        + "security, "
                        + "remote_program, "
                        + "work_dir, "
                        + "async_channel, "
                        + "async_transport, "
                        + "async_input, "
                        + "async_update, "
                        + "console_mode, "
                        + "debug_level ) "
                        + "VALUES ( "
                        + "'Test Server', "
                        + "'testservice.afreerdp.com', "
                        + "'', "
                        + "'', "
                        + "'', "
                        + "3389, "
                        + "1, 1, 2, 2, 0, 0, 0, 0, "
                        + "'', '', "
                        + "1, 1, 1, 1, 0, 'INFO');";
        db.execSQL(sqlInsertDefaultSessionEntry);
    }

    private String getManualBookmarksCreationString() {
        return (
                "CREATE TABLE IF NOT EXISTS " + DB_TABLE_BOOKMARK + " ("
                        + ID + " INTEGER PRIMARY KEY, "
                        + "label TEXT NOT NULL, "
                        + "hostname TEXT NOT NULL, "
                        + "username TEXT NOT NULL, "
                        + "password TEXT, "
                        + "domain TEXT, "
                        + "port TEXT, "
                        + "screen_settings INTEGER NOT NULL, "
                        + "performance_flags INTEGER NOT NULL, "

                        + "enable_gateway_settings INTEGER DEFAULT 0, "
                        + "gateway_hostname TEXT, "
                        + "gateway_port INTEGER DEFAULT 443, "
                        + "gateway_username TEXT, "
                        + "gateway_password TEXT, "
                        + "gateway_domain TEXT, "

                        + "enable_3g_settings INTEGER DEFAULT 0, "
                        + "screen_3g INTEGER NOT NULL, "
                        + "performance_3g INTEGER NOT NULL, "
                        + "redirect_sdcard INTEGER DEFAULT 0, "
                        + "redirect_sound INTEGER DEFAULT 0, "
                        + "redirect_microphone INTEGER DEFAULT 0, "
                        + "security INTEGER, "
                        + "remote_program TEXT, "
                        + "work_dir TEXT, "
                        + "async_channel INTEGER DEFAULT 0, "
                        + "async_transport INTEGER DEFAULT 0, "
                        + "async_input INTEGER DEFAULT 0, "
                        + "async_update INTEGER DEFAULT 0, "
                        + "console_mode INTEGER, "
                        + "debug_level TEXT DEFAULT 'INFO', "

                        + "FOREIGN KEY(screen_settings) REFERENCES " + DB_TABLE_SCREEN + "(" + ID + "), "
                        + "FOREIGN KEY(performance_flags) REFERENCES " + DB_TABLE_PERFORMANCE + "(" + ID + "), "
                        + "FOREIGN KEY(screen_3g) REFERENCES " + DB_TABLE_SCREEN + "(" + ID + "), "
                        + "FOREIGN KEY(performance_3g) REFERENCES " + DB_TABLE_PERFORMANCE + "(" + ID + ") "

                        + ");");
    }

    private void recreateDB(SQLiteDatabase db) {
        db.beginTransaction();
        try {
            List<String> tables = new ArrayList<>();
            Cursor c = db.rawQuery("SELECT name FROM sqlite_master WHERE type='table'", null);
            try {
                if (c.moveToFirst()) {
                    while (!c.isAfterLast()) {
                        final String name = c.getString(c.getColumnIndex("name"));
                        tables.add(name);
                        c.moveToNext();
                    }
                }
            } finally {
                c.close();
            }
            for (String table : tables) {
                db.execSQL("DROP TABLE IF EXISTS " + table);
            }
            onCreate(db);
        } finally {
            db.endTransaction();
        }
    }

    private void upgradeDB(SQLiteDatabase db) {
        db.beginTransaction();
        try {
            // run a table creation with if not exists (we are doing an upgrade, so the table might
            // not exists yet, it will fail alter and drop)
            db.execSQL(getManualBookmarksCreationString());
            // put in a list the existing columns
            List<String> columns = GetColumns(db, DB_TABLE_BOOKMARK);
            // backup table
            db.execSQL("ALTER TABLE " + DB_TABLE_BOOKMARK + " RENAME TO 'temp_" + DB_TABLE_BOOKMARK + "'");
            // create new table (with new scheme)
            db.execSQL(getManualBookmarksCreationString());
            // get the intersection with the new columns, this time columns taken from the upgraded table
            columns.retainAll(GetColumns(db, DB_TABLE_BOOKMARK));
            // restore data
            String cols = joinStrings(columns, ",");
            db.execSQL(String.format("INSERT INTO %s (%s) SELECT %s from 'temp_%s", DB_TABLE_BOOKMARK, cols, cols, DB_TABLE_BOOKMARK + "'"));
            // remove backup table
            db.execSQL("DROP table 'temp_" + DB_TABLE_BOOKMARK + "'");

            db.setTransactionSuccessful();
        } finally {
            db.endTransaction();
        }
    }

    // from http://stackoverflow.com/questions/3424156/upgrade-sqlite-database-from-one-version-to-another
    @Override
    public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
        switch (newVersion) {
            default:
                recreateDB(db);
                break;
        }
    }

    @Override
    public void onDowngrade(SQLiteDatabase db, int oldVersion, int newVersion) {
        recreateDB(db);
    }

}
