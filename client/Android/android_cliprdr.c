/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Android Clipboard Redirection
 *
 * Copyright 2013 Felix Long
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

#include <jni.h>

#include <winpr/crt.h>
#include <winpr/stream.h>

#include <freerdp/client/channels.h>
#include <freerdp/client/cliprdr.h>

#include "android_cliprdr.h"
#include "android_jni_utils.h"
#include "android_jni_callback.h"

UINT android_cliprdr_send_client_format_list(CliprdrClientContext* cliprdr)
{
	UINT rc = ERROR_INTERNAL_ERROR;
	UINT32 index;
	UINT32 formatId;
	UINT32 numFormats;
	UINT32* pFormatIds;
	const char* formatName;
	CLIPRDR_FORMAT* formats;
	CLIPRDR_FORMAT_LIST formatList;

	if (!cliprdr)
		return ERROR_INVALID_PARAMETER;

	androidContext* afc = (androidContext*) cliprdr->custom;

	if (!afc || !afc->cliprdr)
		return ERROR_INVALID_PARAMETER;

	ZeroMemory(&formatList, sizeof(CLIPRDR_FORMAT_LIST));
	pFormatIds = NULL;
	numFormats = ClipboardGetFormatIds(afc->clipboard, &pFormatIds);
	formats = (CLIPRDR_FORMAT*) calloc(numFormats, sizeof(CLIPRDR_FORMAT));

	if (!formats)
		goto fail;

	for (index = 0; index < numFormats; index++)
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

	formatList.msgFlags = CB_RESPONSE_OK;
	formatList.numFormats = numFormats;
	formatList.formats = formats;

	if (!afc->cliprdr->ClientFormatList)
		goto fail;

	rc = afc->cliprdr->ClientFormatList(afc->cliprdr, &formatList);
fail:
	free(pFormatIds);
	free(formats);
	return rc;
}

static UINT android_cliprdr_send_client_format_data_request(
    CliprdrClientContext* cliprdr, UINT32 formatId)
{
	UINT rc = ERROR_INVALID_PARAMETER;
	CLIPRDR_FORMAT_DATA_REQUEST formatDataRequest;
	androidContext* afc;

	if (!cliprdr)
		goto fail;

	afc = (androidContext*) cliprdr->custom;

	if (!afc || !afc->clipboardRequestEvent || !cliprdr->ClientFormatDataRequest)
		goto fail;

	ZeroMemory(&formatDataRequest, sizeof(CLIPRDR_FORMAT_DATA_REQUEST));
	formatDataRequest.msgType = CB_FORMAT_DATA_REQUEST;
	formatDataRequest.msgFlags = 0;
	formatDataRequest.requestedFormatId = formatId;
	afc->requestedFormatId = formatId;
	ResetEvent(afc->clipboardRequestEvent);
	rc = cliprdr->ClientFormatDataRequest(cliprdr, &formatDataRequest);
fail:
	return rc;
}

static UINT android_cliprdr_send_client_capabilities(CliprdrClientContext*
        cliprdr)
{
	CLIPRDR_CAPABILITIES capabilities;
	CLIPRDR_GENERAL_CAPABILITY_SET generalCapabilitySet;

	if (!cliprdr || !cliprdr->ClientCapabilities)
		return ERROR_INVALID_PARAMETER;

	capabilities.cCapabilitiesSets = 1;
	capabilities.capabilitySets = (CLIPRDR_CAPABILITY_SET*) &
	                              (generalCapabilitySet);
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
static UINT android_cliprdr_monitor_ready(CliprdrClientContext* cliprdr,
        CLIPRDR_MONITOR_READY* monitorReady)
{
	UINT rc;
	androidContext* afc;

	if (!cliprdr || !monitorReady)
		return ERROR_INVALID_PARAMETER;

	afc = (androidContext*) cliprdr->custom;

	if (!afc)
		return ERROR_INVALID_PARAMETER;

	if ((rc = android_cliprdr_send_client_capabilities(cliprdr)) != CHANNEL_RC_OK)
		return rc;

	if ((rc = android_cliprdr_send_client_format_list(cliprdr)) != CHANNEL_RC_OK)
		return rc;

	afc->clipboardSync = TRUE;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT android_cliprdr_server_capabilities(CliprdrClientContext* cliprdr,
        CLIPRDR_CAPABILITIES* capabilities)
{
	UINT32 index;
	CLIPRDR_CAPABILITY_SET* capabilitySet;
	androidContext* afc;

	if (!cliprdr || !capabilities)
		return ERROR_INVALID_PARAMETER;

	afc = (androidContext*) cliprdr->custom;

	if (!afc)
		return ERROR_INVALID_PARAMETER;

	for (index = 0; index < capabilities->cCapabilitiesSets; index++)
	{
		capabilitySet = &(capabilities->capabilitySets[index]);

		if ((capabilitySet->capabilitySetType == CB_CAPSTYPE_GENERAL) &&
		    (capabilitySet->capabilitySetLength >= CB_CAPSTYPE_GENERAL_LEN))
		{
			CLIPRDR_GENERAL_CAPABILITY_SET* generalCapabilitySet
			    = (CLIPRDR_GENERAL_CAPABILITY_SET*) capabilitySet;
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
static UINT android_cliprdr_server_format_list(CliprdrClientContext* cliprdr,
        CLIPRDR_FORMAT_LIST* formatList)
{
	UINT rc;
	UINT32 index;
	CLIPRDR_FORMAT* format;
	androidContext* afc;

	if (!cliprdr || !formatList)
		return ERROR_INVALID_PARAMETER;

	afc = (androidContext*) cliprdr->custom;

	if (!afc)
		return ERROR_INVALID_PARAMETER;

	if (afc->serverFormats)
	{
		for (index = 0; index < afc->numServerFormats; index++)
			free(afc->serverFormats[index].formatName);

		free(afc->serverFormats);
		afc->serverFormats = NULL;
		afc->numServerFormats = 0;
	}

	if (formatList->numFormats < 1)
		return CHANNEL_RC_OK;

	afc->numServerFormats = formatList->numFormats;
	afc->serverFormats = (CLIPRDR_FORMAT*) calloc(afc->numServerFormats,
	                     sizeof(CLIPRDR_FORMAT));

	if (!afc->serverFormats)
		return CHANNEL_RC_NO_MEMORY;

	for (index = 0; index < afc->numServerFormats; index++)
	{
		afc->serverFormats[index].formatId = formatList->formats[index].formatId;
		afc->serverFormats[index].formatName = NULL;

		if (formatList->formats[index].formatName)
		{
			afc->serverFormats[index].formatName = _strdup(
			        formatList->formats[index].formatName);

			if (!afc->serverFormats[index].formatName)
				return CHANNEL_RC_NO_MEMORY;
		}
	}

	for (index = 0; index < afc->numServerFormats; index++)
	{
		format = &(afc->serverFormats[index]);

		if (format->formatId == CF_UNICODETEXT)
		{
			if ((rc = android_cliprdr_send_client_format_data_request(cliprdr,
			          CF_UNICODETEXT)) != CHANNEL_RC_OK)
				return rc;

			break;
		}
		else if (format->formatId == CF_TEXT)
		{
			if ((rc = android_cliprdr_send_client_format_data_request(cliprdr,
			          CF_TEXT)) != CHANNEL_RC_OK)
				return rc;

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
static UINT android_cliprdr_server_format_list_response(
    CliprdrClientContext* cliprdr,
    CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse)
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
static UINT android_cliprdr_server_lock_clipboard_data(CliprdrClientContext*
        cliprdr,
        CLIPRDR_LOCK_CLIPBOARD_DATA* lockClipboardData)
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
static UINT android_cliprdr_server_unlock_clipboard_data(
    CliprdrClientContext* cliprdr,
    CLIPRDR_UNLOCK_CLIPBOARD_DATA* unlockClipboardData)
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
static UINT android_cliprdr_server_format_data_request(CliprdrClientContext*
        cliprdr,
        CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest)
{
	UINT rc;
	BYTE* data;
	UINT32 size;
	UINT32 formatId;
	CLIPRDR_FORMAT_DATA_RESPONSE response;
	androidContext* afc;

	if (!cliprdr || !formatDataRequest || !cliprdr->ClientFormatDataResponse)
		return ERROR_INVALID_PARAMETER;

	afc = (androidContext*) cliprdr->custom;

	if (!afc)
		return ERROR_INVALID_PARAMETER;

	ZeroMemory(&response, sizeof(CLIPRDR_FORMAT_DATA_RESPONSE));
	formatId = formatDataRequest->requestedFormatId;
	data = (BYTE*) ClipboardGetData(afc->clipboard, formatId, &size);
	response.msgFlags = CB_RESPONSE_OK;
	response.dataLen = size;
	response.requestedFormatData = data;

	if (!data)
	{
		response.msgFlags = CB_RESPONSE_FAIL;
		response.dataLen = 0;
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
static UINT android_cliprdr_server_format_data_response(
    CliprdrClientContext* cliprdr,
    CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse)
{
	BYTE* data;
	UINT32 size;
	UINT32 index;
	UINT32 formatId;
	CLIPRDR_FORMAT* format = NULL;
	androidContext* afc;
	freerdp* instance;

	if (!cliprdr || !formatDataResponse)
		return ERROR_INVALID_PARAMETER;

	afc = (androidContext*) cliprdr->custom;

	if (!afc)
		return ERROR_INVALID_PARAMETER;

	instance = ((rdpContext*) afc)->instance;

	if (!instance)
		return ERROR_INVALID_PARAMETER;

	for (index = 0; index < afc->numServerFormats; index++)
	{
		if (afc->requestedFormatId == afc->serverFormats[index].formatId)
			format = &(afc->serverFormats[index]);
	}

	if (!format)
	{
		SetEvent(afc->clipboardRequestEvent);
		return ERROR_INTERNAL_ERROR;
	}

	if (format->formatName)
		formatId = ClipboardRegisterFormat(afc->clipboard, format->formatName);
	else
		formatId = format->formatId;

	size = formatDataResponse->dataLen;

	if (!ClipboardSetData(afc->clipboard, formatId,
	                      formatDataResponse->requestedFormatData, size))
		return ERROR_INTERNAL_ERROR;

	SetEvent(afc->clipboardRequestEvent);

	if ((formatId == CF_TEXT) || (formatId == CF_UNICODETEXT))
	{
		JNIEnv* env;
		jstring jdata;
		jboolean attached;
		formatId = ClipboardRegisterFormat(afc->clipboard, "UTF8_STRING");
		data = (void*) ClipboardGetData(afc->clipboard, formatId, &size);
		attached = jni_attach_thread(&env);
		size = strnlen(data, size);
		jdata = jniNewStringUTF(env, data, size);
		freerdp_callback("OnRemoteClipboardChanged", "(JLjava/lang/String;)V", (jlong)instance,
		                 jdata);
		(*env)->DeleteLocalRef(env, jdata);

		if (attached == JNI_TRUE)
			jni_detach_thread();
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT android_cliprdr_server_file_contents_request(
    CliprdrClientContext* cliprdr,
    CLIPRDR_FILE_CONTENTS_REQUEST* fileContentsRequest)
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
static UINT android_cliprdr_server_file_contents_response(
    CliprdrClientContext* cliprdr,
    CLIPRDR_FILE_CONTENTS_RESPONSE* fileContentsResponse)
{
	if (!cliprdr || !fileContentsResponse)
		return ERROR_INVALID_PARAMETER;

	return CHANNEL_RC_OK;
}

BOOL android_cliprdr_init(androidContext* afc, CliprdrClientContext* cliprdr)
{
	wClipboard* clipboard;
	HANDLE hevent;

	if (!afc || !cliprdr)
		return FALSE;

	if (!(hevent = CreateEvent(NULL, TRUE, FALSE, NULL)))
		return FALSE;

	if (!(clipboard = ClipboardCreate()))
	{
		CloseHandle(hevent);
		return FALSE;
	}

	afc->cliprdr = cliprdr;
	afc->clipboard = clipboard;
	afc->clipboardRequestEvent = hevent;
	cliprdr->custom = (void*) afc;
	cliprdr->MonitorReady = android_cliprdr_monitor_ready;
	cliprdr->ServerCapabilities = android_cliprdr_server_capabilities;
	cliprdr->ServerFormatList = android_cliprdr_server_format_list;
	cliprdr->ServerFormatListResponse = android_cliprdr_server_format_list_response;
	cliprdr->ServerLockClipboardData = android_cliprdr_server_lock_clipboard_data;
	cliprdr->ServerUnlockClipboardData =
	    android_cliprdr_server_unlock_clipboard_data;
	cliprdr->ServerFormatDataRequest = android_cliprdr_server_format_data_request;
	cliprdr->ServerFormatDataResponse = android_cliprdr_server_format_data_response;
	cliprdr->ServerFileContentsRequest =
	    android_cliprdr_server_file_contents_request;
	cliprdr->ServerFileContentsResponse =
	    android_cliprdr_server_file_contents_response;
	return TRUE;
}

BOOL android_cliprdr_uninit(androidContext* afc, CliprdrClientContext* cliprdr)
{
	if (!afc || !cliprdr)
		return FALSE;

	cliprdr->custom = NULL;
	afc->cliprdr = NULL;
	ClipboardDestroy(afc->clipboard);
	CloseHandle(afc->clipboardRequestEvent);
	return TRUE;
}
