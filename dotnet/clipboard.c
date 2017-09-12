
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/stream.h>

#include <freerdp/log.h>
#include <freerdp/client/channels.h>
#include <freerdp/client/cliprdr.h>

#include "clipboard.h"

int cs_cliprdr_send_client_format_list(CliprdrClientContext* cliprdr)
{
	UINT32 index;
	UINT32 formatId;
	UINT32 numFormats;
	UINT32* pFormatIds;
	const char* formatName;
	CLIPRDR_FORMAT* formats;
	CLIPRDR_FORMAT_LIST formatList;
	csContext* ctx = (csContext*) cliprdr->custom;

	ZeroMemory(&formatList, sizeof(CLIPRDR_FORMAT_LIST));

	pFormatIds = NULL;
	numFormats = ClipboardGetFormatIds(ctx->clipboard, &pFormatIds);

	formats = (CLIPRDR_FORMAT*) calloc(numFormats, sizeof(CLIPRDR_FORMAT));

	if (!formats)
		return -1;

	for (index = 0; index < numFormats; index++)
	{
		formatId = pFormatIds[index];
		formatName = ClipboardGetFormatName(ctx->clipboard, formatId);

		formats[index].formatId = formatId;
		formats[index].formatName = NULL;

		if ((formatId > CF_MAX) && formatName)
			formats[index].formatName = _strdup(formatName);
	}

	formatList.msgFlags = CB_RESPONSE_OK;
	formatList.numFormats = numFormats;
	formatList.formats = formats;

	ctx->cliprdr->ClientFormatList(ctx->cliprdr, &formatList);

	free(pFormatIds);
	free(formats);

	return 1;
}

int cs_cliprdr_send_client_format_data_request(CliprdrClientContext* cliprdr, UINT32 formatId)
{
	CLIPRDR_FORMAT_DATA_REQUEST formatDataRequest;
	csContext* ctx = (csContext*) cliprdr->custom;

	ZeroMemory(&formatDataRequest, sizeof(CLIPRDR_FORMAT_DATA_REQUEST));

	formatDataRequest.msgType = CB_FORMAT_DATA_REQUEST;
	formatDataRequest.msgFlags = 0;

	formatDataRequest.requestedFormatId = formatId;
	ctx->requestedFormatId = formatId;
	ResetEvent(ctx->clipboardRequestEvent);

	cliprdr->ClientFormatDataRequest(cliprdr, &formatDataRequest);

	return 1;
}

int cs_cliprdr_send_client_capabilities(CliprdrClientContext* cliprdr)
{
	CLIPRDR_CAPABILITIES capabilities;
	CLIPRDR_GENERAL_CAPABILITY_SET generalCapabilitySet;

	capabilities.cCapabilitiesSets = 1;
	capabilities.capabilitySets = (CLIPRDR_CAPABILITY_SET*) &(generalCapabilitySet);

	generalCapabilitySet.capabilitySetType = CB_CAPSTYPE_GENERAL;
	generalCapabilitySet.capabilitySetLength = 12;

	generalCapabilitySet.version = CB_CAPS_VERSION_2;
	generalCapabilitySet.generalFlags = CB_USE_LONG_FORMAT_NAMES;

	cliprdr->ClientCapabilities(cliprdr, &capabilities);

	return 1;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT cs_cliprdr_monitor_ready(CliprdrClientContext* cliprdr, CLIPRDR_MONITOR_READY* monitorReady)
{
	csContext* ctx = (csContext*) cliprdr->custom;

	ctx->clipboardSync = TRUE;
	cs_cliprdr_send_client_capabilities(cliprdr);
	cs_cliprdr_send_client_format_list(cliprdr);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT cs_cliprdr_server_capabilities(CliprdrClientContext* cliprdr, CLIPRDR_CAPABILITIES* capabilities)
{
	UINT32 index;
	CLIPRDR_CAPABILITY_SET* capabilitySet;
	csContext* ctx = (csContext*) cliprdr->custom;

	for (index = 0; index < capabilities->cCapabilitiesSets; index++)
	{
		capabilitySet = &(capabilities->capabilitySets[index]);

		if ((capabilitySet->capabilitySetType == CB_CAPSTYPE_GENERAL) &&
		    (capabilitySet->capabilitySetLength >= CB_CAPSTYPE_GENERAL_LEN))
		{
			CLIPRDR_GENERAL_CAPABILITY_SET* generalCapabilitySet
			= (CLIPRDR_GENERAL_CAPABILITY_SET*) capabilitySet;

			ctx->clipboardCapabilities = generalCapabilitySet->generalFlags;
			break;
		}
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT cs_cliprdr_server_format_list(CliprdrClientContext* cliprdr, CLIPRDR_FORMAT_LIST* formatList)
{
	UINT32 index;
	CLIPRDR_FORMAT* format;
	csContext* ctx = (csContext*) cliprdr->custom;

	if (ctx->serverFormats)
	{
		for (index = 0; index < ctx->numServerFormats; index++)
		{
			free(ctx->serverFormats[index].formatName);
		}

		free(ctx->serverFormats);
		ctx->serverFormats = NULL;
		ctx->numServerFormats = 0;
	}

	if (formatList->numFormats < 1)
		return CHANNEL_RC_OK;

	ctx->numServerFormats = formatList->numFormats;
	ctx->serverFormats = (CLIPRDR_FORMAT*) calloc(ctx->numServerFormats, sizeof(CLIPRDR_FORMAT));

	if (!ctx->serverFormats)
		return CHANNEL_RC_NO_MEMORY;

	for (index = 0; index < ctx->numServerFormats; index++)
	{
		ctx->serverFormats[index].formatId = formatList->formats[index].formatId;
		ctx->serverFormats[index].formatName = NULL;

		if (formatList->formats[index].formatName)
			ctx->serverFormats[index].formatName = _strdup(formatList->formats[index].formatName);
	}

	for (index = 0; index < ctx->numServerFormats; index++)
	{
		format = &(ctx->serverFormats[index]);

		if (format->formatId == CF_UNICODETEXT)
		{
			cs_cliprdr_send_client_format_data_request(cliprdr, CF_UNICODETEXT);
			break;
		}
		else if (format->formatId == CF_TEXT)
		{
			cs_cliprdr_send_client_format_data_request(cliprdr, CF_TEXT);
			break;
		}
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT cs_cliprdr_server_format_list_response(CliprdrClientContext* cliprdr, CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse)
{
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT cs_cliprdr_server_lock_clipboard_data(CliprdrClientContext* cliprdr, CLIPRDR_LOCK_CLIPBOARD_DATA* lockClipboardData)
{
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT cs_cliprdr_server_unlock_clipboard_data(CliprdrClientContext* cliprdr, CLIPRDR_UNLOCK_CLIPBOARD_DATA* unlockClipboardData)
{
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT cs_cliprdr_server_format_data_request(CliprdrClientContext* cliprdr, CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest)
{
	BYTE* data;
	UINT32 size;
	UINT32 formatId;
	CLIPRDR_FORMAT_DATA_RESPONSE response;
	csContext* ctx = (csContext*) cliprdr->custom;

	ZeroMemory(&response, sizeof(CLIPRDR_FORMAT_DATA_RESPONSE));

	formatId = formatDataRequest->requestedFormatId;
	data = (BYTE*) ClipboardGetData(ctx->clipboard, formatId, &size);

	response.msgFlags = CB_RESPONSE_OK;
	response.dataLen = size;
	response.requestedFormatData = data;

	if (!data)
	{
		response.msgFlags = CB_RESPONSE_FAIL;
		response.dataLen = 0;
		response.requestedFormatData = NULL;
	}

	cliprdr->ClientFormatDataResponse(cliprdr, &response);

	free(data);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT cs_cliprdr_server_format_data_response(CliprdrClientContext* cliprdr, CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse)
{
	BYTE* data;
	UINT32 size;
	UINT32 index;
	UINT32 formatId;
	CLIPRDR_FORMAT* format = NULL;
	csContext* ctx = (csContext*) cliprdr->custom;
	freerdp* instance = ((rdpContext*) ctx)->instance;

	for (index = 0; index < ctx->numServerFormats; index++)
	{
		if (ctx->requestedFormatId == ctx->serverFormats[index].formatId)
			format = &(ctx->serverFormats[index]);
	}

	if (!format)
	{
		SetEvent(ctx->clipboardRequestEvent);
		return ERROR_INTERNAL_ERROR;
	}

	if (format->formatName)
		formatId = ClipboardRegisterFormat(ctx->clipboard, format->formatName);
	else
		formatId = format->formatId;

	size = formatDataResponse->dataLen;
	data = (BYTE*) malloc(size);
	CopyMemory(data, formatDataResponse->requestedFormatData, size);

	ClipboardSetData(ctx->clipboard, formatId, data, size);

	SetEvent(ctx->clipboardRequestEvent);

	formatId = ClipboardRegisterFormat(ctx->clipboard, "UTF8_STRING");
	data = (void*) ClipboardGetData(ctx->clipboard, formatId, &size);

	if(ctx->onClipboardUpdate)
		ctx->onClipboardUpdate(instance, data, size);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT cs_cliprdr_server_file_contents_request(CliprdrClientContext* cliprdr, CLIPRDR_FILE_CONTENTS_REQUEST* fileContentsRequest)
{
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT cs_cliprdr_server_file_contents_response(CliprdrClientContext* cliprdr, CLIPRDR_FILE_CONTENTS_RESPONSE* fileContentsResponse)
{
	return CHANNEL_RC_OK;
}

int cs_cliprdr_init(csContext* ctx, CliprdrClientContext* cliprdr)
{
	wClipboard* clipboard;
	HANDLE hevent;

	if (!(hevent = CreateEvent(NULL, TRUE, FALSE, NULL)))
		return 0;

	if (!(clipboard = ClipboardCreate()))
	{
		CloseHandle(hevent);
		return 0;
	}

	ctx->cliprdr = cliprdr;
	ctx->clipboard = clipboard;
	ctx->clipboardRequestEvent = hevent;

	cliprdr->custom = (void*) ctx;
	cliprdr->MonitorReady = cs_cliprdr_monitor_ready;
	cliprdr->ServerCapabilities = cs_cliprdr_server_capabilities;
	cliprdr->ServerFormatList = cs_cliprdr_server_format_list;
	cliprdr->ServerFormatListResponse = cs_cliprdr_server_format_list_response;
	cliprdr->ServerLockClipboardData = cs_cliprdr_server_lock_clipboard_data;
	cliprdr->ServerUnlockClipboardData = cs_cliprdr_server_unlock_clipboard_data;
	cliprdr->ServerFormatDataRequest = cs_cliprdr_server_format_data_request;
	cliprdr->ServerFormatDataResponse = cs_cliprdr_server_format_data_response;
	cliprdr->ServerFileContentsRequest = cs_cliprdr_server_file_contents_request;
	cliprdr->ServerFileContentsResponse = cs_cliprdr_server_file_contents_response;

	return 1;
}

int cs_cliprdr_uninit(csContext* ctx, CliprdrClientContext* cliprdr)
{
	cliprdr->custom = NULL;
	ctx->cliprdr = NULL;

	ClipboardDestroy(ctx->clipboard);
	CloseHandle(ctx->clipboardRequestEvent);

	return 1;
}
