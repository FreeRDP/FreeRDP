/*
   Connection history entity

   Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.data;

import androidx.annotation.NonNull;
import androidx.room.ColumnInfo;
import androidx.room.Entity;
import androidx.room.PrimaryKey;

import java.util.Date;

@Entity(tableName = "quick_connect_history") public class HistoryEntity
{
	@PrimaryKey @ColumnInfo(name = "item", defaultValue = "") @NonNull public String item;

	@ColumnInfo(name = "timestamp", defaultValue = "0") public long timestamp;

	public HistoryEntity(@NonNull String item)
	{
		this.item = item;
		this.timestamp = new Date().getTime();
	}
}
