/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Clipboard Redirection
 *
 * Copyright 2010-2011 Vic Lee
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2023 Armin Novak <armin.novak@thincast.com>
 * Copyright 2023 Pascal Nowack <Pascal.Nowack@gmx.de>
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

#include <freerdp/config.h>

#include <stdlib.h>
#include <errno.h>

#ifdef WITH_FUSE
#define FUSE_USE_VERSION 30
#include <fuse_lowlevel.h>
#endif

#if defined(WITH_FUSE)
#include <sys/mount.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#endif

#include <winpr/crt.h>
#include <winpr/string.h>
#include <winpr/assert.h>
#include <winpr/image.h>
#include <winpr/stream.h>
#include <winpr/clipboard.h>
#include <winpr/path.h>

#include <freerdp/utils/signal.h>
#include <freerdp/log.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/channels/channels.h>
#include <freerdp/channels/cliprdr.h>

#include <freerdp/client/client_cliprdr_file.h>

#define MAX_CLIP_DATA_DIR_LEN 10
#define NO_CLIP_DATA_ID (UINT64_C(1) << 32)
#define WIN32_FILETIME_TO_UNIX_EPOCH INT64_C(11644473600)

#ifdef WITH_DEBUG_CLIPRDR
#define DEBUG_CLIPRDR(log, ...) WLog_Print(log, WLOG_DEBUG, __VA_ARGS__)
#else
#define DEBUG_CLIPRDR(log, ...) \
	do                          \
	{                           \
	} while (0)
#endif

#if defined(WITH_FUSE)
typedef enum eFuseLowlevelOperationType
{
	FUSE_LL_OPERATION_NONE,
	FUSE_LL_OPERATION_LOOKUP,
	FUSE_LL_OPERATION_GETATTR,
	FUSE_LL_OPERATION_READ,
} FuseLowlevelOperationType;

typedef struct sCliprdrFuseFile CliprdrFuseFile;

struct sCliprdrFuseFile
{
	CliprdrFuseFile* parent;
	wArrayList* children;

	char* filename;
	char* filename_with_root;
	UINT32 list_idx;
	fuse_ino_t ino;

	BOOL is_directory;
	BOOL is_readonly;

	BOOL has_size;
	UINT64 size;

	BOOL has_last_write_time;
	INT64 last_write_time_unix;

	BOOL has_clip_data_id;
	UINT32 clip_data_id;
};

typedef struct
{
	CliprdrFileContext* file_context;

	CliprdrFuseFile* clip_data_dir;

	BOOL has_clip_data_id;
	UINT32 clip_data_id;
} CliprdrFuseClipDataEntry;

typedef struct
{
	CliprdrFileContext* file_context;

	wArrayList* fuse_files;

	BOOL all_files;
	BOOL has_clip_data_id;
	UINT32 clip_data_id;
} FuseFileClearContext;

typedef struct
{
	FuseLowlevelOperationType operation_type;
	CliprdrFuseFile* fuse_file;
	fuse_req_t fuse_req;
	UINT32 stream_id;
} CliprdrFuseRequest;

typedef struct
{
	CliprdrFuseFile* parent;
	char* parent_path;
} CliprdrFuseFindParentContext;
#endif

typedef struct
{
	char* name;
	FILE* fp;
	INT64 size;
	CliprdrFileContext* context;
} CliprdrLocalFile;

typedef struct
{
	UINT32 lockId;
	BOOL locked;
	size_t count;
	CliprdrLocalFile* files;
	CliprdrFileContext* context;
} CliprdrLocalStream;

struct cliprdr_file_context
{
#if defined(WITH_FUSE)
	/* FUSE related**/
	HANDLE fuse_start_sync;
	HANDLE fuse_stop_sync;
	HANDLE fuse_thread;
	struct fuse_session* fuse_sess;
#if FUSE_USE_VERSION < 30
	struct fuse_chan* ch;
#endif

	wHashTable* inode_table;
	wHashTable* clip_data_table;
	wHashTable* request_table;

	CliprdrFuseFile* root_dir;
	CliprdrFuseClipDataEntry* clip_data_entry_without_id;
	UINT32 current_clip_data_id;

	fuse_ino_t next_ino;
	UINT32 next_clip_data_id;
	UINT32 next_stream_id;
#endif

	/* File clipping */
	BOOL file_formats_registered;
	UINT32 file_capability_flags;

	UINT32 local_lock_id;

	wHashTable* local_streams;
	wLog* log;
	void* clipboard;
	CliprdrClientContext* context;
	char* path;
	char* exposed_path;
	BYTE server_data_hash[WINPR_SHA256_DIGEST_LENGTH];
	BYTE client_data_hash[WINPR_SHA256_DIGEST_LENGTH];
};

#if defined(WITH_FUSE)
static void fuse_file_free(void* data)
{
	CliprdrFuseFile* fuse_file = data;

	if (!fuse_file)
		return;

	ArrayList_Free(fuse_file->children);
	free(fuse_file->filename_with_root);

	free(fuse_file);
}

static CliprdrFuseFile* fuse_file_new(void)
{
	CliprdrFuseFile* fuse_file = NULL;

	fuse_file = calloc(1, sizeof(CliprdrFuseFile));
	if (!fuse_file)
		return NULL;

	fuse_file->children = ArrayList_New(FALSE);
	if (!fuse_file->children)
	{
		free(fuse_file);
		return NULL;
	}

	return fuse_file;
}

static void clip_data_entry_free(void* data)
{
	CliprdrFuseClipDataEntry* clip_data_entry = data;

	if (!clip_data_entry)
		return;

	if (clip_data_entry->has_clip_data_id)
	{
		CliprdrFileContext* file_context = clip_data_entry->file_context;
		CLIPRDR_UNLOCK_CLIPBOARD_DATA unlock_clipboard_data = { 0 };

		WINPR_ASSERT(file_context);

		unlock_clipboard_data.common.msgType = CB_UNLOCK_CLIPDATA;
		unlock_clipboard_data.clipDataId = clip_data_entry->clip_data_id;

		file_context->context->ClientUnlockClipboardData(file_context->context,
		                                                 &unlock_clipboard_data);
		clip_data_entry->has_clip_data_id = FALSE;

		WLog_Print(file_context->log, WLOG_DEBUG, "Destroyed ClipDataEntry with id %u",
		           clip_data_entry->clip_data_id);
	}

	free(clip_data_entry);
}

static BOOL does_server_support_clipdata_locking(CliprdrFileContext* file_context)
{
	WINPR_ASSERT(file_context);

	if (cliprdr_file_context_remote_get_flags(file_context) & CB_CAN_LOCK_CLIPDATA)
		return TRUE;

	return FALSE;
}

static UINT32 get_next_free_clip_data_id(CliprdrFileContext* file_context)
{
	UINT32 clip_data_id = 0;

	WINPR_ASSERT(file_context);

	HashTable_Lock(file_context->inode_table);
	clip_data_id = file_context->next_clip_data_id;
	while (clip_data_id == 0 ||
	       HashTable_GetItemValue(file_context->clip_data_table, (void*)(uintptr_t)clip_data_id))
		++clip_data_id;

	file_context->next_clip_data_id = clip_data_id + 1;
	HashTable_Unlock(file_context->inode_table);

	return clip_data_id;
}

static CliprdrFuseClipDataEntry* clip_data_entry_new(CliprdrFileContext* file_context,
                                                     BOOL needs_clip_data_id)
{
	CliprdrFuseClipDataEntry* clip_data_entry = NULL;
	CLIPRDR_LOCK_CLIPBOARD_DATA lock_clipboard_data = { 0 };

	WINPR_ASSERT(file_context);

	clip_data_entry = calloc(1, sizeof(CliprdrFuseClipDataEntry));
	if (!clip_data_entry)
		return NULL;

	clip_data_entry->file_context = file_context;
	clip_data_entry->clip_data_id = get_next_free_clip_data_id(file_context);

	if (!needs_clip_data_id)
		return clip_data_entry;

	lock_clipboard_data.common.msgType = CB_LOCK_CLIPDATA;
	lock_clipboard_data.clipDataId = clip_data_entry->clip_data_id;

	if (file_context->context->ClientLockClipboardData(file_context->context, &lock_clipboard_data))
	{
		HashTable_Lock(file_context->inode_table);
		clip_data_entry_free(clip_data_entry);
		HashTable_Unlock(file_context->inode_table);
		return NULL;
	}
	clip_data_entry->has_clip_data_id = TRUE;

	WLog_Print(file_context->log, WLOG_DEBUG, "Created ClipDataEntry with id %u",
	           clip_data_entry->clip_data_id);

	return clip_data_entry;
}

static BOOL should_remove_fuse_file(CliprdrFuseFile* fuse_file, BOOL all_files,
                                    BOOL has_clip_data_id, UINT32 clip_data_id)
{
	if (all_files)
		return TRUE;

	if (fuse_file->ino == FUSE_ROOT_ID)
		return FALSE;
	if (!fuse_file->has_clip_data_id && !has_clip_data_id)
		return TRUE;
	if (fuse_file->has_clip_data_id && has_clip_data_id &&
	    (fuse_file->clip_data_id == clip_data_id))
		return TRUE;

	return FALSE;
}

static BOOL maybe_clear_fuse_request(const void* key, void* value, void* arg)
{
	CliprdrFuseRequest* fuse_request = value;
	FuseFileClearContext* clear_context = arg;
	CliprdrFileContext* file_context = clear_context->file_context;
	CliprdrFuseFile* fuse_file = fuse_request->fuse_file;

	WINPR_ASSERT(file_context);

	if (!should_remove_fuse_file(fuse_file, clear_context->all_files,
	                             clear_context->has_clip_data_id, clear_context->clip_data_id))
		return TRUE;

	DEBUG_CLIPRDR(file_context->log, "Clearing FileContentsRequest for file \"%s\"",
	              fuse_file->filename_with_root);

	fuse_reply_err(fuse_request->fuse_req, EIO);
	HashTable_Remove(file_context->request_table, key);

	return TRUE;
}

static BOOL maybe_steal_inode(const void* key, void* value, void* arg)
{
	CliprdrFuseFile* fuse_file = value;
	FuseFileClearContext* clear_context = arg;
	CliprdrFileContext* file_context = clear_context->file_context;

	WINPR_ASSERT(file_context);

	if (should_remove_fuse_file(fuse_file, clear_context->all_files,
	                            clear_context->has_clip_data_id, clear_context->clip_data_id))
	{
		if (!ArrayList_Append(clear_context->fuse_files, fuse_file))
			WLog_Print(file_context->log, WLOG_ERROR,
			           "Failed to append FUSE file to list for deletion");

		HashTable_Remove(file_context->inode_table, key);
	}

	return TRUE;
}

static BOOL notify_delete_child(void* data, size_t index, va_list ap)
{
	CliprdrFuseFile* child = data;

	WINPR_ASSERT(child);

	CliprdrFileContext* file_context = va_arg(ap, CliprdrFileContext*);
	CliprdrFuseFile* parent = va_arg(ap, CliprdrFuseFile*);

	WINPR_ASSERT(file_context);
	WINPR_ASSERT(parent);

	WINPR_ASSERT(file_context->fuse_sess);
	fuse_lowlevel_notify_delete(file_context->fuse_sess, parent->ino, child->ino, child->filename,
	                            strlen(child->filename));

	return TRUE;
}

static BOOL invalidate_inode(void* data, size_t index, va_list ap)
{
	CliprdrFuseFile* fuse_file = data;

	WINPR_ASSERT(fuse_file);

	CliprdrFileContext* file_context = va_arg(ap, CliprdrFileContext*);
	WINPR_ASSERT(file_context);
	WINPR_ASSERT(file_context->fuse_sess);

	ArrayList_ForEach(fuse_file->children, notify_delete_child, file_context, fuse_file);

	DEBUG_CLIPRDR(file_context->log, "Invalidating inode %lu for file \"%s\"", fuse_file->ino,
	              fuse_file->filename);
	fuse_lowlevel_notify_inval_inode(file_context->fuse_sess, fuse_file->ino, 0, 0);
	WLog_Print(file_context->log, WLOG_DEBUG, "Inode %lu invalidated", fuse_file->ino);

	return TRUE;
}

static void clear_selection(CliprdrFileContext* file_context, BOOL all_selections,
                            CliprdrFuseClipDataEntry* clip_data_entry)
{
	FuseFileClearContext clear_context = { 0 };
	CliprdrFuseFile* root_dir = NULL;
	CliprdrFuseFile* clip_data_dir = NULL;

	WINPR_ASSERT(file_context);

	root_dir = file_context->root_dir;
	WINPR_ASSERT(root_dir);

	clear_context.file_context = file_context;
	clear_context.fuse_files = ArrayList_New(FALSE);
	WINPR_ASSERT(clear_context.fuse_files);

	wObject* aobj = ArrayList_Object(clear_context.fuse_files);
	WINPR_ASSERT(aobj);
	aobj->fnObjectFree = fuse_file_free;

	if (clip_data_entry)
	{
		clip_data_dir = clip_data_entry->clip_data_dir;
		clip_data_entry->clip_data_dir = NULL;

		WINPR_ASSERT(clip_data_dir);

		ArrayList_Remove(root_dir->children, clip_data_dir);

		clear_context.has_clip_data_id = clip_data_dir->has_clip_data_id;
		clear_context.clip_data_id = clip_data_dir->clip_data_id;
	}
	clear_context.all_files = all_selections;

	if (clip_data_entry && clip_data_entry->has_clip_data_id)
		WLog_Print(file_context->log, WLOG_DEBUG, "Clearing selection for clipDataId %u",
		           clip_data_entry->clip_data_id);
	else
		WLog_Print(file_context->log, WLOG_DEBUG, "Clearing selection%s",
		           all_selections ? "s" : "");

	HashTable_Foreach(file_context->request_table, maybe_clear_fuse_request, &clear_context);
	HashTable_Foreach(file_context->inode_table, maybe_steal_inode, &clear_context);
	HashTable_Unlock(file_context->inode_table);

	if (file_context->fuse_sess)
	{
		/*
		 * fuse_lowlevel_notify_inval_inode() is a blocking operation. If we receive a
		 * FUSE request (e.g. read()), then FUSE would block in read(), since the
		 * mutex of the inode_table would still be locked, if we wouldn't unlock it
		 * here.
		 * So, to avoid a deadlock here, unlock the mutex and reply all incoming
		 * operations with -ENOENT until the invalidation process is complete.
		 */
		ArrayList_ForEach(clear_context.fuse_files, invalidate_inode, file_context);
		if (clip_data_dir)
		{
			fuse_lowlevel_notify_delete(file_context->fuse_sess, file_context->root_dir->ino,
			                            clip_data_dir->ino, clip_data_dir->filename,
			                            strlen(clip_data_dir->filename));
		}
	}
	ArrayList_Free(clear_context.fuse_files);

	HashTable_Lock(file_context->inode_table);
	if (clip_data_entry && clip_data_entry->has_clip_data_id)
		WLog_Print(file_context->log, WLOG_DEBUG, "Selection cleared for clipDataId %u",
		           clip_data_entry->clip_data_id);
	else
		WLog_Print(file_context->log, WLOG_DEBUG, "Selection%s cleared", all_selections ? "s" : "");
}

static void clear_entry_selection(CliprdrFuseClipDataEntry* clip_data_entry)
{
	WINPR_ASSERT(clip_data_entry);

	if (!clip_data_entry->clip_data_dir)
		return;

	clear_selection(clip_data_entry->file_context, FALSE, clip_data_entry);
}

static void clear_no_cdi_entry(CliprdrFileContext* file_context)
{
	WINPR_ASSERT(file_context);

	WINPR_ASSERT(file_context->inode_table);
	HashTable_Lock(file_context->inode_table);
	if (file_context->clip_data_entry_without_id)
	{
		clear_entry_selection(file_context->clip_data_entry_without_id);

		clip_data_entry_free(file_context->clip_data_entry_without_id);
		file_context->clip_data_entry_without_id = NULL;
	}
	HashTable_Unlock(file_context->inode_table);
}

static BOOL clear_clip_data_entries(const void* key, void* value, void* arg)
{
	clear_entry_selection(value);

	return TRUE;
}

static void clear_cdi_entries(CliprdrFileContext* file_context)
{
	WINPR_ASSERT(file_context);

	HashTable_Lock(file_context->inode_table);
	HashTable_Foreach(file_context->clip_data_table, clear_clip_data_entries, NULL);

	HashTable_Clear(file_context->clip_data_table);
	HashTable_Unlock(file_context->inode_table);
}

static UINT prepare_clip_data_entry_with_id(CliprdrFileContext* file_context)
{
	CliprdrFuseClipDataEntry* clip_data_entry = NULL;

	WINPR_ASSERT(file_context);

	clip_data_entry = clip_data_entry_new(file_context, TRUE);
	if (!clip_data_entry)
	{
		WLog_Print(file_context->log, WLOG_ERROR, "Failed to create clipDataEntry");
		return ERROR_INTERNAL_ERROR;
	}

	HashTable_Lock(file_context->inode_table);
	if (!HashTable_Insert(file_context->clip_data_table,
	                      (void*)(uintptr_t)clip_data_entry->clip_data_id, clip_data_entry))
	{
		WLog_Print(file_context->log, WLOG_ERROR, "Failed to insert clipDataEntry");
		clip_data_entry_free(clip_data_entry);
		HashTable_Unlock(file_context->inode_table);
		return ERROR_INTERNAL_ERROR;
	}
	HashTable_Unlock(file_context->inode_table);

	// HashTable_Insert owns clip_data_entry
	// NOLINTNEXTLINE(clang-analyzer-unix.Malloc)
	file_context->current_clip_data_id = clip_data_entry->clip_data_id;

	return CHANNEL_RC_OK;
}

static UINT prepare_clip_data_entry_without_id(CliprdrFileContext* file_context)
{
	WINPR_ASSERT(file_context);
	WINPR_ASSERT(!file_context->clip_data_entry_without_id);

	file_context->clip_data_entry_without_id = clip_data_entry_new(file_context, FALSE);
	if (!file_context->clip_data_entry_without_id)
	{
		WLog_Print(file_context->log, WLOG_ERROR, "Failed to create clipDataEntry");
		return ERROR_INTERNAL_ERROR;
	}

	return CHANNEL_RC_OK;
}
#endif

UINT cliprdr_file_context_notify_new_server_format_list(CliprdrFileContext* file_context)
{
	WINPR_ASSERT(file_context);
	WINPR_ASSERT(file_context->context);

#if defined(WITH_FUSE)
	clear_no_cdi_entry(file_context);
	/* TODO: assign timeouts to old locks instead */
	clear_cdi_entries(file_context);

	if (does_server_support_clipdata_locking(file_context))
		return prepare_clip_data_entry_with_id(file_context);
	else
		return prepare_clip_data_entry_without_id(file_context);
#else
	return CHANNEL_RC_OK;
#endif
}

UINT cliprdr_file_context_notify_new_client_format_list(CliprdrFileContext* file_context)
{
	WINPR_ASSERT(file_context);
	WINPR_ASSERT(file_context->context);

#if defined(WITH_FUSE)
	clear_no_cdi_entry(file_context);
	/* TODO: assign timeouts to old locks instead */
	clear_cdi_entries(file_context);
#endif

	return CHANNEL_RC_OK;
}

static CliprdrLocalStream* cliprdr_local_stream_new(CliprdrFileContext* context, UINT32 streamID,
                                                    const char* data, size_t size);
static void cliprdr_file_session_terminate(CliprdrFileContext* file, BOOL stop_thread);
static BOOL local_stream_discard(const void* key, void* value, void* arg);

static void writelog(wLog* log, DWORD level, const char* fname, const char* fkt, size_t line, ...)
{
	if (!WLog_IsLevelActive(log, level))
		return;

	va_list ap = { 0 };
	va_start(ap, line);
	WLog_PrintMessageVA(log, WLOG_MESSAGE_TEXT, level, line, fname, fkt, ap);
	va_end(ap);
}

#if defined(WITH_FUSE)
static void cliprdr_file_fuse_lookup(fuse_req_t req, fuse_ino_t parent, const char* name);
static void cliprdr_file_fuse_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi);
static void cliprdr_file_fuse_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
                                      struct fuse_file_info* fi);
static void cliprdr_file_fuse_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
                                   struct fuse_file_info* fi);
static void cliprdr_file_fuse_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi);
static void cliprdr_file_fuse_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi);

static const struct fuse_lowlevel_ops cliprdr_file_fuse_oper = {
	.lookup = cliprdr_file_fuse_lookup,
	.getattr = cliprdr_file_fuse_getattr,
	.readdir = cliprdr_file_fuse_readdir,
	.open = cliprdr_file_fuse_open,
	.read = cliprdr_file_fuse_read,
	.opendir = cliprdr_file_fuse_opendir,
};

static CliprdrFuseFile* get_fuse_file_by_ino(CliprdrFileContext* file_context, fuse_ino_t fuse_ino)
{
	WINPR_ASSERT(file_context);

	return HashTable_GetItemValue(file_context->inode_table, (void*)(uintptr_t)fuse_ino);
}

static CliprdrFuseFile* get_fuse_file_by_name_from_parent(CliprdrFileContext* file_context,
                                                          CliprdrFuseFile* parent, const char* name)
{
	WINPR_ASSERT(file_context);
	WINPR_ASSERT(parent);

	for (size_t i = 0; i < ArrayList_Count(parent->children); ++i)
	{
		CliprdrFuseFile* child = ArrayList_GetItem(parent->children, i);

		WINPR_ASSERT(child);

		if (strcmp(name, child->filename) == 0)
			return child;
	}

	DEBUG_CLIPRDR(file_context->log, "Requested file \"%s\" in directory \"%s\" does not exist",
	              name, parent->filename);

	return NULL;
}

static CliprdrFuseRequest* cliprdr_fuse_request_new(CliprdrFileContext* file_context,
                                                    CliprdrFuseFile* fuse_file, fuse_req_t fuse_req,
                                                    FuseLowlevelOperationType operation_type)
{
	CliprdrFuseRequest* fuse_request = NULL;
	UINT32 stream_id = file_context->next_stream_id;

	WINPR_ASSERT(file_context);
	WINPR_ASSERT(fuse_file);

	fuse_request = calloc(1, sizeof(CliprdrFuseRequest));
	if (!fuse_request)
	{
		WLog_Print(file_context->log, WLOG_ERROR, "Failed to allocate FUSE request for file \"%s\"",
		           fuse_file->filename_with_root);
		return NULL;
	}

	fuse_request->fuse_file = fuse_file;
	fuse_request->fuse_req = fuse_req;
	fuse_request->operation_type = operation_type;

	while (stream_id == 0 ||
	       HashTable_GetItemValue(file_context->request_table, (void*)(uintptr_t)stream_id))
		++stream_id;
	fuse_request->stream_id = stream_id;

	file_context->next_stream_id = stream_id + 1;

	if (!HashTable_Insert(file_context->request_table, (void*)(uintptr_t)fuse_request->stream_id,
	                      fuse_request))
	{
		WLog_Print(file_context->log, WLOG_ERROR, "Failed to track FUSE request for file \"%s\"",
		           fuse_file->filename_with_root);
		free(fuse_request);
		return NULL;
	}

	return fuse_request;
}

static BOOL request_file_size_async(CliprdrFileContext* file_context, CliprdrFuseFile* fuse_file,
                                    fuse_req_t fuse_req, FuseLowlevelOperationType operation_type)
{
	CliprdrFuseRequest* fuse_request = NULL;
	CLIPRDR_FILE_CONTENTS_REQUEST file_contents_request = { 0 };

	WINPR_ASSERT(file_context);
	WINPR_ASSERT(fuse_file);

	fuse_request = cliprdr_fuse_request_new(file_context, fuse_file, fuse_req, operation_type);
	if (!fuse_request)
		return FALSE;

	file_contents_request.common.msgType = CB_FILECONTENTS_REQUEST;
	file_contents_request.streamId = fuse_request->stream_id;
	file_contents_request.listIndex = fuse_file->list_idx;
	file_contents_request.dwFlags = FILECONTENTS_SIZE;
	file_contents_request.cbRequested = 0x8;
	file_contents_request.haveClipDataId = fuse_file->has_clip_data_id;
	file_contents_request.clipDataId = fuse_file->clip_data_id;

	if (file_context->context->ClientFileContentsRequest(file_context->context,
	                                                     &file_contents_request))
	{
		WLog_Print(file_context->log, WLOG_ERROR,
		           "Failed to send FileContentsRequest for file \"%s\"",
		           fuse_file->filename_with_root);
		HashTable_Remove(file_context->request_table, (void*)(uintptr_t)fuse_request->stream_id);
		return FALSE;
	}
	DEBUG_CLIPRDR(file_context->log, "Requested file size for file \"%s\" with stream id %u",
	              fuse_file->filename, fuse_request->stream_id);

	return TRUE;
}

static void write_file_attributes(CliprdrFuseFile* fuse_file, struct stat* attr)
{
	memset(attr, 0, sizeof(struct stat));

	if (!fuse_file)
		return;

	attr->st_ino = fuse_file->ino;
	if (fuse_file->is_directory)
	{
		attr->st_mode = S_IFDIR | (fuse_file->is_readonly ? 0555 : 0755);
		attr->st_nlink = 2;
	}
	else
	{
		attr->st_mode = S_IFREG | (fuse_file->is_readonly ? 0444 : 0644);
		attr->st_nlink = 1;
		attr->st_size = fuse_file->size;
	}
	attr->st_uid = getuid();
	attr->st_gid = getgid();
	attr->st_atime = attr->st_mtime = attr->st_ctime =
	    (fuse_file->has_last_write_time ? fuse_file->last_write_time_unix : time(NULL));
}

static void cliprdr_file_fuse_lookup(fuse_req_t fuse_req, fuse_ino_t parent_ino, const char* name)
{
	CliprdrFileContext* file_context = fuse_req_userdata(fuse_req);
	CliprdrFuseFile* parent = NULL;
	CliprdrFuseFile* fuse_file = NULL;
	struct fuse_entry_param entry = { 0 };

	WINPR_ASSERT(file_context);

	HashTable_Lock(file_context->inode_table);
	if (!(parent = get_fuse_file_by_ino(file_context, parent_ino)) || !parent->is_directory)
	{
		HashTable_Unlock(file_context->inode_table);
		fuse_reply_err(fuse_req, ENOENT);
		return;
	}
	if (!(fuse_file = get_fuse_file_by_name_from_parent(file_context, parent, name)))
	{
		HashTable_Unlock(file_context->inode_table);
		fuse_reply_err(fuse_req, ENOENT);
		return;
	}

	DEBUG_CLIPRDR(file_context->log, "lookup() has been called for \"%s\"", name);
	DEBUG_CLIPRDR(file_context->log, "Parent is \"%s\", child is \"%s\"",
	              parent->filename_with_root, fuse_file->filename_with_root);

	if (!fuse_file->is_directory && !fuse_file->has_size)
	{
		BOOL result = 0;

		result =
		    request_file_size_async(file_context, fuse_file, fuse_req, FUSE_LL_OPERATION_LOOKUP);
		HashTable_Unlock(file_context->inode_table);

		if (!result)
			fuse_reply_err(fuse_req, EIO);

		return;
	}

	entry.ino = fuse_file->ino;
	write_file_attributes(fuse_file, &entry.attr);
	entry.attr_timeout = 1.0;
	entry.entry_timeout = 1.0;
	HashTable_Unlock(file_context->inode_table);

	fuse_reply_entry(fuse_req, &entry);
}

static void cliprdr_file_fuse_getattr(fuse_req_t fuse_req, fuse_ino_t fuse_ino,
                                      struct fuse_file_info* file_info)
{
	CliprdrFileContext* file_context = fuse_req_userdata(fuse_req);
	CliprdrFuseFile* fuse_file = NULL;
	struct stat attr = { 0 };

	WINPR_ASSERT(file_context);

	HashTable_Lock(file_context->inode_table);
	if (!(fuse_file = get_fuse_file_by_ino(file_context, fuse_ino)))
	{
		HashTable_Unlock(file_context->inode_table);
		fuse_reply_err(fuse_req, ENOENT);
		return;
	}

	DEBUG_CLIPRDR(file_context->log, "getattr() has been called for file \"%s\"",
	              fuse_file->filename_with_root);

	if (!fuse_file->is_directory && !fuse_file->has_size)
	{
		BOOL result = 0;

		result =
		    request_file_size_async(file_context, fuse_file, fuse_req, FUSE_LL_OPERATION_GETATTR);
		HashTable_Unlock(file_context->inode_table);

		if (!result)
			fuse_reply_err(fuse_req, EIO);

		return;
	}

	write_file_attributes(fuse_file, &attr);
	HashTable_Unlock(file_context->inode_table);

	fuse_reply_attr(fuse_req, &attr, 1.0);
}

static void cliprdr_file_fuse_open(fuse_req_t fuse_req, fuse_ino_t fuse_ino,
                                   struct fuse_file_info* file_info)
{
	CliprdrFileContext* file_context = fuse_req_userdata(fuse_req);
	CliprdrFuseFile* fuse_file = NULL;

	WINPR_ASSERT(file_context);

	HashTable_Lock(file_context->inode_table);
	if (!(fuse_file = get_fuse_file_by_ino(file_context, fuse_ino)))
	{
		HashTable_Unlock(file_context->inode_table);
		fuse_reply_err(fuse_req, ENOENT);
		return;
	}
	if (fuse_file->is_directory)
	{
		HashTable_Unlock(file_context->inode_table);
		fuse_reply_err(fuse_req, EISDIR);
		return;
	}
	HashTable_Unlock(file_context->inode_table);

	if ((file_info->flags & O_ACCMODE) != O_RDONLY)
	{
		fuse_reply_err(fuse_req, EACCES);
		return;
	}

	/* Important for KDE to get file correctly */
	file_info->direct_io = 1;

	fuse_reply_open(fuse_req, file_info);
}

static BOOL request_file_range_async(CliprdrFileContext* file_context, CliprdrFuseFile* fuse_file,
                                     fuse_req_t fuse_req, off_t offset, size_t requested_size)
{
	CliprdrFuseRequest* fuse_request = NULL;
	CLIPRDR_FILE_CONTENTS_REQUEST file_contents_request = { 0 };

	WINPR_ASSERT(file_context);
	WINPR_ASSERT(fuse_file);

	fuse_request =
	    cliprdr_fuse_request_new(file_context, fuse_file, fuse_req, FUSE_LL_OPERATION_READ);
	if (!fuse_request)
		return FALSE;

	file_contents_request.common.msgType = CB_FILECONTENTS_REQUEST;
	file_contents_request.streamId = fuse_request->stream_id;
	file_contents_request.listIndex = fuse_file->list_idx;
	file_contents_request.dwFlags = FILECONTENTS_RANGE;
	file_contents_request.nPositionLow = offset & 0xFFFFFFFF;
	file_contents_request.nPositionHigh = offset >> 32 & 0xFFFFFFFF;
	file_contents_request.cbRequested = requested_size;
	file_contents_request.haveClipDataId = fuse_file->has_clip_data_id;
	file_contents_request.clipDataId = fuse_file->clip_data_id;

	if (file_context->context->ClientFileContentsRequest(file_context->context,
	                                                     &file_contents_request))
	{
		WLog_Print(file_context->log, WLOG_ERROR,
		           "Failed to send FileContentsRequest for file \"%s\"",
		           fuse_file->filename_with_root);
		HashTable_Remove(file_context->request_table, (void*)(uintptr_t)fuse_request->stream_id);
		return FALSE;
	}

	// file_context->request_table owns fuse_request
	// NOLINTBEGIN(clang-analyzer-unix.Malloc)
	DEBUG_CLIPRDR(
	    file_context->log,
	    "Requested file range (%zu Bytes at offset %lu) for file \"%s\" with stream id %u",
	    requested_size, offset, fuse_file->filename, fuse_request->stream_id);

	return TRUE;
	// NOLINTEND(clang-analyzer-unix.Malloc)
}

static void cliprdr_file_fuse_read(fuse_req_t fuse_req, fuse_ino_t fuse_ino, size_t size,
                                   off_t offset, struct fuse_file_info* file_info)
{
	CliprdrFileContext* file_context = fuse_req_userdata(fuse_req);
	CliprdrFuseFile* fuse_file = NULL;
	BOOL result = 0;

	WINPR_ASSERT(file_context);

	HashTable_Lock(file_context->inode_table);
	if (!(fuse_file = get_fuse_file_by_ino(file_context, fuse_ino)))
	{
		HashTable_Unlock(file_context->inode_table);
		fuse_reply_err(fuse_req, ENOENT);
		return;
	}
	if (fuse_file->is_directory)
	{
		HashTable_Unlock(file_context->inode_table);
		fuse_reply_err(fuse_req, EISDIR);
		return;
	}
	if (!fuse_file->has_size || (offset < 0) || ((size_t)offset > fuse_file->size))
	{
		HashTable_Unlock(file_context->inode_table);
		fuse_reply_err(fuse_req, EINVAL);
		return;
	}

	size = MIN(size, 8ULL * 1024ULL * 1024ULL);

	result = request_file_range_async(file_context, fuse_file, fuse_req, offset, size);
	HashTable_Unlock(file_context->inode_table);

	if (!result)
		fuse_reply_err(fuse_req, EIO);
}

static void cliprdr_file_fuse_opendir(fuse_req_t fuse_req, fuse_ino_t fuse_ino,
                                      struct fuse_file_info* file_info)
{
	CliprdrFileContext* file_context = fuse_req_userdata(fuse_req);
	CliprdrFuseFile* fuse_file = NULL;

	WINPR_ASSERT(file_context);

	HashTable_Lock(file_context->inode_table);
	if (!(fuse_file = get_fuse_file_by_ino(file_context, fuse_ino)))
	{
		HashTable_Unlock(file_context->inode_table);
		fuse_reply_err(fuse_req, ENOENT);
		return;
	}
	if (!fuse_file->is_directory)
	{
		HashTable_Unlock(file_context->inode_table);
		fuse_reply_err(fuse_req, ENOTDIR);
		return;
	}
	HashTable_Unlock(file_context->inode_table);

	if ((file_info->flags & O_ACCMODE) != O_RDONLY)
	{
		fuse_reply_err(fuse_req, EACCES);
		return;
	}

	fuse_reply_open(fuse_req, file_info);
}

static void cliprdr_file_fuse_readdir(fuse_req_t fuse_req, fuse_ino_t fuse_ino, size_t max_size,
                                      off_t offset, struct fuse_file_info* file_info)
{
	CliprdrFileContext* file_context = fuse_req_userdata(fuse_req);
	CliprdrFuseFile* fuse_file = NULL;
	CliprdrFuseFile* child = NULL;
	struct stat attr = { 0 };
	size_t written_size = 0;
	size_t entry_size = 0;
	char* filename = NULL;
	char* buf = NULL;

	WINPR_ASSERT(file_context);

	HashTable_Lock(file_context->inode_table);
	if (!(fuse_file = get_fuse_file_by_ino(file_context, fuse_ino)))
	{
		HashTable_Unlock(file_context->inode_table);
		fuse_reply_err(fuse_req, ENOENT);
		return;
	}
	if (!fuse_file->is_directory)
	{
		HashTable_Unlock(file_context->inode_table);
		fuse_reply_err(fuse_req, ENOTDIR);
		return;
	}

	DEBUG_CLIPRDR(file_context->log, "Reading directory \"%s\" at offset %lu",
	              fuse_file->filename_with_root, offset);

	if ((offset < 0) || ((size_t)offset >= ArrayList_Count(fuse_file->children)))
	{
		HashTable_Unlock(file_context->inode_table);
		fuse_reply_buf(fuse_req, NULL, 0);
		return;
	}

	buf = calloc(max_size, sizeof(char));
	if (!buf)
	{
		HashTable_Unlock(file_context->inode_table);
		fuse_reply_err(fuse_req, ENOMEM);
		return;
	}
	written_size = 0;

	for (off_t i = offset; i < 2; ++i)
	{
		if (i == 0)
		{
			write_file_attributes(fuse_file, &attr);
			filename = ".";
		}
		else if (i == 1)
		{
			write_file_attributes(fuse_file->parent, &attr);
			attr.st_ino = fuse_file->parent ? attr.st_ino : FUSE_ROOT_ID;
			attr.st_mode = fuse_file->parent ? attr.st_mode : 0555;
			filename = "..";
		}
		else
		{
			WINPR_ASSERT(FALSE);
		}

		/**
		 * buf needs to be large enough to hold the entry. If it's not, then the
		 * entry is not filled in but the size of the entry is still returned.
		 */
		entry_size = fuse_add_direntry(fuse_req, buf + written_size, max_size - written_size,
		                               filename, &attr, i + 1);
		if (entry_size > max_size - written_size)
			break;

		written_size += entry_size;
	}

	for (size_t j = 0, i = 2; j < ArrayList_Count(fuse_file->children); ++j, ++i)
	{
		if (i < (size_t)offset)
			continue;

		child = ArrayList_GetItem(fuse_file->children, j);

		write_file_attributes(child, &attr);
		entry_size = fuse_add_direntry(fuse_req, buf + written_size, max_size - written_size,
		                               child->filename, &attr, i + 1);
		if (entry_size > max_size - written_size)
			break;

		written_size += entry_size;
	}
	HashTable_Unlock(file_context->inode_table);

	fuse_reply_buf(fuse_req, buf, written_size);
	free(buf);
}

static void fuse_abort(int sig, const char* signame, void* context)
{
	CliprdrFileContext* file = (CliprdrFileContext*)context;

	if (file)
	{
		WLog_Print(file->log, WLOG_INFO, "signal %s [%d] aborting session", signame, sig);
		cliprdr_file_session_terminate(file, FALSE);
	}
}

static DWORD WINAPI cliprdr_file_fuse_thread(LPVOID arg)
{
	CliprdrFileContext* file = (CliprdrFileContext*)arg;

	WINPR_ASSERT(file);

	DEBUG_CLIPRDR(file->log, "Starting fuse with mountpoint '%s'", file->path);

	struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
	fuse_opt_add_arg(&args, file->path);
	file->fuse_sess = fuse_session_new(&args, &cliprdr_file_fuse_oper,
	                                   sizeof(cliprdr_file_fuse_oper), (void*)file);
	(void)SetEvent(file->fuse_start_sync);

	if (file->fuse_sess != NULL)
	{
		freerdp_add_signal_cleanup_handler(file, fuse_abort);
		if (0 == fuse_session_mount(file->fuse_sess, file->path))
		{
			fuse_session_loop(file->fuse_sess);
			fuse_session_unmount(file->fuse_sess);
		}
		freerdp_del_signal_cleanup_handler(file, fuse_abort);

		WLog_Print(file->log, WLOG_DEBUG, "Waiting for FUSE stop sync");
		if (WaitForSingleObject(file->fuse_stop_sync, INFINITE) == WAIT_FAILED)
			WLog_Print(file->log, WLOG_ERROR, "Failed to wait for stop sync");
		fuse_session_destroy(file->fuse_sess);
	}
	fuse_opt_free_args(&args);

	DEBUG_CLIPRDR(file->log, "Quitting fuse with mountpoint '%s'", file->path);

	ExitThread(0);
	return 0;
}

static UINT cliprdr_file_context_server_file_contents_response(
    CliprdrClientContext* cliprdr_context,
    const CLIPRDR_FILE_CONTENTS_RESPONSE* file_contents_response)
{
	CliprdrFileContext* file_context = NULL;
	CliprdrFuseRequest* fuse_request = NULL;
	struct fuse_entry_param entry = { 0 };

	WINPR_ASSERT(cliprdr_context);
	WINPR_ASSERT(file_contents_response);

	file_context = cliprdr_context->custom;
	WINPR_ASSERT(file_context);

	HashTable_Lock(file_context->inode_table);
	fuse_request = HashTable_GetItemValue(file_context->request_table,
	                                      (void*)(uintptr_t)file_contents_response->streamId);
	if (!fuse_request)
	{
		HashTable_Unlock(file_context->inode_table);
		return CHANNEL_RC_OK;
	}

	if (!(file_contents_response->common.msgFlags & CB_RESPONSE_OK))
	{
		WLog_Print(file_context->log, WLOG_WARN,
		           "FileContentsRequests for file \"%s\" was unsuccessful",
		           fuse_request->fuse_file->filename);

		fuse_reply_err(fuse_request->fuse_req, EIO);
		HashTable_Remove(file_context->request_table,
		                 (void*)(uintptr_t)file_contents_response->streamId);
		HashTable_Unlock(file_context->inode_table);
		return CHANNEL_RC_OK;
	}

	if ((fuse_request->operation_type == FUSE_LL_OPERATION_LOOKUP ||
	     fuse_request->operation_type == FUSE_LL_OPERATION_GETATTR) &&
	    file_contents_response->cbRequested != sizeof(UINT64))
	{
		WLog_Print(file_context->log, WLOG_WARN,
		           "Received invalid file size for file \"%s\" from the client",
		           fuse_request->fuse_file->filename);
		fuse_reply_err(fuse_request->fuse_req, EIO);
		HashTable_Remove(file_context->request_table,
		                 (void*)(uintptr_t)file_contents_response->streamId);
		HashTable_Unlock(file_context->inode_table);
		return CHANNEL_RC_OK;
	}

	if (fuse_request->operation_type == FUSE_LL_OPERATION_LOOKUP ||
	    fuse_request->operation_type == FUSE_LL_OPERATION_GETATTR)
	{
		DEBUG_CLIPRDR(file_context->log, "Received file size for file \"%s\" with stream id %u",
		              fuse_request->fuse_file->filename, file_contents_response->streamId);

		fuse_request->fuse_file->size = *((const UINT64*)file_contents_response->requestedData);
		fuse_request->fuse_file->has_size = TRUE;

		entry.ino = fuse_request->fuse_file->ino;
		write_file_attributes(fuse_request->fuse_file, &entry.attr);
		entry.attr_timeout = 1.0;
		entry.entry_timeout = 1.0;
	}
	else if (fuse_request->operation_type == FUSE_LL_OPERATION_READ)
	{
		DEBUG_CLIPRDR(file_context->log, "Received file range for file \"%s\" with stream id %u",
		              fuse_request->fuse_file->filename, file_contents_response->streamId);
	}
	HashTable_Unlock(file_context->inode_table);

	switch (fuse_request->operation_type)
	{
		case FUSE_LL_OPERATION_NONE:
			break;
		case FUSE_LL_OPERATION_LOOKUP:
			fuse_reply_entry(fuse_request->fuse_req, &entry);
			break;
		case FUSE_LL_OPERATION_GETATTR:
			fuse_reply_attr(fuse_request->fuse_req, &entry.attr, entry.attr_timeout);
			break;
		case FUSE_LL_OPERATION_READ:
			fuse_reply_buf(fuse_request->fuse_req,
			               (const char*)file_contents_response->requestedData,
			               file_contents_response->cbRequested);
			break;
	}

	HashTable_Remove(file_context->request_table,
	                 (void*)(uintptr_t)file_contents_response->streamId);

	return CHANNEL_RC_OK;
}
#endif

static UINT cliprdr_file_context_send_file_contents_failure(
    CliprdrFileContext* file, const CLIPRDR_FILE_CONTENTS_REQUEST* fileContentsRequest)
{
	CLIPRDR_FILE_CONTENTS_RESPONSE response = { 0 };

	WINPR_ASSERT(file);
	WINPR_ASSERT(fileContentsRequest);

	const UINT64 offset = (((UINT64)fileContentsRequest->nPositionHigh) << 32) |
	                      ((UINT64)fileContentsRequest->nPositionLow);
	writelog(file->log, WLOG_WARN, __FILE__, __func__, __LINE__,
	         "server file contents request [lockID %" PRIu32 ", streamID %" PRIu32
	         ", index %" PRIu32 "] offset %" PRIu64 ", size %" PRIu32 " failed",
	         fileContentsRequest->clipDataId, fileContentsRequest->streamId,
	         fileContentsRequest->listIndex, offset, fileContentsRequest->cbRequested);

	response.common.msgFlags = CB_RESPONSE_FAIL;
	response.streamId = fileContentsRequest->streamId;

	WINPR_ASSERT(file->context);
	WINPR_ASSERT(file->context->ClientFileContentsResponse);
	return file->context->ClientFileContentsResponse(file->context, &response);
}

static UINT
cliprdr_file_context_send_contents_response(CliprdrFileContext* file,
                                            const CLIPRDR_FILE_CONTENTS_REQUEST* request,
                                            const void* data, size_t size)
{
	CLIPRDR_FILE_CONTENTS_RESPONSE response = { .streamId = request->streamId,
		                                        .requestedData = data,
		                                        .cbRequested = size,
		                                        .common.msgFlags = CB_RESPONSE_OK };

	WINPR_ASSERT(request);
	WINPR_ASSERT(file);

	WLog_Print(file->log, WLOG_DEBUG, "send contents response streamID=%" PRIu32 ", size=%" PRIu32,
	           response.streamId, response.cbRequested);
	WINPR_ASSERT(file->context);
	WINPR_ASSERT(file->context->ClientFileContentsResponse);
	return file->context->ClientFileContentsResponse(file->context, &response);
}

static BOOL dump_streams(const void* key, void* value, void* arg)
{
	const UINT32* ukey = key;
	CliprdrLocalStream* cur = value;

	writelog(cur->context->log, WLOG_WARN, __FILE__, __func__, __LINE__,
	         "[key %" PRIu32 "] lockID %" PRIu32 ", count %" PRIuz ", locked %d", *ukey,
	         cur->lockId, cur->count, cur->locked);
	for (size_t x = 0; x < cur->count; x++)
	{
		const CliprdrLocalFile* file = &cur->files[x];
		writelog(cur->context->log, WLOG_WARN, __FILE__, __func__, __LINE__, "file [%" PRIuz "] ",
		         x, file->name, file->size);
	}
	return TRUE;
}

static CliprdrLocalFile* file_info_for_request(CliprdrFileContext* file, UINT32 lockId,
                                               UINT32 listIndex)
{
	WINPR_ASSERT(file);

	CliprdrLocalStream* cur = HashTable_GetItemValue(file->local_streams, &lockId);
	if (cur)
	{
		if (listIndex < cur->count)
		{
			CliprdrLocalFile* f = &cur->files[listIndex];
			return f;
		}
		else
		{
			writelog(file->log, WLOG_WARN, __FILE__, __func__, __LINE__,
			         "invalid entry index for lockID %" PRIu32 ", index %" PRIu32 " [count %" PRIu32
			         "] [locked %d]",
			         lockId, listIndex, cur->count, cur->locked);
		}
	}
	else
	{
		writelog(file->log, WLOG_WARN, __FILE__, __func__, __LINE__,
		         "missing entry for lockID %" PRIu32 ", index %" PRIu32, lockId, listIndex);
		HashTable_Foreach(file->local_streams, dump_streams, file);
	}

	return NULL;
}

static CliprdrLocalFile* file_for_request(CliprdrFileContext* file, UINT32 lockId, UINT32 listIndex)
{
	CliprdrLocalFile* f = file_info_for_request(file, lockId, listIndex);
	if (f)
	{
		if (!f->fp)
		{
			const char* name = f->name;
			f->fp = winpr_fopen(name, "rb");
		}
		if (!f->fp)
		{
			char ebuffer[256] = { 0 };
			writelog(file->log, WLOG_WARN, __FILE__, __func__, __LINE__,
			         "[lockID %" PRIu32 ", index %" PRIu32
			         "] failed to open file '%s' [size %" PRId64 "] %s [%d]",
			         lockId, listIndex, f->name, f->size,
			         winpr_strerror(errno, ebuffer, sizeof(ebuffer)), errno);
			return NULL;
		}
	}

	return f;
}

static void cliprdr_local_file_try_close(CliprdrLocalFile* file, UINT res, UINT64 offset,
                                         UINT64 size)
{
	WINPR_ASSERT(file);

	if (res != 0)
	{
		WINPR_ASSERT(file->context);
		WLog_Print(file->context->log, WLOG_DEBUG, "closing file %s after error %" PRIu32,
		           file->name, res);
	}
	else if (((file->size > 0) && (offset + size >= (UINT64)file->size)))
	{
		WINPR_ASSERT(file->context);
		WLog_Print(file->context->log, WLOG_DEBUG, "closing file %s after read", file->name);
	}
	else
	{
		// TODO: we need to keep track of open files to avoid running out of file descriptors
		// TODO: for the time being just close again.
	}
	if (file->fp)
		(void)fclose(file->fp);
	file->fp = NULL;
}

static UINT cliprdr_file_context_server_file_size_request(
    CliprdrFileContext* file, const CLIPRDR_FILE_CONTENTS_REQUEST* fileContentsRequest)
{
	WINPR_ASSERT(fileContentsRequest);

	if (fileContentsRequest->cbRequested != sizeof(UINT64))
	{
		WLog_Print(file->log, WLOG_WARN, "unexpected FILECONTENTS_SIZE request: %" PRIu32 " bytes",
		           fileContentsRequest->cbRequested);
	}

	HashTable_Lock(file->local_streams);

	UINT res = CHANNEL_RC_OK;
	CliprdrLocalFile* rfile =
	    file_for_request(file, fileContentsRequest->clipDataId, fileContentsRequest->listIndex);
	if (!rfile)
		res = cliprdr_file_context_send_file_contents_failure(file, fileContentsRequest);
	else
	{
		if (_fseeki64(rfile->fp, 0, SEEK_END) < 0)
			res = cliprdr_file_context_send_file_contents_failure(file, fileContentsRequest);
		else
		{
			const INT64 size = _ftelli64(rfile->fp);
			rfile->size = size;
			cliprdr_local_file_try_close(rfile, res, 0, 0);

			res = cliprdr_file_context_send_contents_response(file, fileContentsRequest, &size,
			                                                  sizeof(size));
		}
	}

	HashTable_Unlock(file->local_streams);
	return res;
}

static UINT cliprdr_file_context_server_file_range_request(
    CliprdrFileContext* file, const CLIPRDR_FILE_CONTENTS_REQUEST* fileContentsRequest)
{
	BYTE* data = NULL;

	WINPR_ASSERT(fileContentsRequest);

	HashTable_Lock(file->local_streams);
	const UINT64 offset = (((UINT64)fileContentsRequest->nPositionHigh) << 32) |
	                      ((UINT64)fileContentsRequest->nPositionLow);

	CliprdrLocalFile* rfile =
	    file_for_request(file, fileContentsRequest->clipDataId, fileContentsRequest->listIndex);
	if (!rfile)
		goto fail;

	if (_fseeki64(rfile->fp, offset, SEEK_SET) < 0)
		goto fail;

	data = malloc(fileContentsRequest->cbRequested);
	if (!data)
		goto fail;

	const size_t r = fread(data, 1, fileContentsRequest->cbRequested, rfile->fp);
	const UINT rc = cliprdr_file_context_send_contents_response(file, fileContentsRequest, data, r);
	free(data);

	cliprdr_local_file_try_close(rfile, rc, offset, fileContentsRequest->cbRequested);
	HashTable_Unlock(file->local_streams);
	return rc;
fail:
	if (rfile)
		cliprdr_local_file_try_close(rfile, ERROR_INTERNAL_ERROR, offset,
		                             fileContentsRequest->cbRequested);
	free(data);
	HashTable_Unlock(file->local_streams);
	return cliprdr_file_context_send_file_contents_failure(file, fileContentsRequest);
}

static void cliprdr_local_stream_free(void* obj);

static UINT change_lock(CliprdrFileContext* file, UINT32 lockId, BOOL lock)
{
	UINT rc = CHANNEL_RC_OK;

	WINPR_ASSERT(file);

	HashTable_Lock(file->local_streams);

	{
		CliprdrLocalStream* stream = HashTable_GetItemValue(file->local_streams, &lockId);
		if (lock && !stream)
		{
			stream = cliprdr_local_stream_new(file, lockId, NULL, 0);
			if (!HashTable_Insert(file->local_streams, &lockId, stream))
			{
				rc = ERROR_INTERNAL_ERROR;
				cliprdr_local_stream_free(stream);
				stream = NULL;
			}
			file->local_lock_id = lockId;
		}
		if (stream)
		{
			stream->locked = lock;
			stream->lockId = lockId;
		}
	}

	// NOLINTNEXTLINE(clang-analyzer-unix.Malloc): HashTable_Insert ownership stream
	if (!lock)
	{
		if (!HashTable_Foreach(file->local_streams, local_stream_discard, file))
			rc = ERROR_INTERNAL_ERROR;
	}

	HashTable_Unlock(file->local_streams);
	return rc;
}

static UINT cliprdr_file_context_lock(CliprdrClientContext* context,
                                      const CLIPRDR_LOCK_CLIPBOARD_DATA* lockClipboardData)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(lockClipboardData);
	CliprdrFileContext* file = (context->custom);
	return change_lock(file, lockClipboardData->clipDataId, TRUE);
}

static UINT cliprdr_file_context_unlock(CliprdrClientContext* context,
                                        const CLIPRDR_UNLOCK_CLIPBOARD_DATA* unlockClipboardData)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(unlockClipboardData);
	CliprdrFileContext* file = (context->custom);
	return change_lock(file, unlockClipboardData->clipDataId, FALSE);
}

static UINT cliprdr_file_context_server_file_contents_request(
    CliprdrClientContext* context, const CLIPRDR_FILE_CONTENTS_REQUEST* fileContentsRequest)
{
	UINT error = NO_ERROR;

	WINPR_ASSERT(context);
	WINPR_ASSERT(fileContentsRequest);

	CliprdrFileContext* file = (context->custom);
	WINPR_ASSERT(file);

	/*
	 * MS-RDPECLIP 2.2.5.3 File Contents Request PDU (CLIPRDR_FILECONTENTS_REQUEST):
	 * The FILECONTENTS_SIZE and FILECONTENTS_RANGE flags MUST NOT be set at the same time.
	 */
	if ((fileContentsRequest->dwFlags & (FILECONTENTS_SIZE | FILECONTENTS_RANGE)) ==
	    (FILECONTENTS_SIZE | FILECONTENTS_RANGE))
	{
		WLog_Print(file->log, WLOG_ERROR, "invalid CLIPRDR_FILECONTENTS_REQUEST.dwFlags");
		return cliprdr_file_context_send_file_contents_failure(file, fileContentsRequest);
	}

	if (fileContentsRequest->dwFlags & FILECONTENTS_SIZE)
		error = cliprdr_file_context_server_file_size_request(file, fileContentsRequest);

	if (fileContentsRequest->dwFlags & FILECONTENTS_RANGE)
		error = cliprdr_file_context_server_file_range_request(file, fileContentsRequest);

	if (error)
	{
		WLog_Print(file->log, WLOG_ERROR, "failed to handle CLIPRDR_FILECONTENTS_REQUEST: 0x%08X",
		           error);
		return cliprdr_file_context_send_file_contents_failure(file, fileContentsRequest);
	}

	return CHANNEL_RC_OK;
}

BOOL cliprdr_file_context_init(CliprdrFileContext* file, CliprdrClientContext* cliprdr)
{
	WINPR_ASSERT(file);
	WINPR_ASSERT(cliprdr);

	cliprdr->custom = file;
	file->context = cliprdr;

	cliprdr->ServerLockClipboardData = cliprdr_file_context_lock;
	cliprdr->ServerUnlockClipboardData = cliprdr_file_context_unlock;
	cliprdr->ServerFileContentsRequest = cliprdr_file_context_server_file_contents_request;
#if defined(WITH_FUSE)
	cliprdr->ServerFileContentsResponse = cliprdr_file_context_server_file_contents_response;
#endif

	return TRUE;
}

#if defined(WITH_FUSE)
static void clear_all_selections(CliprdrFileContext* file_context)
{
	WINPR_ASSERT(file_context);
	WINPR_ASSERT(file_context->inode_table);

	HashTable_Lock(file_context->inode_table);
	clear_selection(file_context, TRUE, NULL);

	HashTable_Clear(file_context->clip_data_table);
	HashTable_Unlock(file_context->inode_table);
}
#endif

BOOL cliprdr_file_context_uninit(CliprdrFileContext* file, CliprdrClientContext* cliprdr)
{
	WINPR_ASSERT(file);
	WINPR_ASSERT(cliprdr);

	// Clear all data before the channel is closed
	// the cleanup handlers are dependent on a working channel.
#if defined(WITH_FUSE)
	if (file->inode_table)
	{
		clear_no_cdi_entry(file);
		clear_all_selections(file);
	}
#endif

	HashTable_Clear(file->local_streams);

	file->context = NULL;
#if defined(WITH_FUSE)
	cliprdr->ServerFileContentsResponse = NULL;
#endif

	return TRUE;
}

static BOOL cliprdr_file_content_changed_and_update(void* ihash, size_t hsize, const void* data,
                                                    size_t size)
{

	BYTE hash[WINPR_SHA256_DIGEST_LENGTH] = { 0 };

	if (hsize < sizeof(hash))
		return FALSE;

	if (!winpr_Digest(WINPR_MD_SHA256, data, size, hash, sizeof(hash)))
		return FALSE;

	const BOOL changed = memcmp(hash, ihash, sizeof(hash)) != 0;
	if (changed)
		memcpy(ihash, hash, sizeof(hash));
	return changed;
}

static BOOL cliprdr_file_server_content_changed_and_update(CliprdrFileContext* file,
                                                           const void* data, size_t size)
{
	WINPR_ASSERT(file);
	return cliprdr_file_content_changed_and_update(file->server_data_hash,
	                                               sizeof(file->server_data_hash), data, size);
}

static BOOL cliprdr_file_client_content_changed_and_update(CliprdrFileContext* file,
                                                           const void* data, size_t size)
{
	WINPR_ASSERT(file);
	return cliprdr_file_content_changed_and_update(file->client_data_hash,
	                                               sizeof(file->client_data_hash), data, size);
}

#if defined(WITH_FUSE)
static fuse_ino_t get_next_free_inode(CliprdrFileContext* file_context)
{
	fuse_ino_t ino = 0;

	WINPR_ASSERT(file_context);

	ino = file_context->next_ino;
	while (ino == 0 || ino == FUSE_ROOT_ID ||
	       HashTable_GetItemValue(file_context->inode_table, (void*)(uintptr_t)ino))
		++ino;

	file_context->next_ino = ino + 1;

	return ino;
}

static CliprdrFuseFile* clip_data_dir_new(CliprdrFileContext* file_context, BOOL has_clip_data_id,
                                          UINT32 clip_data_id)
{
	CliprdrFuseFile* root_dir = NULL;
	CliprdrFuseFile* clip_data_dir = NULL;
	size_t path_length = 0;

	WINPR_ASSERT(file_context);

	clip_data_dir = fuse_file_new();
	if (!clip_data_dir)
		return NULL;

	path_length = 1 + MAX_CLIP_DATA_DIR_LEN + 1;

	clip_data_dir->filename_with_root = calloc(path_length, sizeof(char));
	if (!clip_data_dir->filename_with_root)
	{
		WLog_Print(file_context->log, WLOG_ERROR, "Failed to allocate filename");
		fuse_file_free(clip_data_dir);
		return NULL;
	}

	if (has_clip_data_id)
		(void)_snprintf(clip_data_dir->filename_with_root, path_length, "/%u",
		                (unsigned)clip_data_id);
	else
		(void)_snprintf(clip_data_dir->filename_with_root, path_length, "/%" PRIu64,
		                NO_CLIP_DATA_ID);

	clip_data_dir->filename = strrchr(clip_data_dir->filename_with_root, '/') + 1;

	clip_data_dir->ino = get_next_free_inode(file_context);
	clip_data_dir->is_directory = TRUE;
	clip_data_dir->is_readonly = TRUE;
	clip_data_dir->has_clip_data_id = has_clip_data_id;
	clip_data_dir->clip_data_id = clip_data_id;

	root_dir = file_context->root_dir;
	if (!ArrayList_Append(root_dir->children, clip_data_dir))
	{
		WLog_Print(file_context->log, WLOG_ERROR, "Failed to append FUSE file");
		fuse_file_free(clip_data_dir);
		return NULL;
	}
	clip_data_dir->parent = root_dir;

	if (!HashTable_Insert(file_context->inode_table, (void*)(uintptr_t)clip_data_dir->ino,
	                      clip_data_dir))
	{
		WLog_Print(file_context->log, WLOG_ERROR, "Failed to insert inode into inode table");
		ArrayList_Remove(root_dir->children, clip_data_dir);
		fuse_file_free(clip_data_dir);
		return NULL;
	}

	return clip_data_dir;
}

static char* get_parent_path(const char* filepath)
{
	char* base = NULL;
	size_t parent_path_length = 0;
	char* parent_path = NULL;

	base = strrchr(filepath, '/');
	WINPR_ASSERT(base);

	while (base > filepath && *base == '/')
		--base;

	parent_path_length = 1 + base - filepath;
	parent_path = calloc(parent_path_length + 1, sizeof(char));
	if (!parent_path)
		return NULL;

	memcpy(parent_path, filepath, parent_path_length);

	return parent_path;
}

static BOOL is_fuse_file_not_parent(const void* key, void* value, void* arg)
{
	CliprdrFuseFile* fuse_file = value;
	CliprdrFuseFindParentContext* find_context = arg;

	if (!fuse_file->is_directory)
		return TRUE;

	if (strcmp(find_context->parent_path, fuse_file->filename_with_root) == 0)
	{
		find_context->parent = fuse_file;
		return FALSE;
	}

	return TRUE;
}

static CliprdrFuseFile* get_parent_directory(CliprdrFileContext* file_context, const char* path)
{
	CliprdrFuseFindParentContext find_context = { 0 };

	WINPR_ASSERT(file_context);
	WINPR_ASSERT(path);

	find_context.parent_path = get_parent_path(path);
	if (!find_context.parent_path)
		return NULL;

	WINPR_ASSERT(!find_context.parent);

	if (HashTable_Foreach(file_context->inode_table, is_fuse_file_not_parent, &find_context))
	{
		free(find_context.parent_path);
		return NULL;
	}
	WINPR_ASSERT(find_context.parent);

	free(find_context.parent_path);

	return find_context.parent;
}

static BOOL set_selection_for_clip_data_entry(CliprdrFileContext* file_context,
                                              CliprdrFuseClipDataEntry* clip_data_entry,
                                              const FILEDESCRIPTORW* files, UINT32 n_files)
{
	CliprdrFuseFile* clip_data_dir = NULL;

	WINPR_ASSERT(file_context);
	WINPR_ASSERT(clip_data_entry);
	WINPR_ASSERT(files);

	clip_data_dir = clip_data_entry->clip_data_dir;
	WINPR_ASSERT(clip_data_dir);

	if (clip_data_entry->has_clip_data_id)
		WLog_Print(file_context->log, WLOG_DEBUG, "Setting selection for clipDataId %u",
		           clip_data_entry->clip_data_id);
	else
		WLog_Print(file_context->log, WLOG_DEBUG, "Setting selection");

	// NOLINTBEGIN(clang-analyzer-unix.Malloc) HashTable_Insert owns fuse_file
	for (UINT32 i = 0; i < n_files; ++i)
	{
		const FILEDESCRIPTORW* file = &files[i];
		CliprdrFuseFile* fuse_file = NULL;
		char* filename = NULL;
		size_t path_length = 0;

		fuse_file = fuse_file_new();
		if (!fuse_file)
		{
			WLog_Print(file_context->log, WLOG_ERROR, "Failed to create FUSE file");
			clear_entry_selection(clip_data_entry);
			return FALSE;
		}

		filename = ConvertWCharToUtf8Alloc(file->cFileName, NULL);
		if (!filename)
		{
			WLog_Print(file_context->log, WLOG_ERROR, "Failed to convert filename");
			fuse_file_free(fuse_file);
			clear_entry_selection(clip_data_entry);
			return FALSE;
		}

		for (size_t j = 0; filename[j]; ++j)
		{
			if (filename[j] == '\\')
				filename[j] = '/';
		}

		path_length = strlen(clip_data_dir->filename_with_root) + 1 + strlen(filename) + 1;
		fuse_file->filename_with_root = calloc(path_length, sizeof(char));
		if (!fuse_file->filename_with_root)
		{
			WLog_Print(file_context->log, WLOG_ERROR, "Failed to allocate filename");
			free(filename);
			fuse_file_free(fuse_file);
			clear_entry_selection(clip_data_entry);
			return FALSE;
		}

		(void)_snprintf(fuse_file->filename_with_root, path_length, "%s/%s",
		                clip_data_dir->filename_with_root, filename);
		free(filename);

		fuse_file->filename = strrchr(fuse_file->filename_with_root, '/') + 1;

		fuse_file->parent = get_parent_directory(file_context, fuse_file->filename_with_root);
		if (!fuse_file->parent)
		{
			WLog_Print(file_context->log, WLOG_ERROR, "Found no parent for FUSE file");
			fuse_file_free(fuse_file);
			clear_entry_selection(clip_data_entry);
			return FALSE;
		}

		if (!ArrayList_Append(fuse_file->parent->children, fuse_file))
		{
			WLog_Print(file_context->log, WLOG_ERROR, "Failed to append FUSE file");
			fuse_file_free(fuse_file);
			clear_entry_selection(clip_data_entry);
			return FALSE;
		}

		fuse_file->list_idx = i;
		fuse_file->ino = get_next_free_inode(file_context);
		fuse_file->has_clip_data_id = clip_data_entry->has_clip_data_id;
		fuse_file->clip_data_id = clip_data_entry->clip_data_id;
		if (file->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			fuse_file->is_directory = TRUE;
		if (file->dwFileAttributes & FILE_ATTRIBUTE_READONLY)
			fuse_file->is_readonly = TRUE;
		if (file->dwFlags & FD_FILESIZE)
		{
			fuse_file->size = ((UINT64)file->nFileSizeHigh << 32) + file->nFileSizeLow;
			fuse_file->has_size = TRUE;
		}
		if (file->dwFlags & FD_WRITESTIME)
		{
			INT64 filetime = 0;

			filetime = file->ftLastWriteTime.dwHighDateTime;
			filetime <<= 32;
			filetime += file->ftLastWriteTime.dwLowDateTime;

			fuse_file->last_write_time_unix =
			    1LL * filetime / (10LL * 1000LL * 1000LL) - WIN32_FILETIME_TO_UNIX_EPOCH;
			fuse_file->has_last_write_time = TRUE;
		}

		if (!HashTable_Insert(file_context->inode_table, (void*)(uintptr_t)fuse_file->ino,
		                      fuse_file))
		{
			WLog_Print(file_context->log, WLOG_ERROR, "Failed to insert inode into inode table");
			fuse_file_free(fuse_file);
			clear_entry_selection(clip_data_entry);
			return FALSE;
		}
	}
	// NOLINTEND(clang-analyzer-unix.Malloc) HashTable_Insert owns fuse_file

	if (clip_data_entry->has_clip_data_id)
		WLog_Print(file_context->log, WLOG_DEBUG, "Selection set for clipDataId %u",
		           clip_data_entry->clip_data_id);
	else
		WLog_Print(file_context->log, WLOG_DEBUG, "Selection set");

	return TRUE;
}

static BOOL update_exposed_path(CliprdrFileContext* file_context, wClipboard* clip,
                                CliprdrFuseClipDataEntry* clip_data_entry)
{
	wClipboardDelegate* delegate = NULL;
	CliprdrFuseFile* clip_data_dir = NULL;

	WINPR_ASSERT(file_context);
	WINPR_ASSERT(clip);
	WINPR_ASSERT(clip_data_entry);

	delegate = ClipboardGetDelegate(clip);
	WINPR_ASSERT(delegate);

	clip_data_dir = clip_data_entry->clip_data_dir;
	WINPR_ASSERT(clip_data_dir);

	free(file_context->exposed_path);
	file_context->exposed_path = GetCombinedPath(file_context->path, clip_data_dir->filename);
	if (file_context->exposed_path)
		WLog_Print(file_context->log, WLOG_DEBUG, "Updated exposed path to \"%s\"",
		           file_context->exposed_path);

	delegate->basePath = file_context->exposed_path;

	return delegate->basePath != NULL;
}
#endif

BOOL cliprdr_file_context_update_server_data(CliprdrFileContext* file_context, wClipboard* clip,
                                             const void* data, size_t size)
{
#if defined(WITH_FUSE)
	CliprdrFuseClipDataEntry* clip_data_entry = NULL;
	FILEDESCRIPTORW* files = NULL;
	UINT32 n_files = 0;

	WINPR_ASSERT(file_context);
	WINPR_ASSERT(clip);

	if (cliprdr_parse_file_list(data, size, &files, &n_files))
	{
		WLog_Print(file_context->log, WLOG_ERROR, "Failed to parse file list");
		return FALSE;
	}

	HashTable_Lock(file_context->inode_table);
	if (does_server_support_clipdata_locking(file_context))
		clip_data_entry = HashTable_GetItemValue(
		    file_context->clip_data_table, (void*)(uintptr_t)file_context->current_clip_data_id);
	else
		clip_data_entry = file_context->clip_data_entry_without_id;

	WINPR_ASSERT(clip_data_entry);

	clear_entry_selection(clip_data_entry);
	WINPR_ASSERT(!clip_data_entry->clip_data_dir);

	clip_data_entry->clip_data_dir =
	    clip_data_dir_new(file_context, does_server_support_clipdata_locking(file_context),
	                      file_context->current_clip_data_id);
	if (!clip_data_entry->clip_data_dir)
	{
		HashTable_Unlock(file_context->inode_table);
		free(files);
		return FALSE;
	}

	if (!update_exposed_path(file_context, clip, clip_data_entry))
	{
		HashTable_Unlock(file_context->inode_table);
		free(files);
		return FALSE;
	}

	if (!set_selection_for_clip_data_entry(file_context, clip_data_entry, files, n_files))
	{
		HashTable_Unlock(file_context->inode_table);
		free(files);
		return FALSE;
	}
	HashTable_Unlock(file_context->inode_table);

	free(files);
	return TRUE;
#else
	return FALSE;
#endif
}

void* cliprdr_file_context_get_context(CliprdrFileContext* file)
{
	WINPR_ASSERT(file);
	return file->clipboard;
}

void cliprdr_file_session_terminate(CliprdrFileContext* file, BOOL stop_thread)
{
	if (!file)
		return;

#if defined(WITH_FUSE)
	WINPR_ASSERT(file->fuse_stop_sync);

	WLog_Print(file->log, WLOG_DEBUG, "Setting FUSE exit flag");
	if (file->fuse_sess)
		fuse_session_exit(file->fuse_sess);

	if (stop_thread)
	{
		WLog_Print(file->log, WLOG_DEBUG, "Setting FUSE stop event");
		(void)SetEvent(file->fuse_stop_sync);
	}
#endif
	/* 	not elegant but works for umounting FUSE
	    fuse_chan must receive an oper buf to unblock fuse_session_receive_buf function.
	*/
#if defined(WITH_FUSE)
	WLog_Print(file->log, WLOG_DEBUG, "Forcing FUSE to check exit flag");
#endif
	(void)winpr_PathFileExists(file->path);
}

void cliprdr_file_context_free(CliprdrFileContext* file)
{
	if (!file)
		return;

#if defined(WITH_FUSE)
	if (file->inode_table)
	{
		clear_no_cdi_entry(file);
		clear_all_selections(file);
	}

	if (file->fuse_thread)
	{
		WINPR_ASSERT(file->fuse_stop_sync);

		WLog_Print(file->log, WLOG_DEBUG, "Stopping FUSE thread");
		cliprdr_file_session_terminate(file, TRUE);

		WLog_Print(file->log, WLOG_DEBUG, "Waiting on FUSE thread");
		(void)WaitForSingleObject(file->fuse_thread, INFINITE);
		(void)CloseHandle(file->fuse_thread);
	}
	if (file->fuse_stop_sync)
		(void)CloseHandle(file->fuse_stop_sync);
	if (file->fuse_start_sync)
		(void)CloseHandle(file->fuse_start_sync);

	HashTable_Free(file->request_table);
	HashTable_Free(file->clip_data_table);
	HashTable_Free(file->inode_table);
#endif
	HashTable_Free(file->local_streams);
	winpr_RemoveDirectory(file->path);
	free(file->path);
	free(file->exposed_path);
	free(file);
}

static BOOL create_base_path(CliprdrFileContext* file)
{
	WINPR_ASSERT(file);

	char base[64] = { 0 };
	(void)_snprintf(base, sizeof(base), "com.freerdp.client.cliprdr.%" PRIu32,
	                GetCurrentProcessId());

	file->path = GetKnownSubPath(KNOWN_PATH_TEMP, base);
	if (!file->path)
		return FALSE;

	if (!winpr_PathFileExists(file->path) && !winpr_PathMakePath(file->path, 0))
	{
		WLog_Print(file->log, WLOG_ERROR, "Failed to create directory '%s'", file->path);
		return FALSE;
	}
	return TRUE;
}

static void cliprdr_local_file_free(CliprdrLocalFile* file)
{
	const CliprdrLocalFile empty = { 0 };
	if (!file)
		return;
	if (file->fp)
	{
		WLog_Print(file->context->log, WLOG_DEBUG, "closing file %s, discarding entry", file->name);
		(void)fclose(file->fp);
	}
	free(file->name);
	*file = empty;
}

static BOOL cliprdr_local_file_new(CliprdrFileContext* context, CliprdrLocalFile* f,
                                   const char* path)
{
	const CliprdrLocalFile empty = { 0 };
	WINPR_ASSERT(f);
	WINPR_ASSERT(context);
	WINPR_ASSERT(path);

	*f = empty;
	f->context = context;
	f->name = winpr_str_url_decode(path, strlen(path));
	if (!f->name)
		goto fail;

	return TRUE;
fail:
	cliprdr_local_file_free(f);
	return FALSE;
}

static void cliprdr_local_files_free(CliprdrLocalStream* stream)
{
	WINPR_ASSERT(stream);

	for (size_t x = 0; x < stream->count; x++)
		cliprdr_local_file_free(&stream->files[x]);
	free(stream->files);

	stream->files = NULL;
	stream->count = 0;
}

static void cliprdr_local_stream_free(void* obj)
{
	CliprdrLocalStream* stream = (CliprdrLocalStream*)obj;
	if (stream)
		cliprdr_local_files_free(stream);

	free(stream);
}

static BOOL append_entry(CliprdrLocalStream* stream, const char* path)
{
	CliprdrLocalFile* tmp = realloc(stream->files, sizeof(CliprdrLocalFile) * (stream->count + 1));
	if (!tmp)
		return FALSE;
	stream->files = tmp;
	CliprdrLocalFile* f = &stream->files[stream->count++];

	return cliprdr_local_file_new(stream->context, f, path);
}

static BOOL is_directory(const char* path)
{
	WCHAR* wpath = ConvertUtf8ToWCharAlloc(path, NULL);
	if (!wpath)
		return FALSE;

	HANDLE hFile =
	    CreateFileW(wpath, 0, FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	free(wpath);

	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	BY_HANDLE_FILE_INFORMATION fileInformation = { 0 };
	const BOOL status = GetFileInformationByHandle(hFile, &fileInformation);
	(void)CloseHandle(hFile);
	if (!status)
		return FALSE;

	return (fileInformation.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? TRUE : FALSE;
}

static BOOL add_directory(CliprdrLocalStream* stream, const char* path)
{
	char* wildcardpath = GetCombinedPath(path, "*");
	if (!wildcardpath)
		return FALSE;
	WCHAR* wpath = ConvertUtf8ToWCharAlloc(wildcardpath, NULL);
	free(wildcardpath);
	if (!wpath)
		return FALSE;

	WIN32_FIND_DATAW FindFileData = { 0 };
	HANDLE hFind = FindFirstFileW(wpath, &FindFileData);
	free(wpath);

	if (hFind == INVALID_HANDLE_VALUE)
		return FALSE;

	BOOL rc = FALSE;
	char* next = NULL;

	WCHAR dotbuffer[6] = { 0 };
	WCHAR dotdotbuffer[6] = { 0 };
	const WCHAR* dot = InitializeConstWCharFromUtf8(".", dotbuffer, ARRAYSIZE(dotbuffer));
	const WCHAR* dotdot = InitializeConstWCharFromUtf8("..", dotdotbuffer, ARRAYSIZE(dotdotbuffer));
	do
	{
		if (_wcscmp(FindFileData.cFileName, dot) == 0)
			continue;
		if (_wcscmp(FindFileData.cFileName, dotdot) == 0)
			continue;

		char cFileName[MAX_PATH] = { 0 };
		(void)ConvertWCharNToUtf8(FindFileData.cFileName, ARRAYSIZE(FindFileData.cFileName),
		                          cFileName, ARRAYSIZE(cFileName));

		free(next);
		next = GetCombinedPath(path, cFileName);
		if (!next)
			goto fail;

		if (!append_entry(stream, next))
			goto fail;
		if (is_directory(next))
		{
			if (!add_directory(stream, next))
				goto fail;
		}
	} while (FindNextFileW(hFind, &FindFileData));

	rc = TRUE;
fail:
	free(next);
	FindClose(hFind);

	return rc;
}

static BOOL cliprdr_local_stream_update(CliprdrLocalStream* stream, const char* data, size_t size)
{
	BOOL rc = FALSE;
	WINPR_ASSERT(stream);
	if (size == 0)
		return TRUE;

	cliprdr_local_files_free(stream);

	stream->files = calloc(size, sizeof(CliprdrLocalFile));
	if (!stream->files)
		return FALSE;

	char* copy = strndup(data, size);
	if (!copy)
		return FALSE;

	char* saveptr = NULL;
	char* ptr = strtok_s(copy, "\r\n", &saveptr);
	while (ptr)
	{
		const char* name = ptr;
		if (strncmp("file:///", ptr, 8) == 0)
			name = &ptr[7];
		else if (strncmp("file:/", ptr, 6) == 0)
			name = &ptr[5];

		if (!append_entry(stream, name))
			goto fail;

		if (is_directory(name))
		{
			const BOOL res = add_directory(stream, name);
			if (!res)
				goto fail;
		}
		ptr = strtok_s(NULL, "\r\n", &saveptr);
	}

	rc = TRUE;
fail:
	free(copy);
	return rc;
}

CliprdrLocalStream* cliprdr_local_stream_new(CliprdrFileContext* context, UINT32 streamID,
                                             const char* data, size_t size)
{
	WINPR_ASSERT(context);
	CliprdrLocalStream* stream = calloc(1, sizeof(CliprdrLocalStream));
	if (!stream)
		return NULL;

	stream->context = context;
	if (!cliprdr_local_stream_update(stream, data, size))
		goto fail;

	stream->lockId = streamID;
	return stream;

fail:
	cliprdr_local_stream_free(stream);
	return NULL;
}

static UINT32 UINTPointerHash(const void* id)
{
	WINPR_ASSERT(id);
	return *((const UINT32*)id);
}

static BOOL UINTPointerCompare(const void* pointer1, const void* pointer2)
{
	if (!pointer1 || !pointer2)
		return pointer1 == pointer2;

	const UINT32* a = pointer1;
	const UINT32* b = pointer2;
	return *a == *b;
}

static void* UINTPointerClone(const void* other)
{
	const UINT32* src = other;
	if (!src)
		return NULL;

	UINT32* copy = calloc(1, sizeof(UINT32));
	if (!copy)
		return NULL;

	*copy = *src;
	return copy;
}

#if defined(WITH_FUSE)
static CliprdrFuseFile* fuse_file_new_root(CliprdrFileContext* file_context)
{
	CliprdrFuseFile* root_dir = NULL;

	root_dir = fuse_file_new();
	if (!root_dir)
		return NULL;

	root_dir->filename_with_root = calloc(2, sizeof(char));
	if (!root_dir->filename_with_root)
	{
		fuse_file_free(root_dir);
		return NULL;
	}

	(void)_snprintf(root_dir->filename_with_root, 2, "/");
	root_dir->filename = root_dir->filename_with_root;

	root_dir->ino = FUSE_ROOT_ID;
	root_dir->is_directory = TRUE;
	root_dir->is_readonly = TRUE;

	if (!HashTable_Insert(file_context->inode_table, (void*)(uintptr_t)root_dir->ino, root_dir))
	{
		fuse_file_free(root_dir);
		return NULL;
	}

	return root_dir;
}
#endif

CliprdrFileContext* cliprdr_file_context_new(void* context)
{
	CliprdrFileContext* file = calloc(1, sizeof(CliprdrFileContext));
	if (!file)
		return NULL;

	file->log = WLog_Get(CLIENT_TAG("common.cliprdr.file"));
	file->clipboard = context;

	file->local_streams = HashTable_New(FALSE);
	if (!file->local_streams)
		goto fail;

	if (!HashTable_SetHashFunction(file->local_streams, UINTPointerHash))
		goto fail;

	wObject* hkobj = HashTable_KeyObject(file->local_streams);
	WINPR_ASSERT(hkobj);
	hkobj->fnObjectEquals = UINTPointerCompare;
	hkobj->fnObjectFree = free;
	hkobj->fnObjectNew = UINTPointerClone;

	wObject* hobj = HashTable_ValueObject(file->local_streams);
	WINPR_ASSERT(hobj);
	hobj->fnObjectFree = cliprdr_local_stream_free;

#if defined(WITH_FUSE)
	file->inode_table = HashTable_New(FALSE);
	file->clip_data_table = HashTable_New(FALSE);
	file->request_table = HashTable_New(FALSE);
	if (!file->inode_table || !file->clip_data_table || !file->request_table)
		goto fail;

	{
		wObject* ctobj = HashTable_ValueObject(file->request_table);
		WINPR_ASSERT(ctobj);
		ctobj->fnObjectFree = free;
	}
	{
		wObject* ctobj = HashTable_ValueObject(file->clip_data_table);
		WINPR_ASSERT(ctobj);
		ctobj->fnObjectFree = clip_data_entry_free;
	}

	file->root_dir = fuse_file_new_root(file);
	if (!file->root_dir)
		goto fail;
#endif

	if (!create_base_path(file))
		goto fail;

#if defined(WITH_FUSE)
	if (!(file->fuse_start_sync = CreateEvent(NULL, TRUE, FALSE, NULL)))
		goto fail;
	if (!(file->fuse_stop_sync = CreateEvent(NULL, TRUE, FALSE, NULL)))
		goto fail;
	if (!(file->fuse_thread = CreateThread(NULL, 0, cliprdr_file_fuse_thread, file, 0, NULL)))
		goto fail;

	if (WaitForSingleObject(file->fuse_start_sync, INFINITE) == WAIT_FAILED)
		WLog_Print(file->log, WLOG_ERROR, "Failed to wait for start sync");
#endif
	return file;

fail:
	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC
	cliprdr_file_context_free(file);
	WINPR_PRAGMA_DIAG_POP
	return NULL;
}

BOOL local_stream_discard(const void* key, void* value, void* arg)
{
	CliprdrFileContext* file = arg;
	CliprdrLocalStream* stream = value;
	WINPR_ASSERT(file);
	WINPR_ASSERT(stream);

	if (!stream->locked)
		HashTable_Remove(file->local_streams, key);
	return TRUE;
}

BOOL cliprdr_file_context_clear(CliprdrFileContext* file)
{
	WINPR_ASSERT(file);

	WLog_Print(file->log, WLOG_DEBUG, "clear file clipboard...");

	HashTable_Lock(file->local_streams);
	HashTable_Foreach(file->local_streams, local_stream_discard, file);
	HashTable_Unlock(file->local_streams);

	memset(file->server_data_hash, 0, sizeof(file->server_data_hash));
	memset(file->client_data_hash, 0, sizeof(file->client_data_hash));
	return TRUE;
}

BOOL cliprdr_file_context_update_client_data(CliprdrFileContext* file, const char* data,
                                             size_t size)
{
	BOOL rc = FALSE;

	WINPR_ASSERT(file);
	if (!cliprdr_file_client_content_changed_and_update(file, data, size))
		return TRUE;

	if (!cliprdr_file_context_clear(file))
		return FALSE;

	UINT32 lockId = file->local_lock_id;

	HashTable_Lock(file->local_streams);
	CliprdrLocalStream* stream = HashTable_GetItemValue(file->local_streams, &lockId);

	WLog_Print(file->log, WLOG_DEBUG, "update client file list (stream=%p)...", stream);
	if (stream)
		rc = cliprdr_local_stream_update(stream, data, size);
	else
	{
		stream = cliprdr_local_stream_new(file, lockId, data, size);
		rc = HashTable_Insert(file->local_streams, &stream->lockId, stream);
		if (!rc)
			cliprdr_local_stream_free(stream);
	}
	// HashTable_Insert owns stream
	// NOLINTNEXTLINE(clang-analyzer-unix.Malloc)
	HashTable_Unlock(file->local_streams);
	return rc;
}

UINT32 cliprdr_file_context_current_flags(CliprdrFileContext* file)
{
	WINPR_ASSERT(file);

	if ((file->file_capability_flags & CB_STREAM_FILECLIP_ENABLED) == 0)
		return 0;

	if (!file->file_formats_registered)
		return 0;

	return CB_STREAM_FILECLIP_ENABLED | CB_FILECLIP_NO_FILE_PATHS |
	       CB_HUGE_FILE_SUPPORT_ENABLED; // |	       CB_CAN_LOCK_CLIPDATA;
}

BOOL cliprdr_file_context_set_locally_available(CliprdrFileContext* file, BOOL available)
{
	WINPR_ASSERT(file);
	file->file_formats_registered = available;
	return TRUE;
}

BOOL cliprdr_file_context_remote_set_flags(CliprdrFileContext* file, UINT32 flags)
{
	WINPR_ASSERT(file);
	file->file_capability_flags = flags;
	return TRUE;
}

UINT32 cliprdr_file_context_remote_get_flags(CliprdrFileContext* file)
{
	WINPR_ASSERT(file);
	return file->file_capability_flags;
}

BOOL cliprdr_file_context_has_local_support(CliprdrFileContext* file)
{
	WINPR_UNUSED(file);

#if defined(WITH_FUSE)
	return TRUE;
#else
	return FALSE;
#endif
}
