/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Clipboard Redirection
 *
 * Copyright 2010-2011 Vic Lee
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
#include <winpr/stream.h>

#include <freerdp/utils/event.h>
#include <freerdp/log.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/channels/channels.h>

#include "xf_cliprdr.h"

#define TAG CLIENT_TAG("x11")
#ifdef WITH_DEBUG_X11
#define DEBUG_X11(fmt, ...) WLog_DBG(TAG, fmt, ## __VA_ARGS__)
#else
#define DEBUG_X11(fmt, ...) do { } while (0)
#endif

#ifdef WITH_DEBUG_X11_CLIPRDR
#define DEBUG_X11_CLIPRDR(fmt, ...) WLog_DBG(TAG, fmt, ## __VA_ARGS__)
#else
#define DEBUG_X11_CLIPRDR(fmt, ...) do { } while (0)
#endif

struct xf_format_mapping
{
	Atom target_format;
	UINT32 format_id;
};
typedef struct xf_format_mapping xfFormatMapping;

struct xf_clipboard
{
	xfContext* xfc;
	rdpChannels* channels;
	CliprdrClientContext* context;

	UINT32 requestedFormatId;

	Window root_window;
	Atom clipboard_atom;
	Atom property_atom;
	Atom identity_atom;

	int num_format_mappings;
	xfFormatMapping format_mappings[20];

	/* server->client data */
	UINT32* formats;
	int num_formats;
	Atom targets[20];
	int num_targets;
	BYTE* data;
	UINT32 data_format;
	UINT32 data_alt_format;
	int data_length;
	XEvent* respond;

	/* client->server data */
	Window owner;
	int request_index;
	BOOL sync;

	/* INCR mechanism */
	Atom incr_atom;
	BOOL incr_starts;
	BYTE* incr_data;
	int incr_data_length;

	/* X Fixes extension */
	int xfixes_event_base;
	int xfixes_error_base;
	BOOL xfixes_supported;
};

static BYTE* lf2crlf(BYTE* data, int* size)
{
	BYTE c;
	BYTE* outbuf;
	BYTE* out;
	BYTE* in_end;
	BYTE* in;
	int out_size;

	out_size = (*size) * 2 + 1;
	outbuf = (BYTE*) malloc(out_size);
	ZeroMemory(outbuf, out_size);

	out = outbuf;
	in = data;
	in_end = data + (*size);

	while (in < in_end)
	{
		c = *in++;
		if (c == '\n')
		{
			*out++ = '\r';
			*out++ = '\n';
		}
		else
		{
			*out++ = c;
		}
	}

	*out++ = 0;
	*size = out - outbuf;

	return outbuf;
}

static void crlf2lf(BYTE* data, int* size)
{
	BYTE c;
	BYTE* out;
	BYTE* in;
	BYTE* in_end;

	out = data;
	in = data;
	in_end = data + (*size);

	while (in < in_end)
	{
		c = *in++;

		if (c != '\r')
			*out++ = c;
	}

	*size = out - data;
}

static void be2le(BYTE* data, int size)
{
	BYTE c;

	while (size >= 2)
	{
		c = data[0];
		data[0] = data[1];
		data[1] = c;

		data += 2;
		size -= 2;
	}
}

static BOOL xf_cliprdr_is_self_owned(xfClipboard* clipboard)
{
	Atom type;
	UINT32 id = 0;
	UINT32* pid = NULL;
	int format, result = 0;
	unsigned long length, bytes_left;
	xfContext* xfc = clipboard->xfc;

	clipboard->owner = XGetSelectionOwner(xfc->display, clipboard->clipboard_atom);

	if (clipboard->owner != None)
	{
		result = XGetWindowProperty(xfc->display, clipboard->owner,
			clipboard->identity_atom, 0, 4, 0, XA_INTEGER,
			&type, &format, &length, &bytes_left, (BYTE**) &pid);
	}

	if (pid)
	{
		id = *pid;
		XFree(pid);
	}

	if ((clipboard->owner == None) || (clipboard->owner == xfc->drawable))
		return FALSE;

	if (result != Success)
		return FALSE;

	return (id ? TRUE : FALSE);
}

static int xf_cliprdr_select_format_by_id(xfClipboard* clipboard, UINT32 format_id)
{
	int i;

	for (i = 0; i < clipboard->num_format_mappings; i++)
	{
		if (clipboard->format_mappings[i].format_id == format_id)
			return i;
	}

	return -1;
}

static int xf_cliprdr_select_format_by_atom(xfClipboard* clipboard, Atom target)
{
	int i;
	int j;

	if (clipboard->formats == NULL)
		return -1;

	for (i = 0; i < clipboard->num_format_mappings; i++)
	{
		if (clipboard->format_mappings[i].target_format != target)
			continue;

		if (clipboard->format_mappings[i].format_id == CB_FORMAT_RAW)
			return i;

		for (j = 0; j < clipboard->num_formats; j++)
		{
			if (clipboard->format_mappings[i].format_id == clipboard->formats[j])
				return i;
		}
	}

	return -1;
}

static void xf_cliprdr_send_raw_format_list(xfClipboard* clipboard)
{
	Atom type;
	BYTE* format_data;
	int format, result;
	unsigned long length, bytes_left;
	RDP_CB_FORMAT_LIST_EVENT* event;
	xfContext* xfc = clipboard->xfc;

	result = XGetWindowProperty(xfc->display, clipboard->root_window,
		clipboard->property_atom, 0, 3600, 0, XA_STRING,
		&type, &format, &length, &bytes_left, (BYTE**) &format_data);

	if (result != Success)
	{
		WLog_ERR(TAG, "XGetWindowProperty failed");
		return;
	}
	DEBUG_X11_CLIPRDR("format=%d len=%d bytes_left=%d", format, (int) length, (int) bytes_left);

	event = (RDP_CB_FORMAT_LIST_EVENT*) freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_FormatList, NULL, NULL);

	event->raw_format_data = (BYTE*) malloc(length);
	CopyMemory(event->raw_format_data, format_data, length);
	event->raw_format_data_size = length;
	XFree(format_data);

	freerdp_channels_send_event(clipboard->channels, (wMessage*) event);
}

static void xf_cliprdr_send_null_format_list(xfClipboard* clipboard)
{
	RDP_CB_FORMAT_LIST_EVENT* event;

	event = (RDP_CB_FORMAT_LIST_EVENT*) freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_FormatList, NULL, NULL);

	event->num_formats = 0;

	freerdp_channels_send_event(clipboard->channels, (wMessage*) event);
}

static void xf_cliprdr_send_supported_format_list(xfClipboard* clipboard)
{
	int i;
	RDP_CB_FORMAT_LIST_EVENT* event;

	event = (RDP_CB_FORMAT_LIST_EVENT*) freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_FormatList, NULL, NULL);

	event->formats = (UINT32*) malloc(sizeof(UINT32) * clipboard->num_format_mappings);
	event->num_formats = clipboard->num_format_mappings;

	for (i = 0; i < clipboard->num_format_mappings; i++)
		event->formats[i] = clipboard->format_mappings[i].format_id;

	freerdp_channels_send_event(clipboard->channels, (wMessage*) event);
}

static void xf_cliprdr_send_format_list(xfClipboard* clipboard)
{
	xfContext* xfc = clipboard->xfc;

	if (xf_cliprdr_is_self_owned(clipboard))
	{
		xf_cliprdr_send_raw_format_list(clipboard);
	}
	else if (clipboard->owner == None)
	{
		xf_cliprdr_send_null_format_list(clipboard);
	}
	else if (clipboard->owner != xfc->drawable)
	{
		/* Request the owner for TARGETS, and wait for SelectionNotify event */
		XConvertSelection(xfc->display, clipboard->clipboard_atom,
			clipboard->targets[1], clipboard->property_atom, xfc->drawable, CurrentTime);
	}
}

static void xf_cliprdr_send_data_request(xfClipboard* clipboard, UINT32 format)
{
	RDP_CB_DATA_REQUEST_EVENT* event;

	event = (RDP_CB_DATA_REQUEST_EVENT*) freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_DataRequest, NULL, NULL);

	event->format = format;

	freerdp_channels_send_event(clipboard->channels, (wMessage*) event);
}

static void xf_cliprdr_send_data_response(xfClipboard* clipboard, BYTE* data, int size)
{
	RDP_CB_DATA_RESPONSE_EVENT* event;

	event = (RDP_CB_DATA_RESPONSE_EVENT*) freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_DataResponse, NULL, NULL);

	event->data = data;
	event->size = size;

	freerdp_channels_send_event(clipboard->channels, (wMessage*) event);
}

static void xf_cliprdr_send_null_data_response(xfClipboard* clipboard)
{
	xf_cliprdr_send_data_response(clipboard, NULL, 0);
}

static void xf_cliprdr_process_cb_monitor_ready_event(xfClipboard* clipboard)
{
	xf_cliprdr_send_format_list(clipboard);
	clipboard->sync = TRUE;
}

static void xf_cliprdr_process_cb_data_request_event(xfClipboard* clipboard, RDP_CB_DATA_REQUEST_EVENT* event)
{
	int i;
	xfContext* xfc = clipboard->xfc;

	DEBUG_X11_CLIPRDR("format %d", event->format);

	if (xf_cliprdr_is_self_owned(clipboard))
	{
		/* CB_FORMAT_RAW */
		i = 0;
		XChangeProperty(xfc->display, xfc->drawable, clipboard->property_atom,
			XA_INTEGER, 32, PropModeReplace, (BYTE*) &event->format, 1);
	}
	else
	{
		i = xf_cliprdr_select_format_by_id(clipboard, event->format);
	}

	if (i < 0)
	{
		DEBUG_X11_CLIPRDR("unsupported format requested");
		xf_cliprdr_send_null_data_response(clipboard);
	}
	else
	{
		clipboard->request_index = i;

		DEBUG_X11_CLIPRDR("target=%d", (int) clipboard->format_mappings[i].target_format);

		XConvertSelection(xfc->display, clipboard->clipboard_atom,
			clipboard->format_mappings[i].target_format, clipboard->property_atom,
			xfc->drawable, CurrentTime);
		XFlush(xfc->display);
		/* After this point, we expect a SelectionNotify event from the clipboard owner. */
	}
}

static void xf_cliprdr_get_requested_targets(xfClipboard* clipboard)
{
	int num;
	int i, j;
	Atom atom;
	int format;
	BYTE* data = NULL;
	unsigned long length, bytes_left;
	RDP_CB_FORMAT_LIST_EVENT* event;
	xfContext* xfc = clipboard->xfc;

	XGetWindowProperty(xfc->display, xfc->drawable, clipboard->property_atom,
		0, 200, 0, XA_ATOM, &atom, &format, &length, &bytes_left, &data);

	DEBUG_X11_CLIPRDR("type=%d format=%d length=%d bytes_left=%d",
		(int) atom, format, (int) length, (int) bytes_left);

	if (length > 0)
	{
		event = (RDP_CB_FORMAT_LIST_EVENT*) freerdp_event_new(CliprdrChannel_Class,
				CliprdrChannel_FormatList, NULL, NULL);

		event->formats = (UINT32*) malloc(sizeof(UINT32) * clipboard->num_format_mappings);
		num = 0;

		for (i = 0; i < length; i++)
		{
			atom = ((Atom*) data)[i];
			DEBUG_X11("atom %d", (int) atom);

			for (j = 0; j < clipboard->num_format_mappings; j++)
			{
				if (clipboard->format_mappings[j].target_format == atom)
				{
					DEBUG_X11("found format %d for atom %d",
						clipboard->format_mappings[j].format_id, (int)atom);
					event->formats[num++] = clipboard->format_mappings[j].format_id;
					break;
				}
			}
		}

		event->num_formats = num;
		XFree(data);

		freerdp_channels_send_event(clipboard->channels, (wMessage*) event);
	}
	else
	{
		if (data)
			XFree(data);

		xf_cliprdr_send_null_format_list(clipboard);
	}
}

static BYTE* xf_cliprdr_process_requested_raw(BYTE* data, int* size)
{
	BYTE* outbuf;

	outbuf = (BYTE*) malloc(*size);
	CopyMemory(outbuf, data, *size);
	return outbuf;
}

static BYTE* xf_cliprdr_process_requested_unicodetext(BYTE* data, int* size)
{
	char* inbuf;
	WCHAR* outbuf = NULL;
	int out_size;

	inbuf = (char*) lf2crlf(data, size);
	out_size = ConvertToUnicode(CP_UTF8, 0, inbuf, -1, &outbuf, 0);
	free(inbuf);

	*size = (int) ((out_size + 1) * 2);

	return (BYTE*) outbuf;
}

static BYTE* xf_cliprdr_process_requested_text(BYTE* data, int* size)
{
	BYTE* outbuf;

	outbuf = lf2crlf(data, size);

	return outbuf;
}

static BYTE* xf_cliprdr_process_requested_dib(BYTE* data, int* size)
{
	BYTE* outbuf;

	/* length should be at least BMP header (14) + sizeof(BITMAPINFOHEADER) */

	if (*size < 54)
	{
		DEBUG_X11_CLIPRDR("bmp length %d too short", *size);
		return NULL;
	}

	*size -= 14;
	outbuf = (BYTE*) calloc(1, *size);
	CopyMemory(outbuf, data + 14, *size);

	return outbuf;
}

static BYTE* xf_cliprdr_process_requested_html(BYTE* data, int* size)
{
	char* inbuf;
	BYTE* in;
	BYTE* outbuf;
	char num[11];

	inbuf = NULL;

	if (*size > 2)
	{
		if ((BYTE) data[0] == 0xFE && (BYTE) data[1] == 0xFF)
		{
			be2le(data, *size);
		}

		if ((BYTE) data[0] == 0xFF && (BYTE) data[1] == 0xFE)
		{
			ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) (data + 2),
					(*size - 2) / 2, &inbuf, 0, NULL, NULL);
		}
	}

	if (inbuf == NULL)
	{
		inbuf = calloc(1, *size + 1);
		CopyMemory(inbuf, data, *size);
	}

	outbuf = (BYTE*) calloc(1, *size + 200);

	strcpy((char*) outbuf,
		"Version:0.9\r\n"
		"StartHTML:0000000000\r\n"
		"EndHTML:0000000000\r\n"
		"StartFragment:0000000000\r\n"
		"EndFragment:0000000000\r\n");

	in = (BYTE*) strstr((char*) inbuf, "<body");

	if (in == NULL)
	{
		in = (BYTE*) strstr((char*) inbuf, "<BODY");
	}
	/* StartHTML */
	snprintf(num, sizeof(num), "%010lu", (unsigned long) strlen((char*) outbuf));
	CopyMemory(outbuf + 23, num, 10);
	if (in == NULL)
	{
		strcat((char*) outbuf, "<HTML><BODY>");
	}
	strcat((char*) outbuf, "<!--StartFragment-->");
	/* StartFragment */
	snprintf(num, sizeof(num), "%010lu", (unsigned long) strlen((char*) outbuf));
	CopyMemory(outbuf + 69, num, 10);
	strcat((char*) outbuf, (char*) inbuf);
	/* EndFragment */
	snprintf(num, sizeof(num), "%010lu", (unsigned long) strlen((char*) outbuf));
	CopyMemory(outbuf + 93, num, 10);
	strcat((char*) outbuf, "<!--EndFragment-->");
	if (in == NULL)
	{
		strcat((char*) outbuf, "</BODY></HTML>");
	}
	/* EndHTML */
	snprintf(num, sizeof(num), "%010lu", (unsigned long) strlen((char*) outbuf));
	CopyMemory(outbuf + 43, num, 10);

	*size = strlen((char*) outbuf) + 1;
	free(inbuf);

	return outbuf;
}

static void xf_cliprdr_process_requested_data(xfClipboard* clipboard, BOOL has_data, BYTE* data, int size)
{
	BYTE* outbuf;

	if (clipboard->incr_starts && has_data)
		return;

	if (!has_data || !data)
	{
		xf_cliprdr_send_null_data_response(clipboard);
		return;
	}

	switch (clipboard->format_mappings[clipboard->request_index].format_id)
	{
		case CB_FORMAT_RAW:
		case CB_FORMAT_PNG:
		case CB_FORMAT_JPEG:
		case CB_FORMAT_GIF:
			outbuf = xf_cliprdr_process_requested_raw(data, &size);
			break;

		case CB_FORMAT_UNICODETEXT:
			outbuf = xf_cliprdr_process_requested_unicodetext(data, &size);
			break;

		case CB_FORMAT_TEXT:
			outbuf = xf_cliprdr_process_requested_text(data, &size);
			break;

		case CB_FORMAT_DIB:
			outbuf = xf_cliprdr_process_requested_dib(data, &size);
			break;

		case CB_FORMAT_HTML:
			outbuf = xf_cliprdr_process_requested_html(data, &size);
			break;

		default:
			outbuf = NULL;
			break;
	}

	if (outbuf)
		xf_cliprdr_send_data_response(clipboard, outbuf, size);
	else
		xf_cliprdr_send_null_data_response(clipboard);

	if (!clipboard->xfixes_supported)
	{
		/* Resend the format list, otherwise the server won't request again for the next paste */
		xf_cliprdr_send_format_list(clipboard);
	}
}

static BOOL xf_cliprdr_get_requested_data(xfClipboard* clipboard, Atom target)
{
	Atom type;
	int format;
	BYTE* data = NULL;
	BOOL has_data = FALSE;
	unsigned long length, bytes_left, dummy;
	xfContext* xfc = clipboard->xfc;

	if ((clipboard->request_index < 0) || (clipboard->format_mappings[clipboard->request_index].target_format != target))
	{
		DEBUG_X11_CLIPRDR("invalid target");
		xf_cliprdr_send_null_data_response(clipboard);
		return FALSE;
	}

	XGetWindowProperty(xfc->display, xfc->drawable,
		clipboard->property_atom, 0, 0, 0, target,
		&type, &format, &length, &bytes_left, &data);

	DEBUG_X11_CLIPRDR("type=%d format=%d bytes=%d request_index=%d",
		(int) type, format, (int) bytes_left, clipboard->request_index);

	if (data)
	{
		XFree(data);
		data = NULL;
	}
	if (bytes_left <= 0 && !clipboard->incr_starts)
	{
		DEBUG_X11("no data");
	}
	else if (type == clipboard->incr_atom)
	{
		DEBUG_X11("INCR started");
		clipboard->incr_starts = TRUE;
		if (clipboard->incr_data)
		{
			free(clipboard->incr_data);
			clipboard->incr_data = NULL;
		}
		clipboard->incr_data_length = 0;
		/* Data will be followed in PropertyNotify event */
		has_data = TRUE;
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
			DEBUG_X11("INCR finished");
			has_data = TRUE;
		}
		else if (XGetWindowProperty(xfc->display, xfc->drawable,
			clipboard->property_atom, 0, bytes_left, 0, target,
			&type, &format, &length, &dummy, &data) == Success)
		{
			if (clipboard->incr_starts)
			{
				bytes_left = length * format / 8;
				DEBUG_X11("%d bytes", (int)bytes_left);
				clipboard->incr_data = (BYTE*) realloc(clipboard->incr_data, clipboard->incr_data_length + bytes_left);
				CopyMemory(clipboard->incr_data + clipboard->incr_data_length, data, bytes_left);
				clipboard->incr_data_length += bytes_left;
				XFree(data);
				data = NULL;
			}
			has_data = TRUE;
		}
		else
		{
			DEBUG_X11_CLIPRDR("XGetWindowProperty failed");
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

	if (clipboard->num_targets >= ARRAYSIZE(clipboard->targets))
		return;

	for (i = 0; i < clipboard->num_targets; i++)
	{
		if (clipboard->targets[i] == target)
			return;
	}

	clipboard->targets[clipboard->num_targets++] = target;
}

static void xf_cliprdr_provide_targets(xfClipboard* clipboard, XEvent* respond)
{
	xfContext* xfc = clipboard->xfc;

	if (respond->xselection.property != None)
	{
		XChangeProperty(xfc->display,
			respond->xselection.requestor,
			respond->xselection.property,
			XA_ATOM, 32, PropModeReplace,
			(BYTE*) clipboard->targets, clipboard->num_targets);
	}
}

static void xf_cliprdr_provide_data(xfClipboard* clipboard, XEvent* respond)
{
	xfContext* xfc = clipboard->xfc;

	if (respond->xselection.property != None)
	{
		XChangeProperty(xfc->display,
			respond->xselection.requestor,
			respond->xselection.property,
			respond->xselection.target, 8, PropModeReplace,
			(BYTE*) clipboard->data, clipboard->data_length);
	}
}

static void xf_cliprdr_process_cb_format_list_event(xfClipboard* clipboard, RDP_CB_FORMAT_LIST_EVENT* event)
{
	int i, j;
	xfContext* xfc = clipboard->xfc;

	if (clipboard->data)
	{
		free(clipboard->data);
		clipboard->data = NULL;
	}

	if (clipboard->formats)
		free(clipboard->formats);

	clipboard->formats = event->formats;
	clipboard->num_formats = event->num_formats;
	event->formats = NULL;
	event->num_formats = 0;

	clipboard->num_targets = 2;

	for (i = 0; i < clipboard->num_formats; i++)
	{
		for (j = 0; j < clipboard->num_format_mappings; j++)
		{
			if (clipboard->formats[i] == clipboard->format_mappings[j].format_id)
			{
				DEBUG_X11("announce format#%d : %d", i, clipboard->formats[i]);
				xf_cliprdr_append_target(clipboard, clipboard->format_mappings[j].target_format);
			}
		}
	}

	XSetSelectionOwner(xfc->display, clipboard->clipboard_atom, xfc->drawable, CurrentTime);

	if (event->raw_format_data)
	{
		XChangeProperty(xfc->display, clipboard->root_window, clipboard->property_atom,
			XA_STRING, 8, PropModeReplace,
			event->raw_format_data, event->raw_format_data_size);
	}

	XFlush(xfc->display);
}

static void xf_cliprdr_process_text(xfClipboard* clipboard, BYTE* data, int size)
{
	clipboard->data = (BYTE*) malloc(size);
	CopyMemory(clipboard->data, data, size);
	clipboard->data_length = size;
	crlf2lf(clipboard->data, &clipboard->data_length);
}

static void xf_cliprdr_process_unicodetext(xfClipboard* clipboard, BYTE* data, int size)
{
	clipboard->data_length = ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) data, size / 2, (CHAR**) &(clipboard->data), 0, NULL, NULL);
	crlf2lf(clipboard->data, &clipboard->data_length);
}

static BOOL xf_cliprdr_process_dib(xfClipboard* clipboard, BYTE* data, int size)
{
	wStream* s;
	UINT16 bpp;
	UINT32 offset;
	UINT32 ncolors;

	/* size should be at least sizeof(BITMAPINFOHEADER) */

	if (size < 40)
	{
		DEBUG_X11_CLIPRDR("dib size %d too short", size);
		return FALSE;
	}

	s = Stream_New(data, size);
	Stream_Seek(s, 14);
	Stream_Read_UINT16(s, bpp);
	if ((bpp < 1) || (bpp > 32))
	{
		WLog_ERR(TAG, "invalid bpp value %d", bpp);
		return FALSE;
	}

	Stream_Read_UINT32(s, ncolors);
	offset = 14 + 40 + (bpp <= 8 ? (ncolors == 0 ? (1 << bpp) : ncolors) * 4 : 0);
	Stream_Free(s, FALSE);

	DEBUG_X11_CLIPRDR("offset=%d bpp=%d ncolors=%d", offset, bpp, ncolors);

	s = Stream_New(NULL, 14 + size);
	Stream_Write_UINT8(s, 'B');
	Stream_Write_UINT8(s, 'M');
	Stream_Write_UINT32(s, 14 + size);
	Stream_Write_UINT32(s, 0);
	Stream_Write_UINT32(s, offset);
	Stream_Write(s, data, size);

	clipboard->data = Stream_Buffer(s);
	clipboard->data_length = Stream_GetPosition(s);
	Stream_Free(s, FALSE);
	return TRUE;
}

static void xf_cliprdr_process_html(xfClipboard* clipboard, BYTE* data, int size)
{
	char* start_str;
	char* end_str;
	int start;
	int end;

	start_str = strstr((char*) data, "StartHTML:");
	end_str = strstr((char*) data, "EndHTML:");
	if (start_str == NULL || end_str == NULL)
	{
		DEBUG_X11_CLIPRDR("invalid HTML clipboard format");
		return;
	}
	start = atoi(start_str + 10);
	end = atoi(end_str + 8);
	if (start > size || end > size || start >= end)
	{
		DEBUG_X11_CLIPRDR("invalid HTML offset");
		return;
	}

	clipboard->data = (BYTE*) malloc(size - start + 1);
	CopyMemory(clipboard->data, data + start, end - start);
	clipboard->data_length = end - start;
	crlf2lf(clipboard->data, &clipboard->data_length);
}

static void xf_cliprdr_process_cb_data_response_event(xfClipboard* clipboard, RDP_CB_DATA_RESPONSE_EVENT* event)
{
	xfContext* xfc = clipboard->xfc;

	DEBUG_X11_CLIPRDR("size=%d", event->size);

	if (!clipboard->respond)
	{
		DEBUG_X11_CLIPRDR("unexpected data");
		return;
	}

	if (event->size == 0)
	{
		clipboard->respond->xselection.property = None;
	}
	else
	{
		if (clipboard->data)
		{
			free(clipboard->data);
			clipboard->data = NULL;
		}

		switch (clipboard->data_format)
		{
			case CB_FORMAT_RAW:
			case CB_FORMAT_PNG:
			case CB_FORMAT_JPEG:
			case CB_FORMAT_GIF:
				clipboard->data = event->data;
				clipboard->data_length = event->size;
				event->data = NULL;
				event->size = 0;
				break;

			case CB_FORMAT_TEXT:
				xf_cliprdr_process_text(clipboard, event->data, event->size);
				break;

			case CB_FORMAT_UNICODETEXT:
				xf_cliprdr_process_unicodetext(clipboard, event->data, event->size);
				break;

			case CB_FORMAT_DIB:
				xf_cliprdr_process_dib(clipboard, event->data, event->size);
				break;

			case CB_FORMAT_HTML:
				xf_cliprdr_process_html(clipboard, event->data, event->size);
				break;

			default:
				clipboard->respond->xselection.property = None;
				break;
		}
		xf_cliprdr_provide_data(clipboard, clipboard->respond);
	}

	XSendEvent(xfc->display, clipboard->respond->xselection.requestor, 0, 0, clipboard->respond);
	XFlush(xfc->display);
	free(clipboard->respond);
	clipboard->respond = NULL;
}

void xf_process_cliprdr_event(xfContext* xfc, wMessage* event)
{
	switch (GetMessageType(event->id))
	{
		case CliprdrChannel_MonitorReady:
			xf_cliprdr_process_cb_monitor_ready_event(xfc->clipboard);
			break;

		case CliprdrChannel_FormatList:
			xf_cliprdr_process_cb_format_list_event(xfc->clipboard, (RDP_CB_FORMAT_LIST_EVENT*) event);
			break;

		case CliprdrChannel_DataRequest:
			xf_cliprdr_process_cb_data_request_event(xfc->clipboard, (RDP_CB_DATA_REQUEST_EVENT*) event);
			break;

		case CliprdrChannel_DataResponse:
			xf_cliprdr_process_cb_data_response_event(xfc->clipboard, (RDP_CB_DATA_RESPONSE_EVENT*) event);
			break;

		default:
			DEBUG_X11_CLIPRDR("unknown event type %d", GetMessageType(event->id));
			break;
	}
}

static BOOL xf_cliprdr_process_selection_notify(xfClipboard* clipboard, XEvent* xevent)
{
	if (xevent->xselection.target == clipboard->targets[1])
	{
		if (xevent->xselection.property == None)
		{
			DEBUG_X11_CLIPRDR("owner not support TARGETS. sending all format.");
			xf_cliprdr_send_supported_format_list(clipboard);
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
	int i;
	int fmt;
	Atom type;
	UINT32 format;
	XEvent* respond;
	UINT32 alt_format;
	BYTE* data = NULL;
	BOOL delay_respond;
	unsigned long length, bytes_left;
	xfContext* xfc = clipboard->xfc;

	DEBUG_X11_CLIPRDR("target=%d", (int) xevent->xselectionrequest.target);

	if (xevent->xselectionrequest.owner != xfc->drawable)
	{
		DEBUG_X11_CLIPRDR("not owner");
		return FALSE;
	}

	delay_respond = FALSE;

	respond = (XEvent*) calloc(1, sizeof(XEvent));

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
		DEBUG_X11_CLIPRDR("target: TIMESTAMP (unimplemented)");
	}
	else if (xevent->xselectionrequest.target == clipboard->targets[1]) /* TARGETS */
	{
		/* Someone else requests our available formats */
		DEBUG_X11_CLIPRDR("target: TARGETS");
		respond->xselection.property = xevent->xselectionrequest.property;
		xf_cliprdr_provide_targets(clipboard, respond);
	}
	else
	{
		DEBUG_X11_CLIPRDR("target: other");

		i = xf_cliprdr_select_format_by_atom(clipboard, xevent->xselectionrequest.target);

		if (i >= 0 && xevent->xselectionrequest.requestor != xfc->drawable)
		{
			format = clipboard->format_mappings[i].format_id;
			alt_format = format;

			if (format == CB_FORMAT_RAW)
			{
				if (XGetWindowProperty(xfc->display, xevent->xselectionrequest.requestor,
					clipboard->property_atom, 0, 4, 0, XA_INTEGER,
					&type, &fmt, &length, &bytes_left, &data) != Success)
				{
					DEBUG_X11_CLIPRDR("XGetWindowProperty failed");
				}
				if (data)
				{
					CopyMemory(&alt_format, data, 4);
					XFree(data);
				}
			}

			DEBUG_X11_CLIPRDR("provide format 0x%04x alt_format 0x%04x", format, alt_format);

			if ((clipboard->data != 0) && (format == clipboard->data_format) && (alt_format == clipboard->data_alt_format))
			{
				/* Cached clipboard data available. Send it now */
				respond->xselection.property = xevent->xselectionrequest.property;
				xf_cliprdr_provide_data(clipboard, respond);
			}
			else if (clipboard->respond)
			{
				DEBUG_X11_CLIPRDR("duplicated request");
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
				clipboard->data_format = format;
				clipboard->data_alt_format = alt_format;
				delay_respond = TRUE;

				xf_cliprdr_send_data_request(clipboard, alt_format);
			}
		}
	}

	if (delay_respond == FALSE)
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
	xfContext* xfc = clipboard->xfc;

	if (!clipboard)
		return TRUE;

	if (xevent->xproperty.atom != clipboard->property_atom)
		return FALSE; /* Not cliprdr-related */

	if (xevent->xproperty.window == clipboard->root_window)
	{
		DEBUG_X11_CLIPRDR("root window PropertyNotify");
		xf_cliprdr_send_format_list(clipboard);
	}
	else if (xevent->xproperty.window == xfc->drawable &&
		xevent->xproperty.state == PropertyNewValue &&
		clipboard->incr_starts && clipboard->request_index >= 0)
	{
		DEBUG_X11_CLIPRDR("cliprdr window PropertyNotify");
		xf_cliprdr_get_requested_data(clipboard,
			clipboard->format_mappings[clipboard->request_index].target_format);
	}

	return TRUE;
}

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
			xf_cliprdr_send_format_list(clipboard);
		}
	}
}

void xf_cliprdr_handle_xevent(xfContext* xfc, XEvent* event)
{
	xfClipboard* clipboard;

	if (!xfc || !event)
		return;

	if (!(clipboard = (xfClipboard*) xfc->clipboard))
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
			xf_cliprdr_check_owner(xfc->clipboard);
		}

		return;
	}
#endif

	switch (event->type)
	{
		case SelectionNotify:
			xf_cliprdr_process_selection_notify(xfc->clipboard, event);
			break;

		case SelectionRequest:
			xf_cliprdr_process_selection_request(xfc->clipboard, event);
			break;

		case SelectionClear:
			xf_cliprdr_process_selection_clear(xfc->clipboard, event);
			break;

		case PropertyNotify:
			xf_cliprdr_process_property_notify(xfc->clipboard, event);
			break;

		case FocusIn:
			if (!clipboard->xfixes_supported)
			{
				xf_cliprdr_check_owner(xfc->clipboard);
			}
			break;
	}
}

int xf_cliprdr_send_client_capabilities(xfClipboard* clipboard)
{
	CLIPRDR_CAPABILITIES capabilities;
	CLIPRDR_GENERAL_CAPABILITY_SET generalCapabilitySet;

	capabilities.cCapabilitiesSets = 1;
	capabilities.capabilitySets = (CLIPRDR_CAPABILITY_SET*) &(generalCapabilitySet);

	generalCapabilitySet.capabilitySetType = CB_CAPSTYPE_GENERAL;
	generalCapabilitySet.capabilitySetLength = 12;

	generalCapabilitySet.version = CB_CAPS_VERSION_2;
	generalCapabilitySet.generalFlags = CB_USE_LONG_FORMAT_NAMES;

	clipboard->context->ClientCapabilities(clipboard->context, &capabilities);

	return 1;
}

int xf_cliprdr_send_client_format_list(xfClipboard* clipboard)
{
	int index;
	CLIPRDR_FORMAT* formats;
	CLIPRDR_FORMAT_LIST formatList;

	formatList.cFormats = 3;

	formats = (CLIPRDR_FORMAT*) calloc(formatList.cFormats, sizeof(CLIPRDR_FORMAT));
	formatList.formats = formats;

	index = 0;

	formats[index].formatId = CLIPRDR_FORMAT_UNICODETEXT;
	formats[index].formatName = NULL;
	index++;

	formats[index].formatId = CLIPRDR_FORMAT_TEXT;
	formats[index].formatName = NULL;
	index++;

	formats[index].formatId = CLIPRDR_FORMAT_OEMTEXT;
	formats[index].formatName = NULL;
	index++;

	clipboard->context->ClientFormatList(clipboard->context, &formatList);

	for (index = 0; index < formatList.cFormats; index++)
		free(formats[index].formatName);

	free(formats);

	return 1;
}

int xf_cliprdr_send_client_format_list_response(xfClipboard* clipboard, BOOL status)
{
	CLIPRDR_FORMAT_LIST_RESPONSE formatListResponse;

	formatListResponse.msgType = CB_FORMAT_LIST_RESPONSE;
	formatListResponse.msgFlags = status ? CB_RESPONSE_OK : CB_RESPONSE_FAIL;
	formatListResponse.dataLen = 0;

	clipboard->context->ClientFormatListResponse(clipboard->context, &formatListResponse);

	return 1;
}

int xf_cliprdr_send_client_format_data_request(xfClipboard* clipboard, UINT32 formatId)
{
	CLIPRDR_FORMAT_DATA_REQUEST formatDataRequest;

	formatDataRequest.msgType = CB_FORMAT_DATA_REQUEST;
	formatDataRequest.msgFlags = CB_RESPONSE_OK;

	formatDataRequest.requestedFormatId = formatId;
	clipboard->requestedFormatId = formatId;

	clipboard->context->ClientFormatDataRequest(clipboard->context, &formatDataRequest);

	return 1;
}

int xf_cliprdr_recv_server_format_data_response(xfClipboard* clipboard, UINT32 formatId, BYTE* data, UINT32 length)
{
	if (formatId == CLIPRDR_FORMAT_UNICODETEXT)
	{

	}

	return 1;
}

int xf_cliprdr_send_client_format_data_response(xfClipboard* clipboard, UINT32 formatId)
{
	CLIPRDR_FORMAT_DATA_RESPONSE formatDataResponse;

	formatDataResponse.msgType = CB_FORMAT_DATA_RESPONSE;
	formatDataResponse.msgFlags = CB_RESPONSE_FAIL;
	formatDataResponse.dataLen = 0;

	if (formatId == CLIPRDR_FORMAT_UNICODETEXT)
	{
		WCHAR* unicodeText = NULL;

		if (!unicodeText)
		{
			clipboard->context->ClientFormatDataResponse(clipboard->context, &formatDataResponse);
		}
		else
		{
			formatDataResponse.dataLen = (_wcslen(unicodeText) + 1) * 2;
			formatDataResponse.requestedFormatData = (BYTE*) unicodeText;

			formatDataResponse.msgFlags = CB_RESPONSE_OK;
			clipboard->context->ClientFormatDataResponse(clipboard->context, &formatDataResponse);
		}
	}
	else
	{
		clipboard->context->ClientFormatDataResponse(clipboard->context, &formatDataResponse);
	}

	return 0;
}

static int xf_cliprdr_monitor_ready(CliprdrClientContext* context, CLIPRDR_MONITOR_READY* monitorReady)
{
	xfClipboard* clipboard = (xfClipboard*) context->custom;

	xf_cliprdr_send_client_capabilities(clipboard);
	xf_cliprdr_send_client_format_list(clipboard);

	return 1;
}

static int xf_cliprdr_server_capabilities(CliprdrClientContext* context, CLIPRDR_CAPABILITIES* capabilities)
{
	//xfClipboard* clipboard = (xfClipboard*) context->custom;

	return 1;
}

static int xf_cliprdr_server_format_list(CliprdrClientContext* context, CLIPRDR_FORMAT_LIST* formatList)
{
	UINT32 index;
	CLIPRDR_FORMAT* format;
	xfClipboard* clipboard = (xfClipboard*) context->custom;

	xf_cliprdr_send_client_format_list_response(clipboard, TRUE);

	for (index = 0; index < formatList->cFormats; index++)
	{
		format = &formatList->formats[index];

		if (format->formatId == CLIPRDR_FORMAT_UNICODETEXT)
		{
			xf_cliprdr_send_client_format_data_request(clipboard, format->formatId);
		}
	}

	return 1;
}

static int xf_cliprdr_server_format_list_response(CliprdrClientContext* context, CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse)
{
	//xfClipboard* clipboard = (xfClipboard*) context->custom;

	return 1;
}

static int xf_cliprdr_server_format_data_request(CliprdrClientContext* context, CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest)
{
	xfClipboard* clipboard = (xfClipboard*) context->custom;

	xf_cliprdr_send_client_format_data_response(clipboard, formatDataRequest->requestedFormatId);

	return 1;
}

static int xf_cliprdr_server_format_data_response(CliprdrClientContext* context, CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse)
{
	xfClipboard* clipboard = (xfClipboard*) context->custom;

	xf_cliprdr_recv_server_format_data_response(clipboard, clipboard->requestedFormatId,
			formatDataResponse->requestedFormatData, formatDataResponse->dataLen);

	return 1;
}

xfClipboard* xf_clipboard_new(xfContext* xfc)
{
	int n;
	UINT32 id;
	rdpChannels* channels;
	xfClipboard* clipboard;

	clipboard = (xfClipboard*) calloc(1, sizeof(xfClipboard));

	xfc->clipboard = clipboard;

	clipboard->xfc = xfc;

	channels = ((rdpContext*) xfc)->channels;
	clipboard->channels = channels;

	clipboard->request_index = -1;

	clipboard->root_window = DefaultRootWindow(xfc->display);
	clipboard->clipboard_atom = XInternAtom(xfc->display, "CLIPBOARD", FALSE);

	if (clipboard->clipboard_atom == None)
	{
		WLog_ERR(TAG, "unable to get CLIPBOARD atom");
	}

	id = 1;
	clipboard->property_atom = XInternAtom(xfc->display, "_FREERDP_CLIPRDR", FALSE);
	clipboard->identity_atom = XInternAtom(xfc->display, "_FREERDP_CLIPRDR_ID", FALSE);

	XChangeProperty(xfc->display, xfc->drawable, clipboard->identity_atom,
			XA_INTEGER, 32, PropModeReplace, (BYTE*) &id, 1);

	XSelectInput(xfc->display, clipboard->root_window, PropertyChangeMask);

#ifdef WITH_XFIXES
	if (XFixesQueryExtension(xfc->display, &clipboard->xfixes_event_base, &clipboard->xfixes_error_base))
	{
		int xfmajor, xfminor;
		if (XFixesQueryVersion(xfc->display, &xfmajor, &xfminor))
		{
			DEBUG_X11_CLIPRDR("Found X Fixes extension version %d.%d", xfmajor, xfminor);
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
	clipboard->format_mappings[n].target_format = XInternAtom(xfc->display, "_FREERDP_RAW", FALSE);
	clipboard->format_mappings[n].format_id = CB_FORMAT_RAW;

	n++;
	clipboard->format_mappings[n].target_format = XInternAtom(xfc->display, "UTF8_STRING", FALSE);
	clipboard->format_mappings[n].format_id = CB_FORMAT_UNICODETEXT;

	n++;
	clipboard->format_mappings[n].target_format = XA_STRING;
	clipboard->format_mappings[n].format_id = CB_FORMAT_TEXT;

	n++;
	clipboard->format_mappings[n].target_format = XInternAtom(xfc->display, "image/png", FALSE);
	clipboard->format_mappings[n].format_id = CB_FORMAT_PNG;

	n++;
	clipboard->format_mappings[n].target_format = XInternAtom(xfc->display, "image/jpeg", FALSE);
	clipboard->format_mappings[n].format_id = CB_FORMAT_JPEG;

	n++;
	clipboard->format_mappings[n].target_format = XInternAtom(xfc->display, "image/gif", FALSE);
	clipboard->format_mappings[n].format_id = CB_FORMAT_GIF;

	n++;
	clipboard->format_mappings[n].target_format = XInternAtom(xfc->display, "image/bmp", FALSE);
	clipboard->format_mappings[n].format_id = CB_FORMAT_DIB;

	n++;
	clipboard->format_mappings[n].target_format = XInternAtom(xfc->display, "text/html", FALSE);
	clipboard->format_mappings[n].format_id = CB_FORMAT_HTML;

	clipboard->num_format_mappings = n + 1;
	clipboard->targets[0] = XInternAtom(xfc->display, "TIMESTAMP", FALSE);
	clipboard->targets[1] = XInternAtom(xfc->display, "TARGETS", FALSE);
	clipboard->num_targets = 2;

	clipboard->incr_atom = XInternAtom(xfc->display, "INCR", FALSE);

	return clipboard;
}

void xf_clipboard_free(xfClipboard* clipboard)
{
	if (!clipboard)
		return;

	free(clipboard->formats);
	free(clipboard->data);
	free(clipboard->respond);
	free(clipboard->incr_data);
	free(clipboard);
}

void xf_cliprdr_init(xfContext* xfc, CliprdrClientContext* cliprdr)
{
	return;

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
}
