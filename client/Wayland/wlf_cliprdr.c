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

#include "wlf_cliprdr.h"

#define TAG CLIENT_TAG("wayland.cliprdr")

#define MAX_CLIPBOARD_FORMATS 255

static const char* mime_text[] = { "text/plain",  "text/plain;charset=utf-8",
	                               "UTF8_STRING", "COMPOUND_TEXT",
	                               "TEXT",        "STRING" };

static const char* mime_image[] = {
	"image/png",       "image/bmp",   "image/x-bmp",        "image/x-MS-bmp",
	"image/x-icon",    "image/x-ico", "image/x-win-bitmap", "image/vmd.microsoft.icon",
	"application/ico", "image/ico",   "image/icon",         "image/jpeg",
	"image/tiff"
};

static const char* mime_html[] = { "text/html" };

struct wlf_clipboard
{
	wlfContext* wfc;
	rdpChannels* channels;
	CliprdrClientContext* context;
	wLog* log;

	UwacSeat* seat;
	wClipboard* system;
	wClipboardDelegate* delegate;

	size_t numClientFormats;
	CLIPRDR_FORMAT* clientFormats;

	size_t numServerFormats;
	CLIPRDR_FORMAT* serverFormats;

	BOOL sync;

	/* File clipping */
	BOOL streams_supported;
	BOOL file_formats_registered;

	/* Server response stuff */
	FILE* responseFile;
	UINT32 responseFormat;
	const char* responseMime;
	CRITICAL_SECTION lock;
};

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
	size_t x;

	for (x = 0; x < ARRAYSIZE(mime_html); x++)
	{
		if (strcmp(mime, mime_html[x]) == 0)
			return TRUE;
	}

	return FALSE;
}

static void wlf_cliprdr_free_server_formats(wfClipboard* clipboard)
{
	if (clipboard && clipboard->serverFormats)
	{
		size_t j;

		for (j = 0; j < clipboard->numServerFormats; j++)
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
		size_t j;

		for (j = 0; j < clipboard->numClientFormats; j++)
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
	CLIPRDR_FORMAT_LIST formatList = { 0 };
	formatList.common.msgFlags = CB_RESPONSE_OK;
	formatList.numFormats = (UINT32)clipboard->numClientFormats;
	formatList.formats = clipboard->clientFormats;
	formatList.common.msgType = CB_FORMAT_LIST;

	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(clipboard->context);
	WINPR_ASSERT(clipboard->context->ClientFormatList);
	return clipboard->context->ClientFormatList(clipboard->context, &formatList);
}

static void wfl_cliprdr_add_client_format_id(wfClipboard* clipboard, UINT32 formatId)
{
	size_t x;
	CLIPRDR_FORMAT* format;
	const char* name = ClipboardGetFormatName(clipboard->system, formatId);

	for (x = 0; x < clipboard->numClientFormats; x++)
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

static void wlf_cliprdr_add_client_format(wfClipboard* clipboard, const char* mime)
{
	if (wlf_mime_is_html(mime))
	{
		UINT32 formatId = ClipboardGetFormatId(clipboard->system, "HTML Format");
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
		UINT32 formatId = ClipboardGetFormatId(clipboard->system, "image/bmp");
		wfl_cliprdr_add_client_format_id(clipboard, formatId);
		wfl_cliprdr_add_client_format_id(clipboard, CF_DIB);
	}

	wlf_cliprdr_send_client_format_list(clipboard);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT wlf_cliprdr_send_data_request(wfClipboard* clipboard, UINT32 formatId)
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
			wlf_cliprdr_add_client_format(clipboard, event->mime);
			return TRUE;

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
	CLIPRDR_CAPABILITIES capabilities = { 0 };
	CLIPRDR_GENERAL_CAPABILITY_SET generalCapabilitySet = { 0 };
	capabilities.cCapabilitiesSets = 1;
	capabilities.capabilitySets = (CLIPRDR_CAPABILITY_SET*)&(generalCapabilitySet);
	generalCapabilitySet.capabilitySetType = CB_CAPSTYPE_GENERAL;
	generalCapabilitySet.capabilitySetLength = 12;
	generalCapabilitySet.version = CB_CAPS_VERSION_2;
	generalCapabilitySet.generalFlags = CB_USE_LONG_FORMAT_NAMES;

	if (clipboard->streams_supported && clipboard->file_formats_registered)
		generalCapabilitySet.generalFlags |=
		    CB_STREAM_FILECLIP_ENABLED | CB_FILECLIP_NO_FILE_PATHS | CB_HUGE_FILE_SUPPORT_ENABLED;

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
static UINT wlf_cliprdr_monitor_ready(CliprdrClientContext* context,
                                      const CLIPRDR_MONITOR_READY* monitorReady)
{
	wfClipboard* clipboard;
	UINT ret;

	WINPR_UNUSED(monitorReady);
	WINPR_ASSERT(context);
	WINPR_ASSERT(monitorReady);

	clipboard = (wfClipboard*)context->custom;
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
	UINT32 i;
	const BYTE* capsPtr;
	wfClipboard* clipboard;

	WINPR_ASSERT(context);
	WINPR_ASSERT(capabilities);

	capsPtr = (const BYTE*)capabilities->capabilitySets;
	WINPR_ASSERT(capsPtr);

	clipboard = context->custom;
	WINPR_ASSERT(clipboard);

	clipboard->streams_supported = FALSE;

	for (i = 0; i < capabilities->cCapabilitiesSets; i++)
	{
		const CLIPRDR_CAPABILITY_SET* caps = (const CLIPRDR_CAPABILITY_SET*)capsPtr;

		if (caps->capabilitySetType == CB_CAPSTYPE_GENERAL)
		{
			const CLIPRDR_GENERAL_CAPABILITY_SET* generalCaps =
			    (const CLIPRDR_GENERAL_CAPABILITY_SET*)caps;

			if (generalCaps->generalFlags & CB_STREAM_FILECLIP_ENABLED)
			{
				clipboard->streams_supported = TRUE;
			}
		}

		capsPtr += caps->capabilitySetLength;
	}

	return CHANNEL_RC_OK;
}

static void wlf_cliprdr_transfer_data(UwacSeat* seat, void* context, const char* mime, int fd)
{
	wfClipboard* clipboard = (wfClipboard*)context;
	size_t x;
	WINPR_UNUSED(seat);

	EnterCriticalSection(&clipboard->lock);
	clipboard->responseMime = NULL;

	for (x = 0; x < ARRAYSIZE(mime_html); x++)
	{
		const char* mime_cur = mime_html[x];

		if (strcmp(mime_cur, mime) == 0)
		{
			clipboard->responseMime = mime_cur;
			clipboard->responseFormat = ClipboardGetFormatId(clipboard->system, "HTML Format");
			break;
		}
	}

	for (x = 0; x < ARRAYSIZE(mime_text); x++)
	{
		const char* mime_cur = mime_text[x];

		if (strcmp(mime_cur, mime) == 0)
		{
			clipboard->responseMime = mime_cur;
			clipboard->responseFormat = CF_UNICODETEXT;
			break;
		}
	}

	for (x = 0; x < ARRAYSIZE(mime_image); x++)
	{
		const char* mime_cur = mime_image[x];

		if (strcmp(mime_cur, mime) == 0)
		{
			clipboard->responseMime = mime_cur;
			clipboard->responseFormat = CF_DIB;
			break;
		}
	}

	if (clipboard->responseMime != NULL)
	{
		if (clipboard->responseFile != NULL)
			fclose(clipboard->responseFile);
		clipboard->responseFile = fdopen(fd, "w");

		if (clipboard->responseFile)
			wlf_cliprdr_send_data_request(clipboard, clipboard->responseFormat);
		else
			WLog_Print(clipboard->log, WLOG_ERROR,
			           "failed to open clipboard file descriptor for MIME %s",
			           clipboard->responseMime);
	}
	LeaveCriticalSection(&clipboard->lock);
}

static void wlf_cliprdr_cancel_data(UwacSeat* seat, void* context)
{
	WINPR_UNUSED(seat);
	WINPR_UNUSED(context);
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
	UINT32 i;
	wfClipboard* clipboard;
	BOOL html = FALSE;
	BOOL text = FALSE;
	BOOL image = FALSE;

	if (!context || !context->custom)
		return ERROR_INVALID_PARAMETER;

	clipboard = (wfClipboard*)context->custom;
	WINPR_ASSERT(clipboard);

	wlf_cliprdr_free_server_formats(clipboard);

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
				wlf_cliprdr_free_server_formats(clipboard);
				return CHANNEL_RC_NO_MEMORY;
			}
		}

		if (format->formatName)
		{
			if (strcmp(format->formatName, "HTML Format") == 0)
			{
				text = TRUE;
				html = TRUE;
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
		size_t x;

		for (x = 0; x < ARRAYSIZE(mime_html); x++)
			UwacClipboardOfferCreate(clipboard->seat, mime_html[x]);
	}

	if (text)
	{
		size_t x;

		for (x = 0; x < ARRAYSIZE(mime_text); x++)
			UwacClipboardOfferCreate(clipboard->seat, mime_text[x]);
	}

	if (image)
	{
		size_t x;

		for (x = 0; x < ARRAYSIZE(mime_image); x++)
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
	// wfClipboard* clipboard = (wfClipboard*) context->custom;
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
	int cnv;
	UINT rc = CHANNEL_RC_OK;
	BYTE* data;
	LPWSTR cdata;
	size_t size;
	const char* mime;
	UINT32 formatId;
	wfClipboard* clipboard;

	WINPR_ASSERT(context);
	WINPR_ASSERT(formatDataRequest);

	formatId = formatDataRequest->requestedFormatId;
	clipboard = context->custom;
	WINPR_ASSERT(clipboard);

	switch (formatId)
	{
		case CF_TEXT:
		case CF_OEMTEXT:
		case CF_UNICODETEXT:
			mime = "text/plain;charset=utf-8";
			break;

		case CF_DIB:
		case CF_DIBV5:
			mime = "image/bmp";
			break;

		default:
			if (formatId == ClipboardGetFormatId(clipboard->system, "HTML Format"))
				mime = "text/html";
			else if (formatId == ClipboardGetFormatId(clipboard->system, "image/bmp"))
				mime = "image/bmp";
			else
				mime = ClipboardGetFormatName(clipboard->system, formatId);

			break;
	}

	data = UwacClipboardDataGet(clipboard->seat, mime, &size);

	if (!data)
		return ERROR_INTERNAL_ERROR;

	switch (formatId)
	{
		case CF_UNICODETEXT:
			if (size > INT_MAX)
				rc = ERROR_INTERNAL_ERROR;
			else
			{
				cdata = NULL;
				cnv = ConvertToUnicode(CP_UTF8, 0, (LPCSTR)data, (int)size, &cdata, 0);
				free(data);
				data = NULL;

				if (cnv < 0)
					rc = ERROR_INTERNAL_ERROR;
				else
				{
					size = (size_t)cnv;
					data = (BYTE*)cdata;
					size *= sizeof(WCHAR);
				}
			}

			break;

		default:
			// TODO: Image conversions
			break;
	}

	if (rc != CHANNEL_RC_OK)
	{
		free(data);
		return rc;
	}

	rc = wlf_cliprdr_send_data_response(clipboard, data, size);
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
	int cnv;
	UINT rc = ERROR_INTERNAL_ERROR;
	UINT32 size;
	LPSTR cdata = NULL;
	LPCSTR data;
	const WCHAR* wdata;
	wfClipboard* clipboard;

	WINPR_ASSERT(context);
	WINPR_ASSERT(formatDataResponse);

	size = formatDataResponse->common.dataLen;
	data = (LPCSTR)formatDataResponse->requestedFormatData;
	wdata = (const WCHAR*)formatDataResponse->requestedFormatData;

	clipboard = context->custom;
	WINPR_ASSERT(clipboard);

	EnterCriticalSection(&clipboard->lock);

	if (size > INT_MAX * sizeof(WCHAR))
		return ERROR_INTERNAL_ERROR;

	switch (clipboard->responseFormat)
	{
		case CF_UNICODETEXT:
			cnv = ConvertFromUnicode(CP_UTF8, 0, wdata, (int)(size / sizeof(WCHAR)), &cdata, 0,
			                         NULL, NULL);

			if (cnv < 0)
				return ERROR_INTERNAL_ERROR;

			data = cdata;
			size = 0;
			if (cnv > 0)
				size = (UINT32)strnlen(data, (size_t)cnv);
			break;

		default:
			// TODO: Image conversions
			break;
	}

	if (clipboard->responseFile)
	{
		fwrite(data, 1, size, clipboard->responseFile);
		fclose(clipboard->responseFile);
		clipboard->responseFile = NULL;
	}
	rc = CHANNEL_RC_OK;
	free(cdata);

	LeaveCriticalSection(&clipboard->lock);
	return rc;
}

static UINT
wlf_cliprdr_server_file_size_request(wfClipboard* clipboard,
                                     const CLIPRDR_FILE_CONTENTS_REQUEST* fileContentsRequest)
{
	wClipboardFileSizeRequest request = { 0 };

	WINPR_ASSERT(fileContentsRequest);

	request.streamId = fileContentsRequest->streamId;
	request.listIndex = fileContentsRequest->listIndex;

	if (fileContentsRequest->cbRequested != sizeof(UINT64))
	{
		WLog_Print(clipboard->log, WLOG_WARN,
		           "unexpected FILECONTENTS_SIZE request: %" PRIu32 " bytes",
		           fileContentsRequest->cbRequested);
	}

	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(clipboard->delegate);
	WINPR_ASSERT(clipboard->delegate->ClientRequestFileSize);
	return clipboard->delegate->ClientRequestFileSize(clipboard->delegate, &request);
}

static UINT
wlf_cliprdr_server_file_range_request(wfClipboard* clipboard,
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
wlf_cliprdr_send_file_contents_failure(CliprdrClientContext* context,
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
wlf_cliprdr_server_file_contents_request(CliprdrClientContext* context,
                                         const CLIPRDR_FILE_CONTENTS_REQUEST* fileContentsRequest)
{
	UINT error = NO_ERROR;
	wfClipboard* clipboard;

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
		WLog_Print(clipboard->log, WLOG_ERROR, "invalid CLIPRDR_FILECONTENTS_REQUEST.dwFlags");
		return wlf_cliprdr_send_file_contents_failure(context, fileContentsRequest);
	}

	if (fileContentsRequest->dwFlags & FILECONTENTS_SIZE)
		error = wlf_cliprdr_server_file_size_request(clipboard, fileContentsRequest);

	if (fileContentsRequest->dwFlags & FILECONTENTS_RANGE)
		error = wlf_cliprdr_server_file_range_request(clipboard, fileContentsRequest);

	if (error)
	{
		WLog_Print(clipboard->log, WLOG_ERROR,
		           "failed to handle CLIPRDR_FILECONTENTS_REQUEST: 0x%08X", error);
		return wlf_cliprdr_send_file_contents_failure(context, fileContentsRequest);
	}

	return CHANNEL_RC_OK;
}

static UINT wlf_cliprdr_clipboard_file_size_success(wClipboardDelegate* delegate,
                                                    const wClipboardFileSizeRequest* request,
                                                    UINT64 fileSize)
{
	CLIPRDR_FILE_CONTENTS_RESPONSE response = { 0 };
	wfClipboard* clipboard;

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

static UINT wlf_cliprdr_clipboard_file_size_failure(wClipboardDelegate* delegate,
                                                    const wClipboardFileSizeRequest* request,
                                                    UINT errorCode)
{
	CLIPRDR_FILE_CONTENTS_RESPONSE response = { 0 };
	wfClipboard* clipboard;

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

static UINT wlf_cliprdr_clipboard_file_range_success(wClipboardDelegate* delegate,
                                                     const wClipboardFileRangeRequest* request,
                                                     const BYTE* data, UINT32 size)
{
	CLIPRDR_FILE_CONTENTS_RESPONSE response = { 0 };
	wfClipboard* clipboard;

	WINPR_ASSERT(delegate);
	WINPR_ASSERT(request);
	WINPR_ASSERT(data || (size == 0));

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

static UINT wlf_cliprdr_clipboard_file_range_failure(wClipboardDelegate* delegate,
                                                     const wClipboardFileRangeRequest* request,
                                                     UINT errorCode)
{
	CLIPRDR_FILE_CONTENTS_RESPONSE response = { 0 };
	wfClipboard* clipboard;

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

	clipboard->delegate = ClipboardGetDelegate(clipboard->system);
	if (!clipboard->delegate)
		goto fail;

	clipboard->delegate->custom = clipboard;
	/* TODO: set up a filesystem base path for local URI */
	/* clipboard->delegate->basePath = "file:///tmp/foo/bar/gaga"; */
	clipboard->delegate->ClipboardFileSizeSuccess = wlf_cliprdr_clipboard_file_size_success;
	clipboard->delegate->ClipboardFileSizeFailure = wlf_cliprdr_clipboard_file_size_failure;
	clipboard->delegate->ClipboardFileRangeSuccess = wlf_cliprdr_clipboard_file_range_success;
	clipboard->delegate->ClipboardFileRangeFailure = wlf_cliprdr_clipboard_file_range_failure;
	return clipboard;

fail:
	wlf_clipboard_free(clipboard);
	return NULL;
}

void wlf_clipboard_free(wfClipboard* clipboard)
{
	if (!clipboard)
		return;

	wlf_cliprdr_free_server_formats(clipboard);
	wlf_cliprdr_free_client_formats(clipboard);
	ClipboardDestroy(clipboard->system);

	EnterCriticalSection(&clipboard->lock);
	if (clipboard->responseFile)
		fclose(clipboard->responseFile);
	LeaveCriticalSection(&clipboard->lock);
	DeleteCriticalSection(&clipboard->lock);
	free(clipboard);
}

BOOL wlf_cliprdr_init(wfClipboard* clipboard, CliprdrClientContext* cliprdr)
{
	if (!cliprdr || !clipboard)
		return FALSE;

	clipboard->context = cliprdr;
	cliprdr->custom = (void*)clipboard;
	cliprdr->MonitorReady = wlf_cliprdr_monitor_ready;
	cliprdr->ServerCapabilities = wlf_cliprdr_server_capabilities;
	cliprdr->ServerFormatList = wlf_cliprdr_server_format_list;
	cliprdr->ServerFormatListResponse = wlf_cliprdr_server_format_list_response;
	cliprdr->ServerFormatDataRequest = wlf_cliprdr_server_format_data_request;
	cliprdr->ServerFormatDataResponse = wlf_cliprdr_server_format_data_response;
	cliprdr->ServerFileContentsRequest = wlf_cliprdr_server_file_contents_request;
	return TRUE;
}

BOOL wlf_cliprdr_uninit(wfClipboard* clipboard, CliprdrClientContext* cliprdr)
{
	if (cliprdr)
		cliprdr->custom = NULL;

	if (clipboard)
		clipboard->context = NULL;

	return TRUE;
}
