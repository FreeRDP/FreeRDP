/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Clipboard Redirection
 *
 * Copyright 2010-2011 Vic Lee
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2023 Armin Novak <armin.novak@thincast.com>
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

#ifdef WITH_FUSE3
#define FUSE_USE_VERSION 30
#include <fuse_lowlevel.h>
#elif WITH_FUSE2
#define FUSE_USE_VERSION 26
#include <fuse_lowlevel.h>
#endif

#if defined(WITH_FUSE2) || defined(WITH_FUSE3)
#include <sys/mount.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#endif

#include <winpr/crt.h>
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

#define MAX_CLIPBOARD_FORMATS 255
#define WIN32_FILETIME_TO_UNIX_EPOCH_USEC UINT64_C(116444736000000000)

#ifdef WITH_DEBUG_CLIPRDR
#define DEBUG_CLIPRDR(log, ...) WLog_Print(log, WLOG_DEBUG, __VA_ARGS__)
#else
#define DEBUG_CLIPRDR(log, ...) \
	do                          \
	{                           \
	} while (0)
#endif

typedef struct
{
	UINT32 lockId;
	BOOL locked;
	CliprdrFileContext* context;
} CliprdrFileStreamLock;

#if defined(WITH_FUSE2) || defined(WITH_FUSE3)
typedef struct
{
	/* must be one of FILECONTENTS_SIZE or FILECONTENTS_RANGE*/
	UINT32 req_type;
	fuse_req_t req;
	/*for FILECONTENTS_SIZE must be ino number* */
	fuse_ino_t req_ino;
	UINT32 lockId;
	UINT32 streamID;
	CliprdrFileContext* context;
} CliprdrFuseRequest;

typedef struct
{
	size_t parent_ino;
	size_t ino;
	size_t lindex;
	mode_t st_mode;
	off_t st_size;
	BOOL size_set;
	struct timespec st_mtim;
	char* name;
	wArrayList* child_inos;
	UINT32 lockID;
} CliprdrFuseInode;
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
#if defined(WITH_FUSE2) || defined(WITH_FUSE3)
	/* FUSE related**/
	HANDLE fuse_thread;
	struct fuse_session* fuse_sess;

	/* fuse reset per copy*/
	wQueue* requests_in_flight;
	wArrayList* ino_list;
	fuse_ino_t current_inode_id;
	UINT32 next_stream_id;
#endif

	/* File clipping */
	BOOL file_formats_registered;
	UINT32 file_capability_flags;

	UINT32 local_lock_id;
	UINT32 remote_lock_id;

	wHashTable* remote_streams;
	wHashTable* local_streams;
	wLog* log;
	void* clipboard;
	CliprdrClientContext* context;
	char* path;
	char* current_path;
	char* exposed_path;
	BYTE server_data_hash[WINPR_SHA256_DIGEST_LENGTH];
	BYTE client_data_hash[WINPR_SHA256_DIGEST_LENGTH];
};

static CliprdrLocalStream* cliprdr_local_stream_new(CliprdrFileContext* context, UINT32 streamID,
                                                    const char* data, size_t size);
static void cliprdr_file_session_terminate(CliprdrFileContext* file);
static BOOL local_stream_discard(const void* key, void* value, void* arg);

static void writelog(wLog* log, DWORD level, const char* fname, const char* fkt, size_t line, ...)
{
	if (!WLog_IsLevelActive(log, level))
		return;

	va_list ap;
	va_start(ap, line);
	WLog_PrintMessageVA(log, WLOG_MESSAGE_TEXT, level, line, fname, fkt, ap);
	va_end(ap);
}

#if defined(WITH_FUSE2) || defined(WITH_FUSE3)
static BOOL cliprdr_file_fuse_remote_try_unlock(CliprdrFileStreamLock* stream);
#endif

static void cliprdr_file_stream_lock_free(void* arg)
{
	CliprdrFileStreamLock* stream = arg;
	if (!stream)
		return;
#if defined(WITH_FUSE2) || defined(WITH_FUSE3)
	cliprdr_file_fuse_remote_try_unlock(stream);
#endif
	free(arg);
}

static CliprdrFileStreamLock* cliprdr_fuse_stream_lock_new(CliprdrFileContext* context,
                                                           UINT32 lockID)
{
	WINPR_ASSERT(context);
	CliprdrFileStreamLock* stream =
	    (CliprdrFileStreamLock*)calloc(1, sizeof(CliprdrFileStreamLock));
	if (!stream)
		return NULL;
	stream->context = context;
	stream->lockId = lockID;
	return stream;
}

#if defined(WITH_FUSE2) || defined(WITH_FUSE3)
static BOOL cliprdr_fuse_repopulate(CliprdrFileContext* file);
static UINT cliprdr_file_send_client_file_contents_request(
    CliprdrFileContext* file, UINT32 streamId, UINT32 lockId, UINT32 listIndex, UINT32 dwFlags,
    UINT32 nPositionLow, UINT32 nPositionHigh, UINT32 cbRequested);

static void cliprdr_file_fuse_lookup(fuse_req_t req, fuse_ino_t parent, const char* name);
static void cliprdr_file_fuse_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi);
static void cliprdr_file_fuse_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
                                      struct fuse_file_info* fi);
static void cliprdr_file_fuse_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
                                   struct fuse_file_info* fi);
static void cliprdr_file_fuse_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi);
static void cliprdr_file_fuse_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi);

#define fuse_log_and_reply_err(file, req, err) \
	fuse_log_and_reply_err_((file), (req), (err), __FILE__, __FUNCTION__, __LINE__)
static void fuse_log_and_reply_err_(CliprdrFileContext* file, fuse_req_t req, int err,
                                    const char* fname, const char* fkt, size_t line)
{
	WINPR_ASSERT(file);
	WLog_Print(file->log, WLOG_DEBUG, "[%s:%" PRIuz "] fuse_reply_err %s [%d]", fkt, line,
	           strerror(err), err);
	fuse_reply_err(req, err);
}

#define fuse_log_and_reply_open(file, req, fi) \
	fuse_log_and_reply_open_((file), (req), (fi), __FILE__, __FUNCTION__, __LINE__)
static int fuse_log_and_reply_open_(CliprdrFileContext* file, fuse_req_t req,
                                    const struct fuse_file_info* fi, const char* fname,
                                    const char* fkt, size_t line)
{
	WINPR_ASSERT(file);

	const int res = fuse_reply_open(req, fi);
	if (res != 0)
		writelog(file->log, WLOG_WARN, fname, fkt, line, "[%s:%" PRIuz "] fuse_reply_open %s [%d]",
		         strerror(res), res);

	return res;
}

#define fuse_log_and_reply_entry(file, req, fi) \
	fuse_log_and_reply_entry_((file), (req), (fi), __FILE__, __FUNCTION__, __LINE__)
static int fuse_log_and_reply_entry_(CliprdrFileContext* file, fuse_req_t req,
                                     const struct fuse_entry_param* fi, const char* fname,
                                     const char* fkt, size_t line)
{
	WINPR_ASSERT(file);

	const int res = fuse_reply_entry(req, fi);
	if (res != 0)
		writelog(file->log, WLOG_WARN, fname, fkt, line, "[%s:%" PRIuz "] fuse_reply_entry %s [%d]",
		         strerror(res), res);
	return res;
}

#define fuse_log_and_reply_buf(file, req, buf, size) \
	fuse_log_and_reply_buf_((file), (req), (buf), (size), __FILE__, __FUNCTION__, __LINE__)
static int fuse_log_and_reply_buf_(CliprdrFileContext* file, fuse_req_t req, const char* buf,
                                   size_t size, const char* fname, const char* fkt, size_t line)
{
	WINPR_ASSERT(file);

	const int res = fuse_reply_buf(req, buf, size);
	if (res != 0)
		writelog(file->log, WLOG_WARN, fname, fkt, line, "[%s:%" PRIuz "] fuse_reply_buf %s [%d]",
		         strerror(res), res);
	return res;
}

static const struct fuse_lowlevel_ops cliprdr_file_fuse_oper = {
	.lookup = cliprdr_file_fuse_lookup,
	.getattr = cliprdr_file_fuse_getattr,
	.readdir = cliprdr_file_fuse_readdir,
	.open = cliprdr_file_fuse_open,
	.read = cliprdr_file_fuse_read,
	.opendir = cliprdr_file_fuse_opendir,
};

static void cliprdr_fuse_request_free(void* arg)
{
	CliprdrFuseRequest* request = arg;
	if (!request)
		return;
	if (request->req)
		fuse_log_and_reply_err(request->context, request->req, EIO);
	free(request);
}

static CliprdrFuseRequest* cliprdr_fuse_request_new(CliprdrFileContext* context, UINT32 lockID,
                                                    fuse_req_t r, fuse_ino_t ino, UINT32 type)
{
	WINPR_ASSERT(context);

	CliprdrFuseRequest* req = calloc(1, sizeof(CliprdrFuseRequest));
	if (!req)
		return NULL;

	req->context = context;
	req->lockId = lockID;
	req->req = r;
	req->req_ino = ino;
	req->req_type = type;
	req->streamID = context->next_stream_id++;
	return req;
}

static void cliprdr_file_fuse_node_free(void* obj)
{
	CliprdrFuseInode* inode = (CliprdrFuseInode*)obj;
	if (!inode)
		return;

	free(inode->name);
	ArrayList_Free(inode->child_inos);
	free(inode);
}

static CliprdrFuseInode* cliprdr_file_fuse_node_new(UINT32 lockID, size_t lindex, size_t ino,
                                                    size_t parent, const char* path, mode_t mode)
{
	CliprdrFuseInode* node = (CliprdrFuseInode*)calloc(1, sizeof(CliprdrFuseInode));
	if (!node)
		goto fail;

	node->lockID = lockID;
	node->ino = ino;
	node->parent_ino = parent;
	node->lindex = lindex;
	node->st_mode = mode;
	if (path)
	{
		node->name = _strdup(path);
		if (!node->name)
			goto fail;
	}
	node->child_inos = ArrayList_New(TRUE);
	if (!node->child_inos)
		goto fail;
	node->st_mtim.tv_sec = time(NULL);

	return node;

fail:
	cliprdr_file_fuse_node_free(node);
	return NULL;
}

/* For better understanding the relationship between ino and index of arraylist*/
#define cliprdr_file_fuse_util_get_inode(file, ino) \
	cliprdr_file_fuse_util_get_inode_((file), (ino), __FILE__, __FUNCTION__, __LINE__)
static inline CliprdrFuseInode* cliprdr_file_fuse_util_get_inode_(CliprdrFileContext* file,
                                                                  fuse_ino_t ino, const char* fname,
                                                                  const char* fkt, size_t line)
{
	size_t list_index = ino - FUSE_ROOT_ID;
	WINPR_ASSERT(file);

	CliprdrFuseInode* node = (CliprdrFuseInode*)ArrayList_GetItem(file->ino_list, list_index);
	if (!node)
		writelog(file->log, WLOG_WARN, fname, fkt, line,
		         "inode [0x%08" PRIx64 "][index %" PRIuz "] not found", ino, list_index);
	else
		writelog(file->log, WLOG_TRACE, fname, fkt, line,
		         "node %s [0x%09" PRIx64 "][index %" PRIuz "][parent 0x%08" PRIx64 "]", node->name,
		         node->ino, list_index, node->parent_ino);
	return node;
}

static BOOL dump_ino(void* data, size_t index, va_list ap)
{
	CliprdrFuseInode* node = (CliprdrFuseInode*)data;

	CliprdrFileContext* file = va_arg(ap, CliprdrFileContext*);
	const char* fname = va_arg(ap, const char*);
	const char* fkt = va_arg(ap, const char*);
	const size_t line = va_arg(ap, size_t);

	writelog(file->log, WLOG_TRACE, fname, fkt, line,
	         "node %s [0x%09" PRIx64 "][index %" PRIuz "][parent 0x%08" PRIx64 "]", node->name,
	         node->ino, index, node->parent_ino);
	return TRUE;
}

#define dump_inodes(file) dump_inodes_((file), __FILE__, __FUNCTION__, __LINE__)
static void dump_inodes_(CliprdrFileContext* file, const char* fname, const char* fkt, size_t line)
{
	WINPR_ASSERT(file);
	if (!WLog_IsLevelActive(file->log, WLOG_INFO))
		return;

	ArrayList_Lock(file->ino_list);
	ArrayList_ForEach(file->ino_list, dump_ino, file, fname, fkt, line);
	ArrayList_Unlock(file->ino_list);
}

/* the inode list is constructed as:
 *
 * 1. ROOT
 * 2. the streamID subfolders
 * 3. the files/folders of the first streamID
 * ...
 */
static inline CliprdrFuseInode*
cliprdr_file_fuse_util_get_inode_for_stream(CliprdrFileContext* file, UINT32 streamID)
{
	return cliprdr_file_fuse_util_get_inode(file, FUSE_ROOT_ID + streamID + 1);
}

/* fuse helper functions*/
static int cliprdr_file_fuse_util_stat(CliprdrFileContext* file, fuse_ino_t ino, struct stat* stbuf)
{
	int err = 0;
	CliprdrFuseInode* node;

	WINPR_ASSERT(file);
	WINPR_ASSERT(stbuf);

	ArrayList_Lock(file->ino_list);

	node = cliprdr_file_fuse_util_get_inode(file, ino);

	if (!node)
	{
		err = ENOENT;
		goto error;
	}
	memset(stbuf, 0, sizeof(*stbuf));
	stbuf->st_ino = ino;
	stbuf->st_mode = node->st_mode;
	stbuf->st_mtime = node->st_mtim.tv_sec;
	stbuf->st_nlink = 1;
	stbuf->st_size = node->st_size;
error:
	ArrayList_Unlock(file->ino_list);
	return err;
}

static int cliprdr_file_fuse_util_stmode(CliprdrFileContext* file, fuse_ino_t ino, mode_t* mode)
{
	int err = 0;
	CliprdrFuseInode* node;

	WINPR_ASSERT(file);
	WINPR_ASSERT(mode);

	ArrayList_Lock(file->ino_list);

	node = cliprdr_file_fuse_util_get_inode(file, ino);
	if (!node)
	{
		err = ENOENT;
		goto error;
	}
	*mode = node->st_mode;
error:
	ArrayList_Unlock(file->ino_list);
	return err;
}

static int cliprdr_file_fuse_util_lindex(CliprdrFileContext* file, fuse_ino_t ino, UINT32* lindex)
{
	int err = 0;
	CliprdrFuseInode* node;

	WINPR_ASSERT(file);
	WINPR_ASSERT(lindex);

	ArrayList_Lock(file->ino_list);

	node = cliprdr_file_fuse_util_get_inode(file, ino);
	if (!node)
	{
		err = ENOENT;
		goto error;
	}
	if ((node->st_mode & S_IFDIR) != 0)
	{
		err = EISDIR;
		goto error;
	}
	*lindex = node->lindex;

error:
	ArrayList_Unlock(file->ino_list);
	return err;
}

static BOOL cliprdr_file_fuse_remote_try_lock(CliprdrFileStreamLock* stream)
{
	UINT res = CHANNEL_RC_OK;

	WINPR_ASSERT(stream);
	WINPR_ASSERT(stream->context);

	if (cliprdr_file_context_remote_get_flags(stream->context) & CB_CAN_LOCK_CLIPDATA)
	{
		if (!stream->locked)
		{
			const CLIPRDR_LOCK_CLIPBOARD_DATA clip = { .clipDataId = stream->lockId };

			WINPR_ASSERT(stream->context->context);
			WINPR_ASSERT(stream->context->context->ClientLockClipboardData);
			res =
			    stream->context->context->ClientLockClipboardData(stream->context->context, &clip);
			stream->locked = res == CHANNEL_RC_OK;
		}
	}

	return res == CHANNEL_RC_OK;
}

static BOOL cliprdr_file_fuse_remote_try_unlock(CliprdrFileStreamLock* stream)
{
	UINT res = CHANNEL_RC_OK;

	WINPR_ASSERT(stream);
	WINPR_ASSERT(stream->context);

	if (cliprdr_file_context_remote_get_flags(stream->context) & CB_CAN_LOCK_CLIPDATA)
	{
		if (stream->locked)
		{
			const CLIPRDR_UNLOCK_CLIPBOARD_DATA clip = { .clipDataId = stream->lockId };

			WINPR_ASSERT(stream->context->context);
			WINPR_ASSERT(stream->context->context->ClientUnlockClipboardData);
			res = stream->context->context->ClientUnlockClipboardData(stream->context->context,
			                                                          &clip);
			if (res == CHANNEL_RC_OK)
				stream->locked = FALSE;
		}
	}

	return res == CHANNEL_RC_OK;
}

static int cliprdr_file_fuse_util_get_and_update_stream_list(CliprdrFileContext* file,
                                                             fuse_req_t req, size_t ino,
                                                             UINT32 type, UINT32* stream_id,
                                                             UINT32* lockId)
{
	int rc = ENOMEM;

	WINPR_ASSERT(file);
	WINPR_ASSERT(stream_id);

	ArrayList_Lock(file->ino_list);

	CliprdrFuseInode* node = cliprdr_file_fuse_util_get_inode(file, ino);
	if (node)
	{
		HashTable_Lock(file->remote_streams);
		CliprdrFileStreamLock* stream = HashTable_GetItemValue(file->remote_streams, &node->lockID);
		if (stream)
		{
			if (cliprdr_file_fuse_remote_try_lock(stream))
			{
				CliprdrFuseRequest* request =
				    cliprdr_fuse_request_new(file, stream->lockId, req, ino, type);
				if (request)
				{
					if (Queue_Enqueue(file->requests_in_flight, request))
					{
						*stream_id = request->streamID;
						*lockId = request->lockId;
						rc = 0;
					}
				}
			}
		}
		HashTable_Unlock(file->remote_streams);
	}
	ArrayList_Unlock(file->ino_list);
	return rc;
}

static int cliprdr_file_fuse_util_add_stream_list(CliprdrFileContext* file)
{
	int err = ENOMEM;

	WINPR_ASSERT(file);

	CliprdrFileStreamLock* stream = cliprdr_fuse_stream_lock_new(file, file->remote_lock_id++);
	if (!stream)
		return err;

	const BOOL res = HashTable_Insert(file->remote_streams, &stream->lockId, stream);
	if (!res)
	{
		cliprdr_file_stream_lock_free(stream);
		goto error;
	}

	if (!cliprdr_fuse_repopulate(file))
		goto error;

	err = 0;

error:
	return err;
}

void cliprdr_file_fuse_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi)
{
	int err;
	struct stat stbuf;

	CliprdrFileContext* file = (CliprdrFileContext*)fuse_req_userdata(req);
	WINPR_ASSERT(file);

	err = cliprdr_file_fuse_util_stat(file, ino, &stbuf);
	if (err)
	{
		fuse_log_and_reply_err(file, req, err);
		return;
	}

	fuse_reply_attr(req, &stbuf, 0);
}

static UINT cliprdr_file_fuse_readdir_int(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
                                          struct fuse_file_info* fi)
{
	UINT res = ENOMEM;
	size_t count;
	size_t direntry_len;
	char* buf = NULL;
	size_t pos = 0;
	CliprdrFileContext* file = (CliprdrFileContext*)fuse_req_userdata(req);

	WINPR_ASSERT(file);

	ArrayList_Lock(file->ino_list);
	CliprdrFuseInode* node = cliprdr_file_fuse_util_get_inode(file, ino);

	if (!node || !node->child_inos)
	{
		res = ENOENT;
		goto final2;
	}

	if ((node->st_mode & S_IFDIR) == 0)
	{
		res = ENOTDIR;
		goto final2;
	}

	ArrayList_Lock(node->child_inos);
	count = ArrayList_Count(node->child_inos);

	res = 0;
	if (count < off)
		goto final;

	res = ENOMEM;
	buf = (char*)calloc(size, sizeof(char));
	if (!buf)
		goto final;

	for (size_t index = off; index < count + 2; index++)
	{
		struct stat stbuf = { 0 };

		switch (index)
		{
			case 0:
			{
				stbuf.st_ino = ino;
				direntry_len = fuse_add_direntry(req, buf + pos, size - pos, ".", &stbuf, index);
				if (direntry_len > size - pos)
					break;
				pos += direntry_len;
			}
			break;
			case 1:
			{
				stbuf.st_ino = node->parent_ino;
				direntry_len = fuse_add_direntry(req, buf + pos, size - pos, "..", &stbuf, index);
				if (direntry_len > size - pos)
					break;
				pos += direntry_len;
			}
			break;
			default:
			{
				/* execlude . and .. */
				/* previous lock for ino_list still work*/
				CliprdrFuseInode* child_node = ArrayList_GetItem(node->child_inos, index - 2);
				if (!child_node)
					break;

				stbuf.st_ino = child_node->ino;
				direntry_len =
				    fuse_add_direntry(req, buf + pos, size - pos, child_node->name, &stbuf, index);
				if (direntry_len > size - pos)
					break;
				pos += direntry_len;
			}
			break;
		}
	}

	res = 0;

final:
	ArrayList_Unlock(node->child_inos);
final2:
	ArrayList_Unlock(file->ino_list);
	if (res == 0)
		fuse_reply_buf(req, buf, pos);
	free(buf);
	return res;
}

void cliprdr_file_fuse_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
                               struct fuse_file_info* fi)
{
	CliprdrFileContext* file = (CliprdrFileContext*)fuse_req_userdata(req);

	WINPR_ASSERT(file);

	ArrayList_Lock(file->ino_list);
	const UINT rc = cliprdr_file_fuse_readdir_int(req, ino, size, off, fi);
	ArrayList_Unlock(file->ino_list);
	if (rc != 0)
		fuse_log_and_reply_err(file, req, rc);
}

void cliprdr_file_fuse_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi)
{
	int err;
	mode_t mode = 0;
	CliprdrFileContext* file = (CliprdrFileContext*)fuse_req_userdata(req);

	WINPR_ASSERT(file);
	err = cliprdr_file_fuse_util_stmode(file, ino, &mode);
	if (err)
	{
		fuse_log_and_reply_err(file, req, err);
		return;
	}

	if ((mode & S_IFDIR) != 0)
	{
		fuse_log_and_reply_err(file, req, EISDIR);
	}
	else
	{
		/* Important for KDE to get file correctly*/
		fi->direct_io = 1;
		fuse_log_and_reply_open(file, req, fi);
	}
}

void cliprdr_file_fuse_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
                            struct fuse_file_info* fi)
{
	int err;
	CliprdrFileContext* file = (CliprdrFileContext*)fuse_req_userdata(req);
	UINT32 lindex = 0;
	UINT32 stream_id = 0;
	UINT32 lock_id = 0;

	WINPR_ASSERT(file);

	err = cliprdr_file_fuse_util_lindex(file, ino, &lindex);
	if (err)
	{
		fuse_log_and_reply_err(file, req, err);
		return;
	}

	err = cliprdr_file_fuse_util_get_and_update_stream_list(file, req, ino, FILECONTENTS_RANGE,
	                                                        &stream_id, &lock_id);
	if (err)
	{
		fuse_log_and_reply_err(file, req, err);
		return;
	}

	UINT32 nPositionLow = (off >> 0) & 0xFFFFFFFF;
	UINT32 nPositionHigh = (off >> 32) & 0xFFFFFFFF;

	cliprdr_file_send_client_file_contents_request(
	    file, stream_id, lock_id, lindex, FILECONTENTS_RANGE, nPositionLow, nPositionHigh, size);
}

void cliprdr_file_fuse_lookup(fuse_req_t req, fuse_ino_t parent, const char* name)
{
	size_t index;
	size_t count;
	BOOL found = FALSE;
	struct fuse_entry_param e = { 0 };
	CliprdrFileContext* file = (CliprdrFileContext*)fuse_req_userdata(req);

	WINPR_ASSERT(file);

	WLog_Print(file->log, WLOG_DEBUG, "looking up file '%s', parent %" PRIu64, name, parent);

	ArrayList_Lock(file->ino_list);
	CliprdrFuseInode* parent_node = cliprdr_file_fuse_util_get_inode(file, parent);

	if (!parent_node || !parent_node->child_inos)
	{
		ArrayList_Unlock(file->ino_list);
		fuse_log_and_reply_err(file, req, ENOENT);
		return;
	}

	CliprdrFuseInode* child_node = NULL;
	ArrayList_Lock(parent_node->child_inos);
	count = ArrayList_Count(parent_node->child_inos);
	for (index = 0; index < count; index++)
	{
		child_node = ArrayList_GetItem(parent_node->child_inos, index);
		if (child_node && strcmp(name, child_node->name) == 0)
		{
			found = TRUE;
			break;
		}
	}
	ArrayList_Unlock(parent_node->child_inos);

	if (!found || !child_node)
	{
		ArrayList_Unlock(file->ino_list);
		fuse_log_and_reply_err(file, req, ENOENT);
		return;
	}

	BOOL size_set = child_node->size_set;
	size_t lindex = child_node->lindex;
	size_t ino = child_node->ino;
	mode_t st_mode = child_node->st_mode;
	off_t st_size = child_node->st_size;
	time_t tv_sec = child_node->st_mtim.tv_sec;
	ArrayList_Unlock(file->ino_list);

	if (!size_set)
	{
		UINT32 streamID = 0;
		UINT32 lockId = 0;
		const int err = cliprdr_file_fuse_util_get_and_update_stream_list(
		    file, req, ino, FILECONTENTS_SIZE, &streamID, &lockId);
		if (err != 0)
		{
			fuse_log_and_reply_err(file, req, err);
			return;
		}
		cliprdr_file_send_client_file_contents_request(file, streamID, lockId, lindex,
		                                               FILECONTENTS_SIZE, 0, 0, 0);
		return;
	}
	e.ino = ino;
	e.attr_timeout = 1.0;
	e.entry_timeout = 1.0;
	e.attr.st_ino = ino;
	e.attr.st_mode = st_mode;
	e.attr.st_nlink = 1;

	e.attr.st_size = st_size;
	e.attr.st_mtime = tv_sec;
	fuse_log_and_reply_entry(file, req, &e);
}

void cliprdr_file_fuse_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi)
{
	int err;
	mode_t mode = 0;
	CliprdrFileContext* file = (CliprdrFileContext*)fuse_req_userdata(req);
	WINPR_ASSERT(file);

	err = cliprdr_file_fuse_util_stmode(file, ino, &mode);
	if (err)
	{
		fuse_log_and_reply_err(file, req, err);
		return;
	}

	if ((mode & S_IFDIR) == 0)
	{
		fuse_log_and_reply_err(file, req, ENOTDIR);
	}
	else
	{
		fuse_log_and_reply_open(file, req, fi);
	}
}

static void fuse_abort(int sig, const char* signame, void* context)
{
	CliprdrFileContext* file = (CliprdrFileContext*)context;

	if (file)
	{
		WLog_Print(file->log, WLOG_INFO, "signal %s [%d] aborting session", signame, sig);
		cliprdr_file_session_terminate(file);
	}
}

static DWORD WINAPI cliprdr_file_fuse_thread(LPVOID arg)
{
	CliprdrFileContext* file = (CliprdrFileContext*)arg;

	WINPR_ASSERT(file);

	DEBUG_CLIPRDR(file->log, "Starting fuse with mountpoint '%s'", file->path);

	struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
#if FUSE_USE_VERSION >= 30
	fuse_opt_add_arg(&args, file->path);
	if ((file->fuse_sess = fuse_session_new(&args, &cliprdr_file_fuse_oper,
	                                        sizeof(cliprdr_file_fuse_oper), (void*)file)) != NULL)
	{
		freerdp_add_signal_cleanup_handler(file, fuse_abort);
		if (0 == fuse_session_mount(file->fuse_sess, file->path))
		{
			fuse_session_loop(file->fuse_sess);
			fuse_session_unmount(file->fuse_sess);
		}
		freerdp_del_signal_cleanup_handler(file, fuse_abort);
		fuse_session_destroy(file->fuse_sess);
	}
#else
	struct fuse_chan* ch = fuse_mount(file->path, &args);
	if (ch != NULL)
	{
		file->fuse_sess = fuse_lowlevel_new(&args, &cliprdr_file_fuse_oper,
		                                    sizeof(cliprdr_file_fuse_oper), (void*)file);
		if (file->fuse_sess != NULL)
		{
			freerdp_add_signal_cleanup_handler(file, fuse_abort);
			fuse_session_add_chan(file->fuse_sess, ch);
			const int err = fuse_session_loop(file->fuse_sess);
			if (err != 0)
				WLog_Print(file->log, WLOG_WARN, "fuse_session_loop failed with %d", err);
			fuse_session_remove_chan(ch);
			freerdp_del_signal_cleanup_handler(file, fuse_abort);
			fuse_session_destroy(file->fuse_sess);
		}
		fuse_unmount(file->path, ch);
	}
#endif
	fuse_opt_free_args(&args);

	DEBUG_CLIPRDR(file->log, "Quitting fuse with mountpoint '%s'", file->path);

	ExitThread(0);
	return 0;
}

static CliprdrFuseInode* cliprdr_file_fuse_create_root_node(CliprdrFileContext* file)
{
	WINPR_ASSERT(file);

	CliprdrFuseInode* rootNode =
	    cliprdr_file_fuse_node_new(0, 0, FUSE_ROOT_ID, FUSE_ROOT_ID, "/", S_IFDIR | 0700);
	if (!rootNode)
		return NULL;

	rootNode->size_set = TRUE;
	return rootNode;
}

struct stream_node_arg
{
	wArrayList* list;
	CliprdrFuseInode* root;
	fuse_ino_t next_ino;
};

static BOOL update_sub_path(CliprdrFileContext* file, UINT32 lockID);

static BOOL add_stream_node(const void* key, void* value, void* arg)
{
	const CliprdrFileStreamLock* stream = value;
	struct stream_node_arg* node_arg = arg;
	WINPR_ASSERT(stream);
	WINPR_ASSERT(node_arg);
	WINPR_ASSERT(node_arg->root);

	char name[10] = { 0 };
	_snprintf(name, sizeof(name), "%08" PRIx32, stream->lockId);

	if (!update_sub_path(stream->context, stream->lockId))
		return FALSE;

	const size_t idx = ArrayList_Count(node_arg->list);
	CliprdrFuseInode* node = cliprdr_file_fuse_node_new(stream->lockId, idx, node_arg->next_ino++,
	                                                    FUSE_ROOT_ID, name, S_IFDIR | 0700);
	if (!node)
		return FALSE;
	node->size_set = TRUE;
	if (!ArrayList_Append(node_arg->list, node))
		goto fail;
	if (!ArrayList_Append(node_arg->root->child_inos, node))
		goto fail;
	return TRUE;

fail:
	cliprdr_file_fuse_node_free(node);
	return FALSE;
}

static BOOL cliprdr_fuse_repopulate(CliprdrFileContext* file)
{
	BOOL rc = FALSE;

	WINPR_ASSERT(file);

	HashTable_Lock(file->remote_streams);
	ArrayList_Lock(file->ino_list);
	ArrayList_Clear(file->ino_list);

	CliprdrFuseInode* root = cliprdr_file_fuse_create_root_node(file);
	if (!ArrayList_Append(file->ino_list, root))
		goto fail;

	struct stream_node_arg arg = { .root = root,
		                           .list = file->ino_list,
		                           .next_ino = FUSE_ROOT_ID + 1 };
	if (!HashTable_Foreach(file->remote_streams, add_stream_node, &arg))
		goto fail;
	rc = TRUE;

fail:
	ArrayList_Unlock(file->ino_list);
	HashTable_Unlock(file->remote_streams);

	dump_inodes(file);
	return rc;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_file_send_client_file_contents_request(CliprdrFileContext* file,
                                                           UINT32 streamId, UINT32 lockId,
                                                           UINT32 listIndex, UINT32 dwFlags,
                                                           UINT32 nPositionLow,
                                                           UINT32 nPositionHigh, UINT32 cbRequested)
{
	CLIPRDR_FILE_CONTENTS_REQUEST formatFileContentsRequest = {
		.streamId = streamId,
		.clipDataId = lockId,
		.haveClipDataId = cliprdr_file_context_current_flags(file) & CB_CAN_LOCK_CLIPDATA,
		.listIndex = listIndex,
		.dwFlags = dwFlags
	};

	WINPR_ASSERT(file);
	switch (dwFlags)
	{
		/*
		 * [MS-RDPECLIP] 2.2.5.3 File Contents Request PDU (CLIPRDR_FILECONTENTS_REQUEST).
		 *
		 * A request for the size of the file identified by the lindex field. The size MUST be
		 * returned as a 64-bit, unsigned integer. The cbRequested field MUST be set to
		 * 0x00000008 and both the nPositionLow and nPositionHigh fields MUST be
		 * set to 0x00000000.
		 */
		case FILECONTENTS_SIZE:
			formatFileContentsRequest.cbRequested = sizeof(UINT64);
			formatFileContentsRequest.nPositionHigh = 0;
			formatFileContentsRequest.nPositionLow = 0;
			break;
		case FILECONTENTS_RANGE:
			formatFileContentsRequest.cbRequested = cbRequested;
			formatFileContentsRequest.nPositionHigh = nPositionHigh;
			formatFileContentsRequest.nPositionLow = nPositionLow;
			break;
	}

	// TODO: Log this response
	WINPR_ASSERT(file->context);
	WINPR_ASSERT(file->context->ClientFileContentsRequest);
	return file->context->ClientFileContentsRequest(file->context, &formatFileContentsRequest);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT cliprdr_file_context_server_file_contents_response(
    CliprdrClientContext* context, const CLIPRDR_FILE_CONTENTS_RESPONSE* fileContentsResponse)
{
	CliprdrFuseInode* ino;
	UINT32 stream_id;
	const BYTE* data;
	size_t data_len;
	CliprdrFileContext* file;

	WINPR_ASSERT(context);
	WINPR_ASSERT(fileContentsResponse);

	file = context->custom;
	WINPR_ASSERT(file);

	stream_id = fileContentsResponse->streamId;
	data = fileContentsResponse->requestedData;
	data_len = fileContentsResponse->cbRequested;

	CliprdrFuseRequest* request = NULL;
	{
		CliprdrFuseRequest* cur = Queue_Dequeue(file->requests_in_flight);
		if (cur)
		{
			if (cur->streamID == stream_id)
				request = cur;
			else
			{
				WLog_Print(file->log, WLOG_WARN,
				           "file contents response streamID %" PRIu32
				           " does not match first in queue with streamID %" PRIu32,
				           stream_id, cur->streamID);
				cliprdr_fuse_request_free(cur);
			}
		}
	}

	if (fileContentsResponse->common.msgFlags & CB_RESPONSE_FAIL)
	{
		WLog_Print(file->log, WLOG_WARN,
		           "file contents response streamID %" PRIu32 ", size %" PRIu32
		           " status CB_RESPONSE_FAIL",
		           stream_id, data_len);
		if (request)
		{
			WLog_Print(file->log, WLOG_WARN,
			           "matching request: lockID %" PRIu32 ", ino %" PRIu64 ", type %" PRIu32,
			           request->lockId, request->req_ino, request->req_type);
			cliprdr_fuse_request_free(request);
		}
		else
			WLog_Print(file->log, WLOG_WARN, "no matching request found, abort");

		return CHANNEL_RC_OK;
	}

	if (request)
	{
		WLog_Print(file->log, WLOG_DEBUG,
		           "file contents response streamID %" PRIu32 ", size %" PRIu32, stream_id,
		           data_len);
		WLog_Print(file->log, WLOG_DEBUG,
		           "matching request: lockID %" PRIu32 ", ino %" PRIu64 ", type %" PRIu32,
		           request->lockId, request->req_ino, request->req_type);
	}
	else
	{
		WLog_Print(file->log, WLOG_WARN,
		           "file contents response streamID %" PRIu32 ", size %" PRIu32, stream_id,
		           data_len);
		WLog_Print(file->log, WLOG_WARN, "no matching request found, abort");

		return CHANNEL_RC_OK;
	}

	ArrayList_Lock(file->ino_list);
	switch (request->req_type)
	{
		case FILECONTENTS_SIZE:
			/* fileContentsResponse->cbRequested should be 64bit*/
			if (data_len != sizeof(UINT64))
				break;

			UINT64 size;
			wStream sbuffer = { 0 };
			wStream* s = Stream_StaticConstInit(&sbuffer, data, data_len);
			if (!s)
			{
				fuse_log_and_reply_err(file, request->req, ENOMEM);
				request->req = NULL; // reply handled, do not reply EIO on free
				break;
			}
			Stream_Read_UINT64(s, size);

			ino = cliprdr_file_fuse_util_get_inode(file, request->req_ino);
			/* ino must exist  */
			if (!ino)
				break;

			ino->st_size = size;
			ino->size_set = TRUE;
			struct fuse_entry_param e = { 0 };
			e.ino = ino->ino;
			e.attr_timeout = 1.0;
			e.entry_timeout = 1.0;
			e.attr.st_ino = ino->ino;
			e.attr.st_mode = ino->st_mode;
			e.attr.st_nlink = 1;
			e.attr.st_size = ino->st_size;
			e.attr.st_mtime = ino->st_mtim.tv_sec;

			fuse_log_and_reply_entry(file, request->req, &e);
			request->req = NULL; // reply handled, do not reply EIO on free
			break;
		case FILECONTENTS_RANGE:
			fuse_log_and_reply_buf(file, request->req, (const char*)data, data_len);
			request->req = NULL; // reply handled, do not reply EIO on free
			break;
		default:
			break;
	}
	ArrayList_Unlock(file->ino_list);
	cliprdr_fuse_request_free(request);
	return CHANNEL_RC_OK;
}

static char* cliprdr_file_fuse_split_basename(char* name, size_t len)
{
	WINPR_ASSERT(name || (len <= 0));
	for (size_t s = len; s > 0; s--)
	{
		char c = name[s - 1];
		if (c == '\\')
		{
			return &name[s - 1];
		}
	}
	return NULL;
}

static BOOL cliprdr_file_fuse_check_stream(CliprdrFileContext* file, wStream* s, size_t count)
{
	UINT32 nrDescriptors;

	WINPR_ASSERT(file);
	if (!Stream_CheckAndLogRequiredLengthWLog(file->log, s, sizeof(UINT32)))
		return FALSE;

	Stream_Read_UINT32(s, nrDescriptors);
	if (count != nrDescriptors)
	{
		WLog_Print(file->log, WLOG_WARN,
		           "format data response expected %" PRIuz " descriptors, but have %" PRIu32, count,
		           nrDescriptors);
		return FALSE;
	}
	return TRUE;
}

static BOOL cliprdr_file_fuse_create_nodes(CliprdrFileContext* file, wStream* s, size_t count,
                                           const CliprdrFuseInode* rootNode, size_t nextIno)
{
	BOOL status = FALSE;
	wHashTable* mapDir;

	WINPR_ASSERT(file);
	WINPR_ASSERT(s);
	WINPR_ASSERT(rootNode);

	mapDir = HashTable_New(TRUE);
	if (!mapDir)
	{
		WLog_Print(file->log, WLOG_ERROR, "fail to alloc hashtable");
		goto fail;
	}
	if (!HashTable_SetupForStringData(mapDir, FALSE))
		goto fail;

	/* here we assume that parent folder always appears before its children */
	for (size_t lindex = 0; lindex < count; lindex++)
	{
		FILEDESCRIPTORW descriptor = { 0 };
		char curName[ARRAYSIZE(descriptor.cFileName)] = { 0 };
		char dirName[ARRAYSIZE(descriptor.cFileName)] = { 0 };

		if (!cliprdr_read_filedescriptor(s, &descriptor))
			goto fail;

		ConvertWCharNToUtf8(descriptor.cFileName, ARRAYSIZE(descriptor.cFileName), curName,
		                    ARRAYSIZE(curName));
		memcpy(dirName, curName, ARRAYSIZE(dirName));

		char* split_point =
		    cliprdr_file_fuse_split_basename(dirName, strnlen(dirName, ARRAYSIZE(dirName)));

		UINT64 ticks;
		CliprdrFuseInode* parent = NULL;
		CliprdrFuseInode* inode = NULL;
		if (split_point == NULL)
		{
			inode = cliprdr_file_fuse_node_new(rootNode->lockID, lindex, nextIno++, rootNode->ino,
			                                   curName, 0700);
			if (!ArrayList_Append(file->ino_list, inode))
				goto fail;
			if (!ArrayList_Append(rootNode->child_inos, inode))
				goto fail;
		}
		else
		{
			*split_point = '\0';

			/* drop last '\\' */
			const char* baseName = (split_point + 1);

			parent = (CliprdrFuseInode*)HashTable_GetItemValue(mapDir, dirName);
			if (!parent)
				goto fail;
			inode = cliprdr_file_fuse_node_new(rootNode->lockID, lindex, nextIno++, parent->ino,
			                                   baseName, 0700);
			if (!inode)
				goto fail;
			if (!ArrayList_Append(file->ino_list, inode))
				goto fail;
			if (!ArrayList_Append(parent->child_inos, inode))
				goto fail;
		}
		/* TODO: check FD_ATTRIBUTES in dwFlags
		    However if this flag is not valid how can we determine file/folder?
		*/
		if ((descriptor.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
		{
			inode->st_mode = S_IFDIR | 0700;
			inode->size_set = TRUE;
			if (!HashTable_Insert(mapDir, curName, inode))
				goto fail;
		}
		else
		{
			inode->st_mode = S_IFREG | 0600;
			if ((descriptor.dwFlags & FD_FILESIZE) != 0)
			{
				inode->st_size =
				    (((UINT64)descriptor.nFileSizeHigh) << 32) | ((UINT64)descriptor.nFileSizeLow);
				inode->size_set = TRUE;
			}
		}

		if ((descriptor.dwFlags & FD_WRITESTIME) != 0)
		{
			ticks = (((UINT64)descriptor.ftLastWriteTime.dwHighDateTime << 32) |
			         ((UINT64)descriptor.ftLastWriteTime.dwLowDateTime)) -
			        WIN32_FILETIME_TO_UNIX_EPOCH_USEC;
			inode->st_mtim.tv_sec = ticks / 10000000ULL;
			/* tv_nsec Not used for now */
			inode->st_mtim.tv_nsec = ticks % 10000000ULL;
		}
	}

	status = TRUE;

fail:
	if (!status)
		cliprdr_fuse_repopulate(file);
	HashTable_Free(mapDir);
	return status;
}

/**
 * Generate inode list for fuse
 *
 * @return TRUE on success, FALSE on fail
 */
static BOOL cliprdr_file_fuse_generate_list(CliprdrFileContext* file, const BYTE* data, size_t size,
                                            UINT32 streamID)
{
	BOOL status = FALSE;
	wStream sbuffer = { 0 };

	WINPR_ASSERT(file);
	WINPR_ASSERT(data || (size == 0));

	WLog_Print(file->log, WLOG_DEBUG, "updating fuse file lists...");
	if (size < 4)
	{
		WLog_Print(file->log, WLOG_ERROR, "size of format data response invalid : %" PRIu32, size);
		return FALSE;
	}

	const size_t count = (size - 4) / sizeof(FILEDESCRIPTORW);
	if (count < 1)
	{
		WLog_Print(file->log, WLOG_ERROR, "empty file list received");
		return TRUE;
	}

	wStream* s = Stream_StaticConstInit(&sbuffer, data, size);
	if (!s)
	{
		WLog_Print(file->log, WLOG_ERROR, "Stream_New failed");
		return FALSE;
	}
	if (!cliprdr_file_fuse_check_stream(file, s, count))
		return FALSE;

	/* prevent conflict between fuse_thread and this */
	if (cliprdr_file_fuse_util_add_stream_list(file) < 0)
		return FALSE;

	ArrayList_Lock(file->ino_list);
	if (!cliprdr_fuse_repopulate(file))
		goto error2;

	CliprdrFuseInode* rootNode = cliprdr_file_fuse_util_get_inode_for_stream(file, streamID);

	if (!rootNode)
	{
		WLog_Print(file->log, WLOG_ERROR, "fail to alloc rootNode to ino_list");
		goto error2;
	}

	status = cliprdr_file_fuse_create_nodes(file, s, count, rootNode,
	                                        ArrayList_Count(file->ino_list) + FUSE_ROOT_ID);
error2:
	ArrayList_Unlock(file->ino_list);
	return status;
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
	writelog(file->log, WLOG_WARN, __FILE__, __FUNCTION__, __LINE__,
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

	writelog(cur->context->log, WLOG_WARN, __FILE__, __FUNCTION__, __LINE__,
	         "[key %" PRIu32 "] lockID %" PRIu32 ", count %" PRIuz ", locked %d", *ukey,
	         cur->lockId, cur->count, cur->locked);
	for (size_t x = 0; x < cur->count; x++)
	{
		const CliprdrLocalFile* file = &cur->files[x];
		writelog(cur->context->log, WLOG_WARN, __FILE__, __FUNCTION__, __LINE__,
		         "file [%" PRIuz "] ", x, file->name, file->size);
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
			writelog(file->log, WLOG_WARN, __FILE__, __FUNCTION__, __LINE__,
			         "invalid entry index for lockID %" PRIu32 ", index %" PRIu32 " [count %" PRIu32
			         "] [locked %d]",
			         lockId, listIndex, cur->count, cur->locked);
		}
	}
	else
	{
		writelog(file->log, WLOG_WARN, __FILE__, __FUNCTION__, __LINE__,
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
			writelog(file->log, WLOG_WARN, __FILE__, __FUNCTION__, __LINE__,
			         "[lockID %" PRIu32 ", index %" PRIu32
			         "] failed to open file '%s' [size %" PRId64 "] %s [%d]",
			         lockId, listIndex, f->name, f->size, strerror(errno), errno);
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
		fclose(file->fp);
		file->fp = NULL;
	}
	else if (((file->size > 0) && (offset + size >= file->size)))
	{
		WINPR_ASSERT(file->context);
		WLog_Print(file->context->log, WLOG_DEBUG, "closing file %s after read", file->name);
		fclose(file->fp);
		file->fp = NULL;
	}
	else
	{
		// TODO: we need to keep track of open files to avoid running out of file descriptors
		// TODO: for the time being just close again.
		fclose(file->fp);
		file->fp = NULL;
	}
}

static UINT cliprdr_file_context_server_file_size_request(
    CliprdrFileContext* file, const CLIPRDR_FILE_CONTENTS_REQUEST* fileContentsRequest)
{
	wClipboardFileSizeRequest request = { 0 };

	WINPR_ASSERT(fileContentsRequest);

	request.streamId = fileContentsRequest->streamId;
	request.listIndex = fileContentsRequest->listIndex;

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

static BOOL update_sub_path(CliprdrFileContext* file, UINT32 lockID)
{
	WINPR_ASSERT(file);
	WINPR_ASSERT(file->path);

	char lockstr[32] = { 0 };
	_snprintf(lockstr, sizeof(lockstr), "%08" PRIx32, lockID);

	HashTable_Lock(file->remote_streams);
	free(file->current_path);
	file->current_path = GetCombinedPath(file->path, lockstr);
	const BOOL res = file->current_path != NULL;
	HashTable_Unlock(file->remote_streams);
	return res;
}

static UINT change_lock(CliprdrFileContext* file, UINT32 lockId, BOOL lock)
{
	WINPR_ASSERT(file);

	HashTable_Lock(file->local_streams);
	CliprdrLocalStream* stream = HashTable_GetItemValue(file->local_streams, &lockId);
	if (lock && !stream)
	{
		stream = cliprdr_local_stream_new(file, lockId, NULL, 0);
		HashTable_Insert(file->local_streams, &lockId, stream);
		file->local_lock_id = lockId;
	}
	if (stream)
	{
		stream->locked = lock;
		stream->lockId = lockId;
		update_sub_path(file, lockId);
	}

	if (!lock)
		HashTable_Foreach(file->local_streams, local_stream_discard, file);
	HashTable_Unlock(file->local_streams);
	return CHANNEL_RC_OK;
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

static BOOL xf_cliprdr_clipboard_is_valid_unix_filename(LPCWSTR filename)
{
	LPCWSTR c;

	if (!filename)
		return FALSE;

	if (filename[0] == L'\0')
		return FALSE;

	/* Reserved characters */
	for (c = filename; *c; ++c)
	{
		if (*c == L'/')
			return FALSE;
	}

	return TRUE;
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
#if defined(WITH_FUSE2) || defined(WITH_FUSE3)
	cliprdr->ServerFileContentsResponse = cliprdr_file_context_server_file_contents_response;
#endif

	return TRUE;
}

BOOL cliprdr_file_context_uninit(CliprdrFileContext* file, CliprdrClientContext* cliprdr)
{
	WINPR_ASSERT(file);
	WINPR_ASSERT(cliprdr);

	// Clear all data before the channel is closed
	// the cleanup handlers are dependent on a working channel.
#if defined(WITH_FUSE2) || defined(WITH_FUSE3)
	ArrayList_Clear(file->ino_list);
	Queue_Clear(file->requests_in_flight);
#endif

	HashTable_Clear(file->remote_streams);
	HashTable_Clear(file->local_streams);

	file->context = NULL;
#if defined(WITH_FUSE2) || defined(WITH_FUSE3)
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

static BOOL cliprdr_file_context_update_base(CliprdrFileContext* file, wClipboard* clip)
{
	wClipboardDelegate* delegate = ClipboardGetDelegate(clip);
	if (!delegate)
		return FALSE;
	ClipboardLock(clip);
	HashTable_Lock(file->remote_streams);
	free(file->exposed_path);
	file->exposed_path = _strdup(file->current_path);
	HashTable_Unlock(file->remote_streams);

	delegate->basePath = (file->exposed_path);
	ClipboardUnlock(clip);
	return delegate->basePath != NULL;
}

BOOL cliprdr_file_context_update_server_data(CliprdrFileContext* file, wClipboard* clip,
                                             const void* data, size_t size)
{
	WINPR_ASSERT(file);

	if (!cliprdr_file_server_content_changed_and_update(file, data, size))
		return TRUE;

	if (!cliprdr_file_context_clear(file))
		return FALSE;

#if defined(WITH_FUSE2) || defined(WITH_FUSE3)
	/* Build inode table for FILEDESCRIPTORW*/
	if (!cliprdr_file_fuse_generate_list(file, data, size, 0))
		return FALSE;
#endif

	return cliprdr_file_context_update_base(file, clip);
}

void* cliprdr_file_context_get_context(CliprdrFileContext* file)
{
	WINPR_ASSERT(file);
	return file->clipboard;
}

void cliprdr_file_session_terminate(CliprdrFileContext* file)
{
	if (!file)
		return;

#if defined(WITH_FUSE2) || defined(WITH_FUSE3)
	if (file->fuse_sess)
		fuse_session_exit(file->fuse_sess);
#endif
	/* 	not elegant but works for umounting FUSE
	    fuse_chan must receieve a oper buf to unblock fuse_session_receive_buf function.
	*/
	winpr_PathFileExists(file->path);
}

void cliprdr_file_context_free(CliprdrFileContext* file)
{
	if (!file)
		return;

#if defined(WITH_FUSE2) || defined(WITH_FUSE3)
	if (file->fuse_thread)
	{
		cliprdr_file_session_terminate(file);
		WaitForSingleObject(file->fuse_thread, INFINITE);
		CloseHandle(file->fuse_thread);
	}

	// fuse related
	ArrayList_Free(file->ino_list);
	Queue_Free(file->requests_in_flight);
#endif
	HashTable_Free(file->remote_streams);
	HashTable_Free(file->local_streams);
	winpr_RemoveDirectory(file->path);
	free(file->path);
	free(file->current_path);
	free(file->exposed_path);
	free(file);
}

static BOOL create_base_path(CliprdrFileContext* file)
{
	WINPR_ASSERT(file);

	char base[64] = { 0 };
	_snprintf(base, sizeof(base), "com.freerdp.client.cliprdr.%" PRIu32, GetCurrentProcessId());

	file->path = GetKnownSubPath(KNOWN_PATH_TEMP, base);
	if (!file->path)
		return FALSE;

	if (!update_sub_path(file, 0))
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
		fclose(file->fp);
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
	f->name = _strdup(path);
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
	CloseHandle(hFile);
	if (!status)
		return FALSE;

	return fileInformation.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
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
	do
	{
		const WCHAR dot[] = { '.', '\0' };
		const WCHAR dotdot[] = { '.', '.', '\0' };

		if (_wcscmp(FindFileData.cFileName, dot) == 0)
			continue;
		if (_wcscmp(FindFileData.cFileName, dotdot) == 0)
			continue;

		char cFileName[MAX_PATH] = { 0 };
		ConvertWCharNToUtf8(FindFileData.cFileName, ARRAYSIZE(FindFileData.cFileName), cFileName,
		                    ARRAYSIZE(cFileName));

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
	char* ptr = strtok(copy, "\r\n");
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
		ptr = strtok(NULL, "\r\n");
	}

	rc = TRUE;
fail:
	free(copy);
	return rc;
}

CliprdrLocalStream* cliprdr_local_stream_new(CliprdrFileContext* context, UINT32 lockId,
                                             const char* data, size_t size)
{
	WINPR_ASSERT(context);
	CliprdrLocalStream* stream = calloc(1, sizeof(CliprdrLocalStream));
	if (!stream)
		return NULL;

	stream->context = context;
	if (!cliprdr_local_stream_update(stream, data, size))
		goto fail;

	stream->lockId = lockId;
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

#if defined(WITH_FUSE2) || defined(WITH_FUSE3)
	file->requests_in_flight = Queue_New(TRUE, 10, 2);
	if (!file->requests_in_flight)
		goto fail;

	wObject* qobj = Queue_Object(file->requests_in_flight);
	WINPR_ASSERT(qobj);
	qobj->fnObjectFree = cliprdr_fuse_request_free;

	file->current_inode_id = 0;

	file->ino_list = ArrayList_New(TRUE);
	if (!file->ino_list)
	{
		WLog_Print(file->log, WLOG_ERROR, "failed to allocate stream_list");
		goto fail;
	}
	wObject* aobj = ArrayList_Object(file->ino_list);
	aobj->fnObjectFree = cliprdr_file_fuse_node_free;
#endif

	file->remote_streams = HashTable_New(TRUE);
	if (!file->remote_streams)
	{
		WLog_Print(file->log, WLOG_ERROR, "failed to allocate stream_list");
		goto fail;
	}

	if (!HashTable_SetHashFunction(file->remote_streams, UINTPointerHash))
		goto fail;

	wObject* kobj = HashTable_KeyObject(file->remote_streams);
	WINPR_ASSERT(kobj);
	kobj->fnObjectEquals = UINTPointerCompare;
	kobj->fnObjectFree = free;
	kobj->fnObjectNew = UINTPointerClone;

	wObject* obj = HashTable_ValueObject(file->remote_streams);
	WINPR_ASSERT(kobj);
	obj->fnObjectFree = cliprdr_file_stream_lock_free;

	if (!create_base_path(file))
		goto fail;

#if defined(WITH_FUSE2) || defined(WITH_FUSE3)
	if (!cliprdr_fuse_repopulate(file))
		goto fail;

	if (!(file->fuse_thread = CreateThread(NULL, 0, cliprdr_file_fuse_thread, file, 0, NULL)))
		goto fail;

#endif
	return file;

fail:
	cliprdr_file_context_free(file);
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

static BOOL remote_stream_discard(const void* key, void* value, void* arg)
{
	CliprdrFileContext* file = arg;
	WINPR_ASSERT(file);
	WINPR_ASSERT(value);
	WINPR_ASSERT(arg);

#if defined(WITH_FUSE2) || defined(WITH_FUSE3)
	CliprdrFileStreamLock* stream = (CliprdrFileStreamLock*)value;
	// TODO: Only unlock streams that are no longer in use
	cliprdr_file_fuse_remote_try_unlock(stream);
	// TODO: Only remove streams that are not locked anymore.
#endif
	return HashTable_Remove(file->remote_streams, key);
}

BOOL cliprdr_file_context_clear(CliprdrFileContext* file)
{
	WINPR_ASSERT(file);

	WLog_Print(file->log, WLOG_DEBUG, "clear file clipbaord...");

	HashTable_Foreach(file->local_streams, local_stream_discard, file);
	HashTable_Foreach(file->remote_streams, remote_stream_discard, file);

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
	}
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

#if defined(WITH_FUSE2) || defined(WITH_FUSE3)
	return TRUE;
#else
	return FALSE;
#endif
}
