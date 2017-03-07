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
    private static final String DB_BACKUP_PREFIX = "temp_";
    private static final String DB_NAME = "bookmarks.db";
    private static final String DB_TABLE_BOOKMARK = "tbl_manual_bookmarks";
    private static final String DB_TABLE_SCREEN = "tbl_screen_settings";
    private static final String DB_TABLE_PERFORMANCE = "tbl_performance_flags";
    private static final String[] DB_TABLES = {
            DB_TABLE_BOOKMARK,
            DB_TABLE_SCREEN,
            DB_TABLE_PERFORMANCE
    };

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

    private void backupTables(SQLiteDatabase db) {
        for (String table : DB_TABLES) {
            final String tmpTable = DB_BACKUP_PREFIX + table;
            final String query = "ALTER TABLE '" + table + "' RENAME TO '" + tmpTable + "'";
            try {
                db.execSQL(query);
            } catch (Exception e) {
                /* Ignore errors if table does not exist. */
            }
        }
    }

    private void dropOldTables(SQLiteDatabase db) {
        for (String table : DB_TABLES) {
            final String tmpTable = DB_BACKUP_PREFIX + table;
            final String query = "DROP TABLE IF EXISTS '" + tmpTable + "'";
            db.execSQL(query);
        }
    }

    private void createDB(SQLiteDatabase db) {
        final String sqlScreenSettings =
                "CREATE TABLE IF NOT EXISTS " + DB_TABLE_SCREEN + " ("
                        + ID + " INTEGER PRIMARY KEY, "
                        + "colors INTEGER DEFAULT 16, "
                        + "resolution INTEGER DEFAULT 0, "
                        + "width, "
                        + "height);";

        db.execSQL(sqlScreenSettings);

        final String sqlPerformanceFlags =
                "CREATE TABLE IF NOT EXISTS " + DB_TABLE_PERFORMANCE + " ("
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
    }

    private void upgradeTables(SQLiteDatabase db) {
        for (String table : DB_TABLES) {
            final String tmpTable = DB_BACKUP_PREFIX + table;

            final List<String> newColumns = GetColumns(db, table);
            List<String> columns = GetColumns(db, tmpTable);

            if (columns != null) {
                columns.retainAll(newColumns);

                // restore data
                final String cols = joinStrings(columns, ",");
                final String query = String.format("INSERT INTO %s (%s) SELECT %s from '%s'", table, cols, cols, tmpTable);
                db.execSQL(query);
            }
        }
    }

    private void downgradeTables(SQLiteDatabase db) {
        for (String table : DB_TABLES) {
            final String tmpTable = DB_BACKUP_PREFIX + table;

            List<String> oldColumns = GetColumns(db, table);
            final List<String> columns = GetColumns(db, tmpTable);

            if (oldColumns != null) {
                oldColumns.retainAll(columns);

                // restore data
                final String cols = joinStrings(oldColumns, ",");
                final String query = String.format("INSERT INTO %s (%s) SELECT %s from '%s'", table, cols, cols, tmpTable);
                db.execSQL(query);
            }
        }
    }

    private List<String> getTableNames(SQLiteDatabase db) {
        final String query = "SELECT name FROM sqlite_master WHERE type='table'";
        Cursor cursor = db.rawQuery(query, null);
        List<String> list = new ArrayList<>();
        try {
            if (cursor.moveToFirst() && (cursor.getCount() > 0)) {
                while (!cursor.isAfterLast()) {
                    final String name = cursor.getString(cursor.getColumnIndex("name"));
                    list.add(name);
                    cursor.moveToNext();
                }
            }
        } finally {
            cursor.close();
        }

        return list;
    }

    private void insertDefault(SQLiteDatabase db) {
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

    @Override
    public void onCreate(SQLiteDatabase db) {
        createDB(db);
        insertDefault(db);
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
        for (String table : DB_TABLES) {
            final String query = "DROP TABLE IF EXISTS '" + table + "'";
            db.execSQL(query);
        }
        onCreate(db);
    }

    private void upgradeDB(SQLiteDatabase db) {
        db.beginTransaction();
        try {
            /* Back up old tables. */
            dropOldTables(db);
            backupTables(db);
            createDB(db);
            upgradeTables(db);

            db.setTransactionSuccessful();
        } finally {
            db.endTransaction();
            dropOldTables(db);
        }
    }

    private void downgradeDB(SQLiteDatabase db) {
        db.beginTransaction();
        try {
            /* Back up old tables. */
            dropOldTables(db);
            backupTables(db);
            createDB(db);
            downgradeTables(db);

            db.setTransactionSuccessful();
        } finally {
            db.endTransaction();
            dropOldTables(db);
        }
    }

    // from http://stackoverflow.com/questions/3424156/upgrade-sqlite-database-from-one-version-to-another
    @Override
    public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
        switch (oldVersion) {
            case 0:
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
                upgradeDB(db);
                break;
            default:
                recreateDB(db);
                break;
        }
    }

    @Override
    public void onDowngrade(SQLiteDatabase db, int oldVersion, int newVersion) {
        downgradeDB(db);
    }

}
