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

	Window root_window;
	Atom clipboard_atom;
	Atom property_atom;
	Atom identity_atom;

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

int xf_cliprdr_send_client_format_list(xfClipboard* clipboard);

static BYTE* ConvertLineEndingToCRLF(BYTE* str, int* size)
{
	BYTE c;
	BYTE* outbuf;
	BYTE* out;
	BYTE* in_end;
	BYTE* in;
	int out_size;

	out_size = (*size) * 2 + 1;
	outbuf = (BYTE*) malloc(out_size);

	out = outbuf;
	in = str;
	in_end = str + (*size);

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

static int ConvertLineEndingToLF(BYTE* str, int length)
{
	BYTE c;
	BYTE* out;
	BYTE* in;
	BYTE* in_end;
	int status = -1;

	out = str;
	in = str;
	in_end = &str[length];

	while (in < in_end)
	{
		c = *in++;

		if (c != '\r')
			*out++ = c;
	}

	status = out - str;

	return status;
}

static void ByteSwapUnicode(WCHAR* wstr, int length)
{
	WCHAR* end = &wstr[length];

	while (wstr < end)
	{
		*wstr = _byteswap_ushort(*wstr);
		wstr++;
	}
}

static BOOL xf_cliprdr_is_self_owned(xfClipboard* clipboard)
{
	Atom type;
	UINT32 id = 0;
	UINT32* pid = NULL;
	int format, result = 0;
	unsigned long length;
	unsigned long bytes_left;
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

		if (format->formatId == CLIPRDR_FORMAT_RAW)
			return format;

		for (j = 0; j < clipboard->numServerFormats; j++)
		{
			if (clipboard->serverFormats[j].formatId == format->formatId)
				return format;
		}
	}

	return NULL;
}

static void xf_cliprdr_send_data_request(xfClipboard* clipboard, UINT32 formatId)
{
	CLIPRDR_FORMAT_DATA_REQUEST request;

	ZeroMemory(&request, sizeof(CLIPRDR_FORMAT_DATA_REQUEST));

	request.requestedFormatId = formatId;

	clipboard->context->ClientFormatDataRequest(clipboard->context, &request);
}

static void xf_cliprdr_send_data_response(xfClipboard* clipboard, BYTE* data, int size)
{
	CLIPRDR_FORMAT_DATA_RESPONSE response;

	ZeroMemory(&response, sizeof(CLIPRDR_FORMAT_DATA_RESPONSE));

	response.msgFlags = CB_RESPONSE_OK;
	response.dataLen = size;
	response.requestedFormatData = data;

	clipboard->context->ClientFormatDataResponse(clipboard->context, &response);
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
		formats = (CLIPRDR_FORMAT*) calloc(length, sizeof(CLIPRDR_FORMAT));

	for (i = 0; i < length; i++)
	{
		atom = ((Atom*) data)[i];

		format = xf_cliprdr_get_format_by_atom(clipboard, atom);

		if (format)
		{
			formats[numFormats].formatId = format->formatId;
			formats[numFormats].formatName = NULL;
			numFormats++;
		}
	}

	XFree(data);

	ZeroMemory(&formatList, sizeof(CLIPRDR_FORMAT_LIST));

	formatList.msgFlags = CB_RESPONSE_OK;
	formatList.cFormats = numFormats;
	formatList.formats = formats;

	clipboard->context->ClientFormatList(clipboard->context, &formatList);
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

	inbuf = (char*) ConvertLineEndingToCRLF(data, size);
	out_size = ConvertToUnicode(CP_UTF8, 0, inbuf, -1, &outbuf, 0);
	free(inbuf);

	*size = (int) ((out_size + 1) * 2);

	return (BYTE*) outbuf;
}

static BYTE* xf_cliprdr_process_requested_text(BYTE* data, int* size)
{
	BYTE* outbuf;

	outbuf = ConvertLineEndingToCRLF(data, size);

	return outbuf;
}

static BYTE* xf_cliprdr_process_requested_dib(BYTE* data, int* size)
{
	BYTE* outbuf;

	/* length should be at least BMP header (14) + sizeof(BITMAPINFOHEADER) */

	if (*size < 54)
		return NULL;

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
		BYTE bom[2];

		CopyMemory(bom, data, 2);

		if ((bom[0] == 0xFE) && (bom[1] == 0xFF))
		{
			ByteSwapUnicode((WCHAR*) data, *size / 2);
		}

		if ((bom[0] == 0xFF) && (bom[1] == 0xFE))
		{
			ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) (data + 2),
					(*size - 2) / 2, &inbuf, 0, NULL, NULL);
		}
	}

	if (!inbuf)
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

	if (!in)
		in = (BYTE*) strstr((char*) inbuf, "<BODY");

	/* StartHTML */
	snprintf(num, sizeof(num), "%010lu", (unsigned long) strlen((char*) outbuf));
	CopyMemory(outbuf + 23, num, 10);

	if (!in)
		strcat((char*) outbuf, "<HTML><BODY>");

	strcat((char*) outbuf, "<!--StartFragment-->");
	/* StartFragment */
	snprintf(num, sizeof(num), "%010lu", (unsigned long) strlen((char*) outbuf));
	CopyMemory(outbuf + 69, num, 10);
	strcat((char*) outbuf, (char*) inbuf);
	/* EndFragment */
	snprintf(num, sizeof(num), "%010lu", (unsigned long) strlen((char*) outbuf));
	CopyMemory(outbuf + 93, num, 10);
	strcat((char*) outbuf, "<!--EndFragment-->");

	if (!in)
		strcat((char*) outbuf, "</BODY></HTML>");

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
	xfCliprdrFormat* format;

	if (clipboard->incr_starts && has_data)
		return;

	format = xf_cliprdr_get_format_by_id(clipboard, clipboard->requestedFormatId);

	if (!has_data || !data || !format)
	{
		xf_cliprdr_send_data_response(clipboard, NULL, 0);
		return;
	}

	switch (format->formatId)
	{
		case CLIPRDR_FORMAT_RAW:
		case CB_FORMAT_PNG:
		case CB_FORMAT_JPEG:
		case CB_FORMAT_GIF:
			outbuf = xf_cliprdr_process_requested_raw(data, &size);
			break;

		case CLIPRDR_FORMAT_UNICODETEXT:
			outbuf = xf_cliprdr_process_requested_unicodetext(data, &size);
			break;

		case CLIPRDR_FORMAT_TEXT:
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
		xf_cliprdr_send_data_response(clipboard, NULL, 0);

	if (!clipboard->xfixes_supported)
	{
		/* Resend the format list, otherwise the server won't request again for the next paste */
		xf_cliprdr_send_client_format_list(clipboard);
	}
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
				bytes_left = length * format_property / 8;
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

static void xf_cliprdr_provide_data(xfClipboard* clipboard, XEvent* respond)
{
	xfContext* xfc = clipboard->xfc;

	if (respond->xselection.property != None)
	{
		XChangeProperty(xfc->display, respond->xselection.requestor,
			respond->xselection.property, respond->xselection.target, 8, PropModeReplace,
			(BYTE*) clipboard->data, clipboard->data_length);
	}
}

static int xf_cliprdr_process_text(BYTE* pSrcData, int SrcSize, BYTE** ppDstData)
{
	int DstSize = -1;
	BYTE* pDstData = NULL;

	pDstData = (BYTE*) malloc(SrcSize);
	CopyMemory(pDstData, pSrcData, SrcSize);

	DstSize = SrcSize;
	DstSize = ConvertLineEndingToLF(pDstData, DstSize);

	*ppDstData = pDstData;

	return DstSize;
}

static int xf_cliprdr_process_unicodetext(BYTE* pSrcData, int SrcSize, BYTE** ppDstData)
{
	int DstSize = -1;
	BYTE* pDstData = NULL;

	DstSize = ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) pSrcData,
			SrcSize / 2, (CHAR**) &pDstData, 0, NULL, NULL);

	DstSize = ConvertLineEndingToLF(pDstData, DstSize);

	*ppDstData = pDstData;

	return DstSize;
}

static int xf_cliprdr_process_dib(BYTE* pSrcData, int SrcSize, BYTE** ppDstData)
{
	wStream* s;
	UINT16 bpp;
	UINT32 offset;
	UINT32 ncolors;
	int DstSize = -1;
	BYTE* pDstData = NULL;

	/* size should be at least sizeof(BITMAPINFOHEADER) */

	if (SrcSize < 40)
		return -1;

	s = Stream_New(pSrcData, SrcSize);
	Stream_Seek(s, 14);
	Stream_Read_UINT16(s, bpp);

	if ((bpp < 1) || (bpp > 32))
		return -1;

	Stream_Read_UINT32(s, ncolors);
	offset = 14 + 40 + (bpp <= 8 ? (ncolors == 0 ? (1 << bpp) : ncolors) * 4 : 0);
	Stream_Free(s, FALSE);

	s = Stream_New(NULL, 14 + SrcSize);
	Stream_Write_UINT8(s, 'B');
	Stream_Write_UINT8(s, 'M');
	Stream_Write_UINT32(s, 14 + SrcSize);
	Stream_Write_UINT32(s, 0);
	Stream_Write_UINT32(s, offset);
	Stream_Write(s, pSrcData, SrcSize);

	pDstData = Stream_Buffer(s);
	DstSize = Stream_GetPosition(s);
	Stream_Free(s, FALSE);

	*ppDstData = pDstData;

	return DstSize;
}

static int xf_cliprdr_process_html(BYTE* pSrcData, int SrcSize, BYTE** ppDstData)
{
	int start;
	int end;
	char* start_str;
	char* end_str;
	int DstSize = -1;
	BYTE* pDstData = NULL;

	start_str = strstr((char*) pSrcData, "StartHTML:");
	end_str = strstr((char*) pSrcData, "EndHTML:");

	if (!start_str || !end_str)
		return -1;

	start = atoi(start_str + 10);
	end = atoi(end_str + 8);

	if ((start > SrcSize) || (end > SrcSize) || (start >= end))
		return -1;

	DstSize = end - start;

	pDstData = (BYTE*) malloc(SrcSize - start + 1);
	CopyMemory(pDstData, pSrcData + start, DstSize);
	DstSize = ConvertLineEndingToLF(pDstData, DstSize);

	*ppDstData = pDstData;

	return DstSize;
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

			if (formatId == CLIPRDR_FORMAT_RAW)
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
				xf_cliprdr_provide_data(clipboard, respond);
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
	UINT32 i, numFormats;
	CLIPRDR_FORMAT* formats;
	CLIPRDR_FORMAT_LIST formatList;
	xfContext* xfc = clipboard->xfc;

	ZeroMemory(&formatList, sizeof(CLIPRDR_FORMAT_LIST));

	numFormats = clipboard->numClientFormats;
	formats = (CLIPRDR_FORMAT*) calloc(numFormats, sizeof(CLIPRDR_FORMAT));

	for (i = 0; i < numFormats; i++)
	{
		formats[i].formatId = clipboard->clientFormats[i].formatId;
		formats[i].formatName = clipboard->clientFormats[i].formatName;
	}

	formatList.msgFlags = CB_RESPONSE_OK;
	formatList.cFormats = numFormats;
	formatList.formats = formats;

	clipboard->context->ClientFormatList(clipboard->context, &formatList);

	free(formats);

	if (clipboard->owner != xfc->drawable)
	{
		/* Request the owner for TARGETS, and wait for SelectionNotify event */
		XConvertSelection(xfc->display, clipboard->clipboard_atom,
			clipboard->targets[1], clipboard->property_atom, xfc->drawable, CurrentTime);
	}

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

static int xf_cliprdr_monitor_ready(CliprdrClientContext* context, CLIPRDR_MONITOR_READY* monitorReady)
{
	xfClipboard* clipboard = (xfClipboard*) context->custom;

	xf_cliprdr_send_client_capabilities(clipboard);
	xf_cliprdr_send_client_format_list(clipboard);
	clipboard->sync = TRUE;

	return 1;
}

static int xf_cliprdr_server_capabilities(CliprdrClientContext* context, CLIPRDR_CAPABILITIES* capabilities)
{
	//xfClipboard* clipboard = (xfClipboard*) context->custom;

	return 1;
}

static int xf_cliprdr_server_format_list(CliprdrClientContext* context, CLIPRDR_FORMAT_LIST* formatList)
{
	int i, j;
	CLIPRDR_FORMAT* format;
	xfClipboard* clipboard = (xfClipboard*) context->custom;
	xfContext* xfc = clipboard->xfc;

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

	clipboard->numServerFormats = formatList->cFormats;
	clipboard->serverFormats = (CLIPRDR_FORMAT*) calloc(clipboard->numServerFormats, sizeof(CLIPRDR_FORMAT));

	if (!clipboard->serverFormats)
		return -1;

	for (i = 0; i < formatList->cFormats; i++)
	{
		format = &formatList->formats[i];
		clipboard->serverFormats[i].formatId = format->formatId;
		clipboard->serverFormats[i].formatName = _strdup(format->formatName);
	}

	clipboard->numTargets = 2;

	for (i = 0; i < formatList->cFormats; i++)
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

	xf_cliprdr_send_client_format_list_response(clipboard, TRUE);

	XSetSelectionOwner(xfc->display, clipboard->clipboard_atom, xfc->drawable, CurrentTime);

	XFlush(xfc->display);

	return 1;
}

static int xf_cliprdr_server_format_list_response(CliprdrClientContext* context, CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse)
{
	//xfClipboard* clipboard = (xfClipboard*) context->custom;

	return 1;
}

static int xf_cliprdr_server_format_data_request(CliprdrClientContext* context, CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest)
{
	xfCliprdrFormat* format = NULL;
	UINT32 formatId = formatDataRequest->requestedFormatId;
	xfClipboard* clipboard = (xfClipboard*) context->custom;
	xfContext* xfc = clipboard->xfc;

	if (xf_cliprdr_is_self_owned(clipboard))
	{
		format = xf_cliprdr_get_format_by_id(clipboard, CLIPRDR_FORMAT_RAW);

		XChangeProperty(xfc->display, xfc->drawable, clipboard->property_atom,
			XA_INTEGER, 32, PropModeReplace, (BYTE*) &formatId, 1);
	}
	else
	{
		format = xf_cliprdr_get_format_by_id(clipboard, formatId);
	}

	if (!format)
	{
		xf_cliprdr_send_data_response(clipboard, NULL, 0);
		return 1;
	}

	clipboard->requestedFormatId = formatId;

	XConvertSelection(xfc->display, clipboard->clipboard_atom,
		format->atom, clipboard->property_atom, xfc->drawable, CurrentTime);

	XFlush(xfc->display);

	/* After this point, we expect a SelectionNotify event from the clipboard owner. */

	return 1;
}

static int xf_cliprdr_server_format_data_response(CliprdrClientContext* context, CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse)
{
	int status;
	BYTE* pDstData;
	UINT32 size = formatDataResponse->dataLen;
	BYTE* data = formatDataResponse->requestedFormatData;
	xfClipboard* clipboard = (xfClipboard*) context->custom;
	xfContext* xfc = clipboard->xfc;

	if (!clipboard->respond)
		return 1;

	if (size < 1)
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

		status = -1;
		pDstData = NULL;

		switch (clipboard->data_format)
		{
			case CLIPRDR_FORMAT_RAW:
			case CB_FORMAT_PNG:
			case CB_FORMAT_JPEG:
			case CB_FORMAT_GIF:
				status = size;
				clipboard->data = data;
				clipboard->data_length = size;
				data = NULL;
				size = 0;
				break;

			case CLIPRDR_FORMAT_TEXT:
				status = xf_cliprdr_process_text(data, size, &pDstData);
				break;

			case CLIPRDR_FORMAT_UNICODETEXT:
				status = xf_cliprdr_process_unicodetext(data, size, &pDstData);
				break;

			case CLIPRDR_FORMAT_DIB:
				status = xf_cliprdr_process_dib(data, size, &pDstData);
				break;

			case CB_FORMAT_HTML:
				status = xf_cliprdr_process_html(data, size, &pDstData);
				break;

			default:
				clipboard->respond->xselection.property = None;
				break;
		}

		if (status < 1)
		{
			clipboard->data = NULL;
			clipboard->data_length = 0;
		}
		else
		{
			clipboard->data = pDstData;
			clipboard->data_length = status;
		}

		xf_cliprdr_provide_data(clipboard, clipboard->respond);
	}

	XSendEvent(xfc->display, clipboard->respond->xselection.requestor, 0, 0, clipboard->respond);
	XFlush(xfc->display);

	free(clipboard->respond);
	clipboard->respond = NULL;

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

	clipboard->requestedFormatId = -1;

	clipboard->root_window = DefaultRootWindow(xfc->display);
	clipboard->clipboard_atom = XInternAtom(xfc->display, "CLIPBOARD", FALSE);

	if (clipboard->clipboard_atom == None)
	{
		WLog_ERR(TAG, "unable to get CLIPBOARD atom");
		free(clipboard);
		return NULL;
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

	clipboard->clientFormats[n].atom = XInternAtom(xfc->display, "_FREERDP_RAW", FALSE);
	clipboard->clientFormats[n].formatId = CLIPRDR_FORMAT_RAW;
	n++;

	clipboard->clientFormats[n].atom = XInternAtom(xfc->display, "UTF8_STRING", FALSE);
	clipboard->clientFormats[n].formatId = CLIPRDR_FORMAT_UNICODETEXT;
	n++;

	clipboard->clientFormats[n].atom = XA_STRING;
	clipboard->clientFormats[n].formatId = CLIPRDR_FORMAT_TEXT;
	n++;

	clipboard->clientFormats[n].atom = XInternAtom(xfc->display, "image/png", FALSE);
	clipboard->clientFormats[n].formatId = CB_FORMAT_PNG;
	n++;

	clipboard->clientFormats[n].atom = XInternAtom(xfc->display, "image/jpeg", FALSE);
	clipboard->clientFormats[n].formatId = CB_FORMAT_JPEG;
	n++;

	clipboard->clientFormats[n].atom = XInternAtom(xfc->display, "image/gif", FALSE);
	clipboard->clientFormats[n].formatId = CB_FORMAT_GIF;
	n++;

	clipboard->clientFormats[n].atom = XInternAtom(xfc->display, "image/bmp", FALSE);
	clipboard->clientFormats[n].formatId = CLIPRDR_FORMAT_DIB;
	n++;

	clipboard->clientFormats[n].atom = XInternAtom(xfc->display, "text/html", FALSE);
	clipboard->clientFormats[n].formatId = CB_FORMAT_HTML;
	clipboard->clientFormats[n].formatName = _strdup("HTML Format");
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
