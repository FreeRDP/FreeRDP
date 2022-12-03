/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Clipboard Redirection
 *
 * Copyright 2010-2011 Vic Lee
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#ifdef WITH_XFIXES
#include <X11/extensions/Xfixes.h>
#endif

#ifdef WITH_FUSE
#define FUSE_USE_VERSION FUSE_API_VERSION
#if FUSE_USE_VERSION >= 30
#include <fuse3/fuse_lowlevel.h>
#else
#include <fuse/fuse_lowlevel.h>
#endif
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

#include <freerdp/log.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/channels/channels.h>
#include <freerdp/channels/cliprdr.h>

#include "xf_cliprdr.h"

#define TAG CLIENT_TAG("x11")

#define MAX_CLIPBOARD_FORMATS 255
#define WIN32_FILETIME_TO_UNIX_EPOCH_USEC UINT64_C(116444736000000000)

typedef struct
{
	Atom atom;
	UINT32 formatId;
	char* formatName;
} xfCliprdrFormat;

#ifdef WITH_FUSE
typedef struct
{
	UINT32 stream_id;
	/* must be one of FILECONTENTS_SIZE or FILECONTENTS_RANGE*/
	UINT32 req_type;
	fuse_req_t req;
	/*for FILECONTENTS_SIZE must be ino number* */
	size_t req_ino;
} xfCliprdrFuseStream;

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
} xfCliprdrFuseInode;

static void xf_cliprdr_fuse_inode_free(void* obj)
{
	xfCliprdrFuseInode* inode = (xfCliprdrFuseInode*)obj;
	if (!inode)
		return;

	free(inode->name);
	ArrayList_Free(inode->child_inos);

	inode->name = NULL;
	inode->child_inos = NULL;
	free(inode);
}
static inline xfCliprdrFuseInode* xf_cliprdr_fuse_util_get_inode(wArrayList* ino_list,
                                                                 fuse_ino_t ino);
#endif

struct xf_clipboard
{
	xfContext* xfc;
	rdpChannels* channels;
	CliprdrClientContext* context;

	wClipboard* system;
	wClipboardDelegate* delegate;

	Window root_window;
	Atom clipboard_atom;
	Atom property_atom;

	Atom timestamp_property_atom;
	Time selection_ownership_timestamp;

	Atom raw_transfer_atom;
	Atom raw_format_list_atom;

	int numClientFormats;
	xfCliprdrFormat clientFormats[20];

	int numServerFormats;
	CLIPRDR_FORMAT* serverFormats;

	int numTargets;
	Atom targets[20];

	int requestedFormatId;

	BYTE* data;
	BYTE* data_raw;
	BOOL data_raw_format;
	UINT32 data_format_id;
	const char* data_format_name;
	int data_length;
	int data_raw_length;
	XSelectionEvent* respond;

	Window owner;
	BOOL sync;

	/* INCR mechanism */
	Atom incr_atom;
	BOOL incr_starts;
	BYTE* incr_data;
	int incr_data_length;

	/* XFixes extension */
	int xfixes_event_base;
	int xfixes_error_base;
	BOOL xfixes_supported;

	/* File clipping */
	BOOL streams_supported;
	BOOL file_formats_registered;
	UINT32 file_capability_flags;
	/* last sent data */
	CLIPRDR_FORMAT* lastSentFormats;
	UINT32 lastSentNumFormats;
#ifdef WITH_FUSE
	/* FUSE related**/
	HANDLE fuse_thread;
	struct fuse_session* fuse_sess;

	/* fuse reset per copy*/
	wArrayList* stream_list;
	UINT32 current_stream_id;
	wArrayList* ino_list;
#endif
};

static UINT xf_cliprdr_send_client_format_list(xfClipboard* clipboard, BOOL force);
static void xf_cliprdr_set_selection_owner(xfContext* xfc, xfClipboard* clipboard, Time timestamp);

static void xf_cliprdr_check_owner(xfClipboard* clipboard)
{
	Window owner;
	xfContext* xfc;

	WINPR_ASSERT(clipboard);

	xfc = clipboard->xfc;
	WINPR_ASSERT(xfc);

	if (clipboard->sync)
	{
		owner = XGetSelectionOwner(xfc->display, clipboard->clipboard_atom);

		if (clipboard->owner != owner)
		{
			clipboard->owner = owner;
			xf_cliprdr_send_client_format_list(clipboard, FALSE);
		}
	}
}

static BOOL xf_cliprdr_is_self_owned(xfClipboard* clipboard)
{
	xfContext* xfc;

	WINPR_ASSERT(clipboard);

	xfc = clipboard->xfc;
	WINPR_ASSERT(xfc);
	return XGetSelectionOwner(xfc->display, clipboard->clipboard_atom) == xfc->drawable;
}

static void xf_cliprdr_set_raw_transfer_enabled(xfClipboard* clipboard, BOOL enabled)
{
	UINT32 data = enabled;
	xfContext* xfc;

	WINPR_ASSERT(clipboard);

	xfc = clipboard->xfc;
	WINPR_ASSERT(xfc);
	XChangeProperty(xfc->display, xfc->drawable, clipboard->raw_transfer_atom, XA_INTEGER, 32,
	                PropModeReplace, (BYTE*)&data, 1);
}

static BOOL xf_cliprdr_is_raw_transfer_available(xfClipboard* clipboard)
{
	Atom type;
	int format;
	int result = 0;
	unsigned long length;
	unsigned long bytes_left;
	UINT32* data = NULL;
	UINT32 is_enabled = 0;
	Window owner = None;
	xfContext* xfc;

	WINPR_ASSERT(clipboard);

	xfc = clipboard->xfc;
	WINPR_ASSERT(xfc);

	owner = XGetSelectionOwner(xfc->display, clipboard->clipboard_atom);

	if (owner != None)
	{
		result =
		    XGetWindowProperty(xfc->display, owner, clipboard->raw_transfer_atom, 0, 4, 0,
		                       XA_INTEGER, &type, &format, &length, &bytes_left, (BYTE**)&data);
	}

	if (data)
	{
		is_enabled = *data;
		XFree(data);
	}

	if ((owner == None) || (owner == xfc->drawable))
		return FALSE;

	if (result != Success)
		return FALSE;

	return is_enabled ? TRUE : FALSE;
}

static BOOL xf_cliprdr_formats_equal(const CLIPRDR_FORMAT* server, const xfCliprdrFormat* client)
{
	WINPR_ASSERT(server);
	WINPR_ASSERT(client);

	if (server->formatName && client->formatName)
	{
		/* The server may be using short format names while we store them in full form. */
		return (0 == strncmp(server->formatName, client->formatName, strlen(server->formatName)));
	}

	if (!server->formatName && !client->formatName)
	{
		return (server->formatId == client->formatId);
	}

	return FALSE;
}

static const xfCliprdrFormat* xf_cliprdr_get_client_format_by_id(xfClipboard* clipboard,
                                                                 UINT32 formatId)
{
	int index;

	WINPR_ASSERT(clipboard);

	for (index = 0; index < clipboard->numClientFormats; index++)
	{
		const xfCliprdrFormat* format = &(clipboard->clientFormats[index]);

		if (format->formatId == formatId)
			return format;
	}

	return NULL;
}

static const xfCliprdrFormat* xf_cliprdr_get_client_format_by_atom(xfClipboard* clipboard,
                                                                   Atom atom)
{
	int i;

	WINPR_ASSERT(clipboard);

	for (i = 0; i < clipboard->numClientFormats; i++)
	{
		const xfCliprdrFormat* format = &(clipboard->clientFormats[i]);

		if (format->atom == atom)
			return format;
	}

	return NULL;
}

static const CLIPRDR_FORMAT* xf_cliprdr_get_server_format_by_atom(xfClipboard* clipboard, Atom atom)
{
	int i;

	WINPR_ASSERT(clipboard);

	for (i = 0; i < clipboard->numClientFormats; i++)
	{
		const xfCliprdrFormat* client_format = &(clipboard->clientFormats[i]);

		if (client_format->atom == atom)
		{
			int j;
			for (j = 0; j < clipboard->numServerFormats; j++)
			{
				const CLIPRDR_FORMAT* server_format = &(clipboard->serverFormats[j]);

				if (xf_cliprdr_formats_equal(server_format, client_format))
					return server_format;
			}
		}
	}

	return NULL;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT xf_cliprdr_send_data_request(xfClipboard* clipboard, UINT32 formatId)
{
	CLIPRDR_FORMAT_DATA_REQUEST request = { 0 };
	request.requestedFormatId = formatId;

	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(clipboard->context);
	WINPR_ASSERT(clipboard->context->ClientFormatDataRequest);
	return clipboard->context->ClientFormatDataRequest(clipboard->context, &request);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT xf_cliprdr_send_data_response(xfClipboard* clipboard, const BYTE* data, size_t size)
{
	CLIPRDR_FORMAT_DATA_RESPONSE response = { 0 };

	WINPR_ASSERT(clipboard);

	/* No request currently pending, do not send a response. */
	if (clipboard->requestedFormatId < 0)
		return CHANNEL_RC_OK;

	/* Request handled, reset to invalid */
	clipboard->requestedFormatId = -1;

	response.common.msgFlags = (data) ? CB_RESPONSE_OK : CB_RESPONSE_FAIL;
	response.common.dataLen = size;
	response.requestedFormatData = data;

	WINPR_ASSERT(clipboard->context);
	WINPR_ASSERT(clipboard->context->ClientFormatDataResponse);
	return clipboard->context->ClientFormatDataResponse(clipboard->context, &response);
}

static wStream* xf_cliprdr_serialize_server_format_list(xfClipboard* clipboard)
{
	UINT32 i;
	UINT32 formatCount;
	wStream* s = NULL;

	WINPR_ASSERT(clipboard);

	/* Typical MS Word format list is about 80 bytes long. */
	if (!(s = Stream_New(NULL, 128)))
	{
		WLog_ERR(TAG, "failed to allocate serialized format list");
		goto error;
	}

	/* If present, the last format is always synthetic CF_RAW. Do not include it. */
	formatCount = (clipboard->numServerFormats > 0) ? clipboard->numServerFormats - 1 : 0;
	Stream_Write_UINT32(s, formatCount);

	for (i = 0; i < formatCount; i++)
	{
		CLIPRDR_FORMAT* format = &clipboard->serverFormats[i];
		size_t name_length = format->formatName ? strlen(format->formatName) : 0;

		if (!Stream_EnsureRemainingCapacity(s, sizeof(UINT32) + name_length + 1))
		{
			WLog_ERR(TAG, "failed to expand serialized format list");
			goto error;
		}

		Stream_Write_UINT32(s, format->formatId);

		if (format->formatName)
			Stream_Write(s, format->formatName, name_length);

		Stream_Write_UINT8(s, '\0');
	}

	Stream_SealLength(s);
	return s;
error:
	Stream_Free(s, TRUE);
	return NULL;
}

static CLIPRDR_FORMAT* xf_cliprdr_parse_server_format_list(BYTE* data, size_t length,
                                                           UINT32* numFormats)
{
	UINT32 i;
	wStream* s = NULL;
	CLIPRDR_FORMAT* formats = NULL;

	WINPR_ASSERT(data || (length == 0));
	WINPR_ASSERT(numFormats);

	if (!(s = Stream_New(data, length)))
	{
		WLog_ERR(TAG, "failed to allocate stream for parsing serialized format list");
		goto error;
	}

	if (!Stream_CheckAndLogRequiredLength(TAG, s, sizeof(UINT32)))
		goto error;

	Stream_Read_UINT32(s, *numFormats);

	if (*numFormats > MAX_CLIPBOARD_FORMATS)
	{
		WLog_ERR(TAG, "unexpectedly large number of formats: %" PRIu32 "", *numFormats);
		goto error;
	}

	if (!(formats = (CLIPRDR_FORMAT*)calloc(*numFormats, sizeof(CLIPRDR_FORMAT))))
	{
		WLog_ERR(TAG, "failed to allocate format list");
		goto error;
	}

	for (i = 0; i < *numFormats; i++)
	{
		const char* formatName = NULL;
		size_t formatNameLength = 0;

		if (!Stream_CheckAndLogRequiredLength(TAG, s, sizeof(UINT32)))
			goto error;

		Stream_Read_UINT32(s, formats[i].formatId);
		formatName = (const char*)Stream_Pointer(s);
		formatNameLength = strnlen(formatName, Stream_GetRemainingLength(s));

		if (formatNameLength == Stream_GetRemainingLength(s))
		{
			WLog_ERR(TAG, "missing terminating null byte, %" PRIuz " bytes left to read",
			         formatNameLength);
			goto error;
		}

		formats[i].formatName = strndup(formatName, formatNameLength);
		Stream_Seek(s, formatNameLength + 1);
	}

	Stream_Free(s, FALSE);
	return formats;
error:
	Stream_Free(s, FALSE);
	free(formats);
	*numFormats = 0;
	return NULL;
}

static void xf_cliprdr_free_formats(CLIPRDR_FORMAT* formats, UINT32 numFormats)
{
	UINT32 i;

	WINPR_ASSERT(formats || (numFormats == 0));

	for (i = 0; i < numFormats; i++)
	{
		free(formats[i].formatName);
	}

	free(formats);
}

static CLIPRDR_FORMAT* xf_cliprdr_get_raw_server_formats(xfClipboard* clipboard, UINT32* numFormats)
{
	Atom type = None;
	int format = 0;
	unsigned long length = 0;
	unsigned long remaining;
	BYTE* data = NULL;
	CLIPRDR_FORMAT* formats = NULL;
	xfContext* xfc;

	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(numFormats);

	xfc = clipboard->xfc;
	WINPR_ASSERT(xfc);

	*numFormats = 0;
	XGetWindowProperty(xfc->display, clipboard->owner, clipboard->raw_format_list_atom, 0, 4096,
	                   False, clipboard->raw_format_list_atom, &type, &format, &length, &remaining,
	                   &data);

	if (data && length > 0 && format == 8 && type == clipboard->raw_format_list_atom)
	{
		formats = xf_cliprdr_parse_server_format_list(data, length, numFormats);
	}
	else
	{
		WLog_ERR(TAG,
		         "failed to retrieve raw format list: data=%p, length=%lu, format=%d, type=%lu "
		         "(expected=%lu)",
		         (void*)data, length, format, (unsigned long)type,
		         (unsigned long)clipboard->raw_format_list_atom);
	}

	if (data)
		XFree(data);

	return formats;
}

static CLIPRDR_FORMAT* xf_cliprdr_get_formats_from_targets(xfClipboard* clipboard,
                                                           UINT32* numFormats)
{
	unsigned long i;
	Atom atom;
	BYTE* data = NULL;
	int format_property;
	unsigned long length;
	unsigned long bytes_left;
	CLIPRDR_FORMAT* formats = NULL;
	xfContext* xfc;

	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(numFormats);

	xfc = clipboard->xfc;
	WINPR_ASSERT(xfc);

	*numFormats = 0;
	XGetWindowProperty(xfc->display, xfc->drawable, clipboard->property_atom, 0, 200, 0, XA_ATOM,
	                   &atom, &format_property, &length, &bytes_left, &data);

	if (length > 0)
	{
		if (!data)
		{
			WLog_ERR(TAG, "XGetWindowProperty set length = %lu but data is NULL", length);
			goto out;
		}

		if (!(formats = (CLIPRDR_FORMAT*)calloc(length, sizeof(CLIPRDR_FORMAT))))
		{
			WLog_ERR(TAG, "failed to allocate %lu CLIPRDR_FORMAT structs", length);
			goto out;
		}
	}

	for (i = 0; i < length; i++)
	{
		Atom tatom = ((Atom*)data)[i];
		const xfCliprdrFormat* format = xf_cliprdr_get_client_format_by_atom(clipboard, tatom);

		if (format)
		{
			formats[*numFormats].formatId = format->formatId;
			formats[*numFormats].formatName = _strdup(format->formatName);
			*numFormats += 1;
		}
	}

out:

	if (data)
		XFree(data);

	return formats;
}

static CLIPRDR_FORMAT* xf_cliprdr_get_client_formats(xfClipboard* clipboard, UINT32* numFormats)
{
	CLIPRDR_FORMAT* formats = NULL;

	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(numFormats);

	*numFormats = 0;

	if (xf_cliprdr_is_raw_transfer_available(clipboard))
	{
		formats = xf_cliprdr_get_raw_server_formats(clipboard, numFormats);
	}

	if (*numFormats == 0)
	{
		xf_cliprdr_free_formats(formats, *numFormats);
		formats = xf_cliprdr_get_formats_from_targets(clipboard, numFormats);
	}

	return formats;
}

static void xf_cliprdr_provide_server_format_list(xfClipboard* clipboard)
{
	wStream* formats = NULL;
	xfContext* xfc;

	WINPR_ASSERT(clipboard);

	xfc = clipboard->xfc;
	WINPR_ASSERT(xfc);

	formats = xf_cliprdr_serialize_server_format_list(clipboard);

	if (formats)
	{
		XChangeProperty(xfc->display, xfc->drawable, clipboard->raw_format_list_atom,
		                clipboard->raw_format_list_atom, 8, PropModeReplace, Stream_Buffer(formats),
		                Stream_Length(formats));
	}
	else
	{
		XDeleteProperty(xfc->display, xfc->drawable, clipboard->raw_format_list_atom);
	}

	Stream_Free(formats, TRUE);
}

static BOOL xf_clipboard_format_equal(const CLIPRDR_FORMAT* a, const CLIPRDR_FORMAT* b)
{
	WINPR_ASSERT(a);
	WINPR_ASSERT(b);

	if (a->formatId != b->formatId)
		return FALSE;
	if (!a->formatName && !b->formatName)
		return TRUE;
	if (!a->formatName || !b->formatName)
		return FALSE;
	return strcmp(a->formatName, b->formatName) == 0;
}

static BOOL xf_clipboard_changed(xfClipboard* clipboard, const CLIPRDR_FORMAT* formats,
                                 UINT32 numFormats)
{
	UINT32 x, y;

	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(formats || (numFormats == 0));

	if (clipboard->lastSentNumFormats != numFormats)
		return TRUE;

	for (x = 0; x < numFormats; x++)
	{
		const CLIPRDR_FORMAT* cur = &clipboard->lastSentFormats[x];
		BOOL contained = FALSE;
		for (y = 0; y < numFormats; y++)
		{
			if (xf_clipboard_format_equal(cur, &formats[y]))
			{
				contained = TRUE;
				break;
			}
		}
		if (!contained)
			return TRUE;
	}

	return FALSE;
}

static void xf_clipboard_formats_free(xfClipboard* clipboard)
{
	WINPR_ASSERT(clipboard);

	xf_cliprdr_free_formats(clipboard->lastSentFormats, clipboard->lastSentNumFormats);
	clipboard->lastSentFormats = NULL;
	clipboard->lastSentNumFormats = 0;
}

static BOOL xf_clipboard_copy_formats(xfClipboard* clipboard, const CLIPRDR_FORMAT* formats,
                                      UINT32 numFormats)
{
	UINT32 x;

	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(formats || (numFormats == 0));

	xf_clipboard_formats_free(clipboard);
	clipboard->lastSentFormats = calloc(numFormats, sizeof(CLIPRDR_FORMAT));
	if (!clipboard->lastSentFormats)
		return FALSE;
	clipboard->lastSentNumFormats = numFormats;
	for (x = 0; x < numFormats; x++)
	{
		CLIPRDR_FORMAT* lcur = &clipboard->lastSentFormats[x];
		const CLIPRDR_FORMAT* cur = &formats[x];
		*lcur = *cur;
		if (cur->formatName)
			lcur->formatName = _strdup(cur->formatName);
	}
	return FALSE;
}

static UINT xf_cliprdr_send_format_list(xfClipboard* clipboard, const CLIPRDR_FORMAT* formats,
                                        UINT32 numFormats, BOOL force)
{
	union
	{
		const CLIPRDR_FORMAT* cpv;
		CLIPRDR_FORMAT* pv;
	} cnv = { .cpv = formats };
	const CLIPRDR_FORMAT_LIST formatList = { .common.msgFlags = CB_RESPONSE_OK,
		                                     .numFormats = numFormats,
		                                     .formats = cnv.pv,
		                                     .common.msgType = CB_FORMAT_LIST };

	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(formats || (numFormats == 0));

	if (!force && !xf_clipboard_changed(clipboard, formats, numFormats))
		return CHANNEL_RC_OK;

	xf_clipboard_copy_formats(clipboard, formats, numFormats);
	/* Ensure all pending requests are answered. */
	xf_cliprdr_send_data_response(clipboard, NULL, 0);

	WINPR_ASSERT(clipboard->context);
	WINPR_ASSERT(clipboard->context->ClientFormatList);
	return clipboard->context->ClientFormatList(clipboard->context, &formatList);
}

static void xf_cliprdr_get_requested_targets(xfClipboard* clipboard)
{
	UINT32 numFormats = 0;
	CLIPRDR_FORMAT* formats = xf_cliprdr_get_client_formats(clipboard, &numFormats);
	xf_cliprdr_send_format_list(clipboard, formats, numFormats, FALSE);
	xf_cliprdr_free_formats(formats, numFormats);
}

static void xf_cliprdr_process_requested_data(xfClipboard* clipboard, BOOL hasData, BYTE* data,
                                              int size)
{
	BOOL bSuccess;
	UINT32 SrcSize;
	UINT32 DstSize = 0;
	UINT32 srcFormatId;
	UINT32 dstFormatId;
	BYTE* pDstData = NULL;
	const xfCliprdrFormat* format;

	WINPR_ASSERT(clipboard);

	if (clipboard->incr_starts && hasData)
		return;

	format = xf_cliprdr_get_client_format_by_id(clipboard, clipboard->requestedFormatId);

	if (!hasData || !data || !format)
	{
		xf_cliprdr_send_data_response(clipboard, NULL, 0);
		return;
	}

	srcFormatId = 0;

	switch (format->formatId)
	{
		case CF_RAW:
			srcFormatId = CF_RAW;
			break;

		case CF_TEXT:
		case CF_OEMTEXT:
		case CF_UNICODETEXT:
			size = strlen((char*)data) + 1;
			srcFormatId = ClipboardGetFormatId(clipboard->system, "UTF8_STRING");
			break;

		case CF_DIB:
			srcFormatId = ClipboardGetFormatId(clipboard->system, "image/bmp");
			break;

		case CB_FORMAT_HTML:
			srcFormatId = ClipboardGetFormatId(clipboard->system, "text/html");
			break;

		case CB_FORMAT_TEXTURILIST:
			srcFormatId = ClipboardGetFormatId(clipboard->system, "text/uri-list");
			break;

		case CB_FORMAT_GNOMECOPIEDFILES:
			srcFormatId = ClipboardGetFormatId(clipboard->system, "x-special/gnome-copied-files");
			break;

		case CB_FORMAT_MATECOPIEDFILES:
			srcFormatId = ClipboardGetFormatId(clipboard->system, "x-special/mate-copied-files");
			break;

		default:
			xf_cliprdr_send_data_response(clipboard, NULL, 0);
			return;
	}

	SrcSize = (UINT32)size;
	bSuccess = ClipboardSetData(clipboard->system, srcFormatId, data, SrcSize);

	if (format->formatName)
		dstFormatId = ClipboardGetFormatId(clipboard->system, format->formatName);
	else
		dstFormatId = format->formatId;

	if (bSuccess)
	{
		DstSize = 0;
		pDstData = (BYTE*)ClipboardGetData(clipboard->system, dstFormatId, &DstSize);
	}

	if (!pDstData)
	{
		xf_cliprdr_send_data_response(clipboard, NULL, 0);
		return;
	}

	/*
	 * File lists require a bit of postprocessing to convert them from WinPR's FILDESCRIPTOR
	 * format to CLIPRDR_FILELIST expected by the server.
	 *
	 * We check for "FileGroupDescriptorW" format being registered (i.e., nonzero) in order
	 * to not process CF_RAW as a file list in case WinPR does not support file transfers.
	 */
	if (dstFormatId &&
	    (dstFormatId == ClipboardGetFormatId(clipboard->system, "FileGroupDescriptorW")))
	{
		UINT error = NO_ERROR;
		FILEDESCRIPTORW* file_array = (FILEDESCRIPTORW*)pDstData;
		UINT32 file_count = DstSize / sizeof(FILEDESCRIPTORW);
		pDstData = NULL;
		DstSize = 0;
		error = cliprdr_serialize_file_list_ex(clipboard->file_capability_flags, file_array,
		                                       file_count, &pDstData, &DstSize);

		if (error)
			WLog_ERR(TAG, "failed to serialize CLIPRDR_FILELIST: 0x%08X", error);

		free(file_array);
	}

	xf_cliprdr_send_data_response(clipboard, pDstData, DstSize);
	free(pDstData);
}

static BOOL xf_cliprdr_get_requested_data(xfClipboard* clipboard, Atom target)
{
	Atom type;
	BYTE* data = NULL;
	BOOL has_data = FALSE;
	int format_property;
	unsigned long dummy;
	unsigned long length;
	unsigned long bytes_left;
	const xfCliprdrFormat* format;
	xfContext* xfc;

	WINPR_ASSERT(clipboard);

	xfc = clipboard->xfc;
	WINPR_ASSERT(xfc);

	format = xf_cliprdr_get_client_format_by_id(clipboard, clipboard->requestedFormatId);

	if (!format || (format->atom != target))
	{
		xf_cliprdr_send_data_response(clipboard, NULL, 0);
		return FALSE;
	}

	XGetWindowProperty(xfc->display, xfc->drawable, clipboard->property_atom, 0, 0, 0, target,
	                   &type, &format_property, &length, &bytes_left, &data);

	if (data)
	{
		XFree(data);
		data = NULL;
	}

	if (bytes_left <= 0 && !clipboard->incr_starts)
	{
	}
	else if (type == clipboard->incr_atom)
	{
		clipboard->incr_starts = TRUE;

		if (clipboard->incr_data)
		{
			free(clipboard->incr_data);
			clipboard->incr_data = NULL;
		}

		clipboard->incr_data_length = 0;
		has_data = TRUE; /* data will be followed in PropertyNotify event */
	}
	else
	{
		if (bytes_left <= 0)
		{
			/* INCR finish */
			data = clipboard->incr_data;
			clipboard->incr_data = NULL;
			bytes_left = clipboard->incr_data_length;
			clipboard->incr_data_length = 0;
			clipboard->incr_starts = 0;
			has_data = TRUE;
		}
		else if (XGetWindowProperty(xfc->display, xfc->drawable, clipboard->property_atom, 0,
		                            bytes_left, 0, target, &type, &format_property, &length, &dummy,
		                            &data) == Success)
		{
			if (clipboard->incr_starts)
			{
				BYTE* new_data;
				bytes_left = length * format_property / 8;
				new_data =
				    (BYTE*)realloc(clipboard->incr_data, clipboard->incr_data_length + bytes_left);

				if (new_data)
				{

					clipboard->incr_data = new_data;
					CopyMemory(clipboard->incr_data + clipboard->incr_data_length, data,
					           bytes_left);
					clipboard->incr_data_length += bytes_left;
					XFree(data);
					data = NULL;
				}
			}

			has_data = TRUE;
		}
		else
		{
		}
	}

	XDeleteProperty(xfc->display, xfc->drawable, clipboard->property_atom);
	xf_cliprdr_process_requested_data(clipboard, has_data, data, bytes_left);

	if (data)
		XFree(data);

	return TRUE;
}

static void xf_cliprdr_append_target(xfClipboard* clipboard, Atom target)
{
	int i;

	WINPR_ASSERT(clipboard);
	if (clipboard->numTargets < 0)
		return;

	if ((size_t)clipboard->numTargets >= ARRAYSIZE(clipboard->targets))
		return;

	for (i = 0; i < clipboard->numTargets; i++)
	{
		if (clipboard->targets[i] == target)
			return;
	}

	clipboard->targets[clipboard->numTargets++] = target;
}

static void xf_cliprdr_provide_targets(xfClipboard* clipboard, const XSelectionEvent* respond)
{
	xfContext* xfc;

	WINPR_ASSERT(clipboard);

	xfc = clipboard->xfc;
	WINPR_ASSERT(xfc);

	if (respond->property != None)
	{
		XChangeProperty(xfc->display, respond->requestor, respond->property, XA_ATOM, 32,
		                PropModeReplace, (BYTE*)clipboard->targets, clipboard->numTargets);
	}
}

static void xf_cliprdr_provide_timestamp(xfClipboard* clipboard, const XSelectionEvent* respond)
{
	xfContext* xfc;

	WINPR_ASSERT(clipboard);

	xfc = clipboard->xfc;
	WINPR_ASSERT(xfc);

	if (respond->property != None)
	{
		XChangeProperty(xfc->display, respond->requestor, respond->property, XA_INTEGER, 32,
		                PropModeReplace, (BYTE*)&clipboard->selection_ownership_timestamp, 1);
	}
}

static void xf_cliprdr_provide_data(xfClipboard* clipboard, const XSelectionEvent* respond,
                                    const BYTE* data, UINT32 size)
{
	xfContext* xfc;

	WINPR_ASSERT(clipboard);

	xfc = clipboard->xfc;
	WINPR_ASSERT(xfc);

	if (respond->property != None)
	{
		XChangeProperty(xfc->display, respond->requestor, respond->property, respond->target, 8,
		                PropModeReplace, data, size);
	}
}

static BOOL xf_cliprdr_process_selection_notify(xfClipboard* clipboard,
                                                const XSelectionEvent* xevent)
{
	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(xevent);

	if (xevent->target == clipboard->targets[1])
	{
		if (xevent->property == None)
		{
			xf_cliprdr_send_client_format_list(clipboard, FALSE);
		}
		else
		{
			xf_cliprdr_get_requested_targets(clipboard);
		}

		return TRUE;
	}
	else
	{
		return xf_cliprdr_get_requested_data(clipboard, xevent->target);
	}
}

static void xf_cliprdr_clear_cached_data(xfClipboard* clipboard)
{
	WINPR_ASSERT(clipboard);

	if (clipboard->data)
	{
		free(clipboard->data);
		clipboard->data = NULL;
	}

	clipboard->data_length = 0;

	if (clipboard->data_raw)
	{
		free(clipboard->data_raw);
		clipboard->data_raw = NULL;
	}

	clipboard->data_raw_length = 0;
#ifdef WITH_FUSE
	if (clipboard->stream_list)
	{
		size_t index;
		size_t count;
		xfCliprdrFuseStream* stream;
		ArrayList_Lock(clipboard->stream_list);
		clipboard->current_stream_id = 0;
		/* reply error to all req first don't care request type*/
		count = ArrayList_Count(clipboard->stream_list);
		for (index = 0; index < count; index++)
		{
			stream = (xfCliprdrFuseStream*)ArrayList_GetItem(clipboard->stream_list, index);
			fuse_reply_err(stream->req, EIO);
		}
		ArrayList_Unlock(clipboard->stream_list);

		ArrayList_Clear(clipboard->stream_list);
	}
	if (clipboard->ino_list)
	{
		ArrayList_Clear(clipboard->ino_list);
	}
#endif
}

static BOOL xf_cliprdr_process_selection_request(xfClipboard* clipboard,
                                                 const XSelectionRequestEvent* xevent)
{
	int fmt;
	Atom type;
	UINT32 formatId;
	const char* formatName;
	XSelectionEvent* respond;
	BYTE* data = NULL;
	BOOL delayRespond;
	BOOL rawTransfer;
	BOOL matchingFormat;
	unsigned long length;
	unsigned long bytes_left;
	xfContext* xfc;

	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(xevent);

	xfc = clipboard->xfc;
	WINPR_ASSERT(xfc);

	if (xevent->owner != xfc->drawable)
		return FALSE;

	delayRespond = FALSE;

	if (!(respond = (XSelectionEvent*)calloc(1, sizeof(XSelectionEvent))))
	{
		WLog_ERR(TAG, "failed to allocate XEvent data");
		return FALSE;
	}

	respond->property = None;
	respond->type = SelectionNotify;
	respond->display = xevent->display;
	respond->requestor = xevent->requestor;
	respond->selection = xevent->selection;
	respond->target = xevent->target;
	respond->time = xevent->time;

	if (xevent->target == clipboard->targets[0]) /* TIMESTAMP */
	{
		/* Someone else requests the selection's timestamp */
		respond->property = xevent->property;
		xf_cliprdr_provide_timestamp(clipboard, respond);
	}
	else if (xevent->target == clipboard->targets[1]) /* TARGETS */
	{
		/* Someone else requests our available formats */
		respond->property = xevent->property;
		xf_cliprdr_provide_targets(clipboard, respond);
	}
	else
	{
		const CLIPRDR_FORMAT* format =
		    xf_cliprdr_get_server_format_by_atom(clipboard, xevent->target);

		if (format && (xevent->requestor != xfc->drawable))
		{
			formatId = format->formatId;
			formatName = format->formatName;
			rawTransfer = FALSE;

			if (formatId == CF_RAW)
			{
				if (XGetWindowProperty(xfc->display, xevent->requestor, clipboard->property_atom, 0,
				                       4, 0, XA_INTEGER, &type, &fmt, &length, &bytes_left,
				                       &data) != Success)
				{
				}

				if (data)
				{
					rawTransfer = TRUE;
					CopyMemory(&formatId, data, 4);
					XFree(data);
				}
			}

			/* We can compare format names by pointer value here as they are both
			 * taken from the same clipboard->serverFormats array */
			matchingFormat = (formatId == clipboard->data_format_id) &&
			                 (formatName == clipboard->data_format_name);

			if (matchingFormat && (clipboard->data != 0) && !rawTransfer)
			{
				/* Cached converted clipboard data available. Send it now */
				respond->property = xevent->property;
				xf_cliprdr_provide_data(clipboard, respond, clipboard->data,
				                        clipboard->data_length);
			}
			else if (matchingFormat && (clipboard->data_raw != 0) && rawTransfer)
			{
				/* Cached raw clipboard data available. Send it now */
				respond->property = xevent->property;
				xf_cliprdr_provide_data(clipboard, respond, clipboard->data_raw,
				                        clipboard->data_raw_length);
			}
			else if (clipboard->respond)
			{
				/* duplicate request */
			}
			else
			{
				/**
				 * Send clipboard data request to the server.
				 * Response will be postponed after receiving the data
				 */
				xf_cliprdr_clear_cached_data(clipboard);
				respond->property = xevent->property;
				clipboard->respond = respond;
				clipboard->data_format_id = formatId;
				clipboard->data_format_name = formatName;
				clipboard->data_raw_format = rawTransfer;
				delayRespond = TRUE;
				xf_cliprdr_send_data_request(clipboard, formatId);
			}
		}
	}

	if (!delayRespond)
	{
		union
		{
			XEvent* ev;
			XSelectionEvent* sev;
		} conv;

		conv.sev = respond;
		XSendEvent(xfc->display, xevent->requestor, 0, 0, conv.ev);
		XFlush(xfc->display);
		free(respond);
	}

	return TRUE;
}

static BOOL xf_cliprdr_process_selection_clear(xfClipboard* clipboard,
                                               const XSelectionClearEvent* xevent)
{
	xfContext* xfc;

	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(xevent);

	xfc = clipboard->xfc;
	WINPR_ASSERT(xfc);

	WINPR_UNUSED(xevent);

	if (xf_cliprdr_is_self_owned(clipboard))
		return FALSE;

	XDeleteProperty(xfc->display, clipboard->root_window, clipboard->property_atom);
	return TRUE;
}

static BOOL xf_cliprdr_process_property_notify(xfClipboard* clipboard, const XPropertyEvent* xevent)
{
	const xfCliprdrFormat* format;
	xfContext* xfc = NULL;

	if (!clipboard)
		return TRUE;

	xfc = clipboard->xfc;
	WINPR_ASSERT(xfc);
	WINPR_ASSERT(xevent);

	if (xevent->atom == clipboard->timestamp_property_atom)
	{
		/* This is the response to the property change we did
		 * in xf_cliprdr_prepare_to_set_selection_owner. Now
		 * we can set ourselves as the selection owner. (See
		 * comments in those functions below.) */
		xf_cliprdr_set_selection_owner(xfc, clipboard, xevent->time);
		return TRUE;
	}

	if (xevent->atom != clipboard->property_atom)
		return FALSE; /* Not cliprdr-related */

	if (xevent->window == clipboard->root_window)
	{
		xf_cliprdr_send_client_format_list(clipboard, FALSE);
	}
	else if ((xevent->window == xfc->drawable) && (xevent->state == PropertyNewValue) &&
	         clipboard->incr_starts)
	{
		format = xf_cliprdr_get_client_format_by_id(clipboard, clipboard->requestedFormatId);

		if (format)
			xf_cliprdr_get_requested_data(clipboard, format->atom);
	}

	return TRUE;
}

void xf_cliprdr_handle_xevent(xfContext* xfc, const XEvent* event)
{
	xfClipboard* clipboard;

	if (!xfc || !event)
		return;

	clipboard = xfc->clipboard;

	if (!clipboard)
		return;

#ifdef WITH_XFIXES

	if (clipboard->xfixes_supported &&
	    event->type == XFixesSelectionNotify + clipboard->xfixes_event_base)
	{
		const XFixesSelectionNotifyEvent* se = (const XFixesSelectionNotifyEvent*)event;

		if (se->subtype == XFixesSetSelectionOwnerNotify)
		{
			if (se->selection != clipboard->clipboard_atom)
				return;

			if (XGetSelectionOwner(xfc->display, se->selection) == xfc->drawable)
				return;

			clipboard->owner = None;
			xf_cliprdr_check_owner(clipboard);
		}

		return;
	}

#endif

	switch (event->type)
	{
		case SelectionNotify:
			xf_cliprdr_process_selection_notify(clipboard, &event->xselection);
			break;

		case SelectionRequest:
			xf_cliprdr_process_selection_request(clipboard, &event->xselectionrequest);
			break;

		case SelectionClear:
			xf_cliprdr_process_selection_clear(clipboard, &event->xselectionclear);
			break;

		case PropertyNotify:
			xf_cliprdr_process_property_notify(clipboard, &event->xproperty);
			break;

		case FocusIn:
			if (!clipboard->xfixes_supported)
			{
				xf_cliprdr_check_owner(clipboard);
			}

			break;
	}
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT xf_cliprdr_send_client_capabilities(xfClipboard* clipboard)
{
	CLIPRDR_CAPABILITIES capabilities = { 0 };
	CLIPRDR_GENERAL_CAPABILITY_SET generalCapabilitySet = { 0 };

	WINPR_ASSERT(clipboard);

	capabilities.cCapabilitiesSets = 1;
	capabilities.capabilitySets = (CLIPRDR_CAPABILITY_SET*)&(generalCapabilitySet);
	generalCapabilitySet.capabilitySetType = CB_CAPSTYPE_GENERAL;
	generalCapabilitySet.capabilitySetLength = 12;
	generalCapabilitySet.version = CB_CAPS_VERSION_2;
	generalCapabilitySet.generalFlags = CB_USE_LONG_FORMAT_NAMES;

	WINPR_ASSERT(clipboard);
	if (clipboard->streams_supported && clipboard->file_formats_registered)
		generalCapabilitySet.generalFlags |=
		    CB_STREAM_FILECLIP_ENABLED | CB_FILECLIP_NO_FILE_PATHS | CB_HUGE_FILE_SUPPORT_ENABLED;

	clipboard->file_capability_flags = generalCapabilitySet.generalFlags;

	WINPR_ASSERT(clipboard->context);
	WINPR_ASSERT(clipboard->context->ClientCapabilities);
	return clipboard->context->ClientCapabilities(clipboard->context, &capabilities);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT xf_cliprdr_send_client_format_list(xfClipboard* clipboard, BOOL force)
{
	UINT32 i, numFormats;
	CLIPRDR_FORMAT* formats = NULL;
	UINT ret;
	xfContext* xfc;

	WINPR_ASSERT(clipboard);

	xfc = clipboard->xfc;
	WINPR_ASSERT(xfc);
	numFormats = clipboard->numClientFormats;

	if (numFormats)
	{
		if (!(formats = (CLIPRDR_FORMAT*)calloc(numFormats, sizeof(CLIPRDR_FORMAT))))
		{
			WLog_ERR(TAG, "failed to allocate %" PRIu32 " CLIPRDR_FORMAT structs", numFormats);
			return CHANNEL_RC_NO_MEMORY;
		}
	}

	for (i = 0; i < numFormats; i++)
	{
		const xfCliprdrFormat* clientFormat = &clipboard->clientFormats[i];
		CLIPRDR_FORMAT* format = &formats[i];
		format->formatId = clientFormat->formatId;
		format->formatName = clientFormat->formatName;
	}

	ret = xf_cliprdr_send_format_list(clipboard, formats, numFormats, force);
	free(formats);

	if (clipboard->owner && clipboard->owner != xfc->drawable)
	{
		/* Request the owner for TARGETS, and wait for SelectionNotify event */
		XConvertSelection(xfc->display, clipboard->clipboard_atom, clipboard->targets[1],
		                  clipboard->property_atom, xfc->drawable, CurrentTime);
	}

	return ret;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT xf_cliprdr_send_client_format_list_response(xfClipboard* clipboard, BOOL status)
{
	CLIPRDR_FORMAT_LIST_RESPONSE formatListResponse = { 0 };

	formatListResponse.common.msgType = CB_FORMAT_LIST_RESPONSE;
	formatListResponse.common.msgFlags = status ? CB_RESPONSE_OK : CB_RESPONSE_FAIL;
	formatListResponse.common.dataLen = 0;

	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(clipboard->context);
	WINPR_ASSERT(clipboard->context->ClientFormatListResponse);
	return clipboard->context->ClientFormatListResponse(clipboard->context, &formatListResponse);
}

#ifdef WITH_FUSE
/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT xf_cliprdr_send_client_file_contents(xfClipboard* clipboard, UINT32 streamId,
                                                 UINT32 listIndex, UINT32 dwFlags,
                                                 UINT32 nPositionLow, UINT32 nPositionHigh,
                                                 UINT32 cbRequested)
{
	CLIPRDR_FILE_CONTENTS_REQUEST formatFileContentsRequest = { 0 };
	formatFileContentsRequest.streamId = streamId;
	formatFileContentsRequest.listIndex = listIndex;
	formatFileContentsRequest.dwFlags = dwFlags;

	WINPR_ASSERT(clipboard);
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

	formatFileContentsRequest.haveClipDataId = FALSE;
	return clipboard->context->ClientFileContentsRequest(clipboard->context,
	                                                     &formatFileContentsRequest);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
xf_cliprdr_server_file_contents_response(CliprdrClientContext* context,
                                         const CLIPRDR_FILE_CONTENTS_RESPONSE* fileContentsResponse)
{
	size_t count;
	size_t index;
	BOOL found = FALSE;
	xfCliprdrFuseStream* stream = NULL;
	xfCliprdrFuseInode* ino;
	UINT32 stream_id;
	const BYTE* data;
	size_t data_len;
	xfClipboard* clipboard;

	WINPR_ASSERT(context);
	WINPR_ASSERT(fileContentsResponse);

	clipboard = context->custom;
	WINPR_ASSERT(clipboard);

	stream_id = fileContentsResponse->streamId;
	data = fileContentsResponse->requestedData;
	data_len = fileContentsResponse->cbRequested;

	ArrayList_Lock(clipboard->stream_list);
	count = ArrayList_Count(clipboard->stream_list);

	for (index = 0; index < count; index++)
	{
		stream = (xfCliprdrFuseStream*)ArrayList_GetItem(clipboard->stream_list, index);

		if (stream->stream_id == stream_id)
		{
			found = TRUE;
			break;
		}
	}
	if (!found || !stream)
	{
		ArrayList_Unlock(clipboard->stream_list);
		return CHANNEL_RC_OK;
	}

	fuse_req_t req = stream->req;
	UINT32 req_type = stream->req_type;
	size_t req_ino = stream->req_ino;

	ArrayList_RemoveAt(clipboard->stream_list, index);
	ArrayList_Unlock(clipboard->stream_list);

	switch (req_type)
	{
		case FILECONTENTS_SIZE:
			/* fileContentsResponse->cbRequested should be 64bit*/
			if (data_len != sizeof(UINT64))
			{
				fuse_reply_err(req, EIO);
				break;
			}
			UINT64 size;
			wStream sbuffer = { 0 };
			wStream* s = Stream_StaticConstInit(&sbuffer, data, data_len);
			if (!s)
			{
				fuse_reply_err(req, ENOMEM);
				break;
			}
			Stream_Read_UINT64(s, size);

			ArrayList_Lock(clipboard->ino_list);
			ino = xf_cliprdr_fuse_util_get_inode(clipboard->ino_list, req_ino);
			/* ino must be exists and  */
			if (!ino)
			{
				ArrayList_Unlock(clipboard->ino_list);
				fuse_reply_err(req, EIO);
				break;
			}

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
			ArrayList_Unlock(clipboard->ino_list);
			fuse_reply_entry(req, &e);
			break;
		case FILECONTENTS_RANGE:
			fuse_reply_buf(req, (const char*)data, data_len);
			break;
	}
	return CHANNEL_RC_OK;
}
#endif

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT xf_cliprdr_monitor_ready(CliprdrClientContext* context,
                                     const CLIPRDR_MONITOR_READY* monitorReady)
{
	UINT ret;
	xfClipboard* clipboard;

	WINPR_ASSERT(context);
	WINPR_ASSERT(monitorReady);

	clipboard = context->custom;
	WINPR_ASSERT(clipboard);

	WINPR_UNUSED(monitorReady);

	if ((ret = xf_cliprdr_send_client_capabilities(clipboard)) != CHANNEL_RC_OK)
		return ret;

	xf_clipboard_formats_free(clipboard);

	if ((ret = xf_cliprdr_send_client_format_list(clipboard, TRUE)) != CHANNEL_RC_OK)
		return ret;

	clipboard->sync = TRUE;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT xf_cliprdr_server_capabilities(CliprdrClientContext* context,
                                           const CLIPRDR_CAPABILITIES* capabilities)
{
	UINT32 i;
	const CLIPRDR_GENERAL_CAPABILITY_SET* generalCaps;
	const BYTE* capsPtr;
	xfClipboard* clipboard;

	WINPR_ASSERT(context);
	WINPR_ASSERT(capabilities);

	clipboard = context->custom;
	WINPR_ASSERT(clipboard);

	capsPtr = (const BYTE*)capabilities->capabilitySets;
	WINPR_ASSERT(capsPtr);

	clipboard->streams_supported = FALSE;

	for (i = 0; i < capabilities->cCapabilitiesSets; i++)
	{
		const CLIPRDR_CAPABILITY_SET* caps = (const CLIPRDR_CAPABILITY_SET*)capsPtr;

		if (caps->capabilitySetType == CB_CAPSTYPE_GENERAL)
		{
			generalCaps = (const CLIPRDR_GENERAL_CAPABILITY_SET*)caps;

			if (generalCaps->generalFlags & CB_STREAM_FILECLIP_ENABLED)
			{
				clipboard->streams_supported = TRUE;
			}
		}

		capsPtr += caps->capabilitySetLength;
	}

	return CHANNEL_RC_OK;
}

static void xf_cliprdr_prepare_to_set_selection_owner(xfContext* xfc, xfClipboard* clipboard)
{
	WINPR_ASSERT(xfc);
	WINPR_ASSERT(clipboard);
	/*
	 * When you're writing to the selection in response to a
	 * normal X event like a mouse click or keyboard action, you
	 * get the selection timestamp by copying the time field out
	 * of that X event. Here, we're doing it on our own
	 * initiative, so we have to _request_ the X server time.
	 *
	 * There isn't a GetServerTime request in the X protocol, so I
	 * work around it by setting a property on our own window, and
	 * waiting for a PropertyNotify event to come back telling me
	 * it's been done - which will have a timestamp we can use.
	 */

	/* We have to set the property to some value, but it doesn't
	 * matter what. Set it to its own name, which we have here
	 * anyway! */
	Atom value = clipboard->timestamp_property_atom;

	XChangeProperty(xfc->display, xfc->drawable, clipboard->timestamp_property_atom, XA_ATOM, 32,
	                PropModeReplace, (BYTE*)&value, 1);
	XFlush(xfc->display);
}

static void xf_cliprdr_set_selection_owner(xfContext* xfc, xfClipboard* clipboard, Time timestamp)
{
	WINPR_ASSERT(xfc);
	WINPR_ASSERT(clipboard);
	/*
	 * Actually set ourselves up as the selection owner, now that
	 * we have a timestamp to use.
	 */

	clipboard->selection_ownership_timestamp = timestamp;
	XSetSelectionOwner(xfc->display, clipboard->clipboard_atom, xfc->drawable, timestamp);
	XFlush(xfc->display);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT xf_cliprdr_server_format_list(CliprdrClientContext* context,
                                          const CLIPRDR_FORMAT_LIST* formatList)
{
	UINT32 i;
	int j;
	xfContext* xfc;
	UINT ret;
	xfClipboard* clipboard;

	WINPR_ASSERT(context);
	WINPR_ASSERT(formatList);

	clipboard = context->custom;
	WINPR_ASSERT(clipboard);

	xfc = clipboard->xfc;
	WINPR_ASSERT(xfc);

	/* Clear the active SelectionRequest, as it is now invalid */
	free(clipboard->respond);
	clipboard->respond = NULL;

	xf_clipboard_formats_free(clipboard);
	xf_cliprdr_clear_cached_data(clipboard);
	clipboard->data_format_id = -1;
	clipboard->data_format_name = NULL;

	if (clipboard->serverFormats)
	{
		for (j = 0; j < clipboard->numServerFormats; j++)
			free(clipboard->serverFormats[j].formatName);

		free(clipboard->serverFormats);
		clipboard->serverFormats = NULL;
		clipboard->numServerFormats = 0;
	}

	clipboard->numServerFormats = formatList->numFormats + 1; /* +1 for CF_RAW */

	if (!(clipboard->serverFormats =
	          (CLIPRDR_FORMAT*)calloc(clipboard->numServerFormats, sizeof(CLIPRDR_FORMAT))))
	{
		WLog_ERR(TAG, "failed to allocate %d CLIPRDR_FORMAT structs", clipboard->numServerFormats);
		return CHANNEL_RC_NO_MEMORY;
	}

	for (i = 0; i < formatList->numFormats; i++)
	{
		const CLIPRDR_FORMAT* format = &formatList->formats[i];
		CLIPRDR_FORMAT* srvFormat = &clipboard->serverFormats[i];

		srvFormat->formatId = format->formatId;

		if (format->formatName)
		{
			srvFormat->formatName = _strdup(format->formatName);

			if (!srvFormat->formatName)
			{
				UINT32 k;

				for (k = 0; k < i; k++)
					free(clipboard->serverFormats[k].formatName);

				clipboard->numServerFormats = 0;
				free(clipboard->serverFormats);
				clipboard->serverFormats = NULL;
				return CHANNEL_RC_NO_MEMORY;
			}
		}
	}

	/* CF_RAW is always implicitly supported by the server */
	{
		CLIPRDR_FORMAT* format = &clipboard->serverFormats[formatList->numFormats];
		format->formatId = CF_RAW;
		format->formatName = NULL;
	}
	xf_cliprdr_provide_server_format_list(clipboard);
	clipboard->numTargets = 2;

	for (i = 0; i < formatList->numFormats; i++)
	{
		const CLIPRDR_FORMAT* format = &formatList->formats[i];

		for (j = 0; j < clipboard->numClientFormats; j++)
		{
			const xfCliprdrFormat* clientFormat = &clipboard->clientFormats[j];
			if (xf_cliprdr_formats_equal(format, clientFormat))
			{
				xf_cliprdr_append_target(clipboard, clientFormat->atom);
			}
		}
	}

	ret = xf_cliprdr_send_client_format_list_response(clipboard, TRUE);
	if (xfc->remote_app)
		xf_cliprdr_set_selection_owner(xfc, clipboard, CurrentTime);
	else
		xf_cliprdr_prepare_to_set_selection_owner(xfc, clipboard);
	return ret;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
xf_cliprdr_server_format_list_response(CliprdrClientContext* context,
                                       const CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(formatListResponse);
	// xfClipboard* clipboard = (xfClipboard*) context->custom;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
xf_cliprdr_server_format_data_request(CliprdrClientContext* context,
                                      const CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest)
{
	BOOL rawTransfer;
	const xfCliprdrFormat* format = NULL;
	UINT32 formatId;
	xfContext* xfc;
	xfClipboard* clipboard;

	WINPR_ASSERT(context);
	WINPR_ASSERT(formatDataRequest);

	clipboard = context->custom;
	WINPR_ASSERT(clipboard);

	xfc = clipboard->xfc;
	WINPR_ASSERT(xfc);

	formatId = formatDataRequest->requestedFormatId;

	rawTransfer = xf_cliprdr_is_raw_transfer_available(clipboard);

	if (rawTransfer)
	{
		format = xf_cliprdr_get_client_format_by_id(clipboard, CF_RAW);
		XChangeProperty(xfc->display, xfc->drawable, clipboard->property_atom, XA_INTEGER, 32,
		                PropModeReplace, (BYTE*)&formatId, 1);
	}
	else
		format = xf_cliprdr_get_client_format_by_id(clipboard, formatId);

	clipboard->requestedFormatId = rawTransfer ? CF_RAW : formatId;
	if (!format)
		return xf_cliprdr_send_data_response(clipboard, NULL, 0);

	XConvertSelection(xfc->display, clipboard->clipboard_atom, format->atom,
	                  clipboard->property_atom, xfc->drawable, CurrentTime);
	XFlush(xfc->display);
	/* After this point, we expect a SelectionNotify event from the clipboard owner. */
	return CHANNEL_RC_OK;
}

#ifdef WITH_FUSE
static const char* xf_cliprdr_fuse_split_basename(const char* name, size_t len)
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

static xfCliprdrFuseInode* xf_cliprdr_fuse_create_root_node(void)
{
	xfCliprdrFuseInode* rootNode = (xfCliprdrFuseInode*)calloc(1, sizeof(xfCliprdrFuseInode));
	if (!rootNode)
		return NULL;

	rootNode->ino = 1;
	rootNode->parent_ino = 1;
	rootNode->st_mode = S_IFDIR | 0755;
	rootNode->name = _strdup("/");
	rootNode->child_inos = ArrayList_New(TRUE);
	rootNode->st_mtim.tv_sec = time(NULL);
	rootNode->st_size = 0;
	rootNode->size_set = TRUE;

	if (!rootNode->child_inos || !rootNode->name)
	{
		xf_cliprdr_fuse_inode_free(rootNode);
		WLog_ERR(TAG, "fail to alloc rootNode's member");
		return NULL;
	}
	return rootNode;
}

static BOOL xf_cliprdr_fuse_check_stream(wStream* s, size_t count)
{
	UINT32 nrDescriptors;
	if (!Stream_CheckAndLogRequiredLength(TAG, s, sizeof(UINT32)))
		return FALSE;

	Stream_Read_UINT32(s, nrDescriptors);
	if (count != nrDescriptors)
	{
		WLog_WARN(TAG, "format data response mismatch");
		return FALSE;
	}
	return TRUE;
}

static BOOL xf_cliprdr_fuse_create_nodes(xfClipboard* clipboard, wStream* s, size_t count,
                                         const xfCliprdrFuseInode* rootNode)
{
	BOOL status = FALSE;
	size_t lindex = 0;
	char* curName = NULL;
	xfCliprdrFuseInode* inode = NULL;
	wHashTable* mapDir;

	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(s);
	WINPR_ASSERT(rootNode);

	mapDir = HashTable_New(TRUE);
	if (!mapDir)
	{
		WLog_ERR(TAG, "fail to alloc hashtable");
		goto error;
	}
	if (!HashTable_SetupForStringData(mapDir, FALSE))
		goto error;

	FILEDESCRIPTORW* descriptor = (FILEDESCRIPTORW*)calloc(1, sizeof(FILEDESCRIPTORW));
	if (!descriptor)
	{
		WLog_ERR(TAG, "fail to alloc FILEDESCRIPTORW");
		goto error;
	}
	/* here we assume that parent folder always appears before its children */
	for (; lindex < count; lindex++)
	{
		Stream_Read(s, descriptor, sizeof(FILEDESCRIPTORW));
		inode = (xfCliprdrFuseInode*)calloc(1, sizeof(xfCliprdrFuseInode));
		if (!inode)
		{
			WLog_ERR(TAG, "fail to alloc ino");
			break;
		}

		free(curName);
		curName =
		    ConvertWCharNToUtf8Alloc(descriptor->cFileName, ARRAYSIZE(descriptor->cFileName), NULL);
		if (!curName)
			break;

		const char* split_point = xf_cliprdr_fuse_split_basename(
		    curName, strnlen(curName, ARRAYSIZE(descriptor->cFileName)));

		UINT64 ticks;
		xfCliprdrFuseInode* parent;

		inode->lindex = lindex;
		inode->ino = lindex + 2;

		if (split_point == NULL)
		{
			char* baseName = _strdup(curName);
			if (!baseName)
				break;
			inode->parent_ino = 1;
			inode->name = baseName;
			if (!ArrayList_Append(rootNode->child_inos, (void*)inode->ino))
				break;
		}
		else
		{
			char* dirName = calloc(split_point - curName + 1, sizeof(char));
			if (!dirName)
				break;
			_snprintf(dirName, split_point - curName + 1, "%s", curName);
			/* drop last '\\' */
			char* baseName = _strdup(split_point + 1);
			if (!baseName)
				break;

			parent = (xfCliprdrFuseInode*)HashTable_GetItemValue(mapDir, dirName);
			if (!parent)
				break;
			inode->parent_ino = parent->ino;
			inode->name = baseName;
			if (!ArrayList_Append(parent->child_inos, (void*)inode->ino))
				break;
			free(dirName);
		}
		/* TODO: check FD_ATTRIBUTES in dwFlags
		    However if this flag is not valid how can we determine file/folder?
		*/
		if ((descriptor->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
		{
			inode->st_mode = S_IFDIR | 0755;
			inode->child_inos = ArrayList_New(TRUE);
			if (!inode->child_inos)
				break;
			inode->st_size = 0;
			inode->size_set = TRUE;
			char* tmpName = _strdup(curName);
			if (!tmpName)
				break;
			if (!HashTable_Insert(mapDir, tmpName, inode))
			{
				free(tmpName);
				break;
			}
		}
		else
		{
			inode->st_mode = S_IFREG | 0644;
			if ((descriptor->dwFlags & FD_FILESIZE) != 0)
			{
				inode->st_size = (((UINT64)descriptor->nFileSizeHigh) << 32) |
				                 ((UINT64)descriptor->nFileSizeLow);
				inode->size_set = TRUE;
			}
			else
			{
				inode->size_set = FALSE;
			}
		}

		if ((descriptor->dwFlags & FD_WRITESTIME) != 0)
		{
			ticks = (((UINT64)descriptor->ftLastWriteTime.dwHighDateTime << 32) |
			         ((UINT64)descriptor->ftLastWriteTime.dwLowDateTime)) -
			        WIN32_FILETIME_TO_UNIX_EPOCH_USEC;
			inode->st_mtim.tv_sec = ticks / 10000000ULL;
			/* tv_nsec Not used for now */
			inode->st_mtim.tv_nsec = ticks % 10000000ULL;
		}
		else
		{
			inode->st_mtim.tv_sec = time(NULL);
			inode->st_mtim.tv_nsec = 0;
		}

		if (!ArrayList_Append(clipboard->ino_list, inode))
			break;
	}
	/* clean up incomplete ino_list*/
	if (lindex != count)
	{
		/* baseName is freed in xf_cliprdr_fuse_inode_free*/
		xf_cliprdr_fuse_inode_free(inode);
		ArrayList_Clear(clipboard->ino_list);
	}
	else
	{
		status = TRUE;
	}

	free(descriptor);

error:
	free(curName);
	HashTable_Free(mapDir);
	return status;
}

/**
 * Generate inode list for fuse
 *
 * @return TRUE on success, FALSE on fail
 */
static BOOL xf_cliprdr_fuse_generate_list(xfClipboard* clipboard, const BYTE* data, UINT32 size)
{
	BOOL status = FALSE;
	wStream sbuffer = { 0 };
	wStream* s;

	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(data || (size == 0));

	if (size < 4)
	{
		WLog_ERR(TAG, "size of format data response invalid : %" PRIu32, size);
		return FALSE;
	}
	size_t count = (size - 4) / sizeof(FILEDESCRIPTORW);
	if (count < 1)
		return FALSE;

	s = Stream_StaticConstInit(&sbuffer, data, size);
	if (!s || !xf_cliprdr_fuse_check_stream(s, count))
	{
		WLog_ERR(TAG, "Stream_New failed");
		goto error;
	}

	/* prevent conflict between fuse_thread and this */
	ArrayList_Lock(clipboard->ino_list);
	xfCliprdrFuseInode* rootNode = xf_cliprdr_fuse_create_root_node();

	if (!rootNode || !ArrayList_Append(clipboard->ino_list, rootNode))
	{
		xf_cliprdr_fuse_inode_free(rootNode);
		WLog_ERR(TAG, "fail to alloc rootNode to ino_list");
		goto error2;
	}

	status = xf_cliprdr_fuse_create_nodes(clipboard, s, count, rootNode);

error2:
	ArrayList_Unlock(clipboard->ino_list);
error:
	return status;
}
#endif

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
xf_cliprdr_server_format_data_response(CliprdrClientContext* context,
                                       const CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse)
{
	BOOL bSuccess;
	BYTE* pDstData;
	UINT32 DstSize;
	UINT32 SrcSize;
	UINT32 srcFormatId;
	UINT32 dstFormatId;
	const xfCliprdrFormat* dstTargetFormat;
	BOOL nullTerminated = FALSE;
	UINT32 size;
	const BYTE* data;
	xfContext* xfc;
	xfClipboard* clipboard;

	WINPR_ASSERT(context);
	WINPR_ASSERT(formatDataResponse);

	clipboard = context->custom;
	WINPR_ASSERT(clipboard);

	xfc = clipboard->xfc;
	WINPR_ASSERT(xfc);

	size = formatDataResponse->common.dataLen;
	data = formatDataResponse->requestedFormatData;

	if (formatDataResponse->common.msgFlags == CB_RESPONSE_FAIL)
	{
		WLog_WARN(TAG, "Format Data Response PDU msgFlags is CB_RESPONSE_FAIL");
		free(clipboard->respond);
		clipboard->respond = NULL;
		return CHANNEL_RC_OK;
	}

	if (!clipboard->respond)
		return CHANNEL_RC_OK;

	xf_cliprdr_clear_cached_data(clipboard);
	pDstData = NULL;
	DstSize = 0;
	srcFormatId = 0;
	dstFormatId = 0;

	if (clipboard->data_raw_format)
	{
		srcFormatId = CF_RAW;
		dstFormatId = CF_RAW;
	}
	else if (clipboard->data_format_name)
	{
		if (strcmp(clipboard->data_format_name, "HTML Format") == 0)
		{
			srcFormatId = ClipboardGetFormatId(clipboard->system, "HTML Format");
			dstFormatId = ClipboardGetFormatId(clipboard->system, "text/html");
			nullTerminated = TRUE;
		}

		if (strcmp(clipboard->data_format_name, "FileGroupDescriptorW") == 0)
		{
#ifdef WITH_FUSE
			/* Build inode table for FILEDESCRIPTORW*/
			if (!xf_cliprdr_fuse_generate_list(clipboard, data, size))
			{
				/* just continue */
				WLog_WARN(TAG, "fail to generate list for FILEDESCRIPTOR");
			}
#endif

			srcFormatId = ClipboardGetFormatId(clipboard->system, "FileGroupDescriptorW");
			dstTargetFormat =
			    xf_cliprdr_get_client_format_by_atom(clipboard, clipboard->respond->target);
			if (!dstTargetFormat)
			{
				dstFormatId = ClipboardGetFormatId(clipboard->system, "text/uri-list");
			}
			else
			{
				switch (dstTargetFormat->formatId)
				{
					case CF_UNICODETEXT:
						dstFormatId = ClipboardGetFormatId(clipboard->system, "UTF8_STRING");
						break;
					case CB_FORMAT_TEXTURILIST:
						dstFormatId = ClipboardGetFormatId(clipboard->system, "text/uri-list");
						break;
					case CB_FORMAT_GNOMECOPIEDFILES:
						dstFormatId =
						    ClipboardGetFormatId(clipboard->system, "x-special/gnome-copied-files");
						break;
					case CB_FORMAT_MATECOPIEDFILES:
						dstFormatId =
						    ClipboardGetFormatId(clipboard->system, "x-special/mate-copied-files");
						break;
					default:
						break;
				}
			}

			nullTerminated = TRUE;
		}
	}
	else
	{
		switch (clipboard->data_format_id)
		{
			case CF_TEXT:
				srcFormatId = CF_TEXT;
				dstFormatId = ClipboardGetFormatId(clipboard->system, "UTF8_STRING");
				nullTerminated = TRUE;
				break;

			case CF_OEMTEXT:
				srcFormatId = CF_OEMTEXT;
				dstFormatId = ClipboardGetFormatId(clipboard->system, "UTF8_STRING");
				nullTerminated = TRUE;
				break;

			case CF_UNICODETEXT:
				srcFormatId = CF_UNICODETEXT;
				dstFormatId = ClipboardGetFormatId(clipboard->system, "UTF8_STRING");
				nullTerminated = TRUE;
				break;

			case CF_DIB:
				srcFormatId = CF_DIB;
				dstFormatId = ClipboardGetFormatId(clipboard->system, "image/bmp");
				break;

			default:
				break;
		}
	}

	SrcSize = (UINT32)size;
	bSuccess = ClipboardSetData(clipboard->system, srcFormatId, data, SrcSize);

	if (bSuccess)
	{
		if (SrcSize == 0)
		{
			WLog_DBG(TAG, "skipping, empty data detected!");
			free(clipboard->respond);
			clipboard->respond = NULL;
			return CHANNEL_RC_OK;
		}

		pDstData = (BYTE*)ClipboardGetData(clipboard->system, dstFormatId, &DstSize);

		if (!pDstData)
		{
			WLog_WARN(TAG, "failed to get clipboard data in format %s [source format %s]",
			          ClipboardGetFormatName(clipboard->system, dstFormatId),
			          ClipboardGetFormatName(clipboard->system, srcFormatId));
		}

		if (nullTerminated && pDstData)
		{
			BYTE* nullTerminator = memchr(pDstData, '\0', DstSize);
			if (nullTerminator)
				DstSize = nullTerminator - pDstData;
		}
	}

	/* Cache converted and original data to avoid doing a possibly costly
	 * conversion again on subsequent requests */
	clipboard->data = pDstData;
	clipboard->data_length = DstSize;
	/* We have to copy the original data again, as pSrcData is now owned
	 * by clipboard->system. Memory allocation failure is not fatal here
	 * as this is only a cached value. */
	clipboard->data_raw = (BYTE*)malloc(size);

	if (clipboard->data_raw)
	{
		CopyMemory(clipboard->data_raw, data, size);
		clipboard->data_raw_length = size;
	}
	else
	{
		WLog_WARN(TAG, "failed to allocate %" PRIu32 " bytes for a copy of raw clipboard data",
		          size);
	}

	xf_cliprdr_provide_data(clipboard, clipboard->respond, pDstData, DstSize);
	{
		union
		{
			XEvent* ev;
			XSelectionEvent* sev;
		} conv;

		conv.sev = clipboard->respond;

		XSendEvent(xfc->display, clipboard->respond->requestor, 0, 0, conv.ev);
		XFlush(xfc->display);
	}
	free(clipboard->respond);
	clipboard->respond = NULL;
	return CHANNEL_RC_OK;
}

static UINT
xf_cliprdr_server_file_size_request(xfClipboard* clipboard,
                                    const CLIPRDR_FILE_CONTENTS_REQUEST* fileContentsRequest)
{
	wClipboardFileSizeRequest request = { 0 };

	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(fileContentsRequest);

	request.streamId = fileContentsRequest->streamId;
	request.listIndex = fileContentsRequest->listIndex;

	if (fileContentsRequest->cbRequested != sizeof(UINT64))
	{
		WLog_WARN(TAG, "unexpected FILECONTENTS_SIZE request: %" PRIu32 " bytes",
		          fileContentsRequest->cbRequested);
	}

	WINPR_ASSERT(clipboard->delegate);
	WINPR_ASSERT(clipboard->delegate->ClientRequestFileSize);
	return clipboard->delegate->ClientRequestFileSize(clipboard->delegate, &request);
}

static UINT
xf_cliprdr_server_file_range_request(xfClipboard* clipboard,
                                     const CLIPRDR_FILE_CONTENTS_REQUEST* fileContentsRequest)
{
	wClipboardFileRangeRequest request = { 0 };

	WINPR_ASSERT(fileContentsRequest);

	request.streamId = fileContentsRequest->streamId;
	request.listIndex = fileContentsRequest->listIndex;
	request.nPositionLow = fileContentsRequest->nPositionLow;
	request.nPositionHigh = fileContentsRequest->nPositionHigh;
	request.cbRequested = fileContentsRequest->cbRequested;

	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(clipboard->delegate);
	WINPR_ASSERT(clipboard->delegate->ClientRequestFileRange);
	return clipboard->delegate->ClientRequestFileRange(clipboard->delegate, &request);
}

static UINT
xf_cliprdr_send_file_contents_failure(CliprdrClientContext* context,
                                      const CLIPRDR_FILE_CONTENTS_REQUEST* fileContentsRequest)
{
	CLIPRDR_FILE_CONTENTS_RESPONSE response = { 0 };

	WINPR_ASSERT(fileContentsRequest);

	response.common.msgFlags = CB_RESPONSE_FAIL;
	response.streamId = fileContentsRequest->streamId;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->ClientFileContentsResponse);
	return context->ClientFileContentsResponse(context, &response);
}

static UINT
xf_cliprdr_server_file_contents_request(CliprdrClientContext* context,
                                        const CLIPRDR_FILE_CONTENTS_REQUEST* fileContentsRequest)
{
	UINT error = NO_ERROR;
	xfClipboard* clipboard;

	WINPR_ASSERT(context);
	WINPR_ASSERT(fileContentsRequest);

	clipboard = context->custom;
	WINPR_ASSERT(clipboard);

	/*
	 * MS-RDPECLIP 2.2.5.3 File Contents Request PDU (CLIPRDR_FILECONTENTS_REQUEST):
	 * The FILECONTENTS_SIZE and FILECONTENTS_RANGE flags MUST NOT be set at the same time.
	 */
	if ((fileContentsRequest->dwFlags & (FILECONTENTS_SIZE | FILECONTENTS_RANGE)) ==
	    (FILECONTENTS_SIZE | FILECONTENTS_RANGE))
	{
		WLog_ERR(TAG, "invalid CLIPRDR_FILECONTENTS_REQUEST.dwFlags");
		return xf_cliprdr_send_file_contents_failure(context, fileContentsRequest);
	}

	if (fileContentsRequest->dwFlags & FILECONTENTS_SIZE)
		error = xf_cliprdr_server_file_size_request(clipboard, fileContentsRequest);

	if (fileContentsRequest->dwFlags & FILECONTENTS_RANGE)
		error = xf_cliprdr_server_file_range_request(clipboard, fileContentsRequest);

	if (error)
	{
		WLog_ERR(TAG, "failed to handle CLIPRDR_FILECONTENTS_REQUEST: 0x%08X", error);
		return xf_cliprdr_send_file_contents_failure(context, fileContentsRequest);
	}

	return CHANNEL_RC_OK;
}

static UINT xf_cliprdr_clipboard_file_size_success(wClipboardDelegate* delegate,
                                                   const wClipboardFileSizeRequest* request,
                                                   UINT64 fileSize)
{
	CLIPRDR_FILE_CONTENTS_RESPONSE response = { 0 };
	xfClipboard* clipboard;

	WINPR_ASSERT(delegate);
	WINPR_ASSERT(request);

	response.common.msgFlags = CB_RESPONSE_OK;
	response.streamId = request->streamId;
	response.cbRequested = sizeof(UINT64);
	response.requestedData = (BYTE*)&fileSize;

	clipboard = delegate->custom;
	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(clipboard->context);
	WINPR_ASSERT(clipboard->context->ClientFileContentsResponse);
	return clipboard->context->ClientFileContentsResponse(clipboard->context, &response);
}

static UINT xf_cliprdr_clipboard_file_size_failure(wClipboardDelegate* delegate,
                                                   const wClipboardFileSizeRequest* request,
                                                   UINT errorCode)
{
	CLIPRDR_FILE_CONTENTS_RESPONSE response = { 0 };
	xfClipboard* clipboard;

	WINPR_ASSERT(delegate);
	WINPR_ASSERT(request);
	WINPR_UNUSED(errorCode);

	response.common.msgFlags = CB_RESPONSE_FAIL;
	response.streamId = request->streamId;

	clipboard = delegate->custom;
	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(clipboard->context);
	WINPR_ASSERT(clipboard->context->ClientFileContentsResponse);
	return clipboard->context->ClientFileContentsResponse(clipboard->context, &response);
}

static UINT xf_cliprdr_clipboard_file_range_success(wClipboardDelegate* delegate,
                                                    const wClipboardFileRangeRequest* request,
                                                    const BYTE* data, UINT32 size)
{
	CLIPRDR_FILE_CONTENTS_RESPONSE response = { 0 };
	xfClipboard* clipboard;

	WINPR_ASSERT(delegate);
	WINPR_ASSERT(request);

	response.common.msgFlags = CB_RESPONSE_OK;
	response.streamId = request->streamId;
	response.cbRequested = size;
	response.requestedData = (const BYTE*)data;

	clipboard = delegate->custom;
	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(clipboard->context);
	WINPR_ASSERT(clipboard->context->ClientFileContentsResponse);
	return clipboard->context->ClientFileContentsResponse(clipboard->context, &response);
}

static UINT xf_cliprdr_clipboard_file_range_failure(wClipboardDelegate* delegate,
                                                    const wClipboardFileRangeRequest* request,
                                                    UINT errorCode)
{
	CLIPRDR_FILE_CONTENTS_RESPONSE response = { 0 };
	xfClipboard* clipboard;

	WINPR_ASSERT(delegate);
	WINPR_ASSERT(request);
	WINPR_UNUSED(errorCode);

	response.common.msgFlags = CB_RESPONSE_FAIL;
	response.streamId = request->streamId;

	clipboard = delegate->custom;
	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(clipboard->context);
	WINPR_ASSERT(clipboard->context->ClientFileContentsResponse);
	return clipboard->context->ClientFileContentsResponse(clipboard->context, &response);
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

#ifdef WITH_FUSE
/* For better understanding the relationship between ino and index of arraylist*/
static inline xfCliprdrFuseInode* xf_cliprdr_fuse_util_get_inode(wArrayList* ino_list,
                                                                 fuse_ino_t ino)
{
	size_t list_index = ino - 1;
	return (xfCliprdrFuseInode*)ArrayList_GetItem(ino_list, list_index);
}

/* fuse helper functions*/
static int xf_cliprdr_fuse_util_stat(xfClipboard* clipboard, fuse_ino_t ino, struct stat* stbuf)
{
	int err = 0;
	xfCliprdrFuseInode* node;

	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(stbuf);

	ArrayList_Lock(clipboard->ino_list);

	node = xf_cliprdr_fuse_util_get_inode(clipboard->ino_list, ino);

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
	ArrayList_Unlock(clipboard->ino_list);
	return err;
}

static int xf_cliprdr_fuse_util_stmode(xfClipboard* clipboard, fuse_ino_t ino, mode_t* mode)
{
	int err = 0;
	xfCliprdrFuseInode* node;

	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(mode);

	ArrayList_Lock(clipboard->ino_list);

	node = xf_cliprdr_fuse_util_get_inode(clipboard->ino_list, ino);
	if (!node)
	{
		err = ENOENT;
		goto error;
	}
	*mode = node->st_mode;
error:
	ArrayList_Unlock(clipboard->ino_list);
	return err;
}

static int xf_cliprdr_fuse_util_lindex(xfClipboard* clipboard, fuse_ino_t ino, UINT32* lindex)
{
	int err = 0;
	xfCliprdrFuseInode* node;

	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(lindex);

	ArrayList_Lock(clipboard->ino_list);

	node = xf_cliprdr_fuse_util_get_inode(clipboard->ino_list, ino);
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
	ArrayList_Unlock(clipboard->ino_list);
	return err;
}

static int xf_cliprdr_fuse_util_add_stream_list(xfClipboard* clipboard, fuse_req_t req,
                                                UINT32* stream_id)
{
	int err = 0;
	xfCliprdrFuseStream* stream;

	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(stream_id);

	stream = (xfCliprdrFuseStream*)calloc(1, sizeof(xfCliprdrFuseStream));
	if (!stream)
	{
		err = ENOMEM;
		return err;
	}
	ArrayList_Lock(clipboard->stream_list);
	stream->req = req;
	stream->req_type = FILECONTENTS_RANGE;
	stream->stream_id = clipboard->current_stream_id;
	*stream_id = stream->stream_id;
	stream->req_ino = 0;
	clipboard->current_stream_id++;
	if (!ArrayList_Append(clipboard->stream_list, stream))
	{
		err = ENOMEM;
		goto error;
	}
error:
	ArrayList_Unlock(clipboard->stream_list);
	return err;
}

static void xf_cliprdr_fuse_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi)
{
	int err;
	struct stat stbuf;

	xfClipboard* clipboard = (xfClipboard*)fuse_req_userdata(req);
	WINPR_ASSERT(clipboard);

	err = xf_cliprdr_fuse_util_stat(clipboard, ino, &stbuf);
	if (err)
	{
		fuse_reply_err(req, err);
		return;
	}

	fuse_reply_attr(req, &stbuf, 0);
}

static void xf_cliprdr_fuse_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
                                    struct fuse_file_info* fi)
{
	size_t count;
	size_t index;
	size_t child_ino;
	size_t direntry_len;
	char* buf;
	size_t pos = 0;
	xfCliprdrFuseInode* child_node;
	xfCliprdrFuseInode* node;
	xfClipboard* clipboard = (xfClipboard*)fuse_req_userdata(req);

	WINPR_ASSERT(clipboard);

	ArrayList_Lock(clipboard->ino_list);
	node = xf_cliprdr_fuse_util_get_inode(clipboard->ino_list, ino);

	if (!node || !node->child_inos)
	{
		ArrayList_Unlock(clipboard->ino_list);
		fuse_reply_err(req, ENOENT);
		return;
	}
	else if ((node->st_mode & S_IFDIR) == 0)
	{
		ArrayList_Unlock(clipboard->ino_list);
		fuse_reply_err(req, ENOTDIR);
		return;
	}

	ArrayList_Lock(node->child_inos);
	count = ArrayList_Count(node->child_inos);
	if ((count == 0) || ((SSIZE_T)(count + 1) <= off))
	{
		ArrayList_Unlock(node->child_inos);
		ArrayList_Unlock(clipboard->ino_list);
		fuse_reply_buf(req, NULL, 0);
		return;
	}
	else
	{
		buf = (char*)calloc(size, sizeof(char));
		if (!buf)
		{
			ArrayList_Unlock(node->child_inos);
			ArrayList_Unlock(clipboard->ino_list);
			fuse_reply_err(req, ENOMEM);
			return;
		}
		for (index = off; index < count + 2; index++)
		{
			struct stat stbuf = { 0 };
			if (index == 0)
			{
				stbuf.st_ino = ino;
				direntry_len = fuse_add_direntry(req, buf + pos, size - pos, ".", &stbuf, index);
				if (direntry_len > size - pos)
					break;
				pos += direntry_len;
			}
			else if (index == 1)
			{
				stbuf.st_ino = node->parent_ino;
				direntry_len = fuse_add_direntry(req, buf + pos, size - pos, "..", &stbuf, index);
				if (direntry_len > size - pos)
					break;
				pos += direntry_len;
			}
			else
			{
				/* execlude . and .. */
				child_ino = (size_t)ArrayList_GetItem(node->child_inos, index - 2);
				/* previous lock for ino_list still work*/
				child_node = xf_cliprdr_fuse_util_get_inode(clipboard->ino_list, child_ino);
				if (!child_node)
					break;
				stbuf.st_ino = child_node->ino;
				direntry_len =
				    fuse_add_direntry(req, buf + pos, size - pos, child_node->name, &stbuf, index);
				if (direntry_len > size - pos)
					break;
				pos += direntry_len;
			}
		}

		ArrayList_Unlock(node->child_inos);
		ArrayList_Unlock(clipboard->ino_list);
		fuse_reply_buf(req, buf, pos);
		free(buf);
		return;
	}
}

static void xf_cliprdr_fuse_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi)
{
	int err;
	mode_t mode = 0;
	xfClipboard* clipboard = (xfClipboard*)fuse_req_userdata(req);

	WINPR_ASSERT(clipboard);
	err = xf_cliprdr_fuse_util_stmode(clipboard, ino, &mode);
	if (err)
	{
		fuse_reply_err(req, err);
		return;
	}

	if ((mode & S_IFDIR) != 0)
	{
		fuse_reply_err(req, EISDIR);
	}
	else
	{
		/* Important for KDE to get file correctly*/
		fi->direct_io = 1;
		fuse_reply_open(req, fi);
	}
}

static void xf_cliprdr_fuse_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
                                 struct fuse_file_info* fi)
{
	if (ino < 2)
	{
		fuse_reply_err(req, ENOENT);
		return;
	}
	int err;
	xfClipboard* clipboard = (xfClipboard*)fuse_req_userdata(req);
	UINT32 lindex;
	UINT32 stream_id;

	WINPR_ASSERT(clipboard);

	err = xf_cliprdr_fuse_util_lindex(clipboard, ino, &lindex);
	if (err)
	{
		fuse_reply_err(req, err);
		return;
	}

	err = xf_cliprdr_fuse_util_add_stream_list(clipboard, req, &stream_id);
	if (err)
	{
		fuse_reply_err(req, err);
		return;
	}

	UINT32 nPositionLow = (off >> 0) & 0xFFFFFFFF;
	UINT32 nPositionHigh = (off >> 32) & 0xFFFFFFFF;

	xf_cliprdr_send_client_file_contents(clipboard, stream_id, lindex, FILECONTENTS_RANGE,
	                                     nPositionLow, nPositionHigh, size);
}

static void xf_cliprdr_fuse_lookup(fuse_req_t req, fuse_ino_t parent, const char* name)
{
	size_t index;
	size_t count;
	size_t child_ino;
	BOOL found = FALSE;
	struct fuse_entry_param e = { 0 };
	xfCliprdrFuseInode* parent_node;
	xfCliprdrFuseInode* child_node = NULL;
	xfClipboard* clipboard = (xfClipboard*)fuse_req_userdata(req);

	WINPR_ASSERT(clipboard);

	ArrayList_Lock(clipboard->ino_list);
	parent_node = xf_cliprdr_fuse_util_get_inode(clipboard->ino_list, parent);

	if (!parent_node || !parent_node->child_inos)
	{
		ArrayList_Unlock(clipboard->ino_list);
		fuse_reply_err(req, ENOENT);
		return;
	}

	ArrayList_Lock(parent_node->child_inos);
	count = ArrayList_Count(parent_node->child_inos);
	for (index = 0; index < count; index++)
	{
		child_ino = (size_t)ArrayList_GetItem(parent_node->child_inos, index);
		child_node = xf_cliprdr_fuse_util_get_inode(clipboard->ino_list, child_ino);
		if (child_node && strcmp(name, child_node->name) == 0)
		{
			found = TRUE;
			break;
		}
	}
	ArrayList_Unlock(parent_node->child_inos);

	if (!found || !child_node)
	{
		ArrayList_Unlock(clipboard->ino_list);
		fuse_reply_err(req, ENOENT);
		return;
	}

	BOOL res;
	UINT32 stream_id;
	BOOL size_set = child_node->size_set;
	size_t lindex = child_node->lindex;
	size_t ino = child_node->ino;
	mode_t st_mode = child_node->st_mode;
	off_t st_size = child_node->st_size;
	time_t tv_sec = child_node->st_mtim.tv_sec;
	ArrayList_Unlock(clipboard->ino_list);

	if (!size_set)
	{
		xfCliprdrFuseStream* stream = (xfCliprdrFuseStream*)calloc(1, sizeof(xfCliprdrFuseStream));
		if (!stream)
		{
			fuse_reply_err(req, ENOMEM);
			return;
		}
		ArrayList_Lock(clipboard->stream_list);
		stream->req = req;
		stream->req_type = FILECONTENTS_SIZE;
		stream->stream_id = clipboard->current_stream_id;
		stream_id = stream->stream_id;
		stream->req_ino = ino;
		clipboard->current_stream_id++;
		res = ArrayList_Append(clipboard->stream_list, stream);
		ArrayList_Unlock(clipboard->stream_list);
		if (!res)
		{
			fuse_reply_err(req, ENOMEM);
			return;
		}
		xf_cliprdr_send_client_file_contents(clipboard, stream_id, lindex, FILECONTENTS_SIZE, 0, 0,
		                                     0);
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
	fuse_reply_entry(req, &e);
	return;
}

static void xf_cliprdr_fuse_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi)
{
	int err;
	mode_t mode = 0;
	xfClipboard* clipboard = (xfClipboard*)fuse_req_userdata(req);
	WINPR_ASSERT(clipboard);

	err = xf_cliprdr_fuse_util_stmode(clipboard, ino, &mode);
	if (err)
	{
		fuse_reply_err(req, err);
		return;
	}

	if ((mode & S_IFDIR) == 0)
	{
		fuse_reply_err(req, ENOTDIR);
	}
	else
	{
		fuse_reply_open(req, fi);
	}
}

static struct fuse_lowlevel_ops xf_cliprdr_fuse_oper = {
	.lookup = xf_cliprdr_fuse_lookup,
	.getattr = xf_cliprdr_fuse_getattr,
	.readdir = xf_cliprdr_fuse_readdir,
	.open = xf_cliprdr_fuse_open,
	.read = xf_cliprdr_fuse_read,
	.opendir = xf_cliprdr_fuse_opendir,
};

static DWORD WINAPI xf_cliprdr_fuse_thread(LPVOID arg)
{
	xfClipboard* clipboard = (xfClipboard*)arg;

	/* TODO get basePath from config or use default*/
	UINT32 basePathSize;
	char* basePath;
	char* tmpPath;

	WINPR_ASSERT(clipboard);

	tmpPath = GetKnownPath(KNOWN_PATH_TEMP);
	/* 10 is max length of DWORD string and 1 for \0 */
	basePathSize = strlen(tmpPath) + strlen("/.xfreerdp.cliprdr.") + 11;
	basePath = calloc(basePathSize, sizeof(char));
	if (!basePath)
	{
		WLog_ERR(TAG, "Failed to alloc for basepath");
		free(tmpPath);
		return 0;
	}
	_snprintf(&basePath[0], basePathSize, "%s/.xfreerdp.cliprdr.%" PRIu32, tmpPath,
	          GetCurrentProcessId());
	free(tmpPath);

	if (!winpr_PathFileExists(basePath) && !winpr_PathMakePath(basePath, 0))
	{
		WLog_ERR(TAG, "Failed to create directory '%s'", basePath);
		free(basePath);
		return 0;
	}
	clipboard->delegate->basePath = basePath;

	struct fuse_args args = FUSE_ARGS_INIT(0, NULL);
#if FUSE_USE_VERSION >= 30
	fuse_opt_add_arg(&args, clipboard->delegate->basePath);
	if ((clipboard->fuse_sess = fuse_session_new(
	         &args, &xf_cliprdr_fuse_oper, sizeof(xf_cliprdr_fuse_oper), (void*)clipboard)) != NULL)
	{
		if (0 == fuse_session_mount(clipboard->fuse_sess, clipboard->delegate->basePath))
		{
			fuse_session_loop(clipboard->fuse_sess);
			fuse_session_unmount(clipboard->fuse_sess);
		}
		fuse_session_destroy(clipboard->fuse_sess);
	}
#else
	struct fuse_chan* ch;
	int err;

	if ((ch = fuse_mount(clipboard->delegate->basePath, &args)) != NULL)
	{
		clipboard->fuse_sess = fuse_lowlevel_new(&args, &xf_cliprdr_fuse_oper,
		                                         sizeof(xf_cliprdr_fuse_oper), (void*)clipboard);
		if (clipboard->fuse_sess != NULL)
		{
			fuse_session_add_chan(clipboard->fuse_sess, ch);
			err = fuse_session_loop(clipboard->fuse_sess);
			fuse_session_remove_chan(ch);
			fuse_session_destroy(clipboard->fuse_sess);
		}
		fuse_unmount(clipboard->delegate->basePath, ch);
	}
#endif
	fuse_opt_free_args(&args);

	winpr_RemoveDirectory(clipboard->delegate->basePath);

	ExitThread(0);
	return 0;
}
#endif

xfClipboard* xf_clipboard_new(xfContext* xfc, BOOL relieveFilenameRestriction)
{
	int i, n = 0;
	rdpChannels* channels;
	xfClipboard* clipboard;
	const char* selectionAtom;
	xfCliprdrFormat* clientFormat;

	WINPR_ASSERT(xfc);
	WINPR_ASSERT(xfc->common.context.settings);

	if (!(clipboard = (xfClipboard*)calloc(1, sizeof(xfClipboard))))
	{
		WLog_ERR(TAG, "failed to allocate xfClipboard data");
		return NULL;
	}

	xfc->clipboard = clipboard;
	clipboard->xfc = xfc;
	channels = xfc->common.context.channels;
	clipboard->channels = channels;
	clipboard->system = ClipboardCreate();
	clipboard->requestedFormatId = -1;
	clipboard->root_window = DefaultRootWindow(xfc->display);
	selectionAtom = "CLIPBOARD";
	if (xfc->common.context.settings->XSelectionAtom)
		selectionAtom = xfc->common.context.settings->XSelectionAtom;
	clipboard->clipboard_atom = XInternAtom(xfc->display, selectionAtom, FALSE);

	if (clipboard->clipboard_atom == None)
	{
		WLog_ERR(TAG, "unable to get %s atom", selectionAtom);
		goto error;
	}

	clipboard->timestamp_property_atom =
	    XInternAtom(xfc->display, "_FREERDP_TIMESTAMP_PROPERTY", FALSE);
	clipboard->property_atom = XInternAtom(xfc->display, "_FREERDP_CLIPRDR", FALSE);
	clipboard->raw_transfer_atom = XInternAtom(xfc->display, "_FREERDP_CLIPRDR_RAW", FALSE);
	clipboard->raw_format_list_atom = XInternAtom(xfc->display, "_FREERDP_CLIPRDR_FORMATS", FALSE);
	xf_cliprdr_set_raw_transfer_enabled(clipboard, TRUE);
	XSelectInput(xfc->display, clipboard->root_window, PropertyChangeMask);
#ifdef WITH_XFIXES

	if (XFixesQueryExtension(xfc->display, &clipboard->xfixes_event_base,
	                         &clipboard->xfixes_error_base))
	{
		int xfmajor, xfminor;

		if (XFixesQueryVersion(xfc->display, &xfmajor, &xfminor))
		{
			XFixesSelectSelectionInput(xfc->display, clipboard->root_window,
			                           clipboard->clipboard_atom,
			                           XFixesSetSelectionOwnerNotifyMask);
			clipboard->xfixes_supported = TRUE;
		}
		else
		{
			WLog_ERR(TAG, "Error querying X Fixes extension version");
		}
	}
	else
	{
		WLog_ERR(TAG, "Error loading X Fixes extension");
	}

#else
	WLog_ERR(
	    TAG,
	    "Warning: Using clipboard redirection without XFIXES extension is strongly discouraged!");
#endif
	clientFormat = &clipboard->clientFormats[n++];
	clientFormat->atom = XInternAtom(xfc->display, "_FREERDP_RAW", False);
	clientFormat->formatId = CF_RAW;

	clientFormat = &clipboard->clientFormats[n++];
	clientFormat->atom = XInternAtom(xfc->display, "UTF8_STRING", False);
	clientFormat->formatId = CF_UNICODETEXT;

	clientFormat = &clipboard->clientFormats[n++];
	clientFormat->atom = XA_STRING;
	clientFormat->formatId = CF_TEXT;

	clientFormat = &clipboard->clientFormats[n++];
	clientFormat->atom = XInternAtom(xfc->display, "image/png", False);
	clientFormat->formatId = CB_FORMAT_PNG;

	clientFormat = &clipboard->clientFormats[n++];
	clientFormat->atom = XInternAtom(xfc->display, "image/jpeg", False);
	clientFormat->formatId = CB_FORMAT_JPEG;

	clientFormat = &clipboard->clientFormats[n++];
	clientFormat->atom = XInternAtom(xfc->display, "image/gif", False);
	clientFormat->formatId = CB_FORMAT_GIF;

	clientFormat = &clipboard->clientFormats[n++];
	clientFormat->atom = XInternAtom(xfc->display, "image/bmp", False);
	clientFormat->formatId = CF_DIB;

	clientFormat = &clipboard->clientFormats[n++];
	clientFormat->atom = XInternAtom(xfc->display, "text/html", False);
	clientFormat->formatId = CB_FORMAT_HTML;
	clientFormat->formatName = _strdup("HTML Format");

	if (!clientFormat->formatName)
		goto error;

	clientFormat = &clipboard->clientFormats[n++];

	/*
	 * Existence of registered format IDs for file formats does not guarantee that they are
	 * in fact supported by wClipboard (as further initialization may have failed after format
	 * registration). However, they are definitely not supported if there are no registered
	 * formats. In this case we should not list file formats in TARGETS.
	 */
	if (ClipboardGetFormatId(clipboard->system, "text/uri-list"))
	{
		clipboard->file_formats_registered = TRUE;
		clientFormat->atom = XInternAtom(xfc->display, "text/uri-list", False);
		clientFormat->formatId = CB_FORMAT_TEXTURILIST;
		clientFormat->formatName = _strdup("FileGroupDescriptorW");

		if (!clientFormat->formatName)
			goto error;

		clientFormat = &clipboard->clientFormats[n++];
	}
	if (ClipboardGetFormatId(clipboard->system, "x-special/gnome-copied-files"))
	{
		clipboard->file_formats_registered = TRUE;
		clientFormat->atom = XInternAtom(xfc->display, "x-special/gnome-copied-files", False);
		clientFormat->formatId = CB_FORMAT_GNOMECOPIEDFILES;
		clientFormat->formatName = _strdup("FileGroupDescriptorW");

		if (!clientFormat->formatName)
			goto error;

		clientFormat = &clipboard->clientFormats[n++];
	}
	if (ClipboardGetFormatId(clipboard->system, "x-special/mate-copied-files"))
	{
		clipboard->file_formats_registered = TRUE;
		clientFormat->atom = XInternAtom(xfc->display, "x-special/mate-copied-files", False);
		clientFormat->formatId = CB_FORMAT_MATECOPIEDFILES;
		clientFormat->formatName = _strdup("FileGroupDescriptorW");

		if (!clientFormat->formatName)
			goto error;
	}

	clipboard->numClientFormats = n;
	clipboard->targets[0] = XInternAtom(xfc->display, "TIMESTAMP", FALSE);
	clipboard->targets[1] = XInternAtom(xfc->display, "TARGETS", FALSE);
	clipboard->numTargets = 2;
	clipboard->incr_atom = XInternAtom(xfc->display, "INCR", FALSE);
	clipboard->delegate = ClipboardGetDelegate(clipboard->system);
	clipboard->delegate->custom = clipboard;

#ifdef WITH_FUSE
	clipboard->current_stream_id = 0;
	clipboard->stream_list = ArrayList_New(TRUE);
	if (!clipboard->stream_list)
	{
		WLog_ERR(TAG, "failed to allocate stream_list");
		goto error;
	}
	wObject* obj = ArrayList_Object(clipboard->stream_list);
	obj->fnObjectFree = free;

	clipboard->ino_list = ArrayList_New(TRUE);
	if (!clipboard->ino_list)
	{
		WLog_ERR(TAG, "failed to allocate stream_list");
		goto error2;
	}
	obj = ArrayList_Object(clipboard->ino_list);
	obj->fnObjectFree = xf_cliprdr_fuse_inode_free;

	if (!(clipboard->fuse_thread =
	          CreateThread(NULL, 0, xf_cliprdr_fuse_thread, clipboard, 0, NULL)))
	{
		goto error3;
	}
#endif

	clipboard->delegate->ClipboardFileSizeSuccess = xf_cliprdr_clipboard_file_size_success;
	clipboard->delegate->ClipboardFileSizeFailure = xf_cliprdr_clipboard_file_size_failure;
	clipboard->delegate->ClipboardFileRangeSuccess = xf_cliprdr_clipboard_file_range_success;
	clipboard->delegate->ClipboardFileRangeFailure = xf_cliprdr_clipboard_file_range_failure;

	if (relieveFilenameRestriction)
	{
		WLog_DBG(TAG, "Relieving CLIPRDR filename restriction");
		clipboard->delegate->IsFileNameComponentValid = xf_cliprdr_clipboard_is_valid_unix_filename;
	}

	return clipboard;

#ifdef WITH_FUSE
error3:

	ArrayList_Free(clipboard->ino_list);
error2:

	ArrayList_Free(clipboard->stream_list);
#endif
error:

	for (i = 0; i < n; i++)
	{
		xfCliprdrFormat* format = &clipboard->clientFormats[i];
		free(format->formatName);
	}

	ClipboardDestroy(clipboard->system);
	free(clipboard);
	return NULL;
}

void xf_clipboard_free(xfClipboard* clipboard)
{
	int i;

	if (!clipboard)
		return;

	if (clipboard->serverFormats)
	{
		for (i = 0; i < clipboard->numServerFormats; i++)
		{
			CLIPRDR_FORMAT* format = &clipboard->serverFormats[i];
			free(format->formatName);
		}

		free(clipboard->serverFormats);
		clipboard->serverFormats = NULL;
	}

	if (clipboard->numClientFormats)
	{
		for (i = 0; i < clipboard->numClientFormats; i++)
		{
			xfCliprdrFormat* format = &clipboard->clientFormats[i];
			free(format->formatName);
		}
	}

#ifdef WITH_FUSE
	if (clipboard->fuse_thread)
	{
		if (clipboard->fuse_sess)
		{
			fuse_session_exit(clipboard->fuse_sess);
			/* 	not elegant but works for umounting FUSE
			    fuse_chan must receieve a oper buf to unblock fuse_session_receive_buf function.
			*/
			winpr_PathFileExists(clipboard->delegate->basePath);
		}
		WaitForSingleObject(clipboard->fuse_thread, INFINITE);
		CloseHandle(clipboard->fuse_thread);
	}

	if (clipboard->delegate)
		free(clipboard->delegate->basePath);

	// fuse related
	ArrayList_Free(clipboard->stream_list);
	ArrayList_Free(clipboard->ino_list);
#endif

	ClipboardDestroy(clipboard->system);
	xf_clipboard_formats_free(clipboard);
	free(clipboard->data);
	free(clipboard->data_raw);
	free(clipboard->respond);
	free(clipboard->incr_data);
	free(clipboard);
}

void xf_cliprdr_init(xfContext* xfc, CliprdrClientContext* cliprdr)
{
	WINPR_ASSERT(xfc);
	WINPR_ASSERT(cliprdr);

	xfc->cliprdr = cliprdr;
	xfc->clipboard->context = cliprdr;
	cliprdr->custom = (void*)xfc->clipboard;
	cliprdr->MonitorReady = xf_cliprdr_monitor_ready;
	cliprdr->ServerCapabilities = xf_cliprdr_server_capabilities;
	cliprdr->ServerFormatList = xf_cliprdr_server_format_list;
	cliprdr->ServerFormatListResponse = xf_cliprdr_server_format_list_response;
	cliprdr->ServerFormatDataRequest = xf_cliprdr_server_format_data_request;
	cliprdr->ServerFormatDataResponse = xf_cliprdr_server_format_data_response;
	cliprdr->ServerFileContentsRequest = xf_cliprdr_server_file_contents_request;
#ifdef WITH_FUSE
	cliprdr->ServerFileContentsResponse = xf_cliprdr_server_file_contents_response;
#endif
}

void xf_cliprdr_uninit(xfContext* xfc, CliprdrClientContext* cliprdr)
{
	WINPR_ASSERT(xfc);
	WINPR_ASSERT(cliprdr);

	xfc->cliprdr = NULL;
	cliprdr->custom = NULL;

	if (xfc->clipboard)
		xfc->clipboard->context = NULL;
}
