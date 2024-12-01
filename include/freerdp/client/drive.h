/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Drive Virtual Channel
 *
 * Copyright 2024 Armin Novak <armin.novak@gmail.com>
 * Copyright 2024 Thincast Technologies GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <freerdp/channels/rdpdr.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/** @struct rdpDriveContext
	 * @briefopaque type holding a drive context
	 * @since version 3.10.0
	 */
	typedef struct rdp_drive_context rdpDriveContext;

	/** @struct rdpDriveDriver
	 * @brief This structure contains all function pointers required to implement a drive channel
	 * backend.
	 * @since version 3.10.0
	 *
	 * @var rdpDriveDriver::resolve_path
	 * Member 'resolve_path' takes a path with wildcards '%' or '*' as input and resolves these to
	 * an absolute local path. Returns this resolved path as allocated string. Returns \b NULL in
	 * case of failure.
	 *
	 * @var rdpDriveDriver::resolve_name
	 * Member 'resolve_name' takes the path to redirect and an optional name suggestion and converts
	 * these to a usable name for the drive redirection. Preferred is the suggested name, but the
	 * supplied path is used as fallback. In any case, forbidden symbols are replaced before a newly
	 * allocated string is returned. Returns \b NULL in case of failure.
	 *
	 * @var rdpDriveDriver::new
	 * Member 'new' allocates a new \b rdpDriveContext for a given \b rdpContext. Returns \b NULL in
	 * case of failure.
	 *
	 * @var rdpDriveDriver::free
	 * Member 'free' cleans up a previously allocated \b rdpDriveContext. Argument may be \b NULL
	 *
	 * @var rdpDriveDriver::setPath
	 * Member 'setPath' initializes a \b rdpDriveContext. \b base_path argument is the (local)
	 * absolute path to prefix, \b filename the path this context is for.
	 *
	 * @var rdpDriveDriver::createDirectory
	 * Create a directory for a given context. Fails if the directory can not be created or the
	 * context is not holding a directory
	 *
	 * @var rdpDriveDriver::createFile
	 * Create or open a file for a given context. Fails if the context holds a directory or the file
	 * creation failed.
	 *
	 * @var rdpDriveDriver::seek
	 * Position the file pointer in an opened file. Fails if the file is not open, the seek can not
	 * be done or the context is a directory.
	 *
	 * @var rdpDriveDriver::read
	 * Read data from an opened file. Fails if the file can not be read, is not open or the context
	 * holds a directory.
	 *
	 * @var rdpDriveDriver::write
	 * Write data to an opened file. Fails if the file can not be written to, the file is not open
	 * or the context holds a directory.
	 *
	 * @var rdpDriveDriver::remove
	 * Delete a file or directory identified by context (recursively)
	 *
	 * @var rdpDriveDriver::move
	 * Move a file or directory from the name the context holds to the new name supplied by \b
	 * newName. Optionally overwrite existing entries.
	 *
	 * @var rdpDriveDriver::exists
	 * Check a given context (file or directory) already exists
	 *
	 * @var rdpDriveDriver::empty
	 * Check if a given context is a directory and if it is empty.
	 *
	 * @var rdpDriveDriver::setSize
	 * Set the file size for a given context.
	 *
	 * @var rdpDriveDriver::getFileAttributes
	 * Return the file attributes of a given context
	 *
	 * @var rdpDriveDriver::setFileAttributesAndTimes
	 * Update file attributes and times for a given context
	 *
	 * @var rdpDriveDriver::first
	 * Reset a directory iterator and return the first entry found or \b NULL in case of failure.
	 *
	 * @var rdpDriveDriver::next
	 * Get the next directory iterator or \b NULL in case of no more elements.
	 *
	 * @var rdpDriveDriver::getFileAttributeData
	 * Get file attribute data for a given context. Returns the attribute data or \b NULL in case of
	 * failure.
	 */
	typedef struct rdp_drive_driver
	{
		char* (*resolve_path)(const char* what);

		char* (*resolve_name)(const char* path, const char* suggested);

		rdpDriveContext* (*new)(rdpContext* context);
		void (*free)(rdpDriveContext*);

		bool (*setPath)(rdpDriveContext* context, const WCHAR* base_path, const WCHAR* filename,
		                size_t nbFileNameChar);
		bool (*createDirectory)(rdpDriveContext* context);
		bool (*createFile)(rdpDriveContext* context, UINT32 dwDesiredAccess, UINT32 dwShareMode,
		                   UINT32 dwCreationDisposition, UINT32 dwFlagsAndAttributes);

		SSIZE_T (*seek)(rdpDriveContext* context, SSIZE_T offset, int whence);
		SSIZE_T (*read)(rdpDriveContext* context, void* buf, size_t nbyte);
		SSIZE_T (*write)(rdpDriveContext* context, const void* buf, size_t nbyte);
		bool (*remove)(rdpDriveContext* context);
		bool (*move)(rdpDriveContext* context, const WCHAR* newName, size_t numCharacters,
		             bool replaceIfExists);
		bool (*exists)(rdpDriveContext* context);
		bool (*empty)(rdpDriveContext* context);
		bool (*setSize)(rdpDriveContext* context, INT64 size);

		UINT32 (*getFileAttributes)(rdpDriveContext* context);
		bool (*setFileAttributesAndTimes)(rdpDriveContext* context, UINT64 CreationTime,
		                                  UINT64 LastAccessTime, UINT64 LastWriteTime,
		                                  UINT64 ChangeTime, UINT32 dwFileAttributes);

		const WIN32_FIND_DATAW* (*first)(rdpDriveContext* context, const WCHAR* query,
		                                 size_t numCharacters);
		const WIN32_FIND_DATAW* (*next)(rdpDriveContext* context);

		const BY_HANDLE_FILE_INFORMATION* (*getFileAttributeData)(rdpDriveContext* context);
	} rdpDriveDriver;

#ifdef __cplusplus
}
#endif
