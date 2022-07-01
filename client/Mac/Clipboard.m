/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#import "Clipboard.h"
#import "MRDPView.h"

int mac_cliprdr_send_client_format_list(CliprdrClientContext *cliprdr)
{
	UINT32 index;
	UINT32 formatId;
	UINT32 numFormats;
	UINT32 *pFormatIds;
	const char *formatName;
	CLIPRDR_FORMAT *formats;
	CLIPRDR_FORMAT_LIST formatList = { 0 };
	mfContext *mfc = (mfContext *)cliprdr->custom;

	ZeroMemory(&formatList, sizeof(CLIPRDR_FORMAT_LIST));

	pFormatIds = NULL;
	numFormats = ClipboardGetFormatIds(mfc->clipboard, &pFormatIds);

	formats = (CLIPRDR_FORMAT *)calloc(numFormats, sizeof(CLIPRDR_FORMAT));

	if (!formats)
		return -1;

	for (index = 0; index < numFormats; index++)
	{
		formatId = pFormatIds[index];
		formatName = ClipboardGetFormatName(mfc->clipboard, formatId);

		formats[index].formatId = formatId;
		formats[index].formatName = NULL;

		if ((formatId > CF_MAX) && formatName)
			formats[index].formatName = _strdup(formatName);
	}

	formatList.common.msgFlags = CB_RESPONSE_OK;
	formatList.numFormats = numFormats;
	formatList.formats = formats;
	formatList.common.msgType = CB_FORMAT_LIST;

	mfc->cliprdr->ClientFormatList(mfc->cliprdr, &formatList);

	for (index = 0; index < numFormats; index++)
	{
		free(formats[index].formatName);
	}

	free(pFormatIds);
	free(formats);

	return 1;
}

static int mac_cliprdr_send_client_format_list_response(CliprdrClientContext *cliprdr, BOOL status)
{
	CLIPRDR_FORMAT_LIST_RESPONSE formatListResponse;

	formatListResponse.common.msgType = CB_FORMAT_LIST_RESPONSE;
	formatListResponse.common.msgFlags = status ? CB_RESPONSE_OK : CB_RESPONSE_FAIL;
	formatListResponse.common.dataLen = 0;

	cliprdr->ClientFormatListResponse(cliprdr, &formatListResponse);

	return 1;
}

static int mac_cliprdr_send_client_format_data_request(CliprdrClientContext *cliprdr,
                                                       UINT32 formatId)
{
	CLIPRDR_FORMAT_DATA_REQUEST formatDataRequest;
	mfContext *mfc = (mfContext *)cliprdr->custom;

	ZeroMemory(&formatDataRequest, sizeof(CLIPRDR_FORMAT_DATA_REQUEST));

	formatDataRequest.common.msgType = CB_FORMAT_DATA_REQUEST;
	formatDataRequest.common.msgFlags = 0;

	formatDataRequest.requestedFormatId = formatId;
	mfc->requestedFormatId = formatId;
	ResetEvent(mfc->clipboardRequestEvent);

	cliprdr->ClientFormatDataRequest(cliprdr, &formatDataRequest);

	return 1;
}

static int mac_cliprdr_send_client_capabilities(CliprdrClientContext *cliprdr)
{
	CLIPRDR_CAPABILITIES capabilities;
	CLIPRDR_GENERAL_CAPABILITY_SET generalCapabilitySet;

	capabilities.cCapabilitiesSets = 1;
	capabilities.capabilitySets = (CLIPRDR_CAPABILITY_SET *)&(generalCapabilitySet);

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
static UINT mac_cliprdr_monitor_ready(CliprdrClientContext *cliprdr,
                                      const CLIPRDR_MONITOR_READY *monitorReady)
{
	mfContext *mfc = (mfContext *)cliprdr->custom;

	mfc->clipboardSync = TRUE;
	mac_cliprdr_send_client_capabilities(cliprdr);
	mac_cliprdr_send_client_format_list(cliprdr);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT mac_cliprdr_server_capabilities(CliprdrClientContext *cliprdr,
                                            const CLIPRDR_CAPABILITIES *capabilities)
{
	UINT32 index;
	CLIPRDR_CAPABILITY_SET *capabilitySet;
	mfContext *mfc = (mfContext *)cliprdr->custom;

	for (index = 0; index < capabilities->cCapabilitiesSets; index++)
	{
		capabilitySet = &(capabilities->capabilitySets[index]);

		if ((capabilitySet->capabilitySetType == CB_CAPSTYPE_GENERAL) &&
		    (capabilitySet->capabilitySetLength >= CB_CAPSTYPE_GENERAL_LEN))
		{
			CLIPRDR_GENERAL_CAPABILITY_SET *generalCapabilitySet =
			    (CLIPRDR_GENERAL_CAPABILITY_SET *)capabilitySet;

			mfc->clipboardCapabilities = generalCapabilitySet->generalFlags;
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
static UINT mac_cliprdr_server_format_list(CliprdrClientContext *cliprdr,
                                           const CLIPRDR_FORMAT_LIST *formatList)
{
	UINT32 index;
	CLIPRDR_FORMAT *format;
	mfContext *mfc = (mfContext *)cliprdr->custom;

	if (mfc->serverFormats)
	{
		for (index = 0; index < mfc->numServerFormats; index++)
		{
			free(mfc->serverFormats[index].formatName);
		}

		free(mfc->serverFormats);
		mfc->serverFormats = NULL;
		mfc->numServerFormats = 0;
	}

	if (formatList->numFormats < 1)
		return CHANNEL_RC_OK;

	mfc->numServerFormats = formatList->numFormats;
	mfc->serverFormats = (CLIPRDR_FORMAT *)calloc(mfc->numServerFormats, sizeof(CLIPRDR_FORMAT));

	if (!mfc->serverFormats)
		return CHANNEL_RC_NO_MEMORY;

	for (index = 0; index < mfc->numServerFormats; index++)
	{
		mfc->serverFormats[index].formatId = formatList->formats[index].formatId;
		mfc->serverFormats[index].formatName = NULL;

		if (formatList->formats[index].formatName)
			mfc->serverFormats[index].formatName = _strdup(formatList->formats[index].formatName);
	}

	mac_cliprdr_send_client_format_list_response(cliprdr, TRUE);

	for (index = 0; index < mfc->numServerFormats; index++)
	{
		format = &(mfc->serverFormats[index]);

		if (format->formatId == CF_UNICODETEXT)
		{
			mac_cliprdr_send_client_format_data_request(cliprdr, CF_UNICODETEXT);
			break;
		}
		else if (format->formatId == CF_OEMTEXT)
		{
			mac_cliprdr_send_client_format_data_request(cliprdr, CF_OEMTEXT);
			break;
		}
		else if (format->formatId == CF_TEXT)
		{
			mac_cliprdr_send_client_format_data_request(cliprdr, CF_TEXT);
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
static UINT
mac_cliprdr_server_format_list_response(CliprdrClientContext *cliprdr,
                                        const CLIPRDR_FORMAT_LIST_RESPONSE *formatListResponse)
{
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
mac_cliprdr_server_lock_clipboard_data(CliprdrClientContext *cliprdr,
                                       const CLIPRDR_LOCK_CLIPBOARD_DATA *lockClipboardData)
{
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
mac_cliprdr_server_unlock_clipboard_data(CliprdrClientContext *cliprdr,
                                         const CLIPRDR_UNLOCK_CLIPBOARD_DATA *unlockClipboardData)
{
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
mac_cliprdr_server_format_data_request(CliprdrClientContext *cliprdr,
                                       const CLIPRDR_FORMAT_DATA_REQUEST *formatDataRequest)
{
	BYTE *data;
	UINT32 size;
	UINT32 formatId;
	CLIPRDR_FORMAT_DATA_RESPONSE response;
	mfContext *mfc = (mfContext *)cliprdr->custom;

	ZeroMemory(&response, sizeof(CLIPRDR_FORMAT_DATA_RESPONSE));

	formatId = formatDataRequest->requestedFormatId;
	data = (BYTE *)ClipboardGetData(mfc->clipboard, formatId, &size);

	response.common.msgFlags = CB_RESPONSE_OK;
	response.common.dataLen = size;
	response.requestedFormatData = data;

	if (!data)
	{
		response.common.msgFlags = CB_RESPONSE_FAIL;
		response.common.dataLen = 0;
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
static UINT
mac_cliprdr_server_format_data_response(CliprdrClientContext *cliprdr,
                                        const CLIPRDR_FORMAT_DATA_RESPONSE *formatDataResponse)
{
	BYTE *data;
	UINT32 size;
	UINT32 index;
	UINT32 formatId;
	CLIPRDR_FORMAT *format = NULL;
	mfContext *mfc = (mfContext *)cliprdr->custom;
	MRDPView *view = (MRDPView *)mfc->view;

	if (formatDataResponse->common.msgFlags & CB_RESPONSE_FAIL)
	{
		SetEvent(mfc->clipboardRequestEvent);
		return ERROR_INTERNAL_ERROR;
	}

	for (index = 0; index < mfc->numServerFormats; index++)
	{
		if (mfc->requestedFormatId == mfc->serverFormats[index].formatId)
			format = &(mfc->serverFormats[index]);
	}

	if (!format)
	{
		SetEvent(mfc->clipboardRequestEvent);
		return ERROR_INTERNAL_ERROR;
	}

	if (format->formatName)
		formatId = ClipboardRegisterFormat(mfc->clipboard, format->formatName);
	else
		formatId = format->formatId;

	size = formatDataResponse->common.dataLen;

	ClipboardSetData(mfc->clipboard, formatId, formatDataResponse->requestedFormatData, size);

	SetEvent(mfc->clipboardRequestEvent);

	if ((formatId == CF_TEXT) || (formatId == CF_OEMTEXT) || (formatId == CF_UNICODETEXT))
	{
		formatId = ClipboardRegisterFormat(mfc->clipboard, "UTF8_STRING");

		data = (void *)ClipboardGetData(mfc->clipboard, formatId, &size);

		if (size > 1)
			size--; /* we need the size without the null terminator */

		NSString *str = [[NSString alloc] initWithBytes:(void *)data
		                                         length:size
		                                       encoding:NSUTF8StringEncoding];
		free(data);

		NSArray *types = [[NSArray alloc] initWithObjects:NSPasteboardTypeString, nil];
		[view->pasteboard_wr declareTypes:types owner:view];
		[view->pasteboard_wr setString:str forType:NSPasteboardTypeString];
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
mac_cliprdr_server_file_contents_request(CliprdrClientContext *cliprdr,
                                         const CLIPRDR_FILE_CONTENTS_REQUEST *fileContentsRequest)
{
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT mac_cliprdr_server_file_contents_response(
    CliprdrClientContext *cliprdr, const CLIPRDR_FILE_CONTENTS_RESPONSE *fileContentsResponse)
{
	return CHANNEL_RC_OK;
}

void mac_cliprdr_init(mfContext *mfc, CliprdrClientContext *cliprdr)
{
	cliprdr->custom = (void *)mfc;
	mfc->cliprdr = cliprdr;

	mfc->clipboard = ClipboardCreate();
	mfc->clipboardRequestEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	cliprdr->MonitorReady = mac_cliprdr_monitor_ready;
	cliprdr->ServerCapabilities = mac_cliprdr_server_capabilities;
	cliprdr->ServerFormatList = mac_cliprdr_server_format_list;
	cliprdr->ServerFormatListResponse = mac_cliprdr_server_format_list_response;
	cliprdr->ServerLockClipboardData = mac_cliprdr_server_lock_clipboard_data;
	cliprdr->ServerUnlockClipboardData = mac_cliprdr_server_unlock_clipboard_data;
	cliprdr->ServerFormatDataRequest = mac_cliprdr_server_format_data_request;
	cliprdr->ServerFormatDataResponse = mac_cliprdr_server_format_data_response;
	cliprdr->ServerFileContentsRequest = mac_cliprdr_server_file_contents_request;
	cliprdr->ServerFileContentsResponse = mac_cliprdr_server_file_contents_response;
}

void mac_cliprdr_uninit(mfContext *mfc, CliprdrClientContext *cliprdr)
{
	cliprdr->custom = NULL;
	mfc->cliprdr = NULL;

	ClipboardDestroy(mfc->clipboard);
	CloseHandle(mfc->clipboardRequestEvent);
}
