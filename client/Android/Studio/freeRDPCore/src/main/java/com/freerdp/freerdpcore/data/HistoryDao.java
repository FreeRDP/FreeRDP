/*
   Connection history data access object

   Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.data;

import androidx.room.Dao;
import androidx.room.Insert;
import androidx.room.OnConflictStrategy;
import androidx.room.Query;

import java.util.List;

@Dao
public interface HistoryDao {
	@Query("SELECT * FROM quick_connect_history WHERE item LIKE :filter "
	       + "ORDER BY timestamp DESC LIMIT 50")
	List<HistoryEntity>
	findHistory(String filter);

	@Query("SELECT COUNT(*) FROM quick_connect_history WHERE item = :item") int exists(String item);

	@Insert(onConflict = OnConflictStrategy.REPLACE) void insertOrReplace(HistoryEntity entity);

	@Query("DELETE FROM quick_connect_history WHERE item = :item") void deleteByItem(String item);
}
