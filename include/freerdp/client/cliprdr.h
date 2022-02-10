/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Clipboard Virtual Channel Extension
 *
 * Copyright 2011 Vic Lee
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

#ifndef FREERDP_CHANNEL_CLIPRDR_CLIENT_CLIPRDR_H
#define FREERDP_CHANNEL_CLIPRDR_CLIENT_CLIPRDR_H

#include <freerdp/types.h>

#include <freerdp/message.h>
#include <freerdp/channels/cliprdr.h>
#include <freerdp/freerdp.h>

/**
 * Client Interface
 */

typedef struct s_cliprdr_client_context CliprdrClientContext;

typedef UINT (*pcCliprdrServerCapabilities)(CliprdrClientContext* context,
                                            const CLIPRDR_CAPABILITIES* capabilities);
typedef UINT (*pcCliprdrClientCapabilities)(CliprdrClientContext* context,
                                            const CLIPRDR_CAPABILITIES* capabilities);
typedef UINT (*pcCliprdrMonitorReady)(CliprdrClientContext* context,
                                      const CLIPRDR_MONITOR_READY* monitorReady);
typedef UINT (*pcCliprdrTempDirectory)(CliprdrClientContext* context,
                                       const CLIPRDR_TEMP_DIRECTORY* tempDirectory);
typedef UINT (*pcCliprdrClientFormatList)(CliprdrClientContext* context,
                                          const CLIPRDR_FORMAT_LIST* formatList);
typedef UINT (*pcCliprdrServerFormatList)(CliprdrClientContext* context,
                                          const CLIPRDR_FORMAT_LIST* formatList);
typedef UINT (*pcCliprdrClientFormatListResponse)(
    CliprdrClientContext* context, const CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse);
typedef UINT (*pcCliprdrServerFormatListResponse)(
    CliprdrClientContext* context, const CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse);
typedef UINT (*pcCliprdrClientLockClipboardData)(
    CliprdrClientContext* context, const CLIPRDR_LOCK_CLIPBOARD_DATA* lockClipboardData);
typedef UINT (*pcCliprdrServerLockClipboardData)(
    CliprdrClientContext* context, const CLIPRDR_LOCK_CLIPBOARD_DATA* lockClipboardData);
typedef UINT (*pcCliprdrClientUnlockClipboardData)(
    CliprdrClientContext* context, const CLIPRDR_UNLOCK_CLIPBOARD_DATA* unlockClipboardData);
typedef UINT (*pcCliprdrServerUnlockClipboardData)(
    CliprdrClientContext* context, const CLIPRDR_UNLOCK_CLIPBOARD_DATA* unlockClipboardData);
typedef UINT (*pcCliprdrClientFormatDataRequest)(
    CliprdrClientContext* context, const CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest);
typedef UINT (*pcCliprdrServerFormatDataRequest)(
    CliprdrClientContext* context, const CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest);
typedef UINT (*pcCliprdrClientFormatDataResponse)(
    CliprdrClientContext* context, const CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse);
typedef UINT (*pcCliprdrServerFormatDataResponse)(
    CliprdrClientContext* context, const CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse);
typedef UINT (*pcCliprdrClientFileContentsRequest)(
    CliprdrClientContext* context, const CLIPRDR_FILE_CONTENTS_REQUEST* fileContentsRequest);
typedef UINT (*pcCliprdrServerFileContentsRequest)(
    CliprdrClientContext* context, const CLIPRDR_FILE_CONTENTS_REQUEST* fileContentsRequest);
typedef UINT (*pcCliprdrClientFileContentsResponse)(
    CliprdrClientContext* context, const CLIPRDR_FILE_CONTENTS_RESPONSE* fileContentsResponse);
typedef UINT (*pcCliprdrServerFileContentsResponse)(
    CliprdrClientContext* context, const CLIPRDR_FILE_CONTENTS_RESPONSE* fileContentsResponse);

struct s_cliprdr_client_context
{
	void* handle;
	void* custom;

	pcCliprdrServerCapabilities ServerCapabilities;
	pcCliprdrClientCapabilities ClientCapabilities;
	pcCliprdrMonitorReady MonitorReady;
	pcCliprdrTempDirectory TempDirectory;
	pcCliprdrClientFormatList ClientFormatList;
	pcCliprdrServerFormatList ServerFormatList;
	pcCliprdrClientFormatListResponse ClientFormatListResponse;
	pcCliprdrServerFormatListResponse ServerFormatListResponse;
	pcCliprdrClientLockClipboardData ClientLockClipboardData;
	pcCliprdrServerLockClipboardData ServerLockClipboardData;
	pcCliprdrClientUnlockClipboardData ClientUnlockClipboardData;
	pcCliprdrServerUnlockClipboardData ServerUnlockClipboardData;
	pcCliprdrClientFormatDataRequest ClientFormatDataRequest;
	pcCliprdrServerFormatDataRequest ServerFormatDataRequest;
	pcCliprdrClientFormatDataResponse ClientFormatDataResponse;
	pcCliprdrServerFormatDataResponse ServerFormatDataResponse;
	pcCliprdrClientFileContentsRequest ClientFileContentsRequest;
	pcCliprdrServerFileContentsRequest ServerFileContentsRequest;
	pcCliprdrClientFileContentsResponse ClientFileContentsResponse;
	pcCliprdrServerFileContentsResponse ServerFileContentsResponse;

	UINT32 lastRequestedFormatId;
	rdpContext* rdpcontext;
};

typedef struct
{
	UINT32 id;
	char* name;
	int length;
} CLIPRDR_FORMAT_NAME;

/**
 * Clipboard Events
 */

typedef struct
{
	wMessage event;
	UINT32 capabilities;
} RDP_CB_CLIP_CAPS;

typedef struct
{
	wMessage event;
	UINT32 capabilities;
} RDP_CB_MONITOR_READY_EVENT;

typedef struct
{
	wMessage event;
	UINT32* formats;
	UINT16 num_formats;
	BYTE* raw_format_data;
	UINT32 raw_format_data_size;
	BOOL raw_format_unicode;
} RDP_CB_FORMAT_LIST_EVENT;

typedef struct
{
	wMessage event;
	UINT32 format;
} RDP_CB_DATA_REQUEST_EVENT;

typedef struct
{
	wMessage event;
	BYTE* data;
	UINT32 size;
} RDP_CB_DATA_RESPONSE_EVENT;

typedef struct
{
	wMessage event;
	UINT32 streamId;
	UINT32 lindex;
	UINT32 dwFlags;
	UINT32 nPositionLow;
	UINT32 nPositionHigh;
	UINT32 cbRequested;
	UINT32 clipDataId;
} RDP_CB_FILECONTENTS_REQUEST_EVENT;

typedef struct
{
	wMessage event;
	BYTE* data;
	UINT32 size;
	UINT32 streamId;
} RDP_CB_FILECONTENTS_RESPONSE_EVENT;

typedef struct
{
	wMessage event;
	UINT32 clipDataId;
} RDP_CB_LOCK_CLIPDATA_EVENT;

typedef struct
{
	wMessage event;
	UINT32 clipDataId;
} RDP_CB_UNLOCK_CLIPDATA_EVENT;

typedef struct
{
	wMessage event;
	char dirname[520];
} RDP_CB_TEMPDIR_EVENT;

#endif /* FREERDP_CHANNEL_CLIPRDR_CLIENT_CLIPRDR_H */
