/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Android Clipboard Redirection
 *
 * Copyright 2013 Felix Long
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2023 Iordan Iordanov
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

#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/clipboard.h>

#include <freerdp/client/channels.h>
#include <freerdp/client/cliprdr.h>

#include "ios_cliprdr.h"

UINT ios_cliprdr_send_client_format_list(CliprdrClientContext *cliprdr)
{
	UINT rc = ERROR_INTERNAL_ERROR;
	UINT32 formatId;
	UINT32 numFormats;
	UINT32 *pFormatIds;
	const char *formatName;
	CLIPRDR_FORMAT *formats;
	CLIPRDR_FORMAT_LIST formatList = { 0 };

	if (!cliprdr)
		return ERROR_INVALID_PARAMETER;

	mfContext *afc = (mfContext *)cliprdr->custom;

	if (!afc || !afc->cliprdr)
		return ERROR_INVALID_PARAMETER;

	pFormatIds = NULL;
	numFormats = ClipboardGetFormatIds(afc->clipboard, &pFormatIds);
	formats = (CLIPRDR_FORMAT *)calloc(numFormats, sizeof(CLIPRDR_FORMAT));

	if (!formats)
		goto fail;

	for (UINT32 index = 0; index < numFormats; index++)
	{
		formatId = pFormatIds[index];
		formatName = ClipboardGetFormatName(afc->clipboard, formatId);
		formats[index].formatId = formatId;
		formats[index].formatName = NULL;

		if ((formatId > CF_MAX) && formatName)
		{
			formats[index].formatName = _strdup(formatName);

			if (!formats[index].formatName)
				goto fail;
		}
	}

	formatList.common.msgFlags = 0;
	formatList.numFormats = numFormats;
	formatList.formats = formats;
	formatList.common.msgType = CB_FORMAT_LIST;

	if (!afc->cliprdr->ClientFormatList)
		goto fail;

	rc = afc->cliprdr->ClientFormatList(afc->cliprdr, &formatList);
fail:
	free(pFormatIds);
	free(formats);
	return rc;
}

static UINT ios_cliprdr_send_client_format_data_request(CliprdrClientContext *cliprdr,
                                                        UINT32 formatId)
{
	UINT rc = ERROR_INVALID_PARAMETER;
	CLIPRDR_FORMAT_DATA_REQUEST formatDataRequest = { 0 };
	mfContext *afc;

	if (!cliprdr)
		goto fail;

	afc = (mfContext *)cliprdr->custom;

	if (!afc || !afc->clipboardRequestEvent || !cliprdr->ClientFormatDataRequest)
		goto fail;

	formatDataRequest.common.msgType = CB_FORMAT_DATA_REQUEST;
	formatDataRequest.common.msgFlags = 0;
	formatDataRequest.requestedFormatId = formatId;
	afc->requestedFormatId = formatId;
	(void)ResetEvent(afc->clipboardRequestEvent);
	rc = cliprdr->ClientFormatDataRequest(cliprdr, &formatDataRequest);
fail:
	return rc;
}

static UINT ios_cliprdr_send_client_capabilities(CliprdrClientContext *cliprdr)
{
	CLIPRDR_CAPABILITIES capabilities;
	CLIPRDR_GENERAL_CAPABILITY_SET generalCapabilitySet;

	if (!cliprdr || !cliprdr->ClientCapabilities)
		return ERROR_INVALID_PARAMETER;

	capabilities.cCapabilitiesSets = 1;
	capabilities.capabilitySets = (CLIPRDR_CAPABILITY_SET *)&(generalCapabilitySet);
	generalCapabilitySet.capabilitySetType = CB_CAPSTYPE_GENERAL;
	generalCapabilitySet.capabilitySetLength = 12;
	generalCapabilitySet.version = CB_CAPS_VERSION_2;
	generalCapabilitySet.generalFlags = CB_USE_LONG_FORMAT_NAMES;
	return cliprdr->ClientCapabilities(cliprdr, &capabilities);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ios_cliprdr_monitor_ready(CliprdrClientContext *cliprdr,
                                      const CLIPRDR_MONITOR_READY *monitorReady)
{
	UINT rc;
	mfContext *afc;

	if (!cliprdr || !monitorReady)
		return ERROR_INVALID_PARAMETER;

	afc = (mfContext *)cliprdr->custom;

	if (!afc)
		return ERROR_INVALID_PARAMETER;

	if ((rc = ios_cliprdr_send_client_capabilities(cliprdr)) != CHANNEL_RC_OK)
		return rc;

	if ((rc = ios_cliprdr_send_client_format_list(cliprdr)) != CHANNEL_RC_OK)
		return rc;

	afc->clipboardSync = TRUE;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ios_cliprdr_server_capabilities(CliprdrClientContext *cliprdr,
                                            const CLIPRDR_CAPABILITIES *capabilities)
{
	CLIPRDR_CAPABILITY_SET *capabilitySet;
	mfContext *afc;

	if (!cliprdr || !capabilities)
		return ERROR_INVALID_PARAMETER;

	afc = (mfContext *)cliprdr->custom;

	if (!afc)
		return ERROR_INVALID_PARAMETER;

	for (UINT32 index = 0; index < capabilities->cCapabilitiesSets; index++)
	{
		capabilitySet = &(capabilities->capabilitySets[index]);

		if ((capabilitySet->capabilitySetType == CB_CAPSTYPE_GENERAL) &&
		    (capabilitySet->capabilitySetLength >= CB_CAPSTYPE_GENERAL_LEN))
		{
			CLIPRDR_GENERAL_CAPABILITY_SET *generalCapabilitySet =
			    (CLIPRDR_GENERAL_CAPABILITY_SET *)capabilitySet;
			afc->clipboardCapabilities = generalCapabilitySet->generalFlags;
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
static UINT ios_cliprdr_server_format_list(CliprdrClientContext *cliprdr,
                                           const CLIPRDR_FORMAT_LIST *formatList)
{
	UINT rc;
	CLIPRDR_FORMAT *format;
	mfContext *afc;

	if (!cliprdr || !formatList)
		return ERROR_INVALID_PARAMETER;

	afc = (mfContext *)cliprdr->custom;

	if (!afc)
		return ERROR_INVALID_PARAMETER;

	if (afc->serverFormats)
	{
		for (UINT32 index = 0; index < afc->numServerFormats; index++)
			free(afc->serverFormats[index].formatName);

		free(afc->serverFormats);
		afc->serverFormats = NULL;
		afc->numServerFormats = 0;
	}

	if (formatList->numFormats < 1)
		return CHANNEL_RC_OK;

	afc->numServerFormats = formatList->numFormats;
	afc->serverFormats = (CLIPRDR_FORMAT *)calloc(afc->numServerFormats, sizeof(CLIPRDR_FORMAT));

	if (!afc->serverFormats)
		return CHANNEL_RC_NO_MEMORY;

	for (UINT32 index = 0; index < afc->numServerFormats; index++)
	{
		afc->serverFormats[index].formatId = formatList->formats[index].formatId;
		afc->serverFormats[index].formatName = NULL;

		if (formatList->formats[index].formatName)
		{
			afc->serverFormats[index].formatName = _strdup(formatList->formats[index].formatName);

			if (!afc->serverFormats[index].formatName)
				return CHANNEL_RC_NO_MEMORY;
		}
	}

	BOOL unicode = FALSE;
	BOOL text = FALSE;
	for (UINT32 index = 0; index < afc->numServerFormats; index++)
	{
		format = &(afc->serverFormats[index]);

		if (format->formatId == CF_UNICODETEXT)
			unicode = TRUE;

		else if (format->formatId == CF_TEXT)
			text = TRUE;
	}

	if (unicode)
		return ios_cliprdr_send_client_format_data_request(cliprdr, CF_UNICODETEXT);
	if (text)
		return ios_cliprdr_send_client_format_data_request(cliprdr, CF_TEXT);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
ios_cliprdr_server_format_list_response(CliprdrClientContext *cliprdr,
                                        const CLIPRDR_FORMAT_LIST_RESPONSE *formatListResponse)
{
	if (!cliprdr || !formatListResponse)
		return ERROR_INVALID_PARAMETER;

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
ios_cliprdr_server_lock_clipboard_data(CliprdrClientContext *cliprdr,
                                       const CLIPRDR_LOCK_CLIPBOARD_DATA *lockClipboardData)
{
	if (!cliprdr || !lockClipboardData)
		return ERROR_INVALID_PARAMETER;

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
ios_cliprdr_server_unlock_clipboard_data(CliprdrClientContext *cliprdr,
                                         const CLIPRDR_UNLOCK_CLIPBOARD_DATA *unlockClipboardData)
{
	if (!cliprdr || !unlockClipboardData)
		return ERROR_INVALID_PARAMETER;

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
ios_cliprdr_server_format_data_request(CliprdrClientContext *cliprdr,
                                       const CLIPRDR_FORMAT_DATA_REQUEST *formatDataRequest)
{
	UINT rc;
	BYTE *data;
	UINT32 size;
	UINT32 formatId;
	CLIPRDR_FORMAT_DATA_RESPONSE response = { 0 };
	mfContext *afc;

	if (!cliprdr || !formatDataRequest || !cliprdr->ClientFormatDataResponse)
		return ERROR_INVALID_PARAMETER;

	afc = (mfContext *)cliprdr->custom;

	if (!afc)
		return ERROR_INVALID_PARAMETER;

	formatId = formatDataRequest->requestedFormatId;
	data = (BYTE *)ClipboardGetData(afc->clipboard, formatId, &size);
	response.common.msgFlags = CB_RESPONSE_OK;
	response.common.dataLen = size;
	response.requestedFormatData = data;

	if (!data)
	{
		response.common.msgFlags = CB_RESPONSE_FAIL;
		response.common.dataLen = 0;
		response.requestedFormatData = NULL;
	}

	rc = cliprdr->ClientFormatDataResponse(cliprdr, &response);
	free(data);
	return rc;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
ios_cliprdr_server_format_data_response(CliprdrClientContext *cliprdr,
                                        const CLIPRDR_FORMAT_DATA_RESPONSE *formatDataResponse)
{
	BYTE *data;
	UINT32 size;
	UINT32 formatId;
	CLIPRDR_FORMAT *format = NULL;
	mfContext *afc;
	freerdp *instance;

	if (!cliprdr || !formatDataResponse)
		return ERROR_INVALID_PARAMETER;

	afc = (mfContext *)cliprdr->custom;

	if (!afc)
		return ERROR_INVALID_PARAMETER;

	instance = ((rdpContext *)afc)->instance;

	if (!instance)
		return ERROR_INVALID_PARAMETER;

	for (UINT32 index = 0; index < afc->numServerFormats; index++)
	{
		if (afc->requestedFormatId == afc->serverFormats[index].formatId)
			format = &(afc->serverFormats[index]);
	}

	if (!format)
	{
		(void)SetEvent(afc->clipboardRequestEvent);
		return ERROR_INTERNAL_ERROR;
	}

	if (format->formatName)
		formatId = ClipboardRegisterFormat(afc->clipboard, format->formatName);
	else
		formatId = format->formatId;

	size = formatDataResponse->common.dataLen;

	ClipboardLock(afc->clipboard);
	if (!ClipboardSetData(afc->clipboard, formatId, formatDataResponse->requestedFormatData, size))
		return ERROR_INTERNAL_ERROR;

	(void)SetEvent(afc->clipboardRequestEvent);

	if ((formatId == CF_TEXT) || (formatId == CF_UNICODETEXT))
	{
		formatId = ClipboardRegisterFormat(afc->clipboard, "UTF8_STRING");
		data = (BYTE *)ClipboardGetData(afc->clipboard, formatId, &size);
		size = (UINT32)strnlen(data, size);
		if (afc->ServerCutText != NULL)
		{
			afc->ServerCutText((rdpContext *)afc, (uint8_t *)data, size);
		}
	}
	ClipboardUnlock(afc->clipboard);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
ios_cliprdr_server_file_contents_request(CliprdrClientContext *cliprdr,
                                         const CLIPRDR_FILE_CONTENTS_REQUEST *fileContentsRequest)
{
	if (!cliprdr || !fileContentsRequest)
		return ERROR_INVALID_PARAMETER;

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ios_cliprdr_server_file_contents_response(
    CliprdrClientContext *cliprdr, const CLIPRDR_FILE_CONTENTS_RESPONSE *fileContentsResponse)
{
	if (!cliprdr || !fileContentsResponse)
		return ERROR_INVALID_PARAMETER;

	return CHANNEL_RC_OK;
}

BOOL ios_cliprdr_init(mfContext *afc, CliprdrClientContext *cliprdr)
{
	wClipboard *clipboard;
	HANDLE hevent;

	if (!afc || !cliprdr)
		return FALSE;

	if (!(hevent = CreateEvent(NULL, TRUE, FALSE, NULL)))
		return FALSE;

	if (!(clipboard = ClipboardCreate()))
	{
		(void)CloseHandle(hevent);
		return FALSE;
	}

	afc->cliprdr = cliprdr;
	afc->clipboard = clipboard;
	afc->clipboardRequestEvent = hevent;
	cliprdr->custom = (void *)afc;
	cliprdr->MonitorReady = ios_cliprdr_monitor_ready;
	cliprdr->ServerCapabilities = ios_cliprdr_server_capabilities;
	cliprdr->ServerFormatList = ios_cliprdr_server_format_list;
	cliprdr->ServerFormatListResponse = ios_cliprdr_server_format_list_response;
	cliprdr->ServerLockClipboardData = ios_cliprdr_server_lock_clipboard_data;
	cliprdr->ServerUnlockClipboardData = ios_cliprdr_server_unlock_clipboard_data;
	cliprdr->ServerFormatDataRequest = ios_cliprdr_server_format_data_request;
	cliprdr->ServerFormatDataResponse = ios_cliprdr_server_format_data_response;
	cliprdr->ServerFileContentsRequest = ios_cliprdr_server_file_contents_request;
	cliprdr->ServerFileContentsResponse = ios_cliprdr_server_file_contents_response;
	return TRUE;
}

BOOL ios_cliprdr_uninit(mfContext *afc, CliprdrClientContext *cliprdr)
{
	if (!afc || !cliprdr)
		return FALSE;

	cliprdr->custom = NULL;
	afc->cliprdr = NULL;
	ClipboardDestroy(afc->clipboard);
	(void)CloseHandle(afc->clipboardRequestEvent);
	return TRUE;
}
