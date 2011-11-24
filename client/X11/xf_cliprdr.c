/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <freerdp/utils/event.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/unicode.h>
#include <freerdp/plugins/cliprdr.h>

#include "xf_cliprdr.h"

typedef struct clipboard_format_mapping clipboardFormatMapping;
struct clipboard_format_mapping
{
	Atom target_format;
	uint32 format_id;
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
	uint32* formats;
	int num_formats;
	Atom targets[20];
	int num_targets;
	uint8* data;
	uint32 data_format;
	uint32 data_alt_format;
	int data_length;
	XEvent* respond;

	/* client->server data */
	Window owner;
	int request_index;
	boolean sync;

	/* INCR mechanism */
	Atom incr_atom;
	boolean incr_starts;
	uint8* incr_data;
	int incr_data_length;
};

void xf_cliprdr_init(xfInfo* xfi, rdpChannels* chanman)
{
	int n;
	uint32 id;
	clipboardContext* cb;

	cb = xnew(clipboardContext);
	xfi->clipboard_context = cb;

	cb->channels = chanman;
	cb->request_index = -1;

	cb->root_window = DefaultRootWindow(xfi->display);
	cb->clipboard_atom = XInternAtom(xfi->display, "CLIPBOARD", false);

	if (cb->clipboard_atom == None)
	{
		DEBUG_WARN("unable to get CLIPBOARD atom");
	}

	id = 1;
	cb->property_atom = XInternAtom(xfi->display, "_FREERDP_CLIPRDR", false);
	cb->identity_atom = XInternAtom(xfi->display, "_FREERDP_CLIPRDR_ID", false);

	XChangeProperty(xfi->display, xfi->drawable, cb->identity_atom,
			XA_INTEGER, 32, PropModeReplace, (uint8*) &id, 1);

	XSelectInput(xfi->display, cb->root_window, PropertyChangeMask);

	n = 0;
	cb->format_mappings[n].target_format = XInternAtom(xfi->display, "_FREERDP_RAW", false);
	cb->format_mappings[n].format_id = CB_FORMAT_RAW;

	n++;
	cb->format_mappings[n].target_format = XInternAtom(xfi->display, "UTF8_STRING", false);
	cb->format_mappings[n].format_id = CB_FORMAT_UNICODETEXT;

	n++;
	cb->format_mappings[n].target_format = XA_STRING;
	cb->format_mappings[n].format_id = CB_FORMAT_TEXT;

	n++;
	cb->format_mappings[n].target_format = XInternAtom(xfi->display, "image/png", false);
	cb->format_mappings[n].format_id = CB_FORMAT_PNG;

	n++;
	cb->format_mappings[n].target_format = XInternAtom(xfi->display, "image/jpeg", false);
	cb->format_mappings[n].format_id = CB_FORMAT_JPEG;

	n++;
	cb->format_mappings[n].target_format = XInternAtom(xfi->display, "image/gif", false);
	cb->format_mappings[n].format_id = CB_FORMAT_GIF;

	n++;
	cb->format_mappings[n].target_format = XInternAtom(xfi->display, "image/bmp", false);
	cb->format_mappings[n].format_id = CB_FORMAT_DIB;

	n++;
	cb->format_mappings[n].target_format = XInternAtom(xfi->display, "text/html", false);
	cb->format_mappings[n].format_id = CB_FORMAT_HTML;

	cb->num_format_mappings = n + 1;
	cb->targets[0] = XInternAtom(xfi->display, "TIMESTAMP", false);
	cb->targets[1] = XInternAtom(xfi->display, "TARGETS", false);
	cb->num_targets = 2;

	cb->incr_atom = XInternAtom(xfi->display, "INCR", false);
}

void xf_cliprdr_uninit(xfInfo* xfi)
{
	clipboardContext* cb = (clipboardContext*) xfi->clipboard_context;

	if (cb)
	{
		xfree(cb->formats);
		xfree(cb->data);
		xfree(cb->respond);
		xfree(cb->incr_data);
		xfree(cb);
		xfi->clipboard_context = NULL;
	}
}

static uint8* lf2crlf(uint8* data, int* size)
{
	uint8 c;
	uint8* outbuf;
	uint8* out;
	uint8* in_end;
	uint8* in;
	int out_size;

	out_size = (*size) * 2 + 1;
	outbuf = (uint8*) xzalloc(out_size);
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

static void crlf2lf(uint8* data, int* size)
{
	uint8 c;
	uint8* out;
	uint8* in;
	uint8* in_end;

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

static void be2le(uint8* data, int size)
{
	uint8 c;

	while (size >= 2)
	{
		c = data[0];
		data[0] = data[1];
		data[1] = c;

		data += 2;
		size -= 2;
	}
}

static boolean xf_cliprdr_is_self_owned(xfInfo* xfi)
{
	Atom type;
	uint32 id = 0;
	uint32* pid = NULL;
	int format, result = 0;
	unsigned long length, bytes_left;
	clipboardContext* cb = (clipboardContext*) xfi->clipboard_context;

	cb->owner = XGetSelectionOwner(xfi->display, cb->clipboard_atom);

	if (cb->owner != None)
	{
		result = XGetWindowProperty(xfi->display, cb->owner,
			cb->identity_atom, 0, 4, 0, XA_INTEGER,
			&type, &format, &length, &bytes_left, (uint8**) &pid);
	}

	if (pid)
	{
		id = *pid;
		XFree(pid);
	}

	if ((cb->owner == None) || (cb->owner == xfi->drawable))
		return false;

	if (result != Success)
		return false;

	return (id ? true : false);
}

static int xf_cliprdr_select_format_by_id(clipboardContext* cb, uint32 format_id)
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

static void xf_cliprdr_send_raw_format_list(xfInfo* xfi)
{
	Atom type;
	uint8* format_data;
	int format, result;
	unsigned long length, bytes_left;
	RDP_CB_FORMAT_LIST_EVENT* event;
	clipboardContext* cb = (clipboardContext*) xfi->clipboard_context;

	result = XGetWindowProperty(xfi->display, cb->root_window,
		cb->property_atom, 0, 3600, 0, XA_STRING,
		&type, &format, &length, &bytes_left, (uint8**) &format_data);

	if (result != Success)
	{
		DEBUG_WARN("XGetWindowProperty failed");
		return;
	}
	DEBUG_X11_CLIPRDR("format=%d len=%d bytes_left=%d", format, (int) length, (int) bytes_left);

	event = (RDP_CB_FORMAT_LIST_EVENT*) freerdp_event_new(RDP_EVENT_CLASS_CLIPRDR,
		RDP_EVENT_TYPE_CB_FORMAT_LIST, NULL, NULL);

	event->raw_format_data = (uint8*) xmalloc(length);
	memcpy(event->raw_format_data, format_data, length);
	event->raw_format_data_size = length;
	XFree(format_data);

	freerdp_channels_send_event(cb->channels, (RDP_EVENT*) event);
}

static void xf_cliprdr_send_null_format_list(xfInfo* xfi)
{
	RDP_CB_FORMAT_LIST_EVENT* event;
	clipboardContext* cb = (clipboardContext*) xfi->clipboard_context;

	event = (RDP_CB_FORMAT_LIST_EVENT*) freerdp_event_new(RDP_EVENT_CLASS_CLIPRDR,
		RDP_EVENT_TYPE_CB_FORMAT_LIST, NULL, NULL);

	event->num_formats = 0;

	freerdp_channels_send_event(cb->channels, (RDP_EVENT*) event);
}

static void xf_cliprdr_send_supported_format_list(xfInfo* xfi)
{
	int i;
	RDP_CB_FORMAT_LIST_EVENT* event;
	clipboardContext* cb = (clipboardContext*) xfi->clipboard_context;

	event = (RDP_CB_FORMAT_LIST_EVENT*) freerdp_event_new(RDP_EVENT_CLASS_CLIPRDR,
		RDP_EVENT_TYPE_CB_FORMAT_LIST, NULL, NULL);

	event->formats = (uint32*) xmalloc(sizeof(uint32) * cb->num_format_mappings);
	event->num_formats = cb->num_format_mappings;

	for (i = 0; i < cb->num_format_mappings; i++)
		event->formats[i] = cb->format_mappings[i].format_id;

	freerdp_channels_send_event(cb->channels, (RDP_EVENT*) event);
}

static void xf_cliprdr_send_format_list(xfInfo* xfi)
{
	clipboardContext* cb = (clipboardContext*) xfi->clipboard_context;

	if (xf_cliprdr_is_self_owned(xfi))
	{
		xf_cliprdr_send_raw_format_list(xfi);
	}
	else if (cb->owner == None)
	{
		xf_cliprdr_send_null_format_list(xfi);
	}
	else if (cb->owner != xfi->drawable)
	{
		/* Request the owner for TARGETS, and wait for SelectionNotify event */
		XConvertSelection(xfi->display, cb->clipboard_atom,
			cb->targets[1], cb->property_atom, xfi->drawable, CurrentTime);
	}
}

static void xf_cliprdr_send_data_request(xfInfo* xfi, uint32 format)
{
	RDP_CB_DATA_REQUEST_EVENT* event;
	clipboardContext* cb = (clipboardContext*) xfi->clipboard_context;

	event = (RDP_CB_DATA_REQUEST_EVENT*) freerdp_event_new(RDP_EVENT_CLASS_CLIPRDR,
		RDP_EVENT_TYPE_CB_DATA_REQUEST, NULL, NULL);

	event->format = format;

	freerdp_channels_send_event(cb->channels, (RDP_EVENT*) event);
}

static void xf_cliprdr_send_data_response(xfInfo* xfi, uint8* data, int size)
{
	RDP_CB_DATA_RESPONSE_EVENT* event;
	clipboardContext* cb = (clipboardContext*) xfi->clipboard_context;

	event = (RDP_CB_DATA_RESPONSE_EVENT*) freerdp_event_new(RDP_EVENT_CLASS_CLIPRDR,
		RDP_EVENT_TYPE_CB_DATA_RESPONSE, NULL, NULL);

	event->data = data;
	event->size = size;

	freerdp_channels_send_event(cb->channels, (RDP_EVENT*) event);
}

static void xf_cliprdr_send_null_data_response(xfInfo* xfi)
{
	xf_cliprdr_send_data_response(xfi, NULL, 0);
}

static void xf_cliprdr_process_cb_monitor_ready_event(xfInfo* xfi)
{
	clipboardContext* cb = (clipboardContext*) xfi->clipboard_context;

	xf_cliprdr_send_format_list(xfi);
	cb->sync = true;
}

static void xf_cliprdr_process_cb_data_request_event(xfInfo* xfi, RDP_CB_DATA_REQUEST_EVENT* event)
{
	int i;
	clipboardContext* cb = (clipboardContext*) xfi->clipboard_context;

	DEBUG_X11_CLIPRDR("format %d", event->format);

	if (xf_cliprdr_is_self_owned(xfi))
	{
		/* CB_FORMAT_RAW */
		i = 0;
		XChangeProperty(xfi->display, xfi->drawable, cb->property_atom,
			XA_INTEGER, 32, PropModeReplace, (uint8*) &event->format, 1);
	}
	else
	{
		i = xf_cliprdr_select_format_by_id(cb, event->format);
	}

	if (i < 0)
	{
		DEBUG_X11_CLIPRDR("unsupported format requested");
		xf_cliprdr_send_null_data_response(xfi);
	}
	else
	{
		cb->request_index = i;

		DEBUG_X11_CLIPRDR("target=%d", (int) cb->format_mappings[i].target_format);

		XConvertSelection(xfi->display, cb->clipboard_atom,
			cb->format_mappings[i].target_format, cb->property_atom,
			xfi->drawable, CurrentTime);
		XFlush(xfi->display);
		/* After this point, we expect a SelectionNotify event from the clipboard owner. */
	}
}

static void xf_cliprdr_get_requested_targets(xfInfo* xfi)
{
	int num;
	int i, j;
	Atom atom;
	int format;
	uint8* data = NULL;
	unsigned long length, bytes_left;
	RDP_CB_FORMAT_LIST_EVENT* event;
	clipboardContext* cb = (clipboardContext*) xfi->clipboard_context;

	XGetWindowProperty(xfi->display, xfi->drawable, cb->property_atom,
		0, 200, 0, XA_ATOM,
		&atom, &format, &length, &bytes_left, &data);

	DEBUG_X11_CLIPRDR("type=%d format=%d length=%d bytes_left=%d",
		(int) atom, format, (int) length, (int) bytes_left);

	if (length > 0)
	{
		event = (RDP_CB_FORMAT_LIST_EVENT*) freerdp_event_new(RDP_EVENT_CLASS_CLIPRDR,
			RDP_EVENT_TYPE_CB_FORMAT_LIST, NULL, NULL);

		event->formats = (uint32*) xmalloc(sizeof(uint32) * cb->num_format_mappings);
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

		freerdp_channels_send_event(cb->channels, (RDP_EVENT*) event);
	}
	else
	{
		if (data)
			XFree(data);

		xf_cliprdr_send_null_format_list(xfi);
	}
}

static uint8* xf_cliprdr_process_requested_raw(uint8* data, int* size)
{
	uint8* outbuf;

	outbuf = (uint8*) xmalloc(*size);
	memcpy(outbuf, data, *size);
	return outbuf;
}

static uint8* xf_cliprdr_process_requested_unicodetext(uint8* data, int* size)
{
	uint8* inbuf;
	uint8* outbuf;
	size_t out_size;
	UNICONV* uniconv;

	inbuf = lf2crlf(data, size);

	uniconv = freerdp_uniconv_new();
	outbuf = (uint8*) freerdp_uniconv_out(uniconv, (char*) inbuf, &out_size);
	freerdp_uniconv_free(uniconv);

	xfree(inbuf);

	*size = (int) out_size + 2;

	return outbuf;
}

static uint8* xf_cliprdr_process_requested_text(uint8* data, int* size)
{
	uint8* outbuf;

	outbuf = lf2crlf(data, size);

	return outbuf;
}

static uint8* xf_cliprdr_process_requested_dib(uint8* data, int* size)
{
	uint8* outbuf;

	/* length should be at least BMP header (14) + sizeof(BITMAPINFOHEADER) */
	if (*size < 54)
	{
		DEBUG_X11_CLIPRDR("bmp length %d too short", *size);
		return NULL;
	}

	*size -= 14;
	outbuf = (uint8*) xzalloc(*size);
	memcpy(outbuf, data + 14, *size);

	return outbuf;
}

static uint8* xf_cliprdr_process_requested_html(uint8* data, int* size)
{
	uint8* inbuf;
	uint8* in;
	uint8* outbuf;
	char num[11];
	UNICONV* uniconv;

	inbuf = NULL;
	if (*size > 2)
	{
		if ((uint8) data[0] == 0xFE && (uint8) data[1] == 0xFF)
		{
			be2le(data, *size);
		}

		if ((uint8) data[0] == 0xFF && (uint8) data[1] == 0xFE)
		{
			uniconv = freerdp_uniconv_new();
			inbuf = (uint8*) freerdp_uniconv_in(uniconv, data + 2, *size - 2);
			freerdp_uniconv_free(uniconv);
		}
	}
	if (inbuf == NULL)
	{
		inbuf = xzalloc(*size + 1);
		memcpy(inbuf, data, *size);
	}

	outbuf = (uint8*) xzalloc(*size + 200);
	strcpy((char*) outbuf,
		"Version:0.9\r\n"
		"StartHTML:0000000000\r\n"
		"EndHTML:0000000000\r\n"
		"StartFragment:0000000000\r\n"
		"EndFragment:0000000000\r\n");

	in = (uint8*) strstr((char*) inbuf, "<body");
	if (in == NULL)
	{
		in = (uint8*) strstr((char*) inbuf, "<BODY");
	}
	/* StartHTML */
	snprintf(num, sizeof(num), "%010lu", (unsigned long) strlen((char*) outbuf));
	memcpy(outbuf + 23, num, 10);
	if (in == NULL)
	{
		strcat((char*) outbuf, "<HTML><BODY>");
	}
	strcat((char*) outbuf, "<!--StartFragment-->");
	/* StartFragment */
	snprintf(num, sizeof(num), "%010lu", (unsigned long) strlen((char*) outbuf));
	memcpy(outbuf + 69, num, 10);
	strcat((char*) outbuf, (char*) inbuf);
	/* EndFragment */
	snprintf(num, sizeof(num), "%010lu", (unsigned long) strlen((char*) outbuf));
	memcpy(outbuf + 93, num, 10);
	strcat((char*) outbuf, "<!--EndFragment-->");
	if (in == NULL)
	{
		strcat((char*) outbuf, "</BODY></HTML>");
	}
	/* EndHTML */
	snprintf(num, sizeof(num), "%010lu", (unsigned long) strlen((char*) outbuf));
	memcpy(outbuf + 43, num, 10);

	*size = strlen((char*) outbuf) + 1;
	xfree(inbuf);

	return outbuf;
}

static void xf_cliprdr_process_requested_data(xfInfo* xfi, boolean has_data, uint8* data, int size)
{
	uint8* outbuf;
	clipboardContext* cb = (clipboardContext*) xfi->clipboard_context;

	if (cb->incr_starts && has_data)
		return;

	if (!has_data || data == NULL)
	{
		xf_cliprdr_send_null_data_response(xfi);
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
		xf_cliprdr_send_data_response(xfi, outbuf, size);
	else
		xf_cliprdr_send_null_data_response(xfi);

	/* Resend the format list, otherwise the server won't request again for the next paste */
	xf_cliprdr_send_format_list(xfi);
}

static boolean xf_cliprdr_get_requested_data(xfInfo* xfi, Atom target)
{
	Atom type;
	int format;
	uint8* data = NULL;
	boolean has_data = false;
	unsigned long length, bytes_left, dummy;
	clipboardContext* cb = (clipboardContext*) xfi->clipboard_context;

	if ((cb->request_index < 0) ||
		(cb->format_mappings[cb->request_index].target_format != target))
	{
		DEBUG_X11_CLIPRDR("invalid target");
		xf_cliprdr_send_null_data_response(xfi);
		return false;
	}

	XGetWindowProperty(xfi->display, xfi->drawable,
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
		cb->incr_starts = true;
		if (cb->incr_data)
		{
			xfree(cb->incr_data);
			cb->incr_data = NULL;
		}
		cb->incr_data_length = 0;
		/* Data will be followed in PropertyNotify event */
		has_data = true;
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
			has_data = true;
		}
		else if (XGetWindowProperty(xfi->display, xfi->drawable,
			cb->property_atom, 0, bytes_left, 0, target,
			&type, &format, &length, &dummy, &data) == Success)
		{
			if (cb->incr_starts)
			{
				bytes_left = length * format / 8;
				DEBUG_X11("%d bytes", (int)bytes_left);
				cb->incr_data = (uint8*) xrealloc(cb->incr_data, cb->incr_data_length + bytes_left);
				memcpy(cb->incr_data + cb->incr_data_length, data, bytes_left);
				cb->incr_data_length += bytes_left;
				XFree(data);
				data = NULL;
			}
			has_data = true;
		}
		else
		{
			DEBUG_X11_CLIPRDR("XGetWindowProperty failed");
		}
	}
	XDeleteProperty(xfi->display, xfi->drawable, cb->property_atom);

	xf_cliprdr_process_requested_data(xfi, has_data, data, (int) bytes_left);

	if (data)
		XFree(data);

	return true;
}

static void xf_cliprdr_append_target(clipboardContext* cb, Atom target)
{
	int i;

	if (cb->num_targets >= sizeof(cb->targets) / sizeof(Atom))
		return;

	for (i = 0; i < cb->num_targets; i++)
	{
		if (cb->targets[i] == target)
			return;
	}

	cb->targets[cb->num_targets++] = target;
}

static void xf_cliprdr_provide_targets(xfInfo* xfi, XEvent* respond)
{
	clipboardContext* cb = (clipboardContext*) xfi->clipboard_context;

	if (respond->xselection.property != None)
	{
		XChangeProperty(xfi->display,
			respond->xselection.requestor,
			respond->xselection.property,
			XA_ATOM, 32, PropModeReplace,
			(uint8*) cb->targets, cb->num_targets);
	}
}

static void xf_cliprdr_provide_data(xfInfo* xfi, XEvent* respond)
{
	clipboardContext* cb = (clipboardContext*) xfi->clipboard_context;

	if (respond->xselection.property != None)
	{
		XChangeProperty(xfi->display,
			respond->xselection.requestor,
			respond->xselection.property,
			respond->xselection.target, 8, PropModeReplace,
			(uint8*) cb->data, cb->data_length);
	}
}

static void xf_cliprdr_process_cb_format_list_event(xfInfo* xfi, RDP_CB_FORMAT_LIST_EVENT* event)
{
	int i, j;
	clipboardContext* cb = (clipboardContext*) xfi->clipboard_context;

	if (cb->data)
	{
		xfree(cb->data);
		cb->data = NULL;
	}

	if (cb->formats)
		xfree(cb->formats);

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

	XSetSelectionOwner(xfi->display, cb->clipboard_atom, xfi->drawable, CurrentTime);
	if (event->raw_format_data)
	{
		XChangeProperty(xfi->display, cb->root_window, cb->property_atom,
			XA_STRING, 8, PropModeReplace,
			event->raw_format_data, event->raw_format_data_size);
	}

	XFlush(xfi->display);
}

static void xf_cliprdr_process_text(clipboardContext* cb, uint8* data, int size)
{
	cb->data = (uint8*) xmalloc(size);
	memcpy(cb->data, data, size);
	cb->data_length = size;
	crlf2lf(cb->data, &cb->data_length);
}

static void xf_cliprdr_process_unicodetext(clipboardContext* cb, uint8* data, int size)
{
	UNICONV* uniconv;

	uniconv = freerdp_uniconv_new();
	cb->data = (uint8*) freerdp_uniconv_in(uniconv, data, size);
	freerdp_uniconv_free(uniconv);
	cb->data_length = strlen((char*) cb->data);
	crlf2lf(cb->data, &cb->data_length);
}

static void xf_cliprdr_process_dib(clipboardContext* cb, uint8* data, int size)
{
	STREAM* s;
	uint16 bpp;
	uint32 offset;
	uint32 ncolors;

	/* size should be at least sizeof(BITMAPINFOHEADER) */
	if (size < 40)
	{
		DEBUG_X11_CLIPRDR("dib size %d too short", size);
		return;
	}

	s = stream_new(0);
	stream_attach(s, data, size);
	stream_seek(s, 14);
	stream_read_uint16(s, bpp);
	stream_read_uint32(s, ncolors);
	offset = 14 + 40 + (bpp <= 8 ? (ncolors == 0 ? (1 << bpp) : ncolors) * 4 : 0);
	stream_detach(s);
	stream_free(s);

	DEBUG_X11_CLIPRDR("offset=%d bpp=%d ncolors=%d", offset, bpp, ncolors);

	s = stream_new(14 + size);
	stream_write_uint8(s, 'B');
	stream_write_uint8(s, 'M');
	stream_write_uint32(s, 14 + size);
	stream_write_uint32(s, 0);
	stream_write_uint32(s, offset);
	stream_write(s, data, size);

	cb->data = stream_get_head(s);
	cb->data_length = stream_get_length(s);
	stream_detach(s);
	stream_free(s);
}

static void xf_cliprdr_process_html(clipboardContext* cb, uint8* data, int size)
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

	cb->data = (uint8*) xmalloc(size - start + 1);
	memcpy(cb->data, data + start, end - start);
	cb->data_length = end - start;
	crlf2lf(cb->data, &cb->data_length);
}

static void xf_cliprdr_process_cb_data_response_event(xfInfo* xfi, RDP_CB_DATA_RESPONSE_EVENT* event)
{
	clipboardContext* cb = (clipboardContext*) xfi->clipboard_context;

	DEBUG_X11_CLIPRDR("size=%d", event->size);

	if (cb->respond == NULL)
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
			xfree(cb->data);
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
		xf_cliprdr_provide_data(xfi, cb->respond);
	}

	XSendEvent(xfi->display, cb->respond->xselection.requestor, 0, 0, cb->respond);
	XFlush(xfi->display);
	xfree(cb->respond);
	cb->respond = NULL;
}

void xf_process_cliprdr_event(xfInfo* xfi, RDP_EVENT* event)
{
	switch (event->event_type)
	{
		case RDP_EVENT_TYPE_CB_MONITOR_READY:
			xf_cliprdr_process_cb_monitor_ready_event(xfi);
			break;

		case RDP_EVENT_TYPE_CB_FORMAT_LIST:
			xf_cliprdr_process_cb_format_list_event(xfi, (RDP_CB_FORMAT_LIST_EVENT*) event);
			break;

		case RDP_EVENT_TYPE_CB_DATA_REQUEST:
			xf_cliprdr_process_cb_data_request_event(xfi, (RDP_CB_DATA_REQUEST_EVENT*) event);
			break;

		case RDP_EVENT_TYPE_CB_DATA_RESPONSE:
			xf_cliprdr_process_cb_data_response_event(xfi, (RDP_CB_DATA_RESPONSE_EVENT*) event);
			break;

		default:
			DEBUG_X11_CLIPRDR("unknown event type %d", event->event_type);
			break;
	}
}

boolean xf_cliprdr_process_selection_notify(xfInfo* xfi, XEvent* xevent)
{
	clipboardContext* cb = (clipboardContext*) xfi->clipboard_context;

	if (xevent->xselection.target == cb->targets[1])
	{
		if (xevent->xselection.property == None)
		{
			DEBUG_X11_CLIPRDR("owner not support TARGETS. sending all format.");
			xf_cliprdr_send_supported_format_list(xfi);
		}
		else
		{
			xf_cliprdr_get_requested_targets(xfi);
		}

		return true;
	}
	else
	{
		return xf_cliprdr_get_requested_data(xfi, xevent->xselection.target);
	}
}

boolean xf_cliprdr_process_selection_request(xfInfo* xfi, XEvent* xevent)
{
	int i;
	int fmt;
	Atom type;
	uint32 format;
	XEvent* respond;
	uint32 alt_format;
	uint8* data = NULL;
	boolean delay_respond;
	unsigned long length, bytes_left;
	clipboardContext* cb = (clipboardContext*) xfi->clipboard_context;

	DEBUG_X11_CLIPRDR("target=%d", (int) xevent->xselectionrequest.target);

	if (xevent->xselectionrequest.owner != xfi->drawable)
	{
		DEBUG_X11_CLIPRDR("not owner");
		return false;
	}

	delay_respond = false;
	respond = xnew(XEvent);
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
		xf_cliprdr_provide_targets(xfi, respond);
	}
	else
	{
		DEBUG_X11_CLIPRDR("target: other");

		i = xf_cliprdr_select_format_by_atom(cb, xevent->xselectionrequest.target);

		if (i >= 0 && xevent->xselectionrequest.requestor != xfi->drawable)
		{
			format = cb->format_mappings[i].format_id;
			alt_format = format;
			if (format == CB_FORMAT_RAW)
			{
				if (XGetWindowProperty(xfi->display, xevent->xselectionrequest.requestor,
					cb->property_atom, 0, 4, 0, XA_INTEGER,
					&type, &fmt, &length, &bytes_left, &data) != Success)
				{
					DEBUG_X11_CLIPRDR("XGetWindowProperty failed");
				}
				if (data)
				{
					memcpy(&alt_format, data, 4);
					XFree(data);
				}
			}
			DEBUG_X11_CLIPRDR("provide format 0x%04x alt_format 0x%04x", format, alt_format);
			if ((cb->data != 0) && (format == cb->data_format) && (alt_format == cb->data_alt_format))
			{
				/* Cached clipboard data available. Send it now */
				respond->xselection.property = xevent->xselectionrequest.property;
				xf_cliprdr_provide_data(xfi, respond);
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
					xfree(cb->data);
					cb->data = NULL;
				}

				respond->xselection.property = xevent->xselectionrequest.property;
				cb->respond = respond;
				cb->data_format = format;
				cb->data_alt_format = alt_format;
				delay_respond = true;

				xf_cliprdr_send_data_request(xfi, alt_format);
			}
		}
	}

	if (delay_respond == false)
	{
		XSendEvent(xfi->display, xevent->xselectionrequest.requestor, 0, 0, respond);
		XFlush(xfi->display);
		xfree(respond);
	}

	return true;
}

boolean xf_cliprdr_process_selection_clear(xfInfo* xfi, XEvent* xevent)
{
	clipboardContext* cb = (clipboardContext*) xfi->clipboard_context;

	if (xf_cliprdr_is_self_owned(xfi))
		return false;

	XDeleteProperty(xfi->display, cb->root_window, cb->property_atom);

	return true;
}

boolean xf_cliprdr_process_property_notify(xfInfo* xfi, XEvent* xevent)
{
	clipboardContext* cb = (clipboardContext*) xfi->clipboard_context;

	if (xevent->xproperty.atom != cb->property_atom)
		return false; /* Not cliprdr-related */

	if (xevent->xproperty.window == cb->root_window)
	{
		DEBUG_X11_CLIPRDR("root window PropertyNotify");
		xf_cliprdr_send_format_list(xfi);
	}
	else if (xevent->xproperty.window == xfi->drawable &&
		xevent->xproperty.state == PropertyNewValue &&
		cb->incr_starts && cb->request_index >= 0)
	{
		DEBUG_X11_CLIPRDR("cliprdr window PropertyNotify");
		xf_cliprdr_get_requested_data(xfi,
			cb->format_mappings[cb->request_index].target_format);
	}

	return true;
}

void xf_cliprdr_check_owner(xfInfo* xfi)
{
	Window owner;
	clipboardContext* cb = (clipboardContext*) xfi->clipboard_context;

	if (cb->sync)
	{
		owner = XGetSelectionOwner(xfi->display, cb->clipboard_atom);

		if (cb->owner != owner)
		{
			cb->owner = owner;
			xf_cliprdr_send_format_list(xfi);
		}
	}
}

