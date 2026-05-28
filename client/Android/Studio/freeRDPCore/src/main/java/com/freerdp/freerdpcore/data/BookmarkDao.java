/*
   Bookmark data access object

   Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.data;

import androidx.lifecycle.LiveData;
import androidx.room.Dao;
import androidx.room.Delete;
import androidx.room.Insert;
import androidx.room.OnConflictStrategy;
import androidx.room.Query;
import androidx.room.Update;

import java.util.List;

@Dao
public interface BookmarkDao {
	@Query("SELECT * FROM bookmarks ORDER BY label COLLATE NOCASE ASC")
	LiveData<List<BookmarkEntity>> getAllLiveData();

	@Query("SELECT * FROM bookmarks ORDER BY label COLLATE NOCASE ASC")
	List<BookmarkEntity> getAll();

	@Query("SELECT * FROM bookmarks WHERE id = :id") BookmarkEntity getById(long id);

	@Query("SELECT * FROM bookmarks WHERE label LIKE :query OR hostname LIKE :query "
	       + "ORDER BY label COLLATE NOCASE ASC")
	List<BookmarkEntity>
	search(String query);

	@Query("SELECT * FROM bookmarks WHERE label LIKE :query OR hostname LIKE :query "
	       + "ORDER BY label COLLATE NOCASE ASC")
	LiveData<List<BookmarkEntity>>
	searchLive(String query);

	@Insert(onConflict = OnConflictStrategy.ABORT) long insert(BookmarkEntity entity);

	@Update void update(BookmarkEntity entity);

	@Delete void delete(BookmarkEntity entity);

	@Query("DELETE FROM bookmarks WHERE id = :id") void deleteById(long id);
}
