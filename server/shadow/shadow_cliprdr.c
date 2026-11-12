/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * The clipboard of the shared session: the part common to all subsystems.
 *
 * Copyright 2026 Cong Zhang <congzhangzh@gmail.com>
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

/* One clipboard in the session, one in every client. A subsystem talks to this
 * file in mime types, the clients in clipboard formats; the conversions between
 * them come from WinPR's wClipboard.
 *
 *   session copies   subsystem -> session_changed -> Format List to every client
 *   client pastes    Format Data Request -> subsystem->ClipboardRead
 *                    subsystem -> session_data   -> Format Data Response
 *
 *   client copies    Format List -> subsystem->ClipboardOffer
 *   session pastes   subsystem -> session_request -> Format Data Request to the owner
 *                    Format Data Response         -> subsystem->ClipboardProvide
 *
 * Nothing is copied before somebody pastes. The last one to copy owns the
 * clipboard: when a client does, the session offers its data, and the other
 * clients get an empty list, as their last list no longer holds.
 *
 * Data goes only to and from clients that may interact: a viewer sees the screen,
 * not the clipboard. */

#include <winpr/assert.h>
#include <winpr/cast.h>
#include <winpr/clipboard.h>
#include <winpr/collections.h>
#include <winpr/image.h>
#include <winpr/synch.h>
#include <winpr/sysinfo.h>
#include <winpr/user.h>

#include <freerdp/log.h>

#include "shadow.h"
#include "shadow_cliprdr.h"

#define TAG SERVER_TAG("shadow.clipboard")

#if defined(CHANNEL_CLIPRDR_SERVER)

/* How long a client's own Format List may take to follow its capabilities */
#define SHADOW_CLIPBOARD_LIST_WAIT_MS 1000

/* A client that stopped answering must not block the pastes after it. */
#define SHADOW_CLIPBOARD_FETCH_TIMEOUT_MS 10000

#define SHADOW_CLIPBOARD_MIME_HTML "text/html"
#define SHADOW_CLIPBOARD_FORMAT_HTML "HTML Format"

/* the most mime types a client's clipboard turns into */
#define SHADOW_CLIPBOARD_MAX_OFFER 16

typedef struct
{
	rdpShadowClient* client;
	BOOL ready; /* it sent its first Format List: it may get ours */
} ShadowClipboardPeer;

/* a client's paste: the session's data, which the subsystem delivers */
typedef struct
{
	rdpShadowClient* client; /* nullptr once it left */
	UINT32 formatId;
	char* mime;
} ShadowClipboardRead;

/* the session's paste: the owner's data, which that client delivers */
typedef struct
{
	rdpShadowClient* client;
	UINT32 request;
	UINT32 formatId; /* as the client numbers it */
	UINT32 sourceId; /* as wClipboard numbers it */
	UINT32 mimeId;
	BOOL text;
	BOOL canceled; /* the answer is on its way, and nobody wants it */
	UINT64 sent;
} ShadowClipboardFetch;

struct rdp_shadow_clipboard
{
	rdpShadowServer* server;
	CRITICAL_SECTION lock;
	wClipboard* convert;
	UINT32 htmlId;

	wArrayList* peers;

	/* the session's clipboard */
	char** sessionMimes;
	size_t nSessionMimes;

	/* the client whose clipboard the session offers */
	rdpShadowClient* owner;
	CLIPRDR_FORMAT* ownerFormats;
	UINT32 nOwnerFormats;

	wArrayList* reads; /* the first is at the subsystem while readBusy */
	BOOL readBusy;
	wArrayList* fetches; /* the first is at the client while fetchBusy */
	BOOL fetchBusy;
};

static const char* const shadow_clipboard_text_mimes[] = { "text/plain;charset=utf-8",
	                                                       "UTF8_STRING", "text/plain" };

static const UINT32 shadow_clipboard_image_formats[] = { WINPR_IMAGE_PNG, WINPR_IMAGE_BITMAP,
	                                                     WINPR_IMAGE_WEBP, WINPR_IMAGE_JPEG };

static void shadow_clipboard_read_free(void* obj)
{
	ShadowClipboardRead* read = obj;
	if (!read)
		return;
	free(read->mime);
	free(read);
}

static void shadow_clipboard_formats_free(CLIPRDR_FORMAT* formats, UINT32 count)
{
	if (!formats)
		return;
	for (UINT32 x = 0; x < count; x++)
		free(formats[x].formatName);
	free(formats);
}

static void shadow_clipboard_mimes_free(char** mimes, size_t count)
{
	if (!mimes)
		return;
	for (size_t x = 0; x < count; x++)
		free(mimes[x]);
	free((void*)mimes);
}

WINPR_ATTR_NODISCARD
static rdpShadowSubsystem* shadow_clipboard_subsystem(const rdpShadowClipboard* clipboard)
{
	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(clipboard->server);
	return clipboard->server->subsystem;
}

WINPR_ATTR_NODISCARD
static ShadowClipboardPeer* shadow_clipboard_peer(rdpShadowClipboard* clipboard,
                                                  const rdpShadowClient* client)
{
	WINPR_ASSERT(clipboard);

	const size_t count = ArrayList_Count(clipboard->peers);
	for (size_t x = 0; x < count; x++)
	{
		ShadowClipboardPeer* peer = ArrayList_GetItem(clipboard->peers, x);
		if (peer && (peer->client == client))
			return peer;
	}
	return nullptr;
}

WINPR_ATTR_NODISCARD
static BOOL shadow_clipboard_session_has(const rdpShadowClipboard* clipboard, const char* mime)
{
	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(mime);

	for (size_t x = 0; x < clipboard->nSessionMimes; x++)
	{
		if (strcmp(clipboard->sessionMimes[x], mime) == 0)
			return TRUE;
	}
	return FALSE;
}

/* The mime type to read from the session for a format a client asks for */
WINPR_ATTR_NODISCARD
static const char* shadow_clipboard_session_mime(const rdpShadowClipboard* clipboard,
                                                 UINT32 formatId)
{
	WINPR_ASSERT(clipboard);

	if ((formatId == CF_UNICODETEXT) || (formatId == CF_TEXT) || (formatId == CF_OEMTEXT))
	{
		for (size_t x = 0; x < ARRAYSIZE(shadow_clipboard_text_mimes); x++)
		{
			if (shadow_clipboard_session_has(clipboard, shadow_clipboard_text_mimes[x]))
				return shadow_clipboard_text_mimes[x];
		}
	}
	else if ((formatId == CF_DIB) || (formatId == CF_DIBV5))
	{
		for (size_t x = 0; x < ARRAYSIZE(shadow_clipboard_image_formats); x++)
		{
			const UINT32 image = shadow_clipboard_image_formats[x];
			const char* mime = winpr_image_format_mime(image);
			if (mime && winpr_image_format_is_supported(image) &&
			    shadow_clipboard_session_has(clipboard, mime))
				return mime;
		}
	}
	else if ((formatId == clipboard->htmlId) &&
	         shadow_clipboard_session_has(clipboard, SHADOW_CLIPBOARD_MIME_HTML))
		return SHADOW_CLIPBOARD_MIME_HTML;

	return nullptr;
}

WINPR_ATTR_NODISCARD
static UINT shadow_clipboard_send_list(const rdpShadowClipboard* clipboard, rdpShadowClient* client)
{
	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(client);
	WINPR_ASSERT(client->cliprdr);

	char htmlName[] = SHADOW_CLIPBOARD_FORMAT_HTML;
	CLIPRDR_FORMAT formats[3] = WINPR_C_ARRAY_INIT;
	UINT32 count = 0;

	if (shadow_clipboard_session_mime(clipboard, CF_UNICODETEXT))
		formats[count++].formatId = CF_UNICODETEXT;
	if (shadow_clipboard_session_mime(clipboard, clipboard->htmlId))
	{
		formats[count].formatId = clipboard->htmlId;
		formats[count++].formatName = htmlName;
	}
	if (shadow_clipboard_session_mime(clipboard, CF_DIB))
		formats[count++].formatId = CF_DIB;

	CLIPRDR_FORMAT_LIST list = WINPR_C_ARRAY_INIT;
	list.common.msgType = CB_FORMAT_LIST;
	list.numFormats = count;
	list.formats = formats;
	return client->cliprdr->ServerFormatList(client->cliprdr, &list);
}

/* To every client but one, which may be nullptr: what the session offers now */
static void shadow_clipboard_announce(rdpShadowClipboard* clipboard, const rdpShadowClient* except)
{
	WINPR_ASSERT(clipboard);

	const size_t count = ArrayList_Count(clipboard->peers);
	for (size_t x = 0; x < count; x++)
	{
		const ShadowClipboardPeer* peer = ArrayList_GetItem(clipboard->peers, x);
		if (!peer || !peer->ready || (peer->client == except) || !peer->client->mayInteract)
			continue;

		const UINT rc = shadow_clipboard_send_list(clipboard, peer->client);
		if (rc != CHANNEL_RC_OK)
			WLog_WARN(TAG, "ServerFormatList failed with %" PRIu32, rc);
	}
}

/* @return the data in the other format, to be freed, or nullptr */
WINPR_ATTR_NODISCARD
static BYTE* shadow_clipboard_convert(rdpShadowClipboard* clipboard, UINT32 sourceId,
                                      UINT32 targetId, const BYTE* data, size_t size,
                                      UINT32* pConverted)
{
	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(pConverted);

	*pConverted = 0;
	if (!data || (size == 0) || (size > SHADOW_CLIPBOARD_MAX_SIZE))
		return nullptr;

	if (!ClipboardEmpty(clipboard->convert))
		return nullptr;
	const BOOL stored = ClipboardSetData(clipboard->convert, sourceId, data,
	                                     WINPR_ASSERTING_INT_CAST(UINT32, size));
	if (!stored)
		return nullptr;
	return ClipboardGetData(clipboard->convert, targetId, pConverted);
}

static void shadow_clipboard_respond(CliprdrServerContext* cliprdr, const BYTE* data, UINT32 size)
{
	WINPR_ASSERT(cliprdr);

	CLIPRDR_FORMAT_DATA_RESPONSE response = WINPR_C_ARRAY_INIT;
	response.common.msgType = CB_FORMAT_DATA_RESPONSE;
	response.common.msgFlags = data ? CB_RESPONSE_OK : CB_RESPONSE_FAIL;
	response.common.dataLen = data ? size : 0;
	response.requestedFormatData = data;

	const UINT rc = cliprdr->ServerFormatDataResponse(cliprdr, &response);
	if (rc != CHANNEL_RC_OK)
		WLog_WARN(TAG, "ServerFormatDataResponse failed with %" PRIu32, rc);
}

/* Reads go to the subsystem one at a time, each one's answer starts the next */
static void shadow_clipboard_read_next(rdpShadowClipboard* clipboard)
{
	WINPR_ASSERT(clipboard);

	while (!clipboard->readBusy && (ArrayList_Count(clipboard->reads) > 0))
	{
		ShadowClipboardRead* read = ArrayList_GetItem(clipboard->reads, 0);
		WINPR_ASSERT(read);

		rdpShadowSubsystem* subsystem = shadow_clipboard_subsystem(clipboard);
		if (read->client && subsystem->ClipboardRead(subsystem, read->mime))
		{
			clipboard->readBusy = TRUE;
			return;
		}

		if (read->client)
			shadow_clipboard_respond(read->client->cliprdr, nullptr, 0);
		if (!ArrayList_RemoveAt(clipboard->reads, 0))
			return;
	}
}

static void shadow_clipboard_provide(rdpShadowClipboard* clipboard, UINT32 request,
                                     const BYTE* data, size_t size)
{
	WINPR_ASSERT(clipboard);

	rdpShadowSubsystem* subsystem = shadow_clipboard_subsystem(clipboard);
	if (!subsystem->ClipboardProvide(subsystem, request, data, size))
		WLog_WARN(TAG, "the subsystem did not take the data of request %" PRIu32, request);
}

/* Fetches go to the owner one at a time: a Format Data Response names no request */
static void shadow_clipboard_fetch_next(rdpShadowClipboard* clipboard)
{
	WINPR_ASSERT(clipboard);

	while (!clipboard->fetchBusy && (ArrayList_Count(clipboard->fetches) > 0))
	{
		ShadowClipboardFetch* fetch = ArrayList_GetItem(clipboard->fetches, 0);
		WINPR_ASSERT(fetch);
		WINPR_ASSERT(fetch->client);
		WINPR_ASSERT(fetch->client->cliprdr);

		CLIPRDR_FORMAT_DATA_REQUEST request = WINPR_C_ARRAY_INIT;
		request.common.msgType = CB_FORMAT_DATA_REQUEST;
		request.common.dataLen = 4;
		request.requestedFormatId = fetch->formatId;

		CliprdrServerContext* cliprdr = fetch->client->cliprdr;
		const UINT rc = cliprdr->ServerFormatDataRequest(cliprdr, &request);
		if (rc == CHANNEL_RC_OK)
		{
			fetch->sent = GetTickCount64();
			clipboard->fetchBusy = TRUE;
			return;
		}

		WLog_WARN(TAG, "ServerFormatDataRequest failed with %" PRIu32, rc);
		shadow_clipboard_provide(clipboard, fetch->request, nullptr, 0);
		if (!ArrayList_RemoveAt(clipboard->fetches, 0))
			return;
	}
}

/* The owner changes: no fetch waiting will be served. A client's answer to the
 * one it has still arrives (unless the client left): it stays to catch that. */
static void shadow_clipboard_cancel_fetches(rdpShadowClipboard* clipboard,
                                            const rdpShadowClient* gone)
{
	WINPR_ASSERT(clipboard);

	for (size_t x = ArrayList_Count(clipboard->fetches); x > 0; x--)
	{
		const size_t index = x - 1;
		ShadowClipboardFetch* fetch = ArrayList_GetItem(clipboard->fetches, index);
		WINPR_ASSERT(fetch);

		const BOOL atClient = (index == 0) && clipboard->fetchBusy;
		if (atClient && (fetch->client != gone))
		{
			if (!fetch->canceled)
				shadow_clipboard_provide(clipboard, fetch->request, nullptr, 0);
			fetch->canceled = TRUE;
			continue;
		}

		if (atClient)
			clipboard->fetchBusy = FALSE;
		if (!fetch->canceled)
			shadow_clipboard_provide(clipboard, fetch->request, nullptr, 0);
		if (!ArrayList_RemoveAt(clipboard->fetches, index))
			WLog_WARN(TAG, "cannot drop a clipboard request");
	}
}

static void shadow_clipboard_clear_owner(rdpShadowClipboard* clipboard)
{
	WINPR_ASSERT(clipboard);

	shadow_clipboard_formats_free(clipboard->ownerFormats, clipboard->nOwnerFormats);
	clipboard->ownerFormats = nullptr;
	clipboard->nOwnerFormats = 0;
	clipboard->owner = nullptr;
}

/* What the session offers is gone with its owner: a paste must not hang on it */
static void shadow_clipboard_release_owner(rdpShadowClipboard* clipboard)
{
	WINPR_ASSERT(clipboard);

	shadow_clipboard_clear_owner(clipboard);

	rdpShadowSubsystem* subsystem = shadow_clipboard_subsystem(clipboard);
	if (!subsystem->ClipboardOffer(subsystem, nullptr, 0))
		WLog_WARN(TAG, "the session keeps a clipboard nobody serves");
}

WINPR_ATTR_NODISCARD
static const CLIPRDR_FORMAT* shadow_clipboard_find_format(const CLIPRDR_FORMAT* formats,
                                                          UINT32 count, UINT32 formatId,
                                                          const char* name)
{
	for (UINT32 x = 0; x < count; x++)
	{
		const CLIPRDR_FORMAT* format = &formats[x];
		if (name)
		{
			if (format->formatName && (strcmp(format->formatName, name) == 0))
				return format;
		}
		else if (format->formatId == formatId)
			return format;
	}
	return nullptr;
}

WINPR_ATTR_NODISCARD
static const CLIPRDR_FORMAT* shadow_clipboard_owner_text(const CLIPRDR_FORMAT* formats,
                                                         UINT32 count)
{
	const UINT32 ids[] = { CF_UNICODETEXT, CF_TEXT, CF_OEMTEXT };
	for (size_t x = 0; x < ARRAYSIZE(ids); x++)
	{
		const CLIPRDR_FORMAT* format =
		    shadow_clipboard_find_format(formats, count, ids[x], nullptr);
		if (format)
			return format;
	}
	return nullptr;
}

WINPR_ATTR_NODISCARD
static const CLIPRDR_FORMAT* shadow_clipboard_owner_image(const CLIPRDR_FORMAT* formats,
                                                          UINT32 count)
{
	const CLIPRDR_FORMAT* format = shadow_clipboard_find_format(formats, count, CF_DIB, nullptr);
	if (format)
		return format;
	return shadow_clipboard_find_format(formats, count, CF_DIBV5, nullptr);
}

/* What a client's list means in the session. @return the number of mime types */
WINPR_ATTR_NODISCARD
static size_t shadow_clipboard_list_mimes(const CLIPRDR_FORMAT* formats, UINT32 count,
                                          const char* mimes[SHADOW_CLIPBOARD_MAX_OFFER])
{
	size_t offered = 0;

	if (shadow_clipboard_owner_text(formats, count))
	{
		for (size_t x = 0; x < ARRAYSIZE(shadow_clipboard_text_mimes); x++)
			mimes[offered++] = shadow_clipboard_text_mimes[x];
	}

	if (shadow_clipboard_find_format(formats, count, 0, SHADOW_CLIPBOARD_FORMAT_HTML))
		mimes[offered++] = SHADOW_CLIPBOARD_MIME_HTML;

	if (shadow_clipboard_owner_image(formats, count))
	{
		for (size_t x = 0; x < ARRAYSIZE(shadow_clipboard_image_formats); x++)
		{
			const UINT32 image = shadow_clipboard_image_formats[x];
			const char* mime = winpr_image_format_mime(image);
			if (mime && winpr_image_format_is_supported(image))
				mimes[offered++] = mime;
		}
	}

	WINPR_ASSERT(offered <= SHADOW_CLIPBOARD_MAX_OFFER);
	return offered;
}

WINPR_ATTR_NODISCARD
static BOOL shadow_clipboard_set_owner(rdpShadowClipboard* clipboard, rdpShadowClient* client,
                                       const CLIPRDR_FORMAT_LIST* list, const char* const* mimes,
                                       size_t count)
{
	WINPR_ASSERT(clipboard);
	WINPR_ASSERT(client);
	WINPR_ASSERT(list);

	CLIPRDR_FORMAT* formats = calloc(list->numFormats, sizeof(CLIPRDR_FORMAT));
	if (!formats)
		return FALSE;

	for (UINT32 x = 0; x < list->numFormats; x++)
	{
		const CLIPRDR_FORMAT* format = &list->formats[x];
		formats[x].formatId = format->formatId;
		if (format->formatName)
		{
			formats[x].formatName = _strdup(format->formatName);
			if (!formats[x].formatName)
			{
				shadow_clipboard_formats_free(formats, list->numFormats);
				return FALSE;
			}
		}
	}

	shadow_clipboard_cancel_fetches(clipboard, nullptr);
	shadow_clipboard_clear_owner(clipboard);
	clipboard->owner = client;
	clipboard->ownerFormats = formats;
	clipboard->nOwnerFormats = list->numFormats;

	shadow_clipboard_mimes_free(clipboard->sessionMimes, clipboard->nSessionMimes);
	clipboard->sessionMimes = nullptr;
	clipboard->nSessionMimes = 0;

	shadow_clipboard_announce(clipboard, client);

	rdpShadowSubsystem* subsystem = shadow_clipboard_subsystem(clipboard);
	return subsystem->ClipboardOffer(subsystem, mimes, count);
}

WINPR_ATTR_NODISCARD
static UINT shadow_clipboard_client_format_list(CliprdrServerContext* context,
                                                const CLIPRDR_FORMAT_LIST* list)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(list);

	rdpShadowClient* client = context->custom;
	WINPR_ASSERT(client);

	CLIPRDR_FORMAT_LIST_RESPONSE response = WINPR_C_ARRAY_INIT;
	response.common.msgType = CB_FORMAT_LIST_RESPONSE;
	response.common.msgFlags = CB_RESPONSE_OK;
	const UINT rc = context->ServerFormatListResponse(context, &response);
	if (rc != CHANNEL_RC_OK)
		return rc;

	rdpShadowClipboard* clipboard = client->server->clipboard;
	WINPR_ASSERT(clipboard);

	EnterCriticalSection(&clipboard->lock);
	ShadowClipboardPeer* peer = shadow_clipboard_peer(clipboard, client);
	if (peer && client->mayInteract)
	{
		const BOOL first = !peer->ready;
		peer->ready = TRUE;

		const char* mimes[SHADOW_CLIPBOARD_MAX_OFFER] = WINPR_C_ARRAY_INIT;
		const size_t count = shadow_clipboard_list_mimes(list->formats, list->numFormats, mimes);
		if (count > 0)
		{
			if (!shadow_clipboard_set_owner(clipboard, client, list, mimes, count))
				WLog_WARN(TAG, "the session did not take the client's clipboard");
		}
		else if (clipboard->owner == client)
		{
			/* its clipboard is empty, or holds nothing the session can use */
			shadow_clipboard_cancel_fetches(clipboard, nullptr);
			shadow_clipboard_release_owner(clipboard);
		}
		else if (first && (clipboard->nSessionMimes > 0))
		{
			/* a client that connects with nothing to offer gets the session's */
			const UINT sent = shadow_clipboard_send_list(clipboard, client);
			if (sent != CHANNEL_RC_OK)
				WLog_WARN(TAG, "ServerFormatList failed with %" PRIu32, sent);
		}
	}
	LeaveCriticalSection(&clipboard->lock);
	return CHANNEL_RC_OK;
}

/* A client's capabilities follow Monitor Ready. A list of its own usually comes right
 * after them, and a client with nothing to offer may send none (FreeRDP's own does not).
 * Our list must not cross one on the wire: it would replace the clipboard the client
 * just announced. The channel's thread runs this: anything to read wakes it, and the
 * list then goes out with shadow_clipboard_client_format_list(). */
WINPR_ATTR_NODISCARD
static UINT shadow_clipboard_client_capabilities(CliprdrServerContext* context,
                                                 WINPR_ATTR_UNUSED const CLIPRDR_CAPABILITIES* caps)
{
	WINPR_ASSERT(context);

	rdpShadowClient* client = context->custom;
	WINPR_ASSERT(client);
	rdpShadowClipboard* clipboard = client->server->clipboard;
	WINPR_ASSERT(clipboard);

	const DWORD waited =
	    WaitForSingleObject(context->GetEventHandle(context), SHADOW_CLIPBOARD_LIST_WAIT_MS);
	if (waited != WAIT_TIMEOUT)
		return CHANNEL_RC_OK;

	EnterCriticalSection(&clipboard->lock);
	ShadowClipboardPeer* peer = shadow_clipboard_peer(clipboard, client);
	if (peer && !peer->ready && client->mayInteract)
	{
		peer->ready = TRUE;
		if (clipboard->nSessionMimes > 0)
		{
			const UINT sent = shadow_clipboard_send_list(clipboard, client);
			if (sent != CHANNEL_RC_OK)
				WLog_WARN(TAG, "ServerFormatList failed with %" PRIu32, sent);
		}
	}
	LeaveCriticalSection(&clipboard->lock);
	return CHANNEL_RC_OK;
}

WINPR_ATTR_NODISCARD
static UINT shadow_clipboard_client_data_request(CliprdrServerContext* context,
                                                 const CLIPRDR_FORMAT_DATA_REQUEST* request)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(request);

	rdpShadowClient* client = context->custom;
	WINPR_ASSERT(client);
	rdpShadowClipboard* clipboard = client->server->clipboard;
	WINPR_ASSERT(clipboard);

	BOOL queued = FALSE;
	EnterCriticalSection(&clipboard->lock);
	const char* mime = shadow_clipboard_session_mime(clipboard, request->requestedFormatId);
	if (mime && client->mayInteract && shadow_clipboard_peer(clipboard, client))
	{
		ShadowClipboardRead* read = calloc(1, sizeof(ShadowClipboardRead));
		if (read)
		{
			read->client = client;
			read->formatId = request->requestedFormatId;
			read->mime = _strdup(mime);
			queued = read->mime && ArrayList_Append(clipboard->reads, read);
			if (!queued)
				shadow_clipboard_read_free(read);
		}
	}

	// NOLINTNEXTLINE(clang-analyzer-unix.Malloc): fetch owned by clipboard->reads
	if (queued)
		shadow_clipboard_read_next(clipboard);
	LeaveCriticalSection(&clipboard->lock);

	if (!queued)
		shadow_clipboard_respond(context, nullptr, 0);
	return CHANNEL_RC_OK;
}

WINPR_ATTR_NODISCARD
static UINT shadow_clipboard_client_data_response(CliprdrServerContext* context,
                                                  const CLIPRDR_FORMAT_DATA_RESPONSE* response)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(response);

	rdpShadowClient* client = context->custom;
	WINPR_ASSERT(client);
	rdpShadowClipboard* clipboard = client->server->clipboard;
	WINPR_ASSERT(clipboard);

	EnterCriticalSection(&clipboard->lock);
	ShadowClipboardFetch* fetch =
	    clipboard->fetchBusy ? ArrayList_GetItem(clipboard->fetches, 0) : nullptr;
	if (fetch && (fetch->client == client))
	{
		clipboard->fetchBusy = FALSE;

		if (!fetch->canceled)
		{
			const BOOL ok = (response->common.msgFlags & CB_RESPONSE_OK) != 0;
			UINT32 size = 0;
			BYTE* data = nullptr;
			if (ok)
				data = shadow_clipboard_convert(clipboard, fetch->sourceId, fetch->mimeId,
				                                response->requestedFormatData,
				                                response->common.dataLen, &size);
			WLog_DBG(TAG, "client data: %" PRIu32 " bytes, %" PRIu32 " for the session",
			         response->common.dataLen, size);

			/* a Windows string ends in a NUL, text in a pipe does not */
			if (data && fetch->text)
				size = WINPR_ASSERTING_INT_CAST(UINT32, strnlen((const char*)data, size));
			shadow_clipboard_provide(clipboard, fetch->request, data, size);
			free(data);
		}

		if (!ArrayList_RemoveAt(clipboard->fetches, 0))
			WLog_WARN(TAG, "cannot drop a clipboard request");
		shadow_clipboard_fetch_next(clipboard);
	}
	LeaveCriticalSection(&clipboard->lock);
	return CHANNEL_RC_OK;
}

#endif /* CHANNEL_CLIPRDR_SERVER */

BOOL shadow_clipboard_session_changed(rdpShadowServer* server, const char* const* mimeTypes,
                                      size_t count)
{
	WINPR_ASSERT(server);
	WINPR_ASSERT(mimeTypes || (count == 0));

#if defined(CHANNEL_CLIPRDR_SERVER)
	rdpShadowClipboard* clipboard = server->clipboard;
	if (!clipboard)
		return FALSE;

	char** mimes = nullptr;
	if (count > 0)
	{
		mimes = (char**)calloc(count, sizeof(char*));
		if (!mimes)
			return FALSE;
		for (size_t x = 0; x < count; x++)
		{
			mimes[x] = _strdup(mimeTypes[x]);
			if (!mimes[x])
			{
				shadow_clipboard_mimes_free(mimes, count);
				return FALSE;
			}
		}
	}

	EnterCriticalSection(&clipboard->lock);
	shadow_clipboard_cancel_fetches(clipboard, nullptr);
	shadow_clipboard_clear_owner(clipboard);
	shadow_clipboard_mimes_free(clipboard->sessionMimes, clipboard->nSessionMimes);
	clipboard->sessionMimes = mimes;
	clipboard->nSessionMimes = count;
	shadow_clipboard_announce(clipboard, nullptr);
	LeaveCriticalSection(&clipboard->lock);
	return TRUE;
#else
	return FALSE;
#endif
}

BOOL shadow_clipboard_session_data(rdpShadowServer* server, const BYTE* data, size_t size)
{
	WINPR_ASSERT(server);

#if defined(CHANNEL_CLIPRDR_SERVER)
	rdpShadowClipboard* clipboard = server->clipboard;
	if (!clipboard)
		return FALSE;

	BOOL rc = FALSE;
	EnterCriticalSection(&clipboard->lock);
	const ShadowClipboardRead* read =
	    clipboard->readBusy ? ArrayList_GetItem(clipboard->reads, 0) : nullptr;
	if (read)
	{
		rc = TRUE;
		clipboard->readBusy = FALSE;

		if (read->client)
		{
			const UINT32 mimeId = ClipboardRegisterFormat(clipboard->convert, read->mime);
			UINT32 converted = 0;
			BYTE* payload =
			    shadow_clipboard_convert(clipboard, mimeId, read->formatId, data, size, &converted);
			WLog_DBG(TAG, "session data: %" PRIuz " bytes of %s, %" PRIu32 " for the client", size,
			         read->mime, converted);
			shadow_clipboard_respond(read->client->cliprdr, payload, converted);
			free(payload);
		}

		if (!ArrayList_RemoveAt(clipboard->reads, 0))
			WLog_WARN(TAG, "cannot drop a clipboard read");
		shadow_clipboard_read_next(clipboard);
	}
	LeaveCriticalSection(&clipboard->lock);
	return rc;
#else
	WINPR_UNUSED(data);
	WINPR_UNUSED(size);
	return FALSE;
#endif
}

BOOL shadow_clipboard_session_request(rdpShadowServer* server, UINT32 request, const char* mimeType)
{
	WINPR_ASSERT(server);
	WINPR_ASSERT(mimeType);

#if defined(CHANNEL_CLIPRDR_SERVER)
	rdpShadowClipboard* clipboard = server->clipboard;
	if (!clipboard)
		return FALSE;

	EnterCriticalSection(&clipboard->lock);

	/* an owner that stopped answering */
	ShadowClipboardFetch* first =
	    clipboard->fetchBusy ? ArrayList_GetItem(clipboard->fetches, 0) : nullptr;
	if (first && (GetTickCount64() - first->sent >= SHADOW_CLIPBOARD_FETCH_TIMEOUT_MS))
	{
		WLog_WARN(TAG, "no clipboard data from the client");
		clipboard->fetchBusy = FALSE;
		if (!first->canceled)
			shadow_clipboard_provide(clipboard, first->request, nullptr, 0);
		if (!ArrayList_RemoveAt(clipboard->fetches, 0))
			WLog_WARN(TAG, "cannot drop a clipboard request");
	}

	const CLIPRDR_FORMAT* source = nullptr;
	if (clipboard->owner)
	{
		const CLIPRDR_FORMAT* formats = clipboard->ownerFormats;
		const UINT32 count = clipboard->nOwnerFormats;
		if (strcmp(mimeType, SHADOW_CLIPBOARD_MIME_HTML) == 0)
			source = shadow_clipboard_find_format(formats, count, 0, SHADOW_CLIPBOARD_FORMAT_HTML);
		else if (strncmp(mimeType, "image/", 6) == 0)
			source = shadow_clipboard_owner_image(formats, count);
		else
			source = shadow_clipboard_owner_text(formats, count);
	}

	BOOL queued = FALSE;
	if (source)
	{
		ShadowClipboardFetch* fetch = calloc(1, sizeof(ShadowClipboardFetch));
		if (fetch)
		{
			fetch->client = clipboard->owner;
			fetch->request = request;
			fetch->formatId = source->formatId;
			fetch->sourceId = source->formatName
			                      ? ClipboardRegisterFormat(clipboard->convert, source->formatName)
			                      : source->formatId;
			fetch->mimeId = ClipboardRegisterFormat(clipboard->convert, mimeType);
			fetch->text = strncmp(mimeType, "image/", 6) != 0;
			queued = ArrayList_Append(clipboard->fetches, fetch);
			if (!queued)
				free(fetch);
		}
	}

	// NOLINTNEXTLINE(clang-analyzer-unix.Malloc): fetch owned by clipboard->fetches
	if (queued)
		shadow_clipboard_fetch_next(clipboard);
	else
		shadow_clipboard_provide(clipboard, request, nullptr, 0);
	LeaveCriticalSection(&clipboard->lock);
	return TRUE;
#else
	WINPR_UNUSED(request);
	return FALSE;
#endif
}

#if defined(CHANNEL_CLIPRDR_SERVER)

rdpShadowClipboard* shadow_clipboard_new(rdpShadowServer* server)
{
	WINPR_ASSERT(server);

	rdpShadowClipboard* clipboard = calloc(1, sizeof(rdpShadowClipboard));
	if (!clipboard)
		return nullptr;

	clipboard->server = server;
	if (!InitializeCriticalSectionAndSpinCount(&clipboard->lock, 4000))
	{
		free(clipboard);
		return nullptr;
	}

	clipboard->convert = ClipboardCreate();
	clipboard->peers = ArrayList_New(FALSE);
	clipboard->reads = ArrayList_New(FALSE);
	clipboard->fetches = ArrayList_New(FALSE);
	if (!clipboard->convert || !clipboard->peers || !clipboard->reads || !clipboard->fetches)
		goto fail;

	wObject* peers = ArrayList_Object(clipboard->peers);
	WINPR_ASSERT(peers);
	peers->fnObjectFree = free;
	wObject* reads = ArrayList_Object(clipboard->reads);
	WINPR_ASSERT(reads);
	reads->fnObjectFree = shadow_clipboard_read_free;
	wObject* fetches = ArrayList_Object(clipboard->fetches);
	WINPR_ASSERT(fetches);
	fetches->fnObjectFree = free;

	clipboard->htmlId = ClipboardRegisterFormat(clipboard->convert, SHADOW_CLIPBOARD_FORMAT_HTML);
	if (clipboard->htmlId == 0)
		goto fail;
	return clipboard;

fail:
	shadow_clipboard_free(clipboard);
	return nullptr;
}

void shadow_clipboard_free(rdpShadowClipboard* clipboard)
{
	if (!clipboard)
		return;

	ArrayList_Free(clipboard->fetches);
	ArrayList_Free(clipboard->reads);
	ArrayList_Free(clipboard->peers);
	shadow_clipboard_formats_free(clipboard->ownerFormats, clipboard->nOwnerFormats);
	shadow_clipboard_mimes_free(clipboard->sessionMimes, clipboard->nSessionMimes);
	ClipboardDestroy(clipboard->convert);
	DeleteCriticalSection(&clipboard->lock);
	free(clipboard);
}

int shadow_client_cliprdr_init(rdpShadowClient* client)
{
	WINPR_ASSERT(client);
	WINPR_ASSERT(client->server);

	rdpShadowClipboard* clipboard = client->server->clipboard;
	const rdpShadowSubsystem* subsystem = client->subsystem;
	WINPR_ASSERT(subsystem);

	if (!clipboard || !subsystem->ClipboardOffer || !subsystem->ClipboardRead ||
	    !subsystem->ClipboardProvide)
		return 0;

	CliprdrServerContext* cliprdr = cliprdr_server_context_new(client->vcm);
	if (!cliprdr)
		return -1;

	cliprdr->custom = client;
	cliprdr->rdpcontext = &client->context;
	cliprdr->autoInitializationSequence = TRUE;
	cliprdr->useLongFormatNames = TRUE;
	cliprdr->ClientCapabilities = shadow_clipboard_client_capabilities;
	cliprdr->ClientFormatList = shadow_clipboard_client_format_list;
	cliprdr->ClientFormatDataRequest = shadow_clipboard_client_data_request;
	cliprdr->ClientFormatDataResponse = shadow_clipboard_client_data_response;

	ShadowClipboardPeer* peer = calloc(1, sizeof(ShadowClipboardPeer));
	if (!peer)
	{
		cliprdr_server_context_free(cliprdr);
		return -1;
	}
	peer->client = client;
	client->cliprdr = cliprdr;

	EnterCriticalSection(&clipboard->lock);
	const BOOL added = ArrayList_Append(clipboard->peers, peer);
	LeaveCriticalSection(&clipboard->lock);
	if (!added)
	{
		free(peer);
		client->cliprdr = nullptr;
		cliprdr_server_context_free(cliprdr);
		return -1;
	}

	// NOLINTNEXTLINE(clang-analyzer-unix.Malloc): peer owned by clipboard->peers
	const UINT rc = cliprdr->Start(cliprdr);
	if (rc != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG, "cannot start the clipboard channel: %" PRIu32, rc);
		return -1; /* shadow_client_cliprdr_uninit() cleans up */
	}
	return 1;
}

void shadow_client_cliprdr_uninit(rdpShadowClient* client)
{
	WINPR_ASSERT(client);
	WINPR_ASSERT(client->server);

	CliprdrServerContext* cliprdr = client->cliprdr;
	if (!cliprdr)
		return;

	rdpShadowClipboard* clipboard = client->server->clipboard;
	WINPR_ASSERT(clipboard);

	EnterCriticalSection(&clipboard->lock);
	ShadowClipboardPeer* peer = shadow_clipboard_peer(clipboard, client);
	if (peer && !ArrayList_Remove(clipboard->peers, peer))
		WLog_WARN(TAG, "cannot drop a client of the clipboard");

	for (size_t x = ArrayList_Count(clipboard->reads); x > 0; x--)
	{
		const size_t index = x - 1;
		ShadowClipboardRead* read = ArrayList_GetItem(clipboard->reads, index);
		WINPR_ASSERT(read);
		if (read->client != client)
			continue;

		/* the one at the subsystem stays until its answer */
		if ((index == 0) && clipboard->readBusy)
			read->client = nullptr;
		else if (!ArrayList_RemoveAt(clipboard->reads, index))
			WLog_WARN(TAG, "cannot drop a clipboard read");
	}

	if (clipboard->owner == client)
	{
		shadow_clipboard_cancel_fetches(clipboard, client);
		shadow_clipboard_release_owner(clipboard);
	}
	else if (clipboard->fetchBusy)
	{
		/* an answer it still owed from when it was the owner */
		const ShadowClipboardFetch* fetch = ArrayList_GetItem(clipboard->fetches, 0);
		if (fetch && (fetch->client == client))
		{
			clipboard->fetchBusy = FALSE;
			if (!ArrayList_RemoveAt(clipboard->fetches, 0))
				WLog_WARN(TAG, "cannot drop a clipboard request");
			shadow_clipboard_fetch_next(clipboard);
		}
	}
	LeaveCriticalSection(&clipboard->lock);

	/* outside the lock: its thread may wait for it */
	const UINT rc = cliprdr->Stop(cliprdr);
	if (rc != CHANNEL_RC_OK)
		WLog_WARN(TAG, "cannot stop the clipboard channel: %" PRIu32, rc);
	cliprdr_server_context_free(cliprdr);
	client->cliprdr = nullptr;
}

#else /* CHANNEL_CLIPRDR_SERVER */

rdpShadowClipboard* shadow_clipboard_new(WINPR_ATTR_UNUSED rdpShadowServer* server)
{
	return nullptr;
}

void shadow_clipboard_free(WINPR_ATTR_UNUSED rdpShadowClipboard* clipboard)
{
}

int shadow_client_cliprdr_init(WINPR_ATTR_UNUSED rdpShadowClient* client)
{
	return 0;
}

void shadow_client_cliprdr_uninit(WINPR_ATTR_UNUSED rdpShadowClient* client)
{
}

#endif /* CHANNEL_CLIPRDR_SERVER */
