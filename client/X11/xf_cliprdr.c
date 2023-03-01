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

#include "xf_cliprdr.h"
#include "xf_event.h"
#include "xf_utils.h"

#define TAG CLIENT_TAG("x11.cliprdr")

#define MAX_CLIPBOARD_FORMATS 255
#define WIN32_FILETIME_TO_UNIX_EPOCH_USEC UINT64_C(116444736000000000)

#ifdef WITH_DEBUG_CLIPRDR
#define DEBUG_CLIPRDR(...) WLog_DBG(TAG, __VA_ARGS__)
#else
#define DEBUG_CLIPRDR(...) \
	do                     \
	{                      \
	} while (0)
#endif

typedef struct
{
	Atom atom;
	UINT32 formatToRequest;
	UINT32 localFormat;
	char* formatName;
} xfCliprdrFormat;

struct xf_clipboard
{
	xfContext* xfc;
	rdpChannels* channels;
	CliprdrClientContext* context;

	wClipboard* system;

	Window root_window;
	Atom clipboard_atom;
	Atom property_atom;

	Atom timestamp_property_atom;
	Time selection_ownership_timestamp;

	Atom raw_transfer_atom;
	Atom raw_format_list_atom;

	UINT32 numClientFormats;
	xfCliprdrFormat clientFormats[20];

	UINT32 numServerFormats;
	CLIPRDR_FORMAT* serverFormats;

	size_t numTargets;
	Atom targets[20];

	int requestedFormatId;

	BYTE* data;
	BYTE* data_raw;
	BOOL data_raw_format;

	const xfCliprdrFormat* requestedFormat;

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

	/* last sent data */
	CLIPRDR_FORMAT* lastSentFormats;
	UINT32 lastSentNumFormats;
	CliprdrFileContext* file;
};

static const char* mime_text_plain = "text/plain";
static const char* mime_uri_list = "text/uri-list";
static const char* mime_html = "text/html";
static const char* mime_bmp = "image/bmp";
static const char* mime_png = "image/png";
static const char* mime_jpeg = "image/jpeg";
static const char* mime_gif = "image/gif";

static const char* mime_nautilus_clipboard = "x-special/nautilus-clipboard";
static const char* mime_gnome_copied_files = "x-special/gnome-copied-files";
static const char* mime_mate_copied_files = "x-special/mate-copied-files";

static const char* type_FileGroupDescriptorW = "FileGroupDescriptorW";
static const char* type_HtmlFormat = "HTML Format";

static void xf_cliprdr_clear_cached_data(xfClipboard* clipboard);
static UINT xf_cliprdr_send_client_format_list(xfClipboard* clipboard, BOOL force);
static void xf_cliprdr_set_selection_owner(xfContext* xfc, xfClipboard* clipboard, Time timestamp);

static void xf_clipboard_free_server_formats(xfClipboard* clipboard)
{
	WINPR_ASSERT(clipboard);
	if (clipboard->serverFormats)
	{
		for (size_t i = 0; i < clipboard->numServerFormats; i++)
		{
			CLIPRDR_FORMAT* format = &clipboard->serverFormats[i];
			free(format->formatName);
		}

		free(clipboard->serverFormats);
		clipboard->serverFormats = NULL;
	}
}

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
	LogTagAndXChangeProperty(TAG, xfc->display, xfc->drawable, clipboard->raw_transfer_atom,
	                         XA_INTEGER, 32, PropModeReplace, (BYTE*)&data, 1);
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
		result = LogTagAndXGetWindowProperty(TAG, xfc->display, owner, clipboard->raw_transfer_atom,
		                                     0, 4, 0, XA_INTEGER, &type, &format, &length,
		                                     &bytes_left, (BYTE**)&data);
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
		return (server->formatId == client->formatToRequest);
	}

	return FALSE;
}

static const xfCliprdrFormat* xf_cliprdr_get_client_format_by_id(xfClipboard* clipboard,
                                                                 UINT32 formatId)
{
	WINPR_ASSERT(clipboard);

	for (size_t index = 0; index < clipboard->numClientFormats; index++)
	{
		const xfCliprdrFormat* format = &(clipboard->clientFormats[index]);

		if (format->formatToRequest == formatId)
			return format;
	}

	return NULL;
}

static const xfCliprdrFormat* xf_cliprdr_get_client_format_by_atom(xfClipboard* clipboard,
                                                                   Atom atom)
{
	WINPR_ASSERT(clipboard);

	for (UINT32 i = 0; i < clipboard->numClientFormats; i++)
	{
		const xfCliprdrFormat* format = &(clipboard->clientFormats[i]);

		if (format->atom == atom)
			return format;
	}

	return NULL;
}

static const CLIPRDR_FORMAT* xf_cliprdr_get_server_format_by_atom(xfClipboard* clipboard, Atom atom)
{
	WINPR_ASSERT(clipboard);

	for (size_t i = 0; i < clipboard->numClientFormats; i++)
	{
		const xfCliprdrFormat* client_format = &(clipboard->clientFormats[i]);

		if (client_format->atom == atom)
		{
			for (size_t j = 0; j < clipboard->numServerFormats; j++)
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
static UINT xf_cliprdr_send_data_request(xfClipboard* clipboard, UINT32 formatId,
                                         const xfCliprdrFormat* cformat)
{
	CLIPRDR_FORMAT_DATA_REQUEST request = { 0 };
	request.requestedFormatId = formatId;

	DEBUG_CLIPRDR("requesting format 0x%08" PRIx32 " [%s] {local 0x%08" PRIx32 "} [%s]", formatId,
	              ClipboardGetFormatIdString(formatId), cformat->localFormat, cformat->formatName);

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
static UINT xf_cliprdr_send_data_response(xfClipboard* clipboard, const xfCliprdrFormat* format,
                                          const BYTE* data, size_t size)
{
	CLIPRDR_FORMAT_DATA_RESPONSE response = { 0 };

	WINPR_ASSERT(clipboard);

	/* No request currently pending, do not send a response. */
	if (clipboard->requestedFormatId < 0)
		return CHANNEL_RC_OK;

	if (size == 0)
	{
		if (format)
			DEBUG_CLIPRDR("send CB_RESPONSE_FAIL response {format 0x%08" PRIx32
			              " [%s] {local 0x%08" PRIx32 "} [%s]",
			              format->formatToRequest,
			              ClipboardGetFormatIdString(format->formatToRequest), format->localFormat,
			              format->formatName);
		else
			DEBUG_CLIPRDR("send CB_RESPONSE_FAIL response");
	}
	else
	{
		WINPR_ASSERT(format);
		DEBUG_CLIPRDR("send response format 0x%08" PRIx32 " [%s] {local 0x%08" PRIx32 "} [%s]",
		              format->formatToRequest, ClipboardGetFormatIdString(format->formatToRequest),
		              format->localFormat, format->formatName);
	}
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

		DEBUG_CLIPRDR("server announced 0x%08" PRIx32 " [%s][%s]", format->formatId,
		              ClipboardGetFormatIdString(format->formatId), format->formatName);
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
	LogTagAndXGetWindowProperty(
	    TAG, xfc->display, clipboard->owner, clipboard->raw_format_list_atom, 0, 4096, False,
	    clipboard->raw_format_list_atom, &type, &format, &length, &remaining, &data);

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
	LogTagAndXGetWindowProperty(TAG, xfc->display, xfc->drawable, clipboard->property_atom, 0, 200,
	                            0, XA_ATOM, &atom, &format_property, &length, &bytes_left, &data);

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
			formats[*numFormats].formatId = format->formatToRequest;
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
		LogTagAndXChangeProperty(TAG, xfc->display, xfc->drawable, clipboard->raw_format_list_atom,
		                         clipboard->raw_format_list_atom, 8, PropModeReplace,
		                         Stream_Buffer(formats), Stream_Length(formats));
	}
	else
	{
		LogTagAndXDeleteProperty(TAG, xfc->display, xfc->drawable, clipboard->raw_format_list_atom);
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

	for (UINT32 x = 0; x < numFormats; x++)
	{
		const CLIPRDR_FORMAT* format = &formats[x];
		DEBUG_CLIPRDR("announcing format 0x%08" PRIx32 " [%s] [%s]", format->formatId,
		              ClipboardGetFormatIdString(format->formatId), format->formatName);
	}

	xf_clipboard_copy_formats(clipboard, formats, numFormats);
	/* Ensure all pending requests are answered. */
	xf_cliprdr_send_data_response(clipboard, NULL, NULL, 0);

	xf_cliprdr_clear_cached_data(clipboard);

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
	BYTE* pDstData = NULL;
	const xfCliprdrFormat* format;

	WINPR_ASSERT(clipboard);

	if (clipboard->incr_starts && hasData)
		return;

	format = xf_cliprdr_get_client_format_by_id(clipboard, clipboard->requestedFormatId);

	if (!hasData || !data || !format)
	{
		xf_cliprdr_send_data_response(clipboard, format, NULL, 0);
		return;
	}

	srcFormatId = 0;

	switch (format->formatToRequest)
	{
		case CF_RAW:
			srcFormatId = CF_RAW;
			break;

		case CF_TEXT:
		case CF_OEMTEXT:
		case CF_UNICODETEXT:
			size = strlen((char*)data) + 1;
			srcFormatId = format->localFormat;
			break;

		default:
			srcFormatId = format->localFormat;
			break;
	}

	if (srcFormatId == 0)
	{
		xf_cliprdr_send_data_response(clipboard, format, NULL, 0);
		return;
	}

	ClipboardLock(clipboard->system);
	SrcSize = (UINT32)size;
	bSuccess = ClipboardSetData(clipboard->system, srcFormatId, data, SrcSize);

	if (bSuccess)
	{
		DstSize = 0;
		pDstData = (BYTE*)ClipboardGetData(clipboard->system, format->formatToRequest, &DstSize);
	}
	ClipboardUnlock(clipboard->system);

	if (!pDstData)
	{
		xf_cliprdr_send_data_response(clipboard, format, NULL, 0);
		return;
	}

	/*
	 * File lists require a bit of postprocessing to convert them from WinPR's FILDESCRIPTOR
	 * format to CLIPRDR_FILELIST expected by the server.
	 *
	 * We check for "FileGroupDescriptorW" format being registered (i.e., nonzero) in order
	 * to not process CF_RAW as a file list in case WinPR does not support file transfers.
	 */
	if (format->formatToRequest &&
	    (format->formatToRequest ==
	     ClipboardGetFormatId(clipboard->system, type_FileGroupDescriptorW)))
	{
		UINT error = NO_ERROR;
		FILEDESCRIPTORW* file_array = (FILEDESCRIPTORW*)pDstData;
		UINT32 file_count = DstSize / sizeof(FILEDESCRIPTORW);
		pDstData = NULL;
		DstSize = 0;

		const UINT32 flags = cliprdr_file_context_remote_get_flags(clipboard->file);
		error = cliprdr_serialize_file_list_ex(flags, file_array, file_count, &pDstData, &DstSize);

		if (error)
			WLog_ERR(TAG, "failed to serialize CLIPRDR_FILELIST: 0x%08X", error);

		UINT32 formatId = ClipboardGetFormatId(clipboard->system, mime_uri_list);
		UINT32 url_size = 0;
		ClipboardLock(clipboard->system);
		char* url = ClipboardGetData(clipboard->system, formatId, &url_size);
		ClipboardUnlock(clipboard->system);
		cliprdr_file_context_update_client_data(clipboard->file, url, url_size);
		free(url);

		free(file_array);
	}

	xf_cliprdr_send_data_response(clipboard, format, pDstData, DstSize);
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
		xf_cliprdr_send_data_response(clipboard, format, NULL, 0);
		return FALSE;
	}

	LogTagAndXGetWindowProperty(TAG, xfc->display, xfc->drawable, clipboard->property_atom, 0, 0, 0,
	                            target, &type, &format_property, &length, &bytes_left, &data);

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
		else if (LogTagAndXGetWindowProperty(
		             TAG, xfc->display, xfc->drawable, clipboard->property_atom, 0, bytes_left, 0,
		             target, &type, &format_property, &length, &dummy, &data) == Success)
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

	LogTagAndXDeleteProperty(TAG, xfc->display, xfc->drawable, clipboard->property_atom);
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
		LogTagAndXChangeProperty(TAG, xfc->display, respond->requestor, respond->property, XA_ATOM,
		                         32, PropModeReplace, (BYTE*)clipboard->targets,
		                         clipboard->numTargets);
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
		LogTagAndXChangeProperty(TAG, xfc->display, respond->requestor, respond->property,
		                         XA_INTEGER, 32, PropModeReplace,
		                         (BYTE*)&clipboard->selection_ownership_timestamp, 1);
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
		LogTagAndXChangeProperty(TAG, xfc->display, respond->requestor, respond->property,
		                         respond->target, 8, PropModeReplace, data, size);
	}
}

static void log_selection_event(xfContext* xfc, const XEvent* event)
{
	const DWORD level = WLOG_TRACE;
	static wLog* _log_cached_ptr = NULL;
	if (!_log_cached_ptr)
		_log_cached_ptr = WLog_Get(TAG);
	if (WLog_IsLevelActive(_log_cached_ptr, level))
	{

		switch (event->type)
		{
			case SelectionClear:
			{
				const XSelectionClearEvent* xevent = &event->xselectionclear;
				char* selection = Safe_XGetAtomName(xfc->display, xevent->selection);
				WLog_Print(_log_cached_ptr, level, "got event %s [selection %s]",
				           x11_event_string(event->type), selection);
				XFree(selection);
			}
			break;
			case SelectionNotify:
			{
				const XSelectionEvent* xevent = &event->xselection;
				char* selection = Safe_XGetAtomName(xfc->display, xevent->selection);
				char* target = Safe_XGetAtomName(xfc->display, xevent->target);
				char* property = Safe_XGetAtomName(xfc->display, xevent->property);
				WLog_Print(_log_cached_ptr, level,
				           "got event %s [selection %s, target %s, property %s]",
				           x11_event_string(event->type), selection, target, property);
				XFree(selection);
				XFree(target);
				XFree(property);
			}
			break;
			case SelectionRequest:
			{
				const XSelectionRequestEvent* xevent = &event->xselectionrequest;
				char* selection = Safe_XGetAtomName(xfc->display, xevent->selection);
				char* target = Safe_XGetAtomName(xfc->display, xevent->target);
				char* property = Safe_XGetAtomName(xfc->display, xevent->property);
				WLog_Print(_log_cached_ptr, level,
				           "got event %s [selection %s, target %s, property %s]",
				           x11_event_string(event->type), selection, target, property);
				XFree(selection);
				XFree(target);
				XFree(property);
			}
			break;
			case PropertyNotify:
			{
				const XPropertyEvent* xevent = &event->xproperty;
				char* atom = Safe_XGetAtomName(xfc->display, xevent->atom);
				WLog_Print(_log_cached_ptr, level, "got event %s [atom %s]",
				           x11_event_string(event->type), atom);
				XFree(atom);
			}
			break;
			default:
				break;
		}
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

void xf_cliprdr_clear_cached_data(xfClipboard* clipboard)
{
	WINPR_ASSERT(clipboard);

	ClipboardLock(clipboard->system);
	ClipboardEmpty(clipboard->system);
	cliprdr_file_context_clear(clipboard->file);

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
	cliprdr_file_context_clear(clipboard->file);
	ClipboardUnlock(clipboard->system);
}

static BOOL xf_cliprdr_process_selection_request(xfClipboard* clipboard,
                                                 const XSelectionRequestEvent* xevent)
{
	int fmt;
	Atom type;
	UINT32 formatId;
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
		const xfCliprdrFormat* cformat =
		    xf_cliprdr_get_client_format_by_atom(clipboard, xevent->target);

		if (format && (xevent->requestor != xfc->drawable))
		{
			formatId = format->formatId;
			rawTransfer = FALSE;

			if (formatId == CF_RAW)
			{
				if (LogTagAndXGetWindowProperty(
				        TAG, xfc->display, xevent->requestor, clipboard->property_atom, 0, 4, 0,
				        XA_INTEGER, &type, &fmt, &length, &bytes_left, &data) != Success)
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
			matchingFormat = format && (formatId == format->formatId);

			if (matchingFormat && (clipboard->data != 0) && !rawTransfer)
			{
				UINT32 DstSize = 0;

				WINPR_ASSERT(cformat);

				/* Cached converted clipboard data available. Send it now */
				respond->property = xevent->property;
				ClipboardLock(clipboard->system);
				void* cdata = ClipboardGetData(clipboard->system, cformat->localFormat, &DstSize);
				ClipboardUnlock(clipboard->system);

				xf_cliprdr_provide_data(clipboard, respond, cdata, DstSize);
				free(cdata);
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
				WINPR_ASSERT(cformat);

				/**
				 * Send clipboard data request to the server.
				 * Response will be postponed after receiving the data
				 */
				respond->property = xevent->property;
				clipboard->respond = respond;
				clipboard->requestedFormat = cformat;
				clipboard->data_raw_format = rawTransfer;
				delayRespond = TRUE;
				xf_cliprdr_send_data_request(clipboard, formatId, cformat);
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

	LogTagAndXDeleteProperty(TAG, xfc->display, clipboard->root_window, clipboard->property_atom);
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
			log_selection_event(xfc, event);
			xf_cliprdr_process_selection_notify(clipboard, &event->xselection);
			break;

		case SelectionRequest:
			log_selection_event(xfc, event);
			xf_cliprdr_process_selection_request(clipboard, &event->xselectionrequest);
			break;

		case SelectionClear:
			log_selection_event(xfc, event);
			xf_cliprdr_process_selection_clear(clipboard, &event->xselectionclear);
			break;

		case PropertyNotify:
			log_selection_event(xfc, event);
			xf_cliprdr_process_property_notify(clipboard, &event->xproperty);
			break;

		case FocusIn:
			if (!clipboard->xfixes_supported)
			{
				xf_cliprdr_check_owner(clipboard);
			}

			break;
		default:
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
	generalCapabilitySet.generalFlags |= cliprdr_file_context_current_flags(clipboard->file);

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
	UINT32 numFormats;
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

	for (size_t i = 0; i < numFormats; i++)
	{
		const xfCliprdrFormat* clientFormat = &clipboard->clientFormats[i];
		CLIPRDR_FORMAT* format = &formats[i];
		format->formatId = clientFormat->formatToRequest;
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

	clipboard = cliprdr_file_context_get_context(context->custom);
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

	clipboard = cliprdr_file_context_get_context(context->custom);
	WINPR_ASSERT(clipboard);

	capsPtr = (const BYTE*)capabilities->capabilitySets;
	WINPR_ASSERT(capsPtr);

	cliprdr_file_context_remote_set_flags(clipboard->file, 0);

	for (i = 0; i < capabilities->cCapabilitiesSets; i++)
	{
		const CLIPRDR_CAPABILITY_SET* caps = (const CLIPRDR_CAPABILITY_SET*)capsPtr;

		if (caps->capabilitySetType == CB_CAPSTYPE_GENERAL)
		{
			generalCaps = (const CLIPRDR_GENERAL_CAPABILITY_SET*)caps;

			cliprdr_file_context_remote_set_flags(clipboard->file, generalCaps->generalFlags);
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

	LogTagAndXChangeProperty(TAG, xfc->display, xfc->drawable, clipboard->timestamp_property_atom,
	                         XA_ATOM, 32, PropModeReplace, (BYTE*)&value, 1);
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
	xfContext* xfc;
	UINT ret;
	xfClipboard* clipboard;

	WINPR_ASSERT(context);
	WINPR_ASSERT(formatList);

	clipboard = cliprdr_file_context_get_context(context->custom);
	WINPR_ASSERT(clipboard);

	xfc = clipboard->xfc;
	WINPR_ASSERT(xfc);

	/* Clear the active SelectionRequest, as it is now invalid */
	free(clipboard->respond);
	clipboard->respond = NULL;

	xf_clipboard_formats_free(clipboard);
	xf_cliprdr_clear_cached_data(clipboard);
	clipboard->requestedFormat = NULL;

	xf_clipboard_free_server_formats(clipboard);

	clipboard->numServerFormats = formatList->numFormats + 1; /* +1 for CF_RAW */

	if (!(clipboard->serverFormats =
	          (CLIPRDR_FORMAT*)calloc(clipboard->numServerFormats, sizeof(CLIPRDR_FORMAT))))
	{
		WLog_ERR(TAG, "failed to allocate %d CLIPRDR_FORMAT structs", clipboard->numServerFormats);
		return CHANNEL_RC_NO_MEMORY;
	}

	for (size_t i = 0; i < formatList->numFormats; i++)
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

	for (size_t i = 0; i < formatList->numFormats; i++)
	{
		const CLIPRDR_FORMAT* format = &formatList->formats[i];

		for (size_t j = 0; j < clipboard->numClientFormats; j++)
		{
			const xfCliprdrFormat* clientFormat = &clipboard->clientFormats[j];
			if (xf_cliprdr_formats_equal(format, clientFormat))
			{
				if ((clientFormat->formatName != NULL) &&
				    (strcmp(type_FileGroupDescriptorW, clientFormat->formatName) == 0))
				{
					if (!cliprdr_file_context_has_local_support(clipboard->file))
						continue;
				}
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

	clipboard = cliprdr_file_context_get_context(context->custom);
	WINPR_ASSERT(clipboard);

	xfc = clipboard->xfc;
	WINPR_ASSERT(xfc);

	formatId = formatDataRequest->requestedFormatId;

	rawTransfer = xf_cliprdr_is_raw_transfer_available(clipboard);

	if (rawTransfer)
	{
		format = xf_cliprdr_get_client_format_by_id(clipboard, CF_RAW);
		LogTagAndXChangeProperty(TAG, xfc->display, xfc->drawable, clipboard->property_atom,
		                         XA_INTEGER, 32, PropModeReplace, (BYTE*)&formatId, 1);
	}
	else
		format = xf_cliprdr_get_client_format_by_id(clipboard, formatId);

	clipboard->requestedFormatId = rawTransfer ? CF_RAW : formatId;
	if (!format)
		return xf_cliprdr_send_data_response(clipboard, format, NULL, 0);

	DEBUG_CLIPRDR("requested format 0x%08" PRIx32 " [%s] {local 0x%08" PRIx32 "} [%s]",
	              format->formatToRequest, ClipboardGetFormatIdString(format->formatToRequest),
	              format->localFormat, format->formatName);
	XConvertSelection(xfc->display, clipboard->clipboard_atom, format->atom,
	                  clipboard->property_atom, xfc->drawable, CurrentTime);
	XFlush(xfc->display);
	/* After this point, we expect a SelectionNotify event from the clipboard owner. */
	return CHANNEL_RC_OK;
}

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
	BOOL nullTerminated = FALSE;
	UINT32 size;
	const BYTE* data;
	xfContext* xfc;
	xfClipboard* clipboard;

	WINPR_ASSERT(context);
	WINPR_ASSERT(formatDataResponse);

	clipboard = cliprdr_file_context_get_context(context->custom);
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

	pDstData = NULL;
	DstSize = 0;
	srcFormatId = 0;
	dstFormatId = 0;

	const xfCliprdrFormat* format = clipboard->requestedFormat;
	if (clipboard->data_raw_format)
	{
		srcFormatId = CF_RAW;
		dstFormatId = CF_RAW;
	}
	else if (!clipboard->requestedFormat)
		return ERROR_INTERNAL_ERROR;
	else if (clipboard->requestedFormat->formatName)
	{
		if (strcmp(clipboard->requestedFormat->formatName, type_HtmlFormat) == 0)
		{
			srcFormatId = ClipboardGetFormatId(clipboard->system, type_HtmlFormat);
			dstFormatId = ClipboardGetFormatId(clipboard->system, mime_html);
			nullTerminated = TRUE;
		}

		if (strcmp(clipboard->requestedFormat->formatName, type_FileGroupDescriptorW) == 0)
		{
			if (!cliprdr_file_context_update_server_data(clipboard->file, clipboard->system, data,
			                                             size))
				WLog_WARN(TAG, "failed to update file descriptors");

			srcFormatId = ClipboardGetFormatId(clipboard->system, type_FileGroupDescriptorW);
			const xfCliprdrFormat* dstTargetFormat =
			    xf_cliprdr_get_client_format_by_atom(clipboard, clipboard->respond->target);
			if (!dstTargetFormat)
			{
				dstFormatId = ClipboardGetFormatId(clipboard->system, mime_uri_list);
			}
			else
			{
				dstFormatId = dstTargetFormat->localFormat;
			}

			nullTerminated = TRUE;
		}
	}
	else
	{
		srcFormatId = clipboard->requestedFormat->formatToRequest;
		dstFormatId = clipboard->requestedFormat->localFormat;
		switch (clipboard->requestedFormat->formatToRequest)
		{
			case CF_TEXT:
				nullTerminated = TRUE;
				break;

			case CF_OEMTEXT:
				nullTerminated = TRUE;
				break;

			case CF_UNICODETEXT:
				nullTerminated = TRUE;
				break;

			case CF_DIB:
				srcFormatId = CF_DIB;
				break;

			default:
				break;
		}
	}

	DEBUG_CLIPRDR("requested format 0x%08" PRIx32 " [%s] {local 0x%08" PRIx32 "} [%s]",
	              format->formatToRequest, ClipboardGetFormatIdString(format->formatToRequest),
	              format->localFormat, format->formatName);
	SrcSize = (UINT32)size;

	ClipboardLock(clipboard->system);
	bSuccess = ClipboardSetData(clipboard->system, srcFormatId, data, SrcSize);

	BOOL willQuit = FALSE;
	if (bSuccess)
	{
		if (SrcSize == 0)
		{
			WLog_DBG(TAG, "skipping, empty data detected!");
			free(clipboard->respond);
			clipboard->respond = NULL;
			willQuit = TRUE;
		}
		else
		{
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
	}
	ClipboardUnlock(clipboard->system);
	if (willQuit)
		return CHANNEL_RC_OK;

	/* Cache converted and original data to avoid doing a possibly costly
	 * conversion again on subsequent requests */
	free(clipboard->data);
	free(clipboard->data_raw);

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

xfClipboard* xf_clipboard_new(xfContext* xfc, BOOL relieveFilenameRestriction)
{
	int n = 0;
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

	clipboard->file = cliprdr_file_context_new(clipboard);
	if (!clipboard->file)
		goto fail;

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
		goto fail;
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
	clientFormat->localFormat = clientFormat->formatToRequest = CF_RAW;

	clientFormat = &clipboard->clientFormats[n++];
	clientFormat->atom = XInternAtom(xfc->display, "UTF8_STRING", False);
	clientFormat->formatToRequest = CF_UNICODETEXT;
	clientFormat->localFormat = ClipboardGetFormatId(xfc->clipboard->system, mime_text_plain);

	clientFormat = &clipboard->clientFormats[n++];
	clientFormat->atom = XA_STRING;
	clientFormat->formatToRequest = CF_TEXT;
	clientFormat->localFormat = ClipboardGetFormatId(xfc->clipboard->system, mime_text_plain);

	clientFormat = &clipboard->clientFormats[n++];
	clientFormat->atom = XInternAtom(xfc->display, mime_png, False);
	clientFormat->formatToRequest = clientFormat->localFormat =
	    ClipboardGetFormatId(xfc->clipboard->system, mime_png);

	clientFormat = &clipboard->clientFormats[n++];
	clientFormat->atom = XInternAtom(xfc->display, mime_jpeg, False);
	clientFormat->formatToRequest = clientFormat->localFormat =
	    ClipboardGetFormatId(xfc->clipboard->system, mime_jpeg);

	clientFormat = &clipboard->clientFormats[n++];
	clientFormat->atom = XInternAtom(xfc->display, mime_gif, False);
	clientFormat->formatToRequest = clientFormat->localFormat =
	    ClipboardGetFormatId(xfc->clipboard->system, mime_gif);

	clientFormat = &clipboard->clientFormats[n++];
	clientFormat->atom = XInternAtom(xfc->display, mime_bmp, False);
	clientFormat->formatToRequest = CF_DIB;
	clientFormat->localFormat = ClipboardGetFormatId(xfc->clipboard->system, mime_bmp);

	clientFormat = &clipboard->clientFormats[n++];
	clientFormat->atom = XInternAtom(xfc->display, mime_html, False);
	clientFormat->formatToRequest = ClipboardGetFormatId(xfc->clipboard->system, type_HtmlFormat);
	clientFormat->localFormat = ClipboardGetFormatId(xfc->clipboard->system, mime_html);
	clientFormat->formatName = _strdup(type_HtmlFormat);

	if (!clientFormat->formatName)
		goto fail;

	clientFormat = &clipboard->clientFormats[n++];

	/*
	 * Existence of registered format IDs for file formats does not guarantee that they are
	 * in fact supported by wClipboard (as further initialization may have failed after format
	 * registration). However, they are definitely not supported if there are no registered
	 * formats. In this case we should not list file formats in TARGETS.
	 */
	const UINT32 fgid = ClipboardGetFormatId(clipboard->system, type_FileGroupDescriptorW);
	const UINT32 uid = ClipboardGetFormatId(clipboard->system, mime_uri_list);
	if (uid)
	{
		cliprdr_file_context_set_locally_available(clipboard->file, TRUE);
		clientFormat->atom = XInternAtom(xfc->display, mime_uri_list, False);
		clientFormat->localFormat = uid;
		clientFormat->formatToRequest = fgid;
		clientFormat->formatName = _strdup(type_FileGroupDescriptorW);

		if (!clientFormat->formatName)
			goto fail;

		clientFormat = &clipboard->clientFormats[n++];
	}

	const UINT32 gid = ClipboardGetFormatId(clipboard->system, mime_gnome_copied_files);
	if (gid != 0)
	{
		cliprdr_file_context_set_locally_available(clipboard->file, TRUE);
		clientFormat->atom = XInternAtom(xfc->display, mime_gnome_copied_files, False);
		clientFormat->localFormat = gid;
		clientFormat->formatToRequest = fgid;
		clientFormat->formatName = _strdup(type_FileGroupDescriptorW);

		if (!clientFormat->formatName)
			goto fail;

		clientFormat = &clipboard->clientFormats[n++];
	}

	const UINT32 mid = ClipboardGetFormatId(clipboard->system, mime_mate_copied_files);
	if (mid != 0)
	{
		cliprdr_file_context_set_locally_available(clipboard->file, TRUE);
		clientFormat->atom = XInternAtom(xfc->display, mime_mate_copied_files, False);
		clientFormat->localFormat = mid;
		clientFormat->formatToRequest = fgid;
		clientFormat->formatName = _strdup(type_FileGroupDescriptorW);

		if (!clientFormat->formatName)
			goto fail;
	}

	clipboard->numClientFormats = n;
	clipboard->targets[0] = XInternAtom(xfc->display, "TIMESTAMP", FALSE);
	clipboard->targets[1] = XInternAtom(xfc->display, "TARGETS", FALSE);
	clipboard->numTargets = 2;
	clipboard->incr_atom = XInternAtom(xfc->display, "INCR", FALSE);

	return clipboard;

fail:
	xf_clipboard_free(clipboard);
	return NULL;
}

void xf_clipboard_free(xfClipboard* clipboard)
{
	if (!clipboard)
		return;

	xf_clipboard_free_server_formats(clipboard);

	if (clipboard->numClientFormats)
	{
		for (UINT32 i = 0; i < clipboard->numClientFormats; i++)
		{
			xfCliprdrFormat* format = &clipboard->clientFormats[i];
			free(format->formatName);
		}
	}

	cliprdr_file_context_free(clipboard->file);

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

	cliprdr->MonitorReady = xf_cliprdr_monitor_ready;
	cliprdr->ServerCapabilities = xf_cliprdr_server_capabilities;
	cliprdr->ServerFormatList = xf_cliprdr_server_format_list;
	cliprdr->ServerFormatListResponse = xf_cliprdr_server_format_list_response;
	cliprdr->ServerFormatDataRequest = xf_cliprdr_server_format_data_request;
	cliprdr->ServerFormatDataResponse = xf_cliprdr_server_format_data_response;

	cliprdr_file_context_init(xfc->clipboard->file, cliprdr);
}

void xf_cliprdr_uninit(xfContext* xfc, CliprdrClientContext* cliprdr)
{
	WINPR_ASSERT(xfc);
	WINPR_ASSERT(cliprdr);

	xfc->cliprdr = NULL;

	if (xfc->clipboard)
	{
		cliprdr_file_context_uninit(xfc->clipboard->file, cliprdr);
		xfc->clipboard->context = NULL;
	}
}
