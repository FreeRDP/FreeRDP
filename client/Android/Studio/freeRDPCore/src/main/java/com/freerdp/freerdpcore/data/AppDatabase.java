/*
   Room database for bookmark storage

   Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.data;

import android.content.Context;

import androidx.annotation.NonNull;

import java.io.File;
import java.util.HashMap;
import java.util.Map;

import androidx.room.Database;
import androidx.room.Room;
import androidx.room.RoomDatabase;
import androidx.room.migration.Migration;
import androidx.sqlite.db.SupportSQLiteDatabase;

import com.freerdp.freerdpcore.security.KeystoreHelper;

import net.zetetic.database.sqlcipher.SQLiteDatabase;
import net.zetetic.database.sqlcipher.SupportOpenHelperFactory;

@Database(entities = { BookmarkEntity.class }, version = AppDatabase.DB_VERSION,
          exportSchema = false)
public abstract class AppDatabase extends RoomDatabase
{
	static final int DB_VERSION = 16;
	private static final String DB_NAME = "bookmarks.db";

	static
	{
		System.loadLibrary("sqlcipher");
	}

	private static volatile AppDatabase instance;

	public abstract BookmarkDao bookmarkDao();

	public static AppDatabase getInstance(Context context)
	{
		if (instance == null)
		{
			synchronized (AppDatabase.class)
			{
				if (instance == null)
				{
					byte[] key = getOrCreateDbKey(context);
					migrateUnencryptedIfNeeded(context, key);

					instance = Room.databaseBuilder(context.getApplicationContext(),
					                                AppDatabase.class, DB_NAME)
					               .openHelperFactory(new SupportOpenHelperFactory(key))
					               .addMigrations(MIGRATION_10_11)
					               .addMigrations(MIGRATION_11_12)
					               .addMigrations(MIGRATION_12_13)
					               .addMigrations(MIGRATION_13_14)
					               .addMigrations(MIGRATION_14_15)
					               .addMigrations(MIGRATION_15_16)
					               .build();
				}
			}
		}
		return instance;
	}

	private static byte[] getOrCreateDbKey(Context context)
	{
		try
		{
			return KeystoreHelper.getInstance(context).getOrCreateDbKey();
		}
		catch (KeystoreHelper.KeystoreException e)
		{
			throw new RuntimeException("Cannot obtain database encryption key", e);
		}
	}

	// Converts an existing unencrypted database to SQLCipher in-place
	private static void migrateUnencryptedIfNeeded(Context context, byte[] key)
	{
		File dbFile = context.getDatabasePath(DB_NAME);
		if (!dbFile.exists())
			return;

		String path = dbFile.getAbsolutePath();

		// Check if already encrypted by trying to open with the key.
		try
		{
			SQLiteDatabase.openDatabase(path, key, null, SQLiteDatabase.OPEN_READONLY, null, null)
			    .close();
			return; // Database is already encrypted, no migration needed
		}
		catch (Exception ignored)
		{
		}

		SQLiteDatabase db = null;
		try
		{
			// Verify it is an unencrypted database by opening it with an empty key
			db = SQLiteDatabase.openDatabase(path, new byte[0], null, SQLiteDatabase.OPEN_READWRITE,
			                                 null, null);
		}
		catch (Exception e)
		{
			// If it fails, the file might be corrupted (not plaintext, not correctly encrypted)
			SQLiteDatabase.deleteDatabase(dbFile);
			return;
		}

		// https://www.zetetic.net/sqlcipher/sqlcipher-api/index.html#sqlcipher_export
		// Prepare a temporary file for the encrypted database migration
		String tmpPath = path + ".migrating";
		File tmpFile = new File(tmpPath);
		SQLiteDatabase.deleteDatabase(tmpFile);

		try
		{
			// Pre-create the encrypted database to avoid CANTOPEN errors on ATTACH
			SQLiteDatabase
			    .openDatabase(tmpPath, key, null,
			                  SQLiteDatabase.OPEN_READWRITE | SQLiteDatabase.CREATE_IF_NECESSARY,
			                  null, null)
			    .close();
		}
		catch (Exception ignored)
		{
		}

		try
		{
			try
			{
				db.execSQL("ATTACH DATABASE '" + tmpPath + "' AS encrypted KEY X'" + toHex(key) +
				           "'");
				db.rawQuery("SELECT sqlcipher_export('encrypted')", null).moveToFirst();
				db.execSQL("DETACH DATABASE encrypted");
			}
			finally
			{
				db.close();
			}

			// Replace the old unencrypted database with the new encrypted one
			SQLiteDatabase.deleteDatabase(dbFile);
			if (!tmpFile.renameTo(dbFile))
				throw new RuntimeException("Could not replace database file after encryption");
		}
		catch (Exception e)
		{
			SQLiteDatabase.deleteDatabase(tmpFile);
			throw new RuntimeException("Failed to encrypt existing database", e);
		}
	}

	private static String toHex(byte[] bytes)
	{
		StringBuilder sb = new StringBuilder(bytes.length * 2);
		for (byte b : bytes)
			sb.append(String.format(java.util.Locale.US, "%02x", b));
		return sb.toString();
	}

	private static final Migration MIGRATION_15_16 = new Migration(15, 16) {
		@Override public void migrate(@NonNull SupportSQLiteDatabase db)
		{
			db.execSQL(
			    "ALTER TABLE 'bookmarks' ADD 'redirect_printer' INTEGER NOT NULL DEFAULT false;");
		}
	};

	private static final Migration MIGRATION_14_15 = new Migration(14, 15) {
		@Override public void migrate(@NonNull SupportSQLiteDatabase db)
		{
			db.execSQL("ALTER TABLE 'bookmarks' ADD 'scale_mode' TEXT NOT NULL DEFAULT '100';");
			db.execSQL("ALTER TABLE 'bookmarks' ADD 'scale_desktop' INTEGER NOT NULL DEFAULT 100;");
			db.execSQL("ALTER TABLE 'bookmarks' ADD 'scale_device' INTEGER NOT NULL DEFAULT 100;");
			db.execSQL(
			    "ALTER TABLE 'bookmarks' ADD 'vmconnect_mode' INTEGER NOT NULL DEFAULT false;");
			db.execSQL("ALTER TABLE 'bookmarks' ADD 'vmconnect_guid' TEXT NOT NULL DEFAULT '';");
		}
	};

	private static final Migration MIGRATION_13_14 = new Migration(13, 14) {
		@Override public void migrate(@NonNull SupportSQLiteDatabase db)
		{
			final String[] columns = new String[] {
				"label",          "hostname",         "username",         "password",
				"domain",         "gateway_hostname", "gateway_username", "gateway_password",
				"gateway_domain", "remote_program",   "work_dir",         "loadbalanceinfo"
			};
			for (String column : columns)
			{
				db.execSQL("ALTER TABLE 'bookmarks' ADD '" + column +
				           "_with_default' TEXT NOT NULL DEFAULT '';");
				db.execSQL("UPDATE bookmarks SET " + column + "_with_default = " + column);
				db.execSQL("ALTER TABLE 'bookmarks' DROP '" + column + "';");
				db.execSQL("ALTER TABLE 'bookmarks' RENAME COLUMN '" + column +
				           "_with_default' to '" + column + "';");
			}

			final String[] debugColumns = new String[] { "debug_level" };
			for (String column : debugColumns)
			{
				db.execSQL("ALTER TABLE 'bookmarks' ADD '" + column +
				           "_with_default' TEXT NOT NULL DEFAULT 'INFO';");
				db.execSQL("UPDATE bookmarks SET " + column + "_with_default = " + column);
				db.execSQL("ALTER TABLE 'bookmarks' DROP '" + column + "';");
				db.execSQL("ALTER TABLE 'bookmarks' RENAME COLUMN '" + column +
				           "_with_default' to '" + column + "';");
			}
			final Map<String, Integer> intColumns = new HashMap<>();
			intColumns.put("port", 3389);
			intColumns.put("colors", 32);
			intColumns.put("resolution", -1);
			intColumns.put("width", 0);
			intColumns.put("height", 0);
			intColumns.put("gateway_port", 443);
			intColumns.put("redirect_sound", 0);
			intColumns.put("security", 0);
			intColumns.put("tlsSecLevel", -1);
			intColumns.put("tlsMinLevel", -1);

			for (Map.Entry<String, Integer> column : intColumns.entrySet())
			{
				db.execSQL("ALTER TABLE 'bookmarks' ADD '" + column.getKey() +
				           "_with_default' INTEGER NOT NULL DEFAULT " +
				           column.getValue().toString() + ";");
				db.execSQL("UPDATE bookmarks SET " + column.getKey() +
				           "_with_default = " + column.getKey());
				db.execSQL("ALTER TABLE 'bookmarks' DROP '" + column.getKey() + "';");
				db.execSQL("ALTER TABLE 'bookmarks' RENAME COLUMN '" + column.getKey() +
				           "_with_default' to '" + column.getKey() + "';");
			}

			final Map<String, Boolean> boolColumns = new HashMap<>();
			boolColumns.put("perf_remotefx", true);
			boolColumns.put("perf_gfx", true);
			boolColumns.put("perf_gfx_h264", true);
			boolColumns.put("perf_wallpaper", true);
			boolColumns.put("perf_theming", true);
			boolColumns.put("perf_full_window_drag", true);
			boolColumns.put("perf_menu_animations", true);
			boolColumns.put("perf_font_smoothing", true);
			boolColumns.put("perf_desktop_composition", true);
			boolColumns.put("enable_gateway_settings", false);
			boolColumns.put("redirect_sdcard", false);
			boolColumns.put("redirect_microphone", false);
			boolColumns.put("console_mode", false);
			boolColumns.put("async_channel", false);
			boolColumns.put("async_update", false);

			for (Map.Entry<String, Boolean> column : boolColumns.entrySet())
			{
				db.execSQL("ALTER TABLE 'bookmarks' ADD '" + column.getKey() +
				           "_with_default' INTEGER NOT NULL DEFAULT " +
				           column.getValue().toString() + ";");
				db.execSQL("UPDATE bookmarks SET " + column.getKey() +
				           "_with_default = " + column.getKey());
				db.execSQL("ALTER TABLE 'bookmarks' DROP '" + column.getKey() + "';");
				db.execSQL("ALTER TABLE 'bookmarks' RENAME COLUMN '" + column.getKey() +
				           "_with_default' to '" + column.getKey() + "';");
			}
		}
	};

	private static final Migration MIGRATION_12_13 = new Migration(12, 13) {
		@Override public void migrate(@NonNull SupportSQLiteDatabase db)
		{
			db.execSQL("ALTER TABLE 'bookmarks' ADD 'loadbalanceinfo' TEXT NOT NULL DEFAULT '';");
		}
	};

	private static final Migration MIGRATION_11_12 = new Migration(11, 12) {
		@Override public void migrate(@NonNull SupportSQLiteDatabase db)
		{
			db.execSQL("ALTER TABLE 'bookmarks' ADD 'tlsSecLevel' INTEGER NOT NULL CONSTRAINT "
			           + "chk_tlsSecLevel "
			           + "CHECK (tlsSecLevel >= -1 AND tlsSecLevel <= 5)  DEFAULT -1;");
			db.execSQL("ALTER TABLE 'bookmarks' ADD 'tlsMinLevel' INTEGER NOT NULL CONSTRAINT "
			           + "chk_tlsMinLevel "
			           + "CHECK (tlsMinLevel >= -1) DEFAULT -1;");
			final String[] list = { "screen_3g_colors",
				                    "screen_3g_resolution",
				                    "screen_3g_width",
				                    "screen_3g_height",
				                    "perf_3g_remotefx",
				                    "perf_3g_gfx",
				                    "perf_3g_gfx_h264",
				                    "perf_3g_wallpaper",
				                    "perf_3g_theming",
				                    "perf_3g_full_window_drag",
				                    "perf_3g_menu_animations",
				                    "perf_3g_font_smoothing",
				                    "perf_3g_desktop_composition",
				                    "enable_3g_settings" };

			for (String s : list)
			{
				db.execSQL("ALTER TABLE 'bookmarks' DROP COLUMN '" + s + "';");
			}
		}
	};

	// v10: tbl_manual_bookmarks + tbl_screen_settings + tbl_performance_flags (SQLiteOpenHelper)
	// v11: single flat `bookmarks` table (Room)
	private static final Migration MIGRATION_10_11 = new Migration(10, 11) {
		@Override public void migrate(@NonNull SupportSQLiteDatabase db)
		{
			db.execSQL("CREATE TABLE IF NOT EXISTS `bookmarks` ("
			           + "`id` INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
			           + "`label` TEXT,"
			           + "`hostname` TEXT,"
			           + "`username` TEXT,"
			           + "`password` TEXT,"
			           + "`domain` TEXT,"
			           + "`port` INTEGER NOT NULL,"
			           + "`colors` INTEGER NOT NULL,"
			           + "`resolution` INTEGER NOT NULL,"
			           + "`width` INTEGER NOT NULL,"
			           + "`height` INTEGER NOT NULL,"
			           + "`perf_remotefx` INTEGER NOT NULL,"
			           + "`perf_gfx` INTEGER NOT NULL,"
			           + "`perf_gfx_h264` INTEGER NOT NULL,"
			           + "`perf_wallpaper` INTEGER NOT NULL,"
			           + "`perf_theming` INTEGER NOT NULL,"
			           + "`perf_full_window_drag` INTEGER NOT NULL,"
			           + "`perf_menu_animations` INTEGER NOT NULL,"
			           + "`perf_font_smoothing` INTEGER NOT NULL,"
			           + "`perf_desktop_composition` INTEGER NOT NULL,"
			           + "`screen_3g_colors` INTEGER NOT NULL,"
			           + "`screen_3g_resolution` INTEGER NOT NULL,"
			           + "`screen_3g_width` INTEGER NOT NULL,"
			           + "`screen_3g_height` INTEGER NOT NULL,"
			           + "`perf_3g_remotefx` INTEGER NOT NULL,"
			           + "`perf_3g_gfx` INTEGER NOT NULL,"
			           + "`perf_3g_gfx_h264` INTEGER NOT NULL,"
			           + "`perf_3g_wallpaper` INTEGER NOT NULL,"
			           + "`perf_3g_theming` INTEGER NOT NULL,"
			           + "`perf_3g_full_window_drag` INTEGER NOT NULL,"
			           + "`perf_3g_menu_animations` INTEGER NOT NULL,"
			           + "`perf_3g_font_smoothing` INTEGER NOT NULL,"
			           + "`perf_3g_desktop_composition` INTEGER NOT NULL,"
			           + "`enable_3g_settings` INTEGER NOT NULL,"
			           + "`enable_gateway_settings` INTEGER NOT NULL,"
			           + "`gateway_hostname` TEXT,"
			           + "`gateway_port` INTEGER NOT NULL,"
			           + "`gateway_username` TEXT,"
			           + "`gateway_password` TEXT,"
			           + "`gateway_domain` TEXT,"
			           + "`redirect_sdcard` INTEGER NOT NULL,"
			           + "`redirect_sound` INTEGER NOT NULL,"
			           + "`redirect_microphone` INTEGER NOT NULL,"
			           + "`security` INTEGER NOT NULL,"
			           + "`remote_program` TEXT,"
			           + "`work_dir` TEXT,"
			           + "`console_mode` INTEGER NOT NULL,"
			           + "`debug_level` TEXT,"
			           + "`async_channel` INTEGER NOT NULL,"
			           + "`async_update` INTEGER NOT NULL"
			           + ")");

			// port was stored as TEXT in the old schema
			db.execSQL(
			    "INSERT INTO bookmarks ("
			    + "  id, label, hostname, username, password, domain, port,"
			    + "  colors, resolution, width, height,"
			    + "  perf_remotefx, perf_gfx, perf_gfx_h264, perf_wallpaper, perf_theming,"
			    + "  perf_full_window_drag, perf_menu_animations, perf_font_smoothing, "
			    + "perf_desktop_composition,"
			    + "  screen_3g_colors, screen_3g_resolution, screen_3g_width, screen_3g_height,"
			    + "  perf_3g_remotefx, perf_3g_gfx, perf_3g_gfx_h264, perf_3g_wallpaper, "
			    + "perf_3g_theming,"
			    + "  perf_3g_full_window_drag, perf_3g_menu_animations, perf_3g_font_smoothing, "
			    + "perf_3g_desktop_composition,"
			    + "  enable_3g_settings, enable_gateway_settings,"
			    + "  gateway_hostname, gateway_port, gateway_username, gateway_password, "
			    + "gateway_domain,"
			    + "  redirect_sdcard, redirect_sound, redirect_microphone,"
			    + "  security, remote_program, work_dir, console_mode,"
			    + "  debug_level, async_channel, async_update"
			    + ") SELECT"
			    + "  b._id, IFNULL(b.label, ''), IFNULL(b.hostname, ''), IFNULL(b.username, ''), "
			    + "b.password, b.domain,"
			    + "  IFNULL(CAST(NULLIF(b.port, '') AS INTEGER), 3389),"
			    + "  IFNULL(s.colors, 32), IFNULL(s.resolution, -1), IFNULL(s.width, 0), "
			    + "IFNULL(s.height, 0),"
			    + "  IFNULL(p.perf_remotefx, 0), IFNULL(p.perf_gfx, 1), IFNULL(p.perf_gfx_h264, "
			    + "0), IFNULL(p.perf_wallpaper, 0), IFNULL(p.perf_theming, 0),"
			    + "  IFNULL(p.perf_full_window_drag, 0), IFNULL(p.perf_menu_animations, 0), "
			    + "IFNULL(p.perf_font_smoothing, 0), IFNULL(p.perf_desktop_composition, 0),"
			    + "  IFNULL(s3.colors, 16), IFNULL(s3.resolution, -1), IFNULL(s3.width, 0), "
			    + "IFNULL(s3.height, 0),"
			    + "  IFNULL(p3.perf_remotefx, 0), IFNULL(p3.perf_gfx, 0), IFNULL(p3.perf_gfx_h264, "
			    + "0), IFNULL(p3.perf_wallpaper, 0), IFNULL(p3.perf_theming, 0),"
			    + "  IFNULL(p3.perf_full_window_drag, 0), IFNULL(p3.perf_menu_animations, 0), "
			    + "IFNULL(p3.perf_font_smoothing, 0), IFNULL(p3.perf_desktop_composition, 0),"
			    + "  IFNULL(b.enable_3g_settings, 0), IFNULL(b.enable_gateway_settings, 0),"
			    + "  b.gateway_hostname, IFNULL(b.gateway_port, 443), b.gateway_username, "
			    + "b.gateway_password, b.gateway_domain,"
			    + "  IFNULL(b.redirect_sdcard, 0), IFNULL(b.redirect_sound, 0), "
			    + "IFNULL(b.redirect_microphone, 0),"
			    +
			    "  IFNULL(b.security, 0), b.remote_program, b.work_dir, IFNULL(b.console_mode, 0),"
			    + "  IFNULL(b.debug_level, 'INFO'), IFNULL(b.async_channel, 0), "
			    + "IFNULL(b.async_update, 0)"
			    + " FROM tbl_manual_bookmarks b"
			    + " LEFT JOIN tbl_screen_settings s  ON s._id  = b.screen_settings"
			    + " LEFT JOIN tbl_screen_settings s3 ON s3._id = b.screen_3g"
			    + " LEFT JOIN tbl_performance_flags p  ON p._id  = b.performance_flags"
			    + " LEFT JOIN tbl_performance_flags p3 ON p3._id = b.performance_3g");

			db.execSQL("DROP TABLE IF EXISTS tbl_manual_bookmarks");
			db.execSQL("DROP TABLE IF EXISTS tbl_screen_settings");
			db.execSQL("DROP TABLE IF EXISTS tbl_performance_flags");
		}
	};
}
