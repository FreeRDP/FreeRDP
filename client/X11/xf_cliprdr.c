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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#ifdef WITH_XFIXES
#include <X11/extensions/Xfixes.h>
#endif

#include <winpr/crt.h>
#include <winpr/image.h>
#include <winpr/stream.h>
#include <winpr/clipboard.h>

#include <freerdp/log.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/channels/channels.h>

#include "xf_cliprdr.h"

#define TAG CLIENT_TAG("x11")

struct xf_cliprdr_format
{
	Atom atom;
	UINT32 formatId;
	char* formatName;
};
typedef struct xf_cliprdr_format xfCliprdrFormat;

struct xf_clipboard
{
	xfContext* xfc;
	rdpChannels* channels;
	CliprdrClientContext* context;

	wClipboard* system;

	Window root_window;
	Atom clipboard_atom;
	Atom property_atom;

	int numClientFormats;
	xfCliprdrFormat clientFormats[20];

	int numServerFormats;
	CLIPRDR_FORMAT* serverFormats;

	int numTargets;
	Atom targets[20];

	int requestedFormatId;

	BYTE* data;
	UINT32 data_format;
	UINT32 data_alt_format;
	int data_length;
	XEvent* respond;

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
};

UINT xf_cliprdr_send_client_format_list(xfClipboard* clipboard);

static void xf_cliprdr_check_owner(xfClipboard* clipboard)
{
	Window owner;
	xfContext* xfc = clipboard->xfc;

	if (clipboard->sync)
	{
		owner = XGetSelectionOwner(xfc->display, clipboard->clipboard_atom);

		if (clipboard->owner != owner)
		{
			clipboard->owner = owner;
			xf_cliprdr_send_client_format_list(clipboard);
		}
	}
}

static BOOL xf_cliprdr_is_self_owned(xfClipboard* clipboard)
{
	xfContext* xfc = clipboard->xfc;

	return XGetSelectionOwner(xfc->display, clipboard->clipboard_atom) == xfc->drawable;
}

static xfCliprdrFormat* xf_cliprdr_get_format_by_id(xfClipboard* clipboard, UINT32 formatId)
{
	UINT32 index;
	xfCliprdrFormat* format;

	for (index = 0; index < clipboard->numClientFormats; index++)
	{
		format = &(clipboard->clientFormats[index]);

		if (format->formatId == formatId)
			return format;
	}

	return NULL;
}

static xfCliprdrFormat* xf_cliprdr_get_format_by_atom(xfClipboard* clipboard, Atom atom)
{
	int i, j;
	xfCliprdrFormat* format;

	for (i = 0; i < clipboard->numClientFormats; i++)
	{
		format = &(clipboard->clientFormats[i]);

		if (format->atom != atom)
			continue;

		if (format->formatId == 0)
			return format;

		for (j = 0; j < clipboard->numServerFormats; j++)
		{
			if (clipboard->serverFormats[j].formatId == format->formatId)
				return format;
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
	CLIPRDR_FORMAT_DATA_REQUEST request;

	ZeroMemory(&request, sizeof(CLIPRDR_FORMAT_DATA_REQUEST));

	request.requestedFormatId = formatId;

	return clipboard->context->ClientFormatDataRequest(clipboard->context, &request);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT xf_cliprdr_send_data_response(xfClipboard* clipboard, BYTE* data, int size)
{
	CLIPRDR_FORMAT_DATA_RESPONSE response;

	ZeroMemory(&response, sizeof(CLIPRDR_FORMAT_DATA_RESPONSE));

	response.msgFlags = CB_RESPONSE_OK;
	response.dataLen = size;
	response.requestedFormatData = data;

	return clipboard->context->ClientFormatDataResponse(clipboard->context, &response);
}

static void xf_cliprdr_get_requested_targets(xfClipboard* clipboard)
{
	int i;
	Atom atom;
	BYTE* data = NULL;
	int format_property;
	unsigned long length;
	unsigned long bytes_left;
	UINT32 numFormats = 0;
	CLIPRDR_FORMAT_LIST formatList;
	xfCliprdrFormat* format = NULL;
	CLIPRDR_FORMAT* formats = NULL;
	xfContext* xfc = clipboard->xfc;

	if (!clipboard->numServerFormats)
		return; /* server format list was not yet received */

	XGetWindowProperty(xfc->display, xfc->drawable, clipboard->property_atom,
		0, 200, 0, XA_ATOM, &atom, &format_property, &length, &bytes_left, &data);

	if (length > 0)
	{
		if (!data)
		{
			WLog_ERR(TAG, "XGetWindowProperty set length = %d but data is NULL", length);
			goto out;
		}
		if (!(formats = (CLIPRDR_FORMAT*) calloc(length, sizeof(CLIPRDR_FORMAT))))
		{
			WLog_ERR(TAG, "failed to allocate %d CLIPRDR_FORMAT structs", length);
			goto out;
		}
	}

	for (i = 0; i < length; i++)
	{
		atom = ((Atom*) data)[i];

		format = xf_cliprdr_get_format_by_atom(clipboard, atom);

		if (format)
		{
			formats[numFormats].formatId = format->formatId;
			formats[numFormats].formatName = format->formatName;
			numFormats++;
		}
	}

	ZeroMemory(&formatList, sizeof(CLIPRDR_FORMAT_LIST));

	formatList.msgFlags = CB_RESPONSE_OK;
	formatList.numFormats = numFormats;
	formatList.formats = formats;

	clipboard->context->ClientFormatList(clipboard->context, &formatList);

out:
	if (data)
		XFree(data);
	free(formats);
}

static void xf_cliprdr_process_requested_data(xfClipboard* clipboard, BOOL hasData, BYTE* data, int size)
{
	BOOL bSuccess;
	UINT32 SrcSize;
	UINT32 DstSize;
	UINT32 formatId;
	UINT32 altFormatId;
	BYTE* pSrcData = NULL;
	BYTE* pDstData = NULL;
	xfCliprdrFormat* format;

	if (clipboard->incr_starts && hasData)
		return;

	format = xf_cliprdr_get_format_by_id(clipboard, clipboard->requestedFormatId);

	if (!hasData || !data || !format)
	{
		xf_cliprdr_send_data_response(clipboard, NULL, 0);
		return;
	}

	formatId = 0;
	altFormatId = 0;

	switch (format->formatId)
	{
		case CF_TEXT:
		case CF_OEMTEXT:
		case CF_UNICODETEXT:
			size = strlen((char*) data) + 1;
			formatId = ClipboardGetFormatId(clipboard->system, "UTF8_STRING");
			break;

		case CF_DIB:
			formatId = ClipboardGetFormatId(clipboard->system, "image/bmp");
			break;

		case CB_FORMAT_HTML:
			size = strlen((char*) data) + 1;
			formatId = ClipboardGetFormatId(clipboard->system, "text/html");
			break;
	}

	SrcSize = (UINT32) size;
	pSrcData = (BYTE*) malloc(SrcSize);

	if (!pSrcData)
		return;

	CopyMemory(pSrcData, data, SrcSize);

	bSuccess = ClipboardSetData(clipboard->system, formatId, (void*) pSrcData, SrcSize);

	if (!bSuccess)
		free(pSrcData);

	altFormatId = clipboard->requestedFormatId;

	if (bSuccess && altFormatId)
	{
		DstSize = 0;
		pDstData = (BYTE*) ClipboardGetData(clipboard->system, altFormatId, &DstSize);
	}

	if (!pDstData)
	{
		xf_cliprdr_send_data_response(clipboard, NULL, 0);
		return;
	}

	xf_cliprdr_send_data_response(clipboard, pDstData, (int) DstSize);
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
	xfCliprdrFormat* format;
	xfContext* xfc = clipboard->xfc;

	format = xf_cliprdr_get_format_by_id(clipboard, clipboard->requestedFormatId);

	if (!format || (format->atom != target))
	{
		xf_cliprdr_send_data_response(clipboard, NULL, 0);
		return FALSE;
	}

	XGetWindowProperty(xfc->display, xfc->drawable,
		clipboard->property_atom, 0, 0, 0, target,
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
		else if (XGetWindowProperty(xfc->display, xfc->drawable,
			clipboard->property_atom, 0, bytes_left, 0, target,
			&type, &format_property, &length, &dummy, &data) == Success)
		{
			if (clipboard->incr_starts)
			{
				BYTE *new_data;

				bytes_left = length * format_property / 8;
				new_data = (BYTE*) realloc(clipboard->incr_data, clipboard->incr_data_length + bytes_left);
				if (!new_data)
					return FALSE;
				clipboard->incr_data = new_data;
				CopyMemory(clipboard->incr_data + clipboard->incr_data_length, data, bytes_left);
				clipboard->incr_data_length += bytes_left;
				XFree(data);
				data = NULL;
			}
			has_data = TRUE;
		}
		else
		{

		}
	}

	XDeleteProperty(xfc->display, xfc->drawable, clipboard->property_atom);

	xf_cliprdr_process_requested_data(clipboard, has_data, data, (int) bytes_left);

	if (data)
		XFree(data);

	return TRUE;
}

static void xf_cliprdr_append_target(xfClipboard* clipboard, Atom target)
{
	int i;

	if (clipboard->numTargets >= ARRAYSIZE(clipboard->targets))
		return;

	for (i = 0; i < clipboard->numTargets; i++)
	{
		if (clipboard->targets[i] == target)
			return;
	}

	clipboard->targets[clipboard->numTargets++] = target;
}

static void xf_cliprdr_provide_targets(xfClipboard* clipboard, XEvent* respond)
{
	xfContext* xfc = clipboard->xfc;

	if (respond->xselection.property != None)
	{
		XChangeProperty(xfc->display, respond->xselection.requestor,
			respond->xselection.property, XA_ATOM, 32, PropModeReplace,
			(BYTE*) clipboard->targets, clipboard->numTargets);
	}
}

static void xf_cliprdr_provide_data(xfClipboard* clipboard, XEvent* respond, BYTE* data, UINT32 size)
{
	xfContext* xfc = clipboard->xfc;

	if (respond->xselection.property != None)
	{
		XChangeProperty(xfc->display, respond->xselection.requestor,
			respond->xselection.property, respond->xselection.target,
			8, PropModeReplace, data, size);
	}
}

static BOOL xf_cliprdr_process_selection_notify(xfClipboard* clipboard, XEvent* xevent)
{
	if (xevent->xselection.target == clipboard->targets[1])
	{
		if (xevent->xselection.property == None)
		{
			xf_cliprdr_send_client_format_list(clipboard);
		}
		else
		{
			xf_cliprdr_get_requested_targets(clipboard);
		}

		return TRUE;
	}
	else
	{
		return xf_cliprdr_get_requested_data(clipboard, xevent->xselection.target);
	}
}

static BOOL xf_cliprdr_process_selection_request(xfClipboard* clipboard, XEvent* xevent)
{
	int fmt;
	Atom type;
	UINT32 formatId;
	XEvent* respond;
	UINT32 altFormatId;
	BYTE* data = NULL;
	BOOL delayRespond;
	unsigned long length;
	unsigned long bytes_left;
	xfCliprdrFormat* format;
	xfContext* xfc = clipboard->xfc;

	if (xevent->xselectionrequest.owner != xfc->drawable)
		return FALSE;

	delayRespond = FALSE;

	if (!(respond = (XEvent*) calloc(1, sizeof(XEvent))))
	{
		WLog_ERR(TAG, "failed to allocate XEvent data");
		return FALSE;
	}

	respond->xselection.property = None;
	respond->xselection.type = SelectionNotify;
	respond->xselection.display = xevent->xselectionrequest.display;
	respond->xselection.requestor = xevent->xselectionrequest.requestor;
	respond->xselection.selection = xevent->xselectionrequest.selection;
	respond->xselection.target = xevent->xselectionrequest.target;
	respond->xselection.time = xevent->xselectionrequest.time;

	if (xevent->xselectionrequest.target == clipboard->targets[0]) /* TIMESTAMP */
	{
		/* TODO */
	}
	else if (xevent->xselectionrequest.target == clipboard->targets[1]) /* TARGETS */
	{
		/* Someone else requests our available formats */
		respond->xselection.property = xevent->xselectionrequest.property;
		xf_cliprdr_provide_targets(clipboard, respond);
	}
	else
	{
		format = xf_cliprdr_get_format_by_atom(clipboard, xevent->xselectionrequest.target);

		if (format && (xevent->xselectionrequest.requestor != xfc->drawable))
		{
			formatId = format->formatId;
			altFormatId = formatId;

			if (formatId == 0)
			{
				if (XGetWindowProperty(xfc->display, xevent->xselectionrequest.requestor,
					clipboard->property_atom, 0, 4, 0, XA_INTEGER,
					&type, &fmt, &length, &bytes_left, &data) != Success)
				{

				}

				if (data)
				{
					CopyMemory(&altFormatId, data, 4);
					XFree(data);
				}
			}

			if ((clipboard->data != 0) && (formatId == clipboard->data_format) && (altFormatId == clipboard->data_alt_format))
			{
				/* Cached clipboard data available. Send it now */
				respond->xselection.property = xevent->xselectionrequest.property;
				xf_cliprdr_provide_data(clipboard, respond, clipboard->data, clipboard->data_length);
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
				if (clipboard->data)
				{
					free(clipboard->data);
					clipboard->data = NULL;
				}

				respond->xselection.property = xevent->xselectionrequest.property;
				clipboard->respond = respond;
				clipboard->data_format = formatId;
				clipboard->data_alt_format = altFormatId;
				delayRespond = TRUE;

				xf_cliprdr_send_data_request(clipboard, altFormatId);
			}
		}
	}

	if (!delayRespond)
	{
		XSendEvent(xfc->display, xevent->xselectionrequest.requestor, 0, 0, respond);
		XFlush(xfc->display);
		free(respond);
	}

	return TRUE;
}

static BOOL xf_cliprdr_process_selection_clear(xfClipboard* clipboard, XEvent* xevent)
{
	xfContext* xfc = clipboard->xfc;

	if (xf_cliprdr_is_self_owned(clipboard))
		return FALSE;

	XDeleteProperty(xfc->display, clipboard->root_window, clipboard->property_atom);

	return TRUE;
}

static BOOL xf_cliprdr_process_property_notify(xfClipboard* clipboard, XEvent* xevent)
{
	xfCliprdrFormat* format;
	xfContext* xfc = clipboard->xfc;

	if (!clipboard)
		return TRUE;

	if (xevent->xproperty.atom != clipboard->property_atom)
		return FALSE; /* Not cliprdr-related */

	if (xevent->xproperty.window == clipboard->root_window)
	{
		xf_cliprdr_send_client_format_list(clipboard);
	}
	else if ((xevent->xproperty.window == xfc->drawable) &&
		(xevent->xproperty.state == PropertyNewValue) && clipboard->incr_starts)
	{
		format = xf_cliprdr_get_format_by_id(clipboard, clipboard->requestedFormatId);

		if (format)
			xf_cliprdr_get_requested_data(clipboard, format->atom);
	}

	return TRUE;
}

void xf_cliprdr_handle_xevent(xfContext* xfc, XEvent* event)
{
	xfClipboard* clipboard;

	if (!xfc || !event)
		return;

	clipboard = xfc->clipboard;

	if (!clipboard)
		return;

#ifdef WITH_XFIXES
	if (clipboard->xfixes_supported && event->type == XFixesSelectionNotify + clipboard->xfixes_event_base)
	{
		XFixesSelectionNotifyEvent* se = (XFixesSelectionNotifyEvent*) event;

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
			xf_cliprdr_process_selection_notify(clipboard, event);
			break;

		case SelectionRequest:
			xf_cliprdr_process_selection_request(clipboard, event);
			break;

		case SelectionClear:
			xf_cliprdr_process_selection_clear(clipboard, event);
			break;

		case PropertyNotify:
			xf_cliprdr_process_property_notify(clipboard, event);
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
UINT xf_cliprdr_send_client_capabilities(xfClipboard* clipboard)
{
	CLIPRDR_CAPABILITIES capabilities;
	CLIPRDR_GENERAL_CAPABILITY_SET generalCapabilitySet;

	capabilities.cCapabilitiesSets = 1;
	capabilities.capabilitySets = (CLIPRDR_CAPABILITY_SET*) &(generalCapabilitySet);

	generalCapabilitySet.capabilitySetType = CB_CAPSTYPE_GENERAL;
	generalCapabilitySet.capabilitySetLength = 12;

	generalCapabilitySet.version = CB_CAPS_VERSION_2;
	generalCapabilitySet.generalFlags = CB_USE_LONG_FORMAT_NAMES;

	return clipboard->context->ClientCapabilities(clipboard->context, &capabilities);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT xf_cliprdr_send_client_format_list(xfClipboard* clipboard)
{
	UINT32 i, numFormats;
	CLIPRDR_FORMAT* formats = NULL;
	CLIPRDR_FORMAT_LIST formatList;
	xfContext* xfc = clipboard->xfc;
	UINT ret;

	ZeroMemory(&formatList, sizeof(CLIPRDR_FORMAT_LIST));

	numFormats = clipboard->numClientFormats;
	if (numFormats)
	{
		if (!(formats = (CLIPRDR_FORMAT*) calloc(numFormats, sizeof(CLIPRDR_FORMAT))))
		{
			WLog_ERR(TAG, "failed to allocate %d CLIPRDR_FORMAT structs", numFormats);
			return CHANNEL_RC_NO_MEMORY;
		}
	}

	for (i = 0; i < numFormats; i++)
	{
		formats[i].formatId = clipboard->clientFormats[i].formatId;
		formats[i].formatName = clipboard->clientFormats[i].formatName;
	}

	formatList.msgFlags = CB_RESPONSE_OK;
	formatList.numFormats = numFormats;
	formatList.formats = formats;

	ret = clipboard->context->ClientFormatList(clipboard->context, &formatList);

	free(formats);

	if (clipboard->owner && clipboard->owner != xfc->drawable)
	{
		/* Request the owner for TARGETS, and wait for SelectionNotify event */
		XConvertSelection(xfc->display, clipboard->clipboard_atom,
			clipboard->targets[1], clipboard->property_atom, xfc->drawable, CurrentTime);
	}

	return ret;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT xf_cliprdr_send_client_format_list_response(xfClipboard* clipboard, BOOL status)
{
	CLIPRDR_FORMAT_LIST_RESPONSE formatListResponse;

	formatListResponse.msgType = CB_FORMAT_LIST_RESPONSE;
	formatListResponse.msgFlags = status ? CB_RESPONSE_OK : CB_RESPONSE_FAIL;
	formatListResponse.dataLen = 0;

	return clipboard->context->ClientFormatListResponse(clipboard->context, &formatListResponse);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
int xf_cliprdr_send_client_format_data_request(xfClipboard* clipboard, UINT32 formatId)
{
	CLIPRDR_FORMAT_DATA_REQUEST formatDataRequest;

	formatDataRequest.msgType = CB_FORMAT_DATA_REQUEST;
	formatDataRequest.msgFlags = CB_RESPONSE_OK;

	formatDataRequest.requestedFormatId = formatId;
	clipboard->requestedFormatId = formatId;

	return clipboard->context->ClientFormatDataRequest(clipboard->context, &formatDataRequest);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT xf_cliprdr_monitor_ready(CliprdrClientContext* context, CLIPRDR_MONITOR_READY* monitorReady)
{
	xfClipboard* clipboard = (xfClipboard*) context->custom;
	UINT ret;

	if ((ret = xf_cliprdr_send_client_capabilities(clipboard)) != CHANNEL_RC_OK)
		return ret;
	if ((ret = xf_cliprdr_send_client_format_list(clipboard)) != CHANNEL_RC_OK)
		return ret;

	clipboard->sync = TRUE;

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT xf_cliprdr_server_capabilities(CliprdrClientContext* context, CLIPRDR_CAPABILITIES* capabilities)
{
	//xfClipboard* clipboard = (xfClipboard*) context->custom;

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT xf_cliprdr_server_format_list(CliprdrClientContext* context, CLIPRDR_FORMAT_LIST* formatList)
{
	int i, j;
	CLIPRDR_FORMAT* format;
	xfClipboard* clipboard = (xfClipboard*) context->custom;
	xfContext* xfc = clipboard->xfc;
	UINT ret;

	if (clipboard->data)
	{
		free(clipboard->data);
		clipboard->data = NULL;
	}

	if (clipboard->serverFormats)
	{
		for (i = 0; i < clipboard->numServerFormats; i++)
			free(clipboard->serverFormats[i].formatName);

		free(clipboard->serverFormats);
		clipboard->serverFormats = NULL;

		clipboard->numServerFormats = 0;
	}

	clipboard->numServerFormats = formatList->numFormats;

	if (clipboard->numServerFormats)
	{
		if (!(clipboard->serverFormats = (CLIPRDR_FORMAT*) calloc(clipboard->numServerFormats, sizeof(CLIPRDR_FORMAT)))) {
			WLog_ERR(TAG, "failed to allocate %d CLIPRDR_FORMAT structs", clipboard->numServerFormats);
			return CHANNEL_RC_NO_MEMORY;
		}
	}

	for (i = 0; i < formatList->numFormats; i++)
	{
		format = &formatList->formats[i];
		clipboard->serverFormats[i].formatId = format->formatId;
		if (format->formatName)
		{
			clipboard->serverFormats[i].formatName = _strdup(format->formatName);
			if (!clipboard->serverFormats[i].formatName)
			{
				for (--i; i >= 0; --i)
					free(clipboard->serverFormats[i].formatName);

				clipboard->numServerFormats = 0;
				free(clipboard->serverFormats);
				clipboard->serverFormats = NULL;
				return CHANNEL_RC_NO_MEMORY;
			}
		}
	}

	clipboard->numTargets = 2;

	for (i = 0; i < formatList->numFormats; i++)
	{
		format = &formatList->formats[i];

		for (j = 0; j < clipboard->numClientFormats; j++)
		{
			if (format->formatId == clipboard->clientFormats[j].formatId)
			{
				xf_cliprdr_append_target(clipboard, clipboard->clientFormats[j].atom);
			}
		}
	}

	ret = xf_cliprdr_send_client_format_list_response(clipboard, TRUE);

	XSetSelectionOwner(xfc->display, clipboard->clipboard_atom, xfc->drawable, CurrentTime);

	XFlush(xfc->display);

	return ret;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT xf_cliprdr_server_format_list_response(CliprdrClientContext* context, CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse)
{
	//xfClipboard* clipboard = (xfClipboard*) context->custom;

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT xf_cliprdr_server_format_data_request(CliprdrClientContext* context, CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest)
{
	xfCliprdrFormat* format = NULL;
	UINT32 formatId = formatDataRequest->requestedFormatId;
	xfClipboard* clipboard = (xfClipboard*) context->custom;
	xfContext* xfc = clipboard->xfc;

	if (xf_cliprdr_is_self_owned(clipboard))
	{
		format = xf_cliprdr_get_format_by_id(clipboard, 0);

		XChangeProperty(xfc->display, xfc->drawable, clipboard->property_atom,
			XA_INTEGER, 32, PropModeReplace, (BYTE*) &formatId, 1);
	}
	else
		format = xf_cliprdr_get_format_by_id(clipboard, formatId);

	if (!format)
		return xf_cliprdr_send_data_response(clipboard, NULL, 0);

	clipboard->requestedFormatId = formatId;

	XConvertSelection(xfc->display, clipboard->clipboard_atom,
		format->atom, clipboard->property_atom, xfc->drawable, CurrentTime);

	XFlush(xfc->display);

	/* After this point, we expect a SelectionNotify event from the clipboard owner. */

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT xf_cliprdr_server_format_data_response(CliprdrClientContext* context, CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse)
{
	BOOL bSuccess;
	BYTE* pSrcData;
	BYTE* pDstData;
	UINT32 DstSize;
	UINT32 SrcSize;
	UINT32 formatId;
	UINT32 altFormatId;
	xfCliprdrFormat* format;
	BOOL nullTerminated = FALSE;
	UINT32 size = formatDataResponse->dataLen;
	BYTE* data = formatDataResponse->requestedFormatData;
	xfClipboard* clipboard = (xfClipboard*) context->custom;
	xfContext* xfc = clipboard->xfc;

	if (!clipboard->respond)
		return CHANNEL_RC_OK;

	format = xf_cliprdr_get_format_by_id(clipboard, clipboard->requestedFormatId);

	if (clipboard->data)
	{
		free(clipboard->data);
		clipboard->data = NULL;
	}

	pDstData = NULL;
	DstSize = 0;

	formatId = 0;
	altFormatId = 0;

	switch (clipboard->data_format)
	{
		case CF_TEXT:
			formatId = CF_TEXT;
			altFormatId = ClipboardGetFormatId(clipboard->system, "UTF8_STRING");
			nullTerminated = TRUE;
			break;

		case CF_OEMTEXT:
			formatId = CF_OEMTEXT;
			altFormatId = ClipboardGetFormatId(clipboard->system, "UTF8_STRING");
			nullTerminated = TRUE;
			break;

		case CF_UNICODETEXT:
			formatId = CF_UNICODETEXT;
			altFormatId = ClipboardGetFormatId(clipboard->system, "UTF8_STRING");
			nullTerminated = TRUE;
			break;

		case CF_DIB:
			formatId = CF_DIB;
			altFormatId = ClipboardGetFormatId(clipboard->system, "image/bmp");
			break;

		case CB_FORMAT_HTML:
			formatId = ClipboardGetFormatId(clipboard->system, "HTML Format");
			altFormatId = ClipboardGetFormatId(clipboard->system, "text/html");
			nullTerminated = TRUE;
			break;
	}

	SrcSize = (UINT32) size;
	pSrcData = (BYTE*) malloc(SrcSize);

	if (!pSrcData)
		return CHANNEL_RC_NO_MEMORY;

	CopyMemory(pSrcData, data, SrcSize);

	bSuccess = ClipboardSetData(clipboard->system, formatId, (void*) pSrcData, SrcSize);

	if (!bSuccess)
		free (pSrcData);

	if (bSuccess && altFormatId)
	{
		DstSize = 0;
		pDstData = (BYTE*) ClipboardGetData(clipboard->system, altFormatId, &DstSize);

		if ((DstSize > 1) && nullTerminated)
			DstSize--;
	}

	clipboard->data = pDstData;
	clipboard->data_length = DstSize;

	xf_cliprdr_provide_data(clipboard, clipboard->respond, pDstData, DstSize);

	XSendEvent(xfc->display, clipboard->respond->xselection.requestor, 0, 0, clipboard->respond);
	XFlush(xfc->display);

	free(clipboard->respond);
	clipboard->respond = NULL;

	return CHANNEL_RC_OK;
}

xfClipboard* xf_clipboard_new(xfContext* xfc)
{
	int n;
	rdpChannels* channels;
	xfClipboard* clipboard;

	if (!(clipboard = (xfClipboard*) calloc(1, sizeof(xfClipboard))))
	{
		WLog_ERR(TAG, "failed to allocate xfClipboard data");
		return NULL;
	}

	xfc->clipboard = clipboard;

	clipboard->xfc = xfc;

	channels = ((rdpContext*) xfc)->channels;
	clipboard->channels = channels;

	clipboard->system = ClipboardCreate();

	clipboard->requestedFormatId = -1;

	clipboard->root_window = DefaultRootWindow(xfc->display);
	clipboard->clipboard_atom = XInternAtom(xfc->display, "CLIPBOARD", FALSE);

	if (clipboard->clipboard_atom == None)
	{
		WLog_ERR(TAG, "unable to get CLIPBOARD atom");
		free(clipboard);
		return NULL;
	}

	clipboard->property_atom = XInternAtom(xfc->display, "_FREERDP_CLIPRDR", FALSE);

	XSelectInput(xfc->display, clipboard->root_window, PropertyChangeMask);

#ifdef WITH_XFIXES
	if (XFixesQueryExtension(xfc->display, &clipboard->xfixes_event_base, &clipboard->xfixes_error_base))
	{
		int xfmajor, xfminor;

		if (XFixesQueryVersion(xfc->display, &xfmajor, &xfminor))
		{
			XFixesSelectSelectionInput(xfc->display, clipboard->root_window,
				clipboard->clipboard_atom, XFixesSetSelectionOwnerNotifyMask);
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
	WLog_ERR(TAG, "Warning: Using clipboard redirection without XFIXES extension is strongly discouraged!");
#endif

	n = 0;

	clipboard->clientFormats[n].atom = XInternAtom(xfc->display, "_FREERDP_RAW", False);
	clipboard->clientFormats[n].formatId = 0;
	n++;

	clipboard->clientFormats[n].atom = XInternAtom(xfc->display, "UTF8_STRING", False);
	clipboard->clientFormats[n].formatId = CF_UNICODETEXT;
	n++;

	clipboard->clientFormats[n].atom = XA_STRING;
	clipboard->clientFormats[n].formatId = CF_TEXT;
	n++;

	clipboard->clientFormats[n].atom = XInternAtom(xfc->display, "image/png", False);
	clipboard->clientFormats[n].formatId = CB_FORMAT_PNG;
	n++;

	clipboard->clientFormats[n].atom = XInternAtom(xfc->display, "image/jpeg", False);
	clipboard->clientFormats[n].formatId = CB_FORMAT_JPEG;
	n++;

	clipboard->clientFormats[n].atom = XInternAtom(xfc->display, "image/gif", False);
	clipboard->clientFormats[n].formatId = CB_FORMAT_GIF;
	n++;

	clipboard->clientFormats[n].atom = XInternAtom(xfc->display, "image/bmp", False);
	clipboard->clientFormats[n].formatId = CF_DIB;
	n++;

	clipboard->clientFormats[n].atom = XInternAtom(xfc->display, "text/html", False);
	clipboard->clientFormats[n].formatId = CB_FORMAT_HTML;
	clipboard->clientFormats[n].formatName = _strdup("HTML Format");
	if (!clipboard->clientFormats[n].formatName)
	{
		ClipboardDestroy(clipboard->system);
		free(clipboard);
		return NULL;
	}
	n++;

	clipboard->numClientFormats = n;

	clipboard->targets[0] = XInternAtom(xfc->display, "TIMESTAMP", FALSE);
	clipboard->targets[1] = XInternAtom(xfc->display, "TARGETS", FALSE);
	clipboard->numTargets = 2;

	clipboard->incr_atom = XInternAtom(xfc->display, "INCR", FALSE);

	return clipboard;
}

void xf_clipboard_free(xfClipboard* clipboard)
{
	int i;

	if (!clipboard)
		return;

	if (clipboard->serverFormats)
	{
		for (i = 0; i < clipboard->numServerFormats; i++)
			free(clipboard->serverFormats[i].formatName);

		free(clipboard->serverFormats);
		clipboard->serverFormats = NULL;
	}

	if (clipboard->numClientFormats)
	{
		for (i = 0; i < clipboard->numClientFormats; i++)
			free(clipboard->clientFormats[i].formatName);
	}

	ClipboardDestroy(clipboard->system);

	free(clipboard->data);
	free(clipboard->respond);
	free(clipboard->incr_data);
	free(clipboard);
}

void xf_cliprdr_init(xfContext* xfc, CliprdrClientContext* cliprdr)
{
	xfc->cliprdr = cliprdr;
	xfc->clipboard->context = cliprdr;
	cliprdr->custom = (void*) xfc->clipboard;

	cliprdr->MonitorReady = xf_cliprdr_monitor_ready;
	cliprdr->ServerCapabilities = xf_cliprdr_server_capabilities;
	cliprdr->ServerFormatList = xf_cliprdr_server_format_list;
	cliprdr->ServerFormatListResponse = xf_cliprdr_server_format_list_response;
	cliprdr->ServerFormatDataRequest = xf_cliprdr_server_format_data_request;
	cliprdr->ServerFormatDataResponse = xf_cliprdr_server_format_data_response;
}

void xf_cliprdr_uninit(xfContext* xfc, CliprdrClientContext* cliprdr)
{
	xfc->cliprdr = NULL;
	cliprdr->custom = NULL;

	if (xfc->clipboard)
		xfc->clipboard->context = NULL;
}
