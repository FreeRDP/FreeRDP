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

#include <winpr/crt.h>
#include <winpr/stream.h>

#include <freerdp/utils/event.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/channels/channels.h>

#include "xf_cliprdr.h"

#ifdef WITH_DEBUG_X11
#define DEBUG_X11(fmt, ...) DEBUG_CLASS(X11, fmt, ## __VA_ARGS__)
#else
#define DEBUG_X11(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#ifdef WITH_DEBUG_X11_CLIPRDR
#define DEBUG_X11_CLIPRDR(fmt, ...) DEBUG_CLASS(X11_CLIPRDR, fmt, ## __VA_ARGS__)
#else
#define DEBUG_X11_CLIPRDR(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

typedef struct clipboard_format_mapping clipboardFormatMapping;

struct clipboard_format_mapping
{
	Atom target_format;
	UINT32 format_id;
};

typedef struct clipboard_context clipboardContext;

struct clipboard_context
{
	rdpChannels* channels;
	Window root_window;
	Atom clipboard_atom;
	Atom property_atom;
	Atom identity_atom;

	clipboardFormatMapping format_mappings[20];
	int num_format_mappings;

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
};

void xf_cliprdr_init(xfContext* xfc, rdpChannels* channels)
{
	int n;
	UINT32 id;
	clipboardContext* cb;

	cb = (clipboardContext*) malloc(sizeof(clipboardContext));
	ZeroMemory(cb, sizeof(clipboardContext));

	xfc->clipboard_context = cb;

	cb->channels = channels;
	cb->request_index = -1;

	cb->root_window = DefaultRootWindow(xfc->display);
	cb->clipboard_atom = XInternAtom(xfc->display, "CLIPBOARD", FALSE);

	if (cb->clipboard_atom == None)
	{
		DEBUG_WARN("unable to get CLIPBOARD atom");
	}

	id = 1;
	cb->property_atom = XInternAtom(xfc->display, "_FREERDP_CLIPRDR", FALSE);
	cb->identity_atom = XInternAtom(xfc->display, "_FREERDP_CLIPRDR_ID", FALSE);

	XChangeProperty(xfc->display, xfc->drawable, cb->identity_atom,
			XA_INTEGER, 32, PropModeReplace, (BYTE*) &id, 1);

	XSelectInput(xfc->display, cb->root_window, PropertyChangeMask);

	n = 0;
	cb->format_mappings[n].target_format = XInternAtom(xfc->display, "_FREERDP_RAW", FALSE);
	cb->format_mappings[n].format_id = CB_FORMAT_RAW;

	n++;
	cb->format_mappings[n].target_format = XInternAtom(xfc->display, "UTF8_STRING", FALSE);
	cb->format_mappings[n].format_id = CB_FORMAT_UNICODETEXT;

	n++;
	cb->format_mappings[n].target_format = XA_STRING;
	cb->format_mappings[n].format_id = CB_FORMAT_TEXT;

	n++;
	cb->format_mappings[n].target_format = XInternAtom(xfc->display, "image/png", FALSE);
	cb->format_mappings[n].format_id = CB_FORMAT_PNG;

	n++;
	cb->format_mappings[n].target_format = XInternAtom(xfc->display, "image/jpeg", FALSE);
	cb->format_mappings[n].format_id = CB_FORMAT_JPEG;

	n++;
	cb->format_mappings[n].target_format = XInternAtom(xfc->display, "image/gif", FALSE);
	cb->format_mappings[n].format_id = CB_FORMAT_GIF;

	n++;
	cb->format_mappings[n].target_format = XInternAtom(xfc->display, "image/bmp", FALSE);
	cb->format_mappings[n].format_id = CB_FORMAT_DIB;

	n++;
	cb->format_mappings[n].target_format = XInternAtom(xfc->display, "text/html", FALSE);
	cb->format_mappings[n].format_id = CB_FORMAT_HTML;

	cb->num_format_mappings = n + 1;
	cb->targets[0] = XInternAtom(xfc->display, "TIMESTAMP", FALSE);
	cb->targets[1] = XInternAtom(xfc->display, "TARGETS", FALSE);
	cb->num_targets = 2;

	cb->incr_atom = XInternAtom(xfc->display, "INCR", FALSE);
}

void xf_cliprdr_uninit(xfContext* xfc)
{
	clipboardContext* cb = (clipboardContext*) xfc->clipboard_context;

	if (cb)
	{
		free(cb->formats);
		free(cb->data);
		free(cb->respond);
		free(cb->incr_data);
		free(cb);
		xfc->clipboard_context = NULL;
	}
}

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

static BOOL xf_cliprdr_is_self_owned(xfContext* xfc)
{
	Atom type;
	UINT32 id = 0;
	UINT32* pid = NULL;
	int format, result = 0;
	unsigned long length, bytes_left;
	clipboardContext* cb = (clipboardContext*) xfc->clipboard_context;

	cb->owner = XGetSelectionOwner(xfc->display, cb->clipboard_atom);

	if (cb->owner != None)
	{
		result = XGetWindowProperty(xfc->display, cb->owner,
			cb->identity_atom, 0, 4, 0, XA_INTEGER,
			&type, &format, &length, &bytes_left, (BYTE**) &pid);
	}

	if (pid)
	{
		id = *pid;
		XFree(pid);
	}

	if ((cb->owner == None) || (cb->owner == xfc->drawable))
		return FALSE;

	if (result != Success)
		return FALSE;

	return (id ? TRUE : FALSE);
}

static int xf_cliprdr_select_format_by_id(clipboardContext* cb, UINT32 format_id)
{
	int i;

	for (i = 0; i < cb->num_format_mappings; i++)
	{
		if (cb->format_mappings[i].format_id == format_id)
			return i;
	}

	return -1;
}

static int xf_cliprdr_select_format_by_atom(clipboardContext* cb, Atom target)
{
	int i;
	int j;

	if (cb->formats == NULL)
		return -1;

	for (i = 0; i < cb->num_format_mappings; i++)
	{
		if (cb->format_mappings[i].target_format != target)
			continue;

		if (cb->format_mappings[i].format_id == CB_FORMAT_RAW)
			return i;

		for (j = 0; j < cb->num_formats; j++)
		{
			if (cb->format_mappings[i].format_id == cb->formats[j])
				return i;
		}
	}

	return -1;
}

static void xf_cliprdr_send_raw_format_list(xfContext* xfc)
{
	Atom type;
	BYTE* format_data;
	int format, result;
	unsigned long length, bytes_left;
	RDP_CB_FORMAT_LIST_EVENT* event;
	clipboardContext* cb = (clipboardContext*) xfc->clipboard_context;

	result = XGetWindowProperty(xfc->display, cb->root_window,
		cb->property_atom, 0, 3600, 0, XA_STRING,
		&type, &format, &length, &bytes_left, (BYTE**) &format_data);

	if (result != Success)
	{
		DEBUG_WARN("XGetWindowProperty failed");
		return;
	}
	DEBUG_X11_CLIPRDR("format=%d len=%d bytes_left=%d", format, (int) length, (int) bytes_left);

	event = (RDP_CB_FORMAT_LIST_EVENT*) freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_FormatList, NULL, NULL);

	event->raw_format_data = (BYTE*) malloc(length);
	CopyMemory(event->raw_format_data, format_data, length);
	event->raw_format_data_size = length;
	XFree(format_data);

	freerdp_channels_send_event(cb->channels, (wMessage*) event);
}

static void xf_cliprdr_send_null_format_list(xfContext* xfc)
{
	RDP_CB_FORMAT_LIST_EVENT* event;
	clipboardContext* cb = (clipboardContext*) xfc->clipboard_context;

	event = (RDP_CB_FORMAT_LIST_EVENT*) freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_FormatList, NULL, NULL);

	event->num_formats = 0;

	freerdp_channels_send_event(cb->channels, (wMessage*) event);
}

static void xf_cliprdr_send_supported_format_list(xfContext* xfc)
{
	int i;
	RDP_CB_FORMAT_LIST_EVENT* event;
	clipboardContext* cb = (clipboardContext*) xfc->clipboard_context;

	event = (RDP_CB_FORMAT_LIST_EVENT*) freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_FormatList, NULL, NULL);

	event->formats = (UINT32*) malloc(sizeof(UINT32) * cb->num_format_mappings);
	event->num_formats = cb->num_format_mappings;

	for (i = 0; i < cb->num_format_mappings; i++)
		event->formats[i] = cb->format_mappings[i].format_id;

	freerdp_channels_send_event(cb->channels, (wMessage*) event);
}

static void xf_cliprdr_send_format_list(xfContext* xfc)
{
	clipboardContext* cb = (clipboardContext*) xfc->clipboard_context;

	if (xf_cliprdr_is_self_owned(xfc))
	{
		xf_cliprdr_send_raw_format_list(xfc);
	}
	else if (cb->owner == None)
	{
		xf_cliprdr_send_null_format_list(xfc);
	}
	else if (cb->owner != xfc->drawable)
	{
		/* Request the owner for TARGETS, and wait for SelectionNotify event */
		XConvertSelection(xfc->display, cb->clipboard_atom,
			cb->targets[1], cb->property_atom, xfc->drawable, CurrentTime);
	}
}

static void xf_cliprdr_send_data_request(xfContext* xfc, UINT32 format)
{
	RDP_CB_DATA_REQUEST_EVENT* event;
	clipboardContext* cb = (clipboardContext*) xfc->clipboard_context;

	event = (RDP_CB_DATA_REQUEST_EVENT*) freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_DataRequest, NULL, NULL);

	event->format = format;

	freerdp_channels_send_event(cb->channels, (wMessage*) event);
}

static void xf_cliprdr_send_data_response(xfContext* xfc, BYTE* data, int size)
{
	RDP_CB_DATA_RESPONSE_EVENT* event;
	clipboardContext* cb = (clipboardContext*) xfc->clipboard_context;

	event = (RDP_CB_DATA_RESPONSE_EVENT*) freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_DataResponse, NULL, NULL);

	event->data = data;
	event->size = size;

	freerdp_channels_send_event(cb->channels, (wMessage*) event);
}

static void xf_cliprdr_send_null_data_response(xfContext* xfc)
{
	xf_cliprdr_send_data_response(xfc, NULL, 0);
}

static void xf_cliprdr_process_cb_monitor_ready_event(xfContext* xfc)
{
	clipboardContext* cb = (clipboardContext*) xfc->clipboard_context;

	xf_cliprdr_send_format_list(xfc);
	cb->sync = TRUE;
}

static void xf_cliprdr_process_cb_data_request_event(xfContext* xfc, RDP_CB_DATA_REQUEST_EVENT* event)
{
	int i;
	clipboardContext* cb = (clipboardContext*) xfc->clipboard_context;

	DEBUG_X11_CLIPRDR("format %d", event->format);

	if (xf_cliprdr_is_self_owned(xfc))
	{
		/* CB_FORMAT_RAW */
		i = 0;
		XChangeProperty(xfc->display, xfc->drawable, cb->property_atom,
			XA_INTEGER, 32, PropModeReplace, (BYTE*) &event->format, 1);
	}
	else
	{
		i = xf_cliprdr_select_format_by_id(cb, event->format);
	}

	if (i < 0)
	{
		DEBUG_X11_CLIPRDR("unsupported format requested");
		xf_cliprdr_send_null_data_response(xfc);
	}
	else
	{
		cb->request_index = i;

		DEBUG_X11_CLIPRDR("target=%d", (int) cb->format_mappings[i].target_format);

		XConvertSelection(xfc->display, cb->clipboard_atom,
			cb->format_mappings[i].target_format, cb->property_atom,
			xfc->drawable, CurrentTime);
		XFlush(xfc->display);
		/* After this point, we expect a SelectionNotify event from the clipboard owner. */
	}
}

static void xf_cliprdr_get_requested_targets(xfContext* xfc)
{
	int num;
	int i, j;
	Atom atom;
	int format;
	BYTE* data = NULL;
	unsigned long length, bytes_left;
	RDP_CB_FORMAT_LIST_EVENT* event;
	clipboardContext* cb = (clipboardContext*) xfc->clipboard_context;

	XGetWindowProperty(xfc->display, xfc->drawable, cb->property_atom,
		0, 200, 0, XA_ATOM, &atom, &format, &length, &bytes_left, &data);

	DEBUG_X11_CLIPRDR("type=%d format=%d length=%d bytes_left=%d",
		(int) atom, format, (int) length, (int) bytes_left);

	if (length > 0)
	{
		event = (RDP_CB_FORMAT_LIST_EVENT*) freerdp_event_new(CliprdrChannel_Class,
				CliprdrChannel_FormatList, NULL, NULL);

		event->formats = (UINT32*) malloc(sizeof(UINT32) * cb->num_format_mappings);
		num = 0;

		for (i = 0; i < length; i++)
		{
			atom = ((Atom*) data)[i];
			DEBUG_X11("atom %d", (int) atom);

			for (j = 0; j < cb->num_format_mappings; j++)
			{
				if (cb->format_mappings[j].target_format == atom)
				{
					DEBUG_X11("found format %d for atom %d",
						cb->format_mappings[j].format_id, (int)atom);
					event->formats[num++] = cb->format_mappings[j].format_id;
					break;
				}
			}
		}

		event->num_formats = num;
		XFree(data);

		freerdp_channels_send_event(cb->channels, (wMessage*) event);
	}
	else
	{
		if (data)
			XFree(data);

		xf_cliprdr_send_null_format_list(xfc);
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
	outbuf = (BYTE*) malloc(*size);
	ZeroMemory(outbuf, *size);

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
		inbuf = malloc(*size + 1);
		ZeroMemory(inbuf, *size + 1);

		CopyMemory(inbuf, data, *size);
	}

	outbuf = (BYTE*) malloc(*size + 200);
	ZeroMemory(outbuf, *size + 200);

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

static void xf_cliprdr_process_requested_data(xfContext* xfc, BOOL has_data, BYTE* data, int size)
{
	BYTE* outbuf;
	clipboardContext* cb = (clipboardContext*) xfc->clipboard_context;

	if (cb->incr_starts && has_data)
		return;

	if (!has_data || data == NULL)
	{
		xf_cliprdr_send_null_data_response(xfc);
		return;
	}

	switch (cb->format_mappings[cb->request_index].format_id)
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
		xf_cliprdr_send_data_response(xfc, outbuf, size);
	else
		xf_cliprdr_send_null_data_response(xfc);

	/* Resend the format list, otherwise the server won't request again for the next paste */
	xf_cliprdr_send_format_list(xfc);
}

static BOOL xf_cliprdr_get_requested_data(xfContext* xfc, Atom target)
{
	Atom type;
	int format;
	BYTE* data = NULL;
	BOOL has_data = FALSE;
	unsigned long length, bytes_left, dummy;
	clipboardContext* cb = (clipboardContext*) xfc->clipboard_context;

	if ((cb->request_index < 0) || (cb->format_mappings[cb->request_index].target_format != target))
	{
		DEBUG_X11_CLIPRDR("invalid target");
		xf_cliprdr_send_null_data_response(xfc);
		return FALSE;
	}

	XGetWindowProperty(xfc->display, xfc->drawable,
		cb->property_atom, 0, 0, 0, target,
		&type, &format, &length, &bytes_left, &data);

	DEBUG_X11_CLIPRDR("type=%d format=%d bytes=%d request_index=%d",
		(int) type, format, (int) bytes_left, cb->request_index);

	if (data)
	{
		XFree(data);
		data = NULL;
	}
	if (bytes_left <= 0 && !cb->incr_starts)
	{
		DEBUG_X11("no data");
	}
	else if (type == cb->incr_atom)
	{
		DEBUG_X11("INCR started");
		cb->incr_starts = TRUE;
		if (cb->incr_data)
		{
			free(cb->incr_data);
			cb->incr_data = NULL;
		}
		cb->incr_data_length = 0;
		/* Data will be followed in PropertyNotify event */
		has_data = TRUE;
	}
	else
	{
		if (bytes_left <= 0)
		{
			/* INCR finish */
			data = cb->incr_data;
			cb->incr_data = NULL;
			bytes_left = cb->incr_data_length;
			cb->incr_data_length = 0;
			cb->incr_starts = 0;
			DEBUG_X11("INCR finished");
			has_data = TRUE;
		}
		else if (XGetWindowProperty(xfc->display, xfc->drawable,
			cb->property_atom, 0, bytes_left, 0, target,
			&type, &format, &length, &dummy, &data) == Success)
		{
			if (cb->incr_starts)
			{
				bytes_left = length * format / 8;
				DEBUG_X11("%d bytes", (int)bytes_left);
				cb->incr_data = (BYTE*) realloc(cb->incr_data, cb->incr_data_length + bytes_left);
				CopyMemory(cb->incr_data + cb->incr_data_length, data, bytes_left);
				cb->incr_data_length += bytes_left;
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
	XDeleteProperty(xfc->display, xfc->drawable, cb->property_atom);

	xf_cliprdr_process_requested_data(xfc, has_data, data, (int) bytes_left);

	if (data)
		XFree(data);

	return TRUE;
}

static void xf_cliprdr_append_target(clipboardContext* cb, Atom target)
{
	int i;

	if (cb->num_targets >= ARRAYSIZE(cb->targets))
		return;

	for (i = 0; i < cb->num_targets; i++)
	{
		if (cb->targets[i] == target)
			return;
	}

	cb->targets[cb->num_targets++] = target;
}

static void xf_cliprdr_provide_targets(xfContext* xfc, XEvent* respond)
{
	clipboardContext* cb = (clipboardContext*) xfc->clipboard_context;

	if (respond->xselection.property != None)
	{
		XChangeProperty(xfc->display,
			respond->xselection.requestor,
			respond->xselection.property,
			XA_ATOM, 32, PropModeReplace,
			(BYTE*) cb->targets, cb->num_targets);
	}
}

static void xf_cliprdr_provide_data(xfContext* xfc, XEvent* respond)
{
	clipboardContext* cb = (clipboardContext*) xfc->clipboard_context;

	if (respond->xselection.property != None)
	{
		XChangeProperty(xfc->display,
			respond->xselection.requestor,
			respond->xselection.property,
			respond->xselection.target, 8, PropModeReplace,
			(BYTE*) cb->data, cb->data_length);
	}
}

static void xf_cliprdr_process_cb_format_list_event(xfContext* xfc, RDP_CB_FORMAT_LIST_EVENT* event)
{
	int i, j;
	clipboardContext* cb = (clipboardContext*) xfc->clipboard_context;

	if (cb->data)
	{
		free(cb->data);
		cb->data = NULL;
	}

	if (cb->formats)
		free(cb->formats);

	cb->formats = event->formats;
	cb->num_formats = event->num_formats;
	event->formats = NULL;
	event->num_formats = 0;

	cb->num_targets = 2;

	for (i = 0; i < cb->num_formats; i++)
	{
		for (j = 0; j < cb->num_format_mappings; j++)
		{
			if (cb->formats[i] == cb->format_mappings[j].format_id)
			{
				DEBUG_X11("announce format#%d : %d", i, cb->formats[i]);
				xf_cliprdr_append_target(cb, cb->format_mappings[j].target_format);
			}
		}
	}

	XSetSelectionOwner(xfc->display, cb->clipboard_atom, xfc->drawable, CurrentTime);

	if (event->raw_format_data)
	{
		XChangeProperty(xfc->display, cb->root_window, cb->property_atom,
			XA_STRING, 8, PropModeReplace,
			event->raw_format_data, event->raw_format_data_size);
	}

	XFlush(xfc->display);
}

static void xf_cliprdr_process_text(clipboardContext* cb, BYTE* data, int size)
{
	cb->data = (BYTE*) malloc(size);
	CopyMemory(cb->data, data, size);
	cb->data_length = size;
	crlf2lf(cb->data, &cb->data_length);
}

static void xf_cliprdr_process_unicodetext(clipboardContext* cb, BYTE* data, int size)
{
	cb->data_length = ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) data, size / 2, (CHAR**) &(cb->data), 0, NULL, NULL);
	crlf2lf(cb->data, &cb->data_length);
}

static void xf_cliprdr_process_dib(clipboardContext* cb, BYTE* data, int size)
{
	wStream* s;
	UINT16 bpp;
	UINT32 offset;
	UINT32 ncolors;

	/* size should be at least sizeof(BITMAPINFOHEADER) */

	if (size < 40)
	{
		DEBUG_X11_CLIPRDR("dib size %d too short", size);
		return;
	}

	s = Stream_New(data, size);
	Stream_Seek(s, 14);
	Stream_Read_UINT16(s, bpp);
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

	cb->data = Stream_Buffer(s);
	cb->data_length = Stream_GetPosition(s);
	Stream_Free(s, FALSE);
}

static void xf_cliprdr_process_html(clipboardContext* cb, BYTE* data, int size)
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

	cb->data = (BYTE*) malloc(size - start + 1);
	CopyMemory(cb->data, data + start, end - start);
	cb->data_length = end - start;
	crlf2lf(cb->data, &cb->data_length);
}

static void xf_cliprdr_process_cb_data_response_event(xfContext* xfc, RDP_CB_DATA_RESPONSE_EVENT* event)
{
	clipboardContext* cb = (clipboardContext*) xfc->clipboard_context;

	DEBUG_X11_CLIPRDR("size=%d", event->size);

	if (!cb->respond)
	{
		DEBUG_X11_CLIPRDR("unexpected data");
		return;
	}

	if (event->size == 0)
	{
		cb->respond->xselection.property = None;
	}
	else
	{
		if (cb->data)
		{
			free(cb->data);
			cb->data = NULL;
		}

		switch (cb->data_format)
		{
			case CB_FORMAT_RAW:
			case CB_FORMAT_PNG:
			case CB_FORMAT_JPEG:
			case CB_FORMAT_GIF:
				cb->data = event->data;
				cb->data_length = event->size;
				event->data = NULL;
				event->size = 0;
				break;

			case CB_FORMAT_TEXT:
				xf_cliprdr_process_text(cb, event->data, event->size);
				break;

			case CB_FORMAT_UNICODETEXT:
				xf_cliprdr_process_unicodetext(cb, event->data, event->size);
				break;

			case CB_FORMAT_DIB:
				xf_cliprdr_process_dib(cb, event->data, event->size);
				break;

			case CB_FORMAT_HTML:
				xf_cliprdr_process_html(cb, event->data, event->size);
				break;

			default:
				cb->respond->xselection.property = None;
				break;
		}
		xf_cliprdr_provide_data(xfc, cb->respond);
	}

	XSendEvent(xfc->display, cb->respond->xselection.requestor, 0, 0, cb->respond);
	XFlush(xfc->display);
	free(cb->respond);
	cb->respond = NULL;
}

void xf_process_cliprdr_event(xfContext* xfc, wMessage* event)
{
	switch (GetMessageType(event->id))
	{
		case CliprdrChannel_MonitorReady:
			xf_cliprdr_process_cb_monitor_ready_event(xfc);
			break;

		case CliprdrChannel_FormatList:
			xf_cliprdr_process_cb_format_list_event(xfc, (RDP_CB_FORMAT_LIST_EVENT*) event);
			break;

		case CliprdrChannel_DataRequest:
			xf_cliprdr_process_cb_data_request_event(xfc, (RDP_CB_DATA_REQUEST_EVENT*) event);
			break;

		case CliprdrChannel_DataResponse:
			xf_cliprdr_process_cb_data_response_event(xfc, (RDP_CB_DATA_RESPONSE_EVENT*) event);
			break;

		default:
			DEBUG_X11_CLIPRDR("unknown event type %d", GetMessageType(event->id));
			break;
	}
}

BOOL xf_cliprdr_process_selection_notify(xfContext* xfc, XEvent* xevent)
{
	clipboardContext* cb = (clipboardContext*) xfc->clipboard_context;

	if (xevent->xselection.target == cb->targets[1])
	{
		if (xevent->xselection.property == None)
		{
			DEBUG_X11_CLIPRDR("owner not support TARGETS. sending all format.");
			xf_cliprdr_send_supported_format_list(xfc);
		}
		else
		{
			xf_cliprdr_get_requested_targets(xfc);
		}

		return TRUE;
	}
	else
	{
		return xf_cliprdr_get_requested_data(xfc, xevent->xselection.target);
	}
}

BOOL xf_cliprdr_process_selection_request(xfContext* xfc, XEvent* xevent)
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
	clipboardContext* cb = (clipboardContext*) xfc->clipboard_context;

	DEBUG_X11_CLIPRDR("target=%d", (int) xevent->xselectionrequest.target);

	if (xevent->xselectionrequest.owner != xfc->drawable)
	{
		DEBUG_X11_CLIPRDR("not owner");
		return FALSE;
	}

	delay_respond = FALSE;

	respond = (XEvent*) malloc(sizeof(XEvent));
	ZeroMemory(respond, sizeof(XEvent));

	respond->xselection.property = None;
	respond->xselection.type = SelectionNotify;
	respond->xselection.display = xevent->xselectionrequest.display;
	respond->xselection.requestor = xevent->xselectionrequest.requestor;
	respond->xselection.selection = xevent->xselectionrequest.selection;
	respond->xselection.target = xevent->xselectionrequest.target;
	respond->xselection.time = xevent->xselectionrequest.time;

	if (xevent->xselectionrequest.target == cb->targets[0]) /* TIMESTAMP */
	{
		/* TODO */
		DEBUG_X11_CLIPRDR("target: TIMESTAMP (unimplemented)");
	}
	else if (xevent->xselectionrequest.target == cb->targets[1]) /* TARGETS */
	{
		/* Someone else requests our available formats */
		DEBUG_X11_CLIPRDR("target: TARGETS");
		respond->xselection.property = xevent->xselectionrequest.property;
		xf_cliprdr_provide_targets(xfc, respond);
	}
	else
	{
		DEBUG_X11_CLIPRDR("target: other");

		i = xf_cliprdr_select_format_by_atom(cb, xevent->xselectionrequest.target);

		if (i >= 0 && xevent->xselectionrequest.requestor != xfc->drawable)
		{
			format = cb->format_mappings[i].format_id;
			alt_format = format;

			if (format == CB_FORMAT_RAW)
			{
				if (XGetWindowProperty(xfc->display, xevent->xselectionrequest.requestor,
					cb->property_atom, 0, 4, 0, XA_INTEGER,
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

			if ((cb->data != 0) && (format == cb->data_format) && (alt_format == cb->data_alt_format))
			{
				/* Cached clipboard data available. Send it now */
				respond->xselection.property = xevent->xselectionrequest.property;
				xf_cliprdr_provide_data(xfc, respond);
			}
			else if (cb->respond)
			{
				DEBUG_X11_CLIPRDR("duplicated request");
			}
			else
			{
				/**
				 * Send clipboard data request to the server.
				 * Response will be postponed after receiving the data
				 */
				if (cb->data)
				{
					free(cb->data);
					cb->data = NULL;
				}

				respond->xselection.property = xevent->xselectionrequest.property;
				cb->respond = respond;
				cb->data_format = format;
				cb->data_alt_format = alt_format;
				delay_respond = TRUE;

				xf_cliprdr_send_data_request(xfc, alt_format);
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

BOOL xf_cliprdr_process_selection_clear(xfContext* xfc, XEvent* xevent)
{
	clipboardContext* cb = (clipboardContext*) xfc->clipboard_context;

	if (xf_cliprdr_is_self_owned(xfc))
		return FALSE;

	XDeleteProperty(xfc->display, cb->root_window, cb->property_atom);

	return TRUE;
}

BOOL xf_cliprdr_process_property_notify(xfContext* xfc, XEvent* xevent)
{
	clipboardContext* cb = (clipboardContext*) xfc->clipboard_context;

	if (!cb)
		return TRUE;

	if (xevent->xproperty.atom != cb->property_atom)
		return FALSE; /* Not cliprdr-related */

	if (xevent->xproperty.window == cb->root_window)
	{
		DEBUG_X11_CLIPRDR("root window PropertyNotify");
		xf_cliprdr_send_format_list(xfc);
	}
	else if (xevent->xproperty.window == xfc->drawable &&
		xevent->xproperty.state == PropertyNewValue &&
		cb->incr_starts && cb->request_index >= 0)
	{
		DEBUG_X11_CLIPRDR("cliprdr window PropertyNotify");
		xf_cliprdr_get_requested_data(xfc,
			cb->format_mappings[cb->request_index].target_format);
	}

	return TRUE;
}

void xf_cliprdr_check_owner(xfContext* xfc)
{
	Window owner;
	clipboardContext* cb = (clipboardContext*) xfc->clipboard_context;

	if (cb->sync)
	{
		owner = XGetSelectionOwner(xfc->display, cb->clipboard_atom);

		if (cb->owner != owner)
		{
			cb->owner = owner;
			xf_cliprdr_send_format_list(xfc);
		}
	}
}

