/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Shadow server clipboard (cliprdr) bridge
 *
 * This implements a text only clipboard bridge for the shadow server, which
 * upstream does not provide for the X11 subsystem at all: the cliprdr virtual
 * channel is not handled, so copy & paste never works.
 *
 * The X11 side is handled with the xclip helper (reading the CLIPBOARD
 * selection and taking ownership of it when the client sends data), the RDP
 * side uses the cliprdr server channel.
 */

#include <freerdp/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/assert.h>
#include <winpr/crt.h>
#include <winpr/string.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/user.h>

#include <freerdp/log.h>
#include <freerdp/channels/cliprdr.h>
#include <freerdp/server/cliprdr.h>

#include "shadow.h"

#include "shadow_cliprdr.h"

#define TAG SERVER_TAG("shadow.cliprdr")

#define SHADOW_CLIPRDR_POLL_MS 500ULL
#define SHADOW_CLIPRDR_MAX_TEXT (8 * 1024 * 1024)

typedef struct
{
	CliprdrServerContext* cliprdr;
	HANDLE thread;
	HANDLE stopEvent;
	CRITICAL_SECTION lock;
	char* text; /* last known clipboard content as UTF-8 */
} ShadowCliprdr;

/* ------------------------------------------------------------------------- */
/* X11 clipboard access (xclip)                                              */
/* ------------------------------------------------------------------------- */

static char* shadow_cliprdr_read_x(void)
{
	FILE* f = nullptr;
	char* buf = nullptr;
	size_t cap = 8192;
	size_t len = 0;

	f = popen("timeout 3 xclip -selection clipboard -o 2>/dev/null", "r");
	if (!f)
		return nullptr;

	buf = (char*)malloc(cap);
	if (!buf)
	{
		pclose(f);
		return nullptr;
	}

	for (;;)
	{
		if ((len + 4096 + 1) > cap)
		{
			char* tmp = nullptr;
			size_t ncap = cap * 2;

			if (cap >= SHADOW_CLIPRDR_MAX_TEXT)
				break;

			tmp = (char*)realloc(buf, ncap);
			if (!tmp)
				break;

			buf = tmp;
			cap = ncap;
		}

		const size_t n = fread(&buf[len], 1, 4096, f);
		if (n == 0)
			break;

		len += n;
	}

	pclose(f);

	if (len == 0)
	{
		free(buf);
		return nullptr;
	}

	buf[len] = '\0';
	return buf;
}

static BOOL shadow_cliprdr_write_x(const char* text, size_t len)
{
	FILE* f = nullptr;
	BOOL rc = FALSE;

	f = popen("xclip -selection clipboard -i 2>/dev/null", "w");
	if (!f)
		return FALSE;

	if (len == 0)
		rc = TRUE;
	else
		rc = (fwrite(text, 1, len, f) == len);

	if (pclose(f) != 0)
		rc = FALSE;

	return rc;
}

static void shadow_cliprdr_set_text(ShadowCliprdr* priv, const char* text)
{
	if (!priv)
		return;

	EnterCriticalSection(&priv->lock);
	free(priv->text);
	priv->text = text ? _strdup(text) : nullptr;
	LeaveCriticalSection(&priv->lock);
}

static char* shadow_cliprdr_get_text(ShadowCliprdr* priv)
{
	char* text = nullptr;

	if (!priv)
		return nullptr;

	EnterCriticalSection(&priv->lock);
	if (priv->text)
		text = _strdup(priv->text);
	LeaveCriticalSection(&priv->lock);

	return text;
}

static UINT shadow_cliprdr_send_format_list(CliprdrServerContext* context)
{
	CLIPRDR_FORMAT format = WINPR_C_ARRAY_INIT;
	CLIPRDR_FORMAT_LIST formatList = WINPR_C_ARRAY_INIT;

	WINPR_ASSERT(context);

	format.formatId = CF_UNICODETEXT;
	format.formatName = nullptr;

	formatList.common.msgType = CB_FORMAT_LIST;
	formatList.common.msgFlags = 0;
	formatList.numFormats = 1;
	formatList.formats = &format;

	return context->ServerFormatList(context, &formatList);
}

/* ------------------------------------------------------------------------- */
/* cliprdr callbacks                                                         */
/* ------------------------------------------------------------------------- */

static UINT shadow_cliprdr_client_capabilities(CliprdrServerContext* context,
                                               const CLIPRDR_CAPABILITIES* capabilities)
{
	WINPR_UNUSED(context);
	WINPR_UNUSED(capabilities);
	return CHANNEL_RC_OK;
}

static UINT shadow_cliprdr_client_format_list(CliprdrServerContext* context,
                                              const CLIPRDR_FORMAT_LIST* formatList)
{
	BOOL haveUnicodeText = FALSE;
	CLIPRDR_FORMAT_DATA_REQUEST request = WINPR_C_ARRAY_INIT;

	WINPR_ASSERT(context);
	WINPR_ASSERT(formatList);

	for (UINT32 i = 0; i < formatList->numFormats; i++)
	{
		if (formatList->formats[i].formatId == CF_UNICODETEXT)
		{
			haveUnicodeText = TRUE;
			break;
		}
	}

	if (!haveUnicodeText)
	{
		WLog_DBG(TAG, "client clipboard has no CF_UNICODETEXT, ignoring");
		return CHANNEL_RC_OK;
	}

	request.common.msgType = CB_FORMAT_DATA_REQUEST;
	request.common.msgFlags = 0;
	request.common.dataLen = 4;
	request.requestedFormatId = CF_UNICODETEXT;

	{
		CLIPRDR_FORMAT_LIST_RESPONSE response = WINPR_C_ARRAY_INIT;
		response.common.msgType = CB_FORMAT_LIST_RESPONSE;
		response.common.msgFlags = CB_RESPONSE_OK;
		const UINT rc = context->ServerFormatListResponse(context, &response);
		if (rc != CHANNEL_RC_OK)
			return rc;
	}

	return context->ServerFormatDataRequest(context, &request);
}

static UINT
shadow_cliprdr_client_format_data_request(CliprdrServerContext* context,
                                          const CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest)
{
	ShadowCliprdr* priv = nullptr;
	char* text = nullptr;
	CLIPRDR_FORMAT_DATA_RESPONSE response = WINPR_C_ARRAY_INIT;

	WINPR_ASSERT(context);
	WINPR_ASSERT(formatDataRequest);

	response.common.msgType = CB_FORMAT_DATA_RESPONSE;

	if (formatDataRequest->requestedFormatId != CF_UNICODETEXT)
	{
		response.common.msgFlags = CB_RESPONSE_FAIL;
		response.common.dataLen = 0;
		response.requestedFormatData = nullptr;
		return context->ServerFormatDataResponse(context, &response);
	}

	priv = (ShadowCliprdr*)context->custom;
	text = shadow_cliprdr_get_text(priv);
	if (!text)
		text = shadow_cliprdr_read_x();

	if (text)
	{
		WCHAR* wtext = nullptr;
		size_t wlen = 0;
		size_t bytes = 0;

		wtext = ConvertUtf8ToWCharAlloc(text, &wlen);
		free(text);

		if (wtext)
		{
			UINT rc = CHANNEL_RC_OK;

			bytes = (_wcslen(wtext) + 1) * sizeof(WCHAR);
			if (bytes > UINT32_MAX)
				bytes = sizeof(WCHAR); /* not going to happen, but be safe */

			response.common.msgFlags = CB_RESPONSE_OK;
			response.common.dataLen = (UINT32)bytes;
			response.requestedFormatData = (const BYTE*)wtext;
			rc = context->ServerFormatDataResponse(context, &response);
			free(wtext);
			return rc;
		}
	}

	response.common.msgFlags = CB_RESPONSE_FAIL;
	response.common.dataLen = 0;
	response.requestedFormatData = nullptr;
	return context->ServerFormatDataResponse(context, &response);
}

static UINT
shadow_cliprdr_client_format_data_response(CliprdrServerContext* context,
                                           const CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse)
{
	ShadowCliprdr* priv = nullptr;
	WCHAR* wtext = nullptr;
	char* text = nullptr;
	size_t wchars = 0;
	UINT32 len = 0;

	WINPR_ASSERT(context);
	WINPR_ASSERT(formatDataResponse);

	priv = (ShadowCliprdr*)context->custom;
	len = formatDataResponse->common.dataLen;

	if (((formatDataResponse->common.msgFlags & CB_RESPONSE_FAIL) != 0) || (len == 0) ||
	    (!formatDataResponse->requestedFormatData) || ((len % sizeof(WCHAR)) != 0))
	{
		WLog_DBG(TAG, "client did not provide any clipboard text");
		return CHANNEL_RC_OK;
	}

	wchars = len / sizeof(WCHAR);
	wtext = (WCHAR*)calloc(wchars + 1, sizeof(WCHAR));
	if (!wtext)
		return CHANNEL_RC_OK;

	memcpy(wtext, formatDataResponse->requestedFormatData, len);
	wtext[wchars] = 0;

	{
		size_t clen = 0;
		text = ConvertWCharToUtf8Alloc(wtext, &clen);
	}
	free(wtext);

	if (!text)
		return CHANNEL_RC_OK;

	shadow_cliprdr_write_x(text, strlen(text));
	/* remember it so the polling thread does not bounce it back to the client */
	shadow_cliprdr_set_text(priv, text);
	free(text);

	WLog_DBG(TAG, "stored client clipboard data in the X11 clipboard");
	return CHANNEL_RC_OK;
}

/* ------------------------------------------------------------------------- */
/* X11 clipboard watcher                                                     */
/* ------------------------------------------------------------------------- */

static DWORD WINAPI shadow_cliprdr_thread(LPVOID arg)
{
	ShadowCliprdr* priv = (ShadowCliprdr*)arg;

	WINPR_ASSERT(priv);

	while (WaitForSingleObject(priv->stopEvent, SHADOW_CLIPRDR_POLL_MS) == WAIT_TIMEOUT)
	{
		char* text = shadow_cliprdr_read_x();
		BOOL changed = FALSE;

		if (!text)
			continue;

		EnterCriticalSection(&priv->lock);
		if (!priv->text || (strcmp(priv->text, text) != 0))
		{
			free(priv->text);
			priv->text = text;
			text = nullptr;
			changed = TRUE;
		}
		LeaveCriticalSection(&priv->lock);

		free(text);

		if (!changed)
			continue;

		if (shadow_cliprdr_send_format_list(priv->cliprdr) != CHANNEL_RC_OK)
		{
			WLog_WARN(TAG, "failed to send clipboard format list to client");
			break;
		}
	}

	return 0;
}

/* ------------------------------------------------------------------------- */
/* init / uninit                                                             */
/* ------------------------------------------------------------------------- */

int shadow_client_cliprdr_init(rdpShadowClient* client)
{
	CliprdrServerContext* cliprdr = nullptr;
	ShadowCliprdr* priv = nullptr;

	WINPR_ASSERT(client);

	if (!WTSVirtualChannelManagerIsChannelJoined(client->vcm, CLIPRDR_SVC_CHANNEL_NAME))
		return 1;

	cliprdr = cliprdr_server_context_new(client->vcm);
	if (!cliprdr)
		return -1;

	priv = (ShadowCliprdr*)calloc(1, sizeof(ShadowCliprdr));
	if (!priv)
	{
		cliprdr_server_context_free(cliprdr);
		return -1;
	}

	priv->cliprdr = cliprdr;
	priv->stopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (!priv->stopEvent)
	{
		free(priv);
		cliprdr_server_context_free(cliprdr);
		return -1;
	}

	InitializeCriticalSection(&priv->lock);
	/* leave priv->text empty: the watcher thread announces the current X11
	   clipboard content to the client on its first poll */

	cliprdr->custom = priv;
	cliprdr->rdpcontext = &client->context;
	cliprdr->ClientCapabilities = shadow_cliprdr_client_capabilities;
	cliprdr->ClientFormatList = shadow_cliprdr_client_format_list;
	cliprdr->ClientFormatDataRequest = shadow_cliprdr_client_format_data_request;
	cliprdr->ClientFormatDataResponse = shadow_cliprdr_client_format_data_response;
	/* note: cliprdr->MonitorReady is the channel's "send CB_MONITOR_READY"
	   function and must not be overwritten */

	if (cliprdr->Start(cliprdr) != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG, "failed to start cliprdr server channel");
		shadow_client_cliprdr_uninit(client);
		return -1;
	}

	priv->thread = CreateThread(nullptr, 0, shadow_cliprdr_thread, priv, 0, nullptr);
	if (!priv->thread)
	{
		WLog_ERR(TAG, "failed to start clipboard watcher thread");
		shadow_client_cliprdr_uninit(client);
		return -1;
	}

	client->cliprdr = cliprdr;
	client->cliprdrPriv = priv;

	return 1;
}

void shadow_client_cliprdr_uninit(rdpShadowClient* client)
{
	ShadowCliprdr* priv = nullptr;
	CliprdrServerContext* cliprdr = nullptr;

	if (!client)
		return;

	cliprdr = client->cliprdr;
	priv = (ShadowCliprdr*)client->cliprdrPriv;

	client->cliprdr = nullptr;
	client->cliprdrPriv = nullptr;

	if (priv)
	{
		if (priv->stopEvent)
			SetEvent(priv->stopEvent);

		if (priv->thread)
		{
			WaitForSingleObject(priv->thread, 2000);
			CloseHandle(priv->thread);
		}

		if (priv->stopEvent)
			CloseHandle(priv->stopEvent);

		free(priv->text);
		DeleteCriticalSection(&priv->lock);
		free(priv);
	}

	if (cliprdr)
	{
		cliprdr->Stop(cliprdr);
		cliprdr_server_context_free(cliprdr);
	}
}
