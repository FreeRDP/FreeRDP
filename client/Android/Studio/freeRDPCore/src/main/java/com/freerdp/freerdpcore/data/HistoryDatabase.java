/*
   Room database for connection history storage

   Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.data;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.room.Database;
import androidx.room.Room;
import androidx.room.RoomDatabase;
import androidx.room.migration.Migration;
import androidx.sqlite.db.SupportSQLiteDatabase;

@Database(entities = { HistoryEntity.class }, version = 3, exportSchema = false)
public abstract class HistoryDatabase extends RoomDatabase
{
	private static final String DB_NAME = "history.db";

	private static volatile HistoryDatabase instance;

	public abstract HistoryDao historyDao();

	public static HistoryDatabase getInstance(Context context)
	{
		if (instance == null)
		{
			synchronized (HistoryDatabase.class)
			{
				if (instance == null)
				{
					instance = Room.databaseBuilder(context.getApplicationContext(),
					                                HistoryDatabase.class, DB_NAME)
					               .addMigrations(MIGRATION_1_2, MIGRATION_2_3)
					               .fallbackToDestructiveMigration()
					               .build();
				}
			}
		}
		return instance;
	}

	// v3: Add DEFAULT '' to item column to match Room schema
	private static final Migration MIGRATION_2_3 = new Migration(2, 3) {
		@Override public void migrate(@NonNull SupportSQLiteDatabase db)
		{
			db.execSQL("CREATE TABLE IF NOT EXISTS `quick_connect_history_new` ("
			           + "`item` TEXT NOT NULL DEFAULT '', "
			           + "`timestamp` INTEGER NOT NULL DEFAULT 0, "
			           + "PRIMARY KEY(`item`))");
			db.execSQL("INSERT INTO `quick_connect_history_new` (item, timestamp) "
			           + "SELECT item, timestamp FROM `quick_connect_history`");
			db.execSQL("DROP TABLE `quick_connect_history`");
			db.execSQL("ALTER TABLE `quick_connect_history_new` RENAME TO `quick_connect_history`");
		}
	};

	// v1: item TEXT PRIMARY KEY, timestamp INTEGER (both nullable — old SQLiteOpenHelper schema)
	// v2: item TEXT NOT NULL PRIMARY KEY, timestamp INTEGER NOT NULL (Room schema)
	private static final Migration MIGRATION_1_2 = new Migration(1, 2) {
		@Override public void migrate(@NonNull SupportSQLiteDatabase db)
		{
			db.execSQL("CREATE TABLE IF NOT EXISTS `quick_connect_history_new` ("
			           + "`item` TEXT NOT NULL, "
			           + "`timestamp` INTEGER NOT NULL DEFAULT 0, "
			           + "PRIMARY KEY(`item`))");
			db.execSQL("INSERT INTO `quick_connect_history_new` (item, timestamp) "
			           + "SELECT item, IFNULL(timestamp, 0) FROM `quick_connect_history`");
			db.execSQL("DROP TABLE `quick_connect_history`");
			db.execSQL("ALTER TABLE `quick_connect_history_new` RENAME TO `quick_connect_history`");
		}
	};
}
