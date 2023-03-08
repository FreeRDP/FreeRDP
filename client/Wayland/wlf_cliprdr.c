/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Wayland Clipboard Redirection
 *
 * Copyright 2018 Armin Novak <armin.novak@thincast.com>
 * Copyright 2018 Thincast Technologies GmbH
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

#include <winpr/crt.h>
#include <winpr/image.h>
#include <winpr/stream.h>
#include <winpr/clipboard.h>

#include <freerdp/log.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/channels/channels.h>
#include <freerdp/channels/cliprdr.h>

#include <freerdp/client/client_cliprdr_file.h>

#include "wlf_cliprdr.h"

#define TAG CLIENT_TAG("wayland.cliprdr")

#define MAX_CLIPBOARD_FORMATS 255

#define mime_text_plain "text/plain"
#define mime_text_utf8 mime_text_plain ";charset=utf-8"

static const char* mime_text[] = { mime_text_plain, mime_text_utf8, "UTF8_STRING",
	                               "COMPOUND_TEXT", "TEXT",         "STRING" };

static const char* mime_image[] = {
	"image/png",       "image/bmp",   "image/x-bmp",        "image/x-MS-bmp",
	"image/x-icon",    "image/x-ico", "image/x-win-bitmap", "image/vmd.microsoft.icon",
	"application/ico", "image/ico",   "image/icon",         "image/jpeg",
	"image/gif",       "image/tiff"
};

static const char mime_uri_list[] = "text/uri-list";
static const char mime_html[] = "text/html";
static const char mime_bmp[] = "image/bmp";

static const char mime_gnome_copied_files[] = "x-special/gnome-copied-files";
static const char mime_mate_copied_files[] = "x-special/mate-copied-files";

static const char type_FileGroupDescriptorW[] = "FileGroupDescriptorW";
static const char type_HtmlFormat[] = "HTML Format";

typedef struct
{
	FILE* responseFile;
	UINT32 responseFormat;
	char* responseMime;
} wlf_request;

struct wlf_clipboard
{
	wlfContext* wfc;
	rdpChannels* channels;
	CliprdrClientContext* context;
	wLog* log;

	UwacSeat* seat;
	wClipboard* system;

	size_t numClientFormats;
	CLIPRDR_FORMAT* clientFormats;

	size_t numServerFormats;
	CLIPRDR_FORMAT* serverFormats;

	BOOL sync;

	CRITICAL_SECTION lock;
	CliprdrFileContext* file;

	wQueue* request_queue;
};

static void wlf_request_free(void* rq)
{
	wlf_request* request = rq;
	if (request)
	{
		free(request->responseMime);
		if (request->responseFile)
			fclose(request->responseFile);
	}
	free(request);
}

static wlf_request* wlf_request_new(void)
{
	return calloc(1, sizeof(wlf_request));
}

static void* wlf_request_clone(const void* oth)
{
	const wlf_request* other = (const wlf_request*)oth;
	wlf_request* copy = wlf_request_new();
	if (!copy)
		return NULL;
	*copy = *other;
	if (other->responseMime)
	{
		copy->responseMime = _strdup(other->responseMime);
		if (!copy->responseMime)
			goto fail;
	}
	return copy;
fail:
	wlf_request_free(copy);
	return NULL;
}

static BOOL wlf_mime_is_file(const char* mime)
{
	if (strncmp(mime_uri_list, mime, sizeof(mime_uri_list)) == 0)
		return TRUE;
	if (strncmp(mime_gnome_copied_files, mime, sizeof(mime_gnome_copied_files)) == 0)
		return TRUE;
	if (strncmp(mime_mate_copied_files, mime, sizeof(mime_mate_copied_files)) == 0)
		return TRUE;
	return FALSE;
}

static BOOL wlf_mime_is_text(const char* mime)
{
	size_t x;

	for (x = 0; x < ARRAYSIZE(mime_text); x++)
	{
		if (strcmp(mime, mime_text[x]) == 0)
			return TRUE;
	}

	return FALSE;
}

static BOOL wlf_mime_is_image(const char* mime)
{
	size_t x;

	for (x = 0; x < ARRAYSIZE(mime_image); x++)
	{
		if (strcmp(mime, mime_image[x]) == 0)
			return TRUE;
	}

	return FALSE;
}

static BOOL wlf_mime_is_html(const char* mime)
{
	if (strcmp(mime, mime_html) == 0)
		return TRUE;

	return FALSE;
}

static void wlf_cliprdr_free_server_formats(wfClipboard* clipboard)
{
	if (clipboard && clipboard->serverFormats)
	{
		for (size_t j = 0; j < clipboard->numServerFormats; j++)
		{
			CLIPRDR_FORMAT* format = &clipboard->serverFormats[j];
			free(format->formatName);
		}

		free(clipboard->serverFormats);
		clipboard->serverFormats = NULL;
		clipboard->numServerFormats = 0;
	}

	if (clipboard)
		UwacClipboardOfferDestroy(clipboard->seat);
}

static void wlf_cliprdr_free_client_formats(wfClipboard* clipboard)
{
	if (clipboard && clipboard->numClientFormats)
	{
		for (size_t j = 0; j < clipboard->numClientFormats; j++)
		{
			CLIPRDR_FORMAT* format = &clipboard->clientFormats[j];
			free(format->formatName);
		}

		free(clipboard->clientFormats);
		clipboard->clientFormats = NULL;
		clipboard->numClientFormats = 0;
	}

	if (clipboard)
		UwacClipboardOfferDestroy(clipboard->seat);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT wlf_cliprdr_send_client_format_list(wfClipboard* clipboard)
{
	WINPR_ASSERT(clipboard);

	const CLIPRDR_FORMAT_LIST formatList = { .common.msgFlags = CB_RESPONSE_OK,
		                                     .numFormats = (UINT32)clipboard->numClientFormats,
		                                     .formats = clipboard->clientFormats,
		                                     .common.msgType = CB_FORMAT_LIST };

	cliprdr_file_context_clear(clipboard->file);

	WLog_VRB(TAG, "-------------- client format list [%" PRIu32 "] ------------------",
	         formatList.numFormats);
	for (UINT32 x = 0; x < formatList.numFormats; x++)
	{
		const CLIPRDR_FORMAT* format = &formatList.formats[x];
		WLog_VRB(TAG, "client announces %" PRIu32 " [%s][%s]", format->formatId,
		         ClipboardGetFormatIdString(format->formatId), format->formatName);
	}
	WINPR_ASSERT(clipboard->context);
	WINPR_ASSERT(clipboard->context->ClientFormatList);
	return clipboard->context->ClientFormatList(clipboard->context, &formatList);
}

static void wfl_cliprdr_add_client_format_id(wfClipboard* clipboard, UINT32 formatId)
{
	CLIPRDR_FORMAT* format = NULL;
	const char* name = ClipboardGetFormatName(clipboard->system, formatId);

	for (size_t x = 0; x < clipboard->numClientFormats; x++)
	{
		format = &clipboard->clientFormats[x];

		if (format->formatId == formatId)
			return;
	}

	format = realloc(clipboard->clientFormats,
	                 (clipboard->numClientFormats + 1) * sizeof(CLIPRDR_FORMAT));

	if (!format)
		return;

	clipboard->clientFormats = format;
	format = &clipboard->clientFormats[clipboard->numClientFormats++];
	format->formatId = formatId;
	format->formatName = NULL;

	if (name && (formatId >= CF_MAX))
		format->formatName = _strdup(name);
}

static BOOL wlf_cliprdr_add_client_format(wfClipboard* clipboard, const char* mime)
{
	WINPR_ASSERT(mime);
	if (wlf_mime_is_html(mime))
	{
		UINT32 formatId = ClipboardGetFormatId(clipboard->system, type_HtmlFormat);
		wfl_cliprdr_add_client_format_id(clipboard, formatId);
	}
	else if (wlf_mime_is_text(mime))
	{
		wfl_cliprdr_add_client_format_id(clipboard, CF_TEXT);
		wfl_cliprdr_add_client_format_id(clipboard, CF_OEMTEXT);
		wfl_cliprdr_add_client_format_id(clipboard, CF_UNICODETEXT);
	}
	else if (wlf_mime_is_image(mime))
	{
		UINT32 formatId = ClipboardGetFormatId(clipboard->system, mime_bmp);
		wfl_cliprdr_add_client_format_id(clipboard, formatId);
		wfl_cliprdr_add_client_format_id(clipboard, CF_DIB);
	}
	else if (wlf_mime_is_file(mime))
	{
		const UINT32 fileFormatId =
		    ClipboardGetFormatId(clipboard->system, type_FileGroupDescriptorW);
		wfl_cliprdr_add_client_format_id(clipboard, fileFormatId);
	}

	if (wlf_cliprdr_send_client_format_list(clipboard) != CHANNEL_RC_OK)
		return FALSE;
	return TRUE;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT wlf_cliprdr_send_data_request(wfClipboard* clipboard, const wlf_request* rq)
{
	WINPR_ASSERT(rq);

	CLIPRDR_FORMAT_DATA_REQUEST request = { .requestedFormatId = rq->responseFormat };

	if (!Queue_Enqueue(clipboard->request_queue, rq))
		return ERROR_INTERNAL_ERROR;

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
static UINT wlf_cliprdr_send_data_response(wfClipboard* clipboard, const BYTE* data, size_t size)
{
	CLIPRDR_FORMAT_DATA_RESPONSE response = { 0 };

	if (size > UINT32_MAX)
		return ERROR_INVALID_PARAMETER;

	response.common.msgFlags = (data) ? CB_RESPONSE_OK : CB_RESPONSE_FAIL;
	response.common.dataLen = (UINT32)size;
	response.requestedFormatData = data;

	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(clipboard->context);
	WINPR_ASSERT(clipboard->context->ClientFormatDataResponse);
	return clipboard->context->ClientFormatDataResponse(clipboard->context, &response);
}

BOOL wlf_cliprdr_handle_event(wfClipboard* clipboard, const UwacClipboardEvent* event)
{
	if (!clipboard || !event)
		return FALSE;

	if (!clipboard->context)
		return TRUE;

	switch (event->type)
	{
		case UWAC_EVENT_CLIPBOARD_AVAILABLE:
			clipboard->seat = event->seat;
			return TRUE;

		case UWAC_EVENT_CLIPBOARD_OFFER:
			WLog_Print(clipboard->log, WLOG_DEBUG, "client announces mime %s", event->mime);
			return wlf_cliprdr_add_client_format(clipboard, event->mime);

		case UWAC_EVENT_CLIPBOARD_SELECT:
			WLog_Print(clipboard->log, WLOG_DEBUG, "client announces new data");
			wlf_cliprdr_free_client_formats(clipboard);
			return TRUE;

		default:
			return FALSE;
	}
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT wlf_cliprdr_send_client_capabilities(wfClipboard* clipboard)
{
	WINPR_ASSERT(clipboard);

	const CLIPRDR_GENERAL_CAPABILITY_SET generalCapabilitySet = {
		.capabilitySetType = CB_CAPSTYPE_GENERAL,
		.capabilitySetLength = 12,
		.version = CB_CAPS_VERSION_2,
		.generalFlags =
		    CB_USE_LONG_FORMAT_NAMES | cliprdr_file_context_current_flags(clipboard->file)
	};
	const CLIPRDR_CAPABILITIES capabilities = {
		.cCapabilitiesSets = 1, .capabilitySets = (CLIPRDR_CAPABILITY_SET*)&(generalCapabilitySet)
	};

	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(clipboard->context);
	WINPR_ASSERT(clipboard->context->ClientCapabilities);
	return clipboard->context->ClientCapabilities(clipboard->context, &capabilities);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT wlf_cliprdr_send_client_format_list_response(wfClipboard* clipboard, BOOL status)
{
	const CLIPRDR_FORMAT_LIST_RESPONSE formatListResponse = {
		.common.msgType = CB_FORMAT_LIST_RESPONSE,
		.common.msgFlags = status ? CB_RESPONSE_OK : CB_RESPONSE_FAIL,
		.common.dataLen = 0
	};
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
static UINT wlf_cliprdr_monitor_ready(CliprdrClientContext* context,
                                      const CLIPRDR_MONITOR_READY* monitorReady)
{
	UINT ret;

	WINPR_UNUSED(monitorReady);
	WINPR_ASSERT(context);
	WINPR_ASSERT(monitorReady);

	wfClipboard* clipboard = cliprdr_file_context_get_context(context->custom);
	WINPR_ASSERT(clipboard);

	if ((ret = wlf_cliprdr_send_client_capabilities(clipboard)) != CHANNEL_RC_OK)
		return ret;

	if ((ret = wlf_cliprdr_send_client_format_list(clipboard)) != CHANNEL_RC_OK)
		return ret;

	clipboard->sync = TRUE;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT wlf_cliprdr_server_capabilities(CliprdrClientContext* context,
                                            const CLIPRDR_CAPABILITIES* capabilities)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(capabilities);

	const BYTE* capsPtr = (const BYTE*)capabilities->capabilitySets;
	WINPR_ASSERT(capsPtr);

	wfClipboard* clipboard = cliprdr_file_context_get_context(context->custom);
	WINPR_ASSERT(clipboard);

	if (!cliprdr_file_context_remote_set_flags(clipboard->file, 0))
		return ERROR_INTERNAL_ERROR;

	for (UINT32 i = 0; i < capabilities->cCapabilitiesSets; i++)
	{
		const CLIPRDR_CAPABILITY_SET* caps = (const CLIPRDR_CAPABILITY_SET*)capsPtr;

		if (caps->capabilitySetType == CB_CAPSTYPE_GENERAL)
		{
			const CLIPRDR_GENERAL_CAPABILITY_SET* generalCaps =
			    (const CLIPRDR_GENERAL_CAPABILITY_SET*)caps;

			if (!cliprdr_file_context_remote_set_flags(clipboard->file, generalCaps->generalFlags))
				return ERROR_INTERNAL_ERROR;
		}

		capsPtr += caps->capabilitySetLength;
	}

	return CHANNEL_RC_OK;
}

static UINT32 wlf_get_server_format_id(const wfClipboard* clipboard, const char* name)
{
	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(name);

	for (UINT32 x = 0; x < clipboard->numServerFormats; x++)
	{
		const CLIPRDR_FORMAT* format = &clipboard->serverFormats[x];
		if (!format->formatName)
			continue;
		if (strcmp(name, format->formatName) == 0)
			return format->formatId;
	}
	return 0;
}

static const char* wlf_get_server_format_name(const wfClipboard* clipboard, UINT32 formatId)
{
	WINPR_ASSERT(clipboard);

	for (UINT32 x = 0; x < clipboard->numServerFormats; x++)
	{
		const CLIPRDR_FORMAT* format = &clipboard->serverFormats[x];
		if (format->formatId == formatId)
			return format->formatName;
	}
	return NULL;
}

static void wlf_cliprdr_transfer_data(UwacSeat* seat, void* context, const char* mime, int fd)
{
	wfClipboard* clipboard = (wfClipboard*)context;
	WINPR_UNUSED(seat);

	EnterCriticalSection(&clipboard->lock);

	wlf_request request = { 0 };
	if (wlf_mime_is_html(mime))
	{
		request.responseMime = mime_html;
		request.responseFormat = wlf_get_server_format_id(clipboard, type_HtmlFormat);
	}
	else if (wlf_mime_is_file(mime))
	{
		request.responseMime = mime;
		request.responseFormat = wlf_get_server_format_id(clipboard, type_FileGroupDescriptorW);
	}
	else if (wlf_mime_is_text(mime))
	{
		request.responseMime = mime_text_plain;
		request.responseFormat = CF_UNICODETEXT;
	}
	else if (wlf_mime_is_image(mime))
	{
		request.responseMime = mime;
		request.responseFormat = CF_DIB;
	}

	if (request.responseMime != NULL)
	{
		request.responseFile = fdopen(fd, "w");

		if (request.responseFile)
			wlf_cliprdr_send_data_request(clipboard, &request);
		else
			WLog_Print(clipboard->log, WLOG_ERROR,
			           "failed to open clipboard file descriptor for MIME %s",
			           request.responseMime);
	}

	LeaveCriticalSection(&clipboard->lock);
}

static void wlf_cliprdr_cancel_data(UwacSeat* seat, void* context)
{
	wfClipboard* clipboard = (wfClipboard*)context;

	WINPR_UNUSED(seat);
	WINPR_ASSERT(clipboard);
	cliprdr_file_context_clear(clipboard->file);
}

/**
 * Called when the clipboard changes server side.
 *
 * Clear the local clipboard offer and replace it with a new one
 * that announces the formats we get listed here.
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT wlf_cliprdr_server_format_list(CliprdrClientContext* context,
                                           const CLIPRDR_FORMAT_LIST* formatList)
{
	BOOL html = FALSE;
	BOOL text = FALSE;
	BOOL image = FALSE;
	BOOL file = FALSE;

	if (!context || !context->custom)
		return ERROR_INVALID_PARAMETER;

	wfClipboard* clipboard = cliprdr_file_context_get_context(context->custom);
	WINPR_ASSERT(clipboard);

	wlf_cliprdr_free_server_formats(clipboard);
	cliprdr_file_context_clear(clipboard->file);

	if (!(clipboard->serverFormats =
	          (CLIPRDR_FORMAT*)calloc(formatList->numFormats, sizeof(CLIPRDR_FORMAT))))
	{
		WLog_Print(clipboard->log, WLOG_ERROR,
		           "failed to allocate %" PRIuz " CLIPRDR_FORMAT structs",
		           clipboard->numServerFormats);
		return CHANNEL_RC_NO_MEMORY;
	}

	clipboard->numServerFormats = formatList->numFormats;

	if (!clipboard->seat)
	{
		WLog_Print(clipboard->log, WLOG_ERROR,
		           "clipboard->seat=NULL, check your client implementation");
		return ERROR_INTERNAL_ERROR;
	}

	for (UINT32 i = 0; i < formatList->numFormats; i++)
	{
		const CLIPRDR_FORMAT* format = &formatList->formats[i];
		CLIPRDR_FORMAT* srvFormat = &clipboard->serverFormats[i];
		srvFormat->formatId = format->formatId;

		if (format->formatName)
		{
			srvFormat->formatName = _strdup(format->formatName);

			if (!srvFormat->formatName)
			{
				wlf_cliprdr_free_server_formats(clipboard);
				return CHANNEL_RC_NO_MEMORY;
			}
		}

		if (format->formatName)
		{
			if (strcmp(format->formatName, type_HtmlFormat) == 0)
			{
				text = TRUE;
				html = TRUE;
			}
			else if (strcmp(format->formatName, type_FileGroupDescriptorW) == 0)
			{
				file = TRUE;
				text = TRUE;
			}
		}
		else
		{
			switch (format->formatId)
			{
				case CF_TEXT:
				case CF_OEMTEXT:
				case CF_UNICODETEXT:
					text = TRUE;
					break;

				case CF_DIB:
					image = TRUE;
					break;

				default:
					break;
			}
		}
	}

	if (html)
	{
		UwacClipboardOfferCreate(clipboard->seat, mime_html);
	}

	if (file && cliprdr_file_context_has_local_support(clipboard->file))
	{
		UwacClipboardOfferCreate(clipboard->seat, mime_uri_list);
		UwacClipboardOfferCreate(clipboard->seat, mime_gnome_copied_files);
		UwacClipboardOfferCreate(clipboard->seat, mime_mate_copied_files);
	}

	if (text)
	{
		size_t x;

		for (x = 0; x < ARRAYSIZE(mime_text); x++)
			UwacClipboardOfferCreate(clipboard->seat, mime_text[x]);
	}

	if (image)
	{
		for (size_t x = 0; x < ARRAYSIZE(mime_image); x++)
			UwacClipboardOfferCreate(clipboard->seat, mime_image[x]);
	}

	UwacClipboardOfferAnnounce(clipboard->seat, clipboard, wlf_cliprdr_transfer_data,
	                           wlf_cliprdr_cancel_data);
	return wlf_cliprdr_send_client_format_list_response(clipboard, TRUE);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
wlf_cliprdr_server_format_list_response(CliprdrClientContext* context,
                                        const CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(formatListResponse);

	if (formatListResponse->common.msgFlags & CB_RESPONSE_FAIL)
		WLog_WARN(TAG, "format list update failed");
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
wlf_cliprdr_server_format_data_request(CliprdrClientContext* context,
                                       const CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest)
{
	UINT rc = CHANNEL_RC_OK;
	BYTE* data = NULL;
	size_t size = 0;
	const char* mime = NULL;
	UINT32 formatId = 0;
	UINT32 localFormatId = 0;
	wfClipboard* clipboard = 0;

	UINT32 dsize = 0;
	BYTE* ddata = NULL;

	WINPR_ASSERT(context);
	WINPR_ASSERT(formatDataRequest);

	localFormatId = formatId = formatDataRequest->requestedFormatId;
	clipboard = cliprdr_file_context_get_context(context->custom);
	WINPR_ASSERT(clipboard);

	const char* idstr = ClipboardGetFormatIdString(formatId);
	const char* name = ClipboardGetFormatName(clipboard->system, formatId);
	const UINT32 fileFormatId = ClipboardGetFormatId(clipboard->system, type_FileGroupDescriptorW);
	const UINT32 htmlFormatId = ClipboardGetFormatId(clipboard->system, type_HtmlFormat);

	switch (formatId)
	{
		case CF_TEXT:
		case CF_OEMTEXT:
		case CF_UNICODETEXT:
			localFormatId = ClipboardGetFormatId(clipboard->system, mime_text_plain);
			mime = mime_text_utf8;
			break;

		case CF_DIB:
		case CF_DIBV5:
			mime = mime_bmp;
			break;

		default:
			if (formatId == fileFormatId)
			{
				localFormatId = ClipboardGetFormatId(clipboard->system, mime_uri_list);
				mime = mime_uri_list;
			}
			else if (formatId == htmlFormatId)
			{
				localFormatId = ClipboardGetFormatId(clipboard->system, mime_html);
				mime = mime_html;
			}
			else
				goto fail;
			break;
	}

	data = UwacClipboardDataGet(clipboard->seat, mime, &size);

	if (!data)
		goto fail;

	if (fileFormatId == formatId)
	{
		if (!cliprdr_file_context_update_client_data(clipboard->file, data, size))
			goto fail;
	}

	ClipboardLock(clipboard->system);
	const BOOL res = ClipboardSetData(clipboard->system, localFormatId, data, size);
	free(data);

	UINT32 len = 0;
	data = NULL;
	if (res)
		data = ClipboardGetData(clipboard->system, formatId, &len);

	ClipboardUnlock(clipboard->system);
	if (!res || !data)
		goto fail;

	if (fileFormatId == formatId)
	{
		const UINT32 flags = cliprdr_file_context_remote_get_flags(clipboard->file);
		const UINT32 error = cliprdr_serialize_file_list_ex(
		    flags, (const FILEDESCRIPTORW*)data, len / sizeof(FILEDESCRIPTORW), &ddata, &dsize);
		if (error)
			goto fail;
	}
fail:
	rc = wlf_cliprdr_send_data_response(clipboard, ddata, dsize);
	free(data);
	return rc;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
wlf_cliprdr_server_format_data_response(CliprdrClientContext* context,
                                        const CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse)
{
	UINT rc = ERROR_INTERNAL_ERROR;

	WINPR_ASSERT(context);
	WINPR_ASSERT(formatDataResponse);

	const UINT32 size = formatDataResponse->common.dataLen;
	const BYTE* data = formatDataResponse->requestedFormatData;

	wfClipboard* clipboard = cliprdr_file_context_get_context(context->custom);
	WINPR_ASSERT(clipboard);

	wlf_request* request = Queue_Dequeue(clipboard->request_queue);
	if (!request)
		goto fail;

	rc = CHANNEL_RC_OK;
	if (formatDataResponse->common.msgFlags & CB_RESPONSE_FAIL)
	{
		WLog_WARN(TAG, "clipboard data request for format %" PRIu32 " [%s], mime %s failed",
		          request->responseFormat, ClipboardGetFormatIdString(request->responseFormat),
		          request->responseMime);
		goto fail;
	}
	rc = ERROR_INTERNAL_ERROR;

	EnterCriticalSection(&clipboard->lock);

	UINT32 srcFormatId = 0;
	UINT32 dstFormatId = 0;
	switch (request->responseFormat)
	{
		case CF_TEXT:
		case CF_OEMTEXT:
		case CF_UNICODETEXT:
			srcFormatId = request->responseFormat;
			dstFormatId = ClipboardGetFormatId(clipboard->system, request->responseMime);
			break;

		case CF_DIB:
		case CF_DIBV5:
			srcFormatId = request->responseFormat;
			dstFormatId = ClipboardGetFormatId(clipboard->system, request->responseMime);
			break;

		default:
		{
			const char* name = wlf_get_server_format_name(clipboard, request->responseFormat);
			if (name)
			{
				if (strcmp(type_FileGroupDescriptorW, name) == 0)
				{
					srcFormatId =
					    ClipboardGetFormatId(clipboard->system, type_FileGroupDescriptorW);
					dstFormatId = ClipboardGetFormatId(clipboard->system, request->responseMime);

					if (!cliprdr_file_context_update_server_data(clipboard->file, clipboard->system,
					                                             data, size))
						goto unlock;
				}
				else if (strcmp(type_HtmlFormat, name) == 0)
				{
					srcFormatId = ClipboardGetFormatId(clipboard->system, type_HtmlFormat);
					dstFormatId = ClipboardGetFormatId(clipboard->system, request->responseMime);
				}
			}
		}
		break;
	}

	ClipboardLock(clipboard->system);
	UINT32 len = 0;

	const BOOL sres = ClipboardSetData(clipboard->system, srcFormatId, data, size);
	if (sres)
		data = ClipboardGetData(clipboard->system, dstFormatId, &len);
	ClipboardUnlock(clipboard->system);

	if (!sres || !data)
		goto unlock;

	if (request->responseFile)
	{
		const size_t res = fwrite(data, 1, len, request->responseFile);
		if (res == len)
			rc = CHANNEL_RC_OK;
	}
	else
		rc = CHANNEL_RC_OK;

unlock:
	LeaveCriticalSection(&clipboard->lock);
fail:
	wlf_request_free(request);
	return rc;
}

wfClipboard* wlf_clipboard_new(wlfContext* wfc)
{
	rdpChannels* channels;
	wfClipboard* clipboard;

	WINPR_ASSERT(wfc);

	clipboard = (wfClipboard*)calloc(1, sizeof(wfClipboard));

	if (!clipboard)
		goto fail;

	InitializeCriticalSection(&clipboard->lock);
	clipboard->wfc = wfc;
	channels = wfc->common.context.channels;
	clipboard->log = WLog_Get(TAG);
	clipboard->channels = channels;
	clipboard->system = ClipboardCreate();
	if (!clipboard->system)
		goto fail;

	clipboard->file = cliprdr_file_context_new(clipboard);
	if (!clipboard->file)
		goto fail;

	if (!cliprdr_file_context_set_locally_available(clipboard->file, TRUE))
		goto fail;

	clipboard->request_queue = Queue_New(TRUE, -1, -1);
	if (!clipboard->request_queue)
		goto fail;

	wObject* obj = Queue_Object(clipboard->request_queue);
	WINPR_ASSERT(obj);
	obj->fnObjectFree = wlf_request_free;
	obj->fnObjectNew = wlf_request_clone;

	return clipboard;

fail:
	wlf_clipboard_free(clipboard);
	return NULL;
}

void wlf_clipboard_free(wfClipboard* clipboard)
{
	if (!clipboard)
		return;

	cliprdr_file_context_free(clipboard->file);

	wlf_cliprdr_free_server_formats(clipboard);
	wlf_cliprdr_free_client_formats(clipboard);
	ClipboardDestroy(clipboard->system);

	EnterCriticalSection(&clipboard->lock);

	Queue_Free(clipboard->request_queue);
	LeaveCriticalSection(&clipboard->lock);
	DeleteCriticalSection(&clipboard->lock);
	free(clipboard);
}

BOOL wlf_cliprdr_init(wfClipboard* clipboard, CliprdrClientContext* cliprdr)
{
	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(cliprdr);

	clipboard->context = cliprdr;
	cliprdr->MonitorReady = wlf_cliprdr_monitor_ready;
	cliprdr->ServerCapabilities = wlf_cliprdr_server_capabilities;
	cliprdr->ServerFormatList = wlf_cliprdr_server_format_list;
	cliprdr->ServerFormatListResponse = wlf_cliprdr_server_format_list_response;
	cliprdr->ServerFormatDataRequest = wlf_cliprdr_server_format_data_request;
	cliprdr->ServerFormatDataResponse = wlf_cliprdr_server_format_data_response;

	return cliprdr_file_context_init(clipboard->file, cliprdr);
}

BOOL wlf_cliprdr_uninit(wfClipboard* clipboard, CliprdrClientContext* cliprdr)
{
	WINPR_ASSERT(clipboard);
	if (!cliprdr_file_context_uninit(clipboard->file, cliprdr))
		return FALSE;

	if (cliprdr)
		cliprdr->custom = NULL;

	return TRUE;
}
