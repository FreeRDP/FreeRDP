/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Clipboard Virtual Channel Server Interface
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CHANNEL_SERVER_CLIPRDR_H
#define FREERDP_CHANNEL_SERVER_CLIPRDR_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/channels/wtsvc.h>

#include <freerdp/channels/cliprdr.h>
#include <freerdp/client/cliprdr.h>

/**
 * Server Interface
 */

typedef struct _cliprdr_server_context CliprdrServerContext;

typedef UINT (*psCliprdrOpen)(CliprdrServerContext* context);
typedef UINT (*psCliprdrClose)(CliprdrServerContext* context);
typedef UINT (*psCliprdrStart)(CliprdrServerContext* context);
typedef UINT (*psCliprdrStop)(CliprdrServerContext* context);
typedef HANDLE (*psCliprdrGetEventHandle)(CliprdrServerContext* context);
typedef UINT (*psCliprdrCheckEventHandle)(CliprdrServerContext* context);

typedef UINT (*psCliprdrServerCapabilities)(CliprdrServerContext* context, CLIPRDR_CAPABILITIES* capabilities);
typedef UINT (*psCliprdrClientCapabilities)(CliprdrServerContext* context, CLIPRDR_CAPABILITIES* capabilities);
typedef UINT (*psCliprdrMonitorReady)(CliprdrServerContext* context, CLIPRDR_MONITOR_READY* monitorReady);
typedef UINT (*psCliprdrTempDirectory)(CliprdrServerContext* context, CLIPRDR_TEMP_DIRECTORY* tempDirectory);
typedef UINT (*psCliprdrClientFormatList)(CliprdrServerContext* context, CLIPRDR_FORMAT_LIST* formatList);
typedef UINT (*psCliprdrServerFormatList)(CliprdrServerContext* context, CLIPRDR_FORMAT_LIST* formatList);
typedef UINT (*psCliprdrClientFormatListResponse)(CliprdrServerContext* context, CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse);
typedef UINT (*psCliprdrServerFormatListResponse)(CliprdrServerContext* context, CLIPRDR_FORMAT_LIST_RESPONSE* formatListResponse);
typedef UINT (*psCliprdrClientLockClipboardData)(CliprdrServerContext* context, CLIPRDR_LOCK_CLIPBOARD_DATA* lockClipboardData);
typedef UINT (*psCliprdrServerLockClipboardData)(CliprdrServerContext* context, CLIPRDR_LOCK_CLIPBOARD_DATA* lockClipboardData);
typedef UINT (*psCliprdrClientUnlockClipboardData)(CliprdrServerContext* context, CLIPRDR_UNLOCK_CLIPBOARD_DATA* unlockClipboardData);
typedef UINT (*psCliprdrServerUnlockClipboardData)(CliprdrServerContext* context, CLIPRDR_UNLOCK_CLIPBOARD_DATA* unlockClipboardData);
typedef UINT (*psCliprdrClientFormatDataRequest)(CliprdrServerContext* context, CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest);
typedef UINT (*psCliprdrServerFormatDataRequest)(CliprdrServerContext* context, CLIPRDR_FORMAT_DATA_REQUEST* formatDataRequest);
typedef UINT (*psCliprdrClientFormatDataResponse)(CliprdrServerContext* context, CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse);
typedef UINT (*psCliprdrServerFormatDataResponse)(CliprdrServerContext* context, CLIPRDR_FORMAT_DATA_RESPONSE* formatDataResponse);
typedef UINT (*psCliprdrClientFileContentsRequest)(CliprdrServerContext* context, CLIPRDR_FILE_CONTENTS_REQUEST* fileContentsRequest);
typedef UINT (*psCliprdrServerFileContentsRequest)(CliprdrServerContext* context, CLIPRDR_FILE_CONTENTS_REQUEST* fileContentsRequest);
typedef UINT (*psCliprdrClientFileContentsResponse)(CliprdrServerContext* context, CLIPRDR_FILE_CONTENTS_RESPONSE* fileContentsResponse);
typedef UINT (*psCliprdrServerFileContentsResponse)(CliprdrServerContext* context, CLIPRDR_FILE_CONTENTS_RESPONSE* fileContentsResponse);

struct _cliprdr_server_context
{
	void* handle;
	void* custom;

	/* server clipboard capabilities - set by server - updated by the channel after client capability exchange */
	BOOL useLongFormatNames;
	BOOL streamFileClipEnabled;
	BOOL fileClipNoFilePaths;
	BOOL canLockClipData;

	psCliprdrOpen Open;
	psCliprdrClose Close;
	psCliprdrStart Start;
	psCliprdrStop Stop;
	psCliprdrGetEventHandle GetEventHandle;
	psCliprdrCheckEventHandle CheckEventHandle;

	psCliprdrServerCapabilities ServerCapabilities;
	psCliprdrClientCapabilities ClientCapabilities;
	psCliprdrMonitorReady MonitorReady;
	psCliprdrTempDirectory TempDirectory;
	psCliprdrClientFormatList ClientFormatList;
	psCliprdrServerFormatList ServerFormatList;
	psCliprdrClientFormatListResponse ClientFormatListResponse;
	psCliprdrServerFormatListResponse ServerFormatListResponse;
	psCliprdrClientLockClipboardData ClientLockClipboardData;
	psCliprdrServerLockClipboardData ServerLockClipboardData;
	psCliprdrClientUnlockClipboardData ClientUnlockClipboardData;
	psCliprdrServerUnlockClipboardData ServerUnlockClipboardData;
	psCliprdrClientFormatDataRequest ClientFormatDataRequest;
	psCliprdrServerFormatDataRequest ServerFormatDataRequest;
	psCliprdrClientFormatDataResponse ClientFormatDataResponse;
	psCliprdrServerFormatDataResponse ServerFormatDataResponse;
	psCliprdrClientFileContentsRequest ClientFileContentsRequest;
	psCliprdrServerFileContentsRequest ServerFileContentsRequest;
	psCliprdrClientFileContentsResponse ClientFileContentsResponse;
	psCliprdrServerFileContentsResponse ServerFileContentsResponse;

	rdpContext* rdpcontext;
};

#ifdef __cplusplus
 extern "C" {
#endif

FREERDP_API CliprdrServerContext* cliprdr_server_context_new(HANDLE vcm);
FREERDP_API void cliprdr_server_context_free(CliprdrServerContext* context);

#ifdef __cplusplus
 }
#endif

#endif /* FREERDP_CHANNEL_SERVER_CLIPRDR_H */
