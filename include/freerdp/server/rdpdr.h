/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Device Redirection Virtual Channel Server Interface
 *
 * Copyright 2014 Dell Software <Mike.McDonald@software.dell.com>
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

#ifndef FREERDP_CHANNEL_RDPDR_SERVER_RDPDR_H
#define FREERDP_CHANNEL_RDPDR_SERVER_RDPDR_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/channels/wtsvc.h>
#include <freerdp/channels/rdpdr.h>

/**
 * Server Interface
 */

typedef struct _rdpdr_server_context RdpdrServerContext;
typedef struct _rdpdr_server_private RdpdrServerPrivate;

struct _FILE_DIRECTORY_INFORMATION
{
	UINT32 NextEntryOffset;
	UINT32 FileIndex;
	UINT64 CreationTime;
	UINT64 LastAccessTime;
	UINT64 LastWriteTime;
	UINT64 ChangeTime;
	UINT64 EndOfFile;
	UINT64 AllocationSize;
	UINT32 FileAttributes;
	char FileName[512];
};
typedef struct _FILE_DIRECTORY_INFORMATION FILE_DIRECTORY_INFORMATION;

typedef UINT (*psRdpdrStart)(RdpdrServerContext* context);
typedef UINT (*psRdpdrStop)(RdpdrServerContext* context);

typedef UINT (*psRdpdrDriveCreateDirectory)(RdpdrServerContext* context, void* callbackData,
                                            UINT32 deviceId, const char* path);
typedef UINT (*psRdpdrDriveDeleteDirectory)(RdpdrServerContext* context, void* callbackData,
                                            UINT32 deviceId, const char* path);
typedef UINT (*psRdpdrDriveQueryDirectory)(RdpdrServerContext* context, void* callbackData,
                                           UINT32 deviceId, const char* path);
typedef UINT (*psRdpdrDriveOpenFile)(RdpdrServerContext* context, void* callbackData,
                                     UINT32 deviceId, const char* path, UINT32 desiredAccess,
                                     UINT32 createDisposition);
typedef UINT (*psRdpdrDriveReadFile)(RdpdrServerContext* context, void* callbackData,
                                     UINT32 deviceId, UINT32 fileId, UINT32 length, UINT32 offset);
typedef UINT (*psRdpdrDriveWriteFile)(RdpdrServerContext* context, void* callbackData,
                                      UINT32 deviceId, UINT32 fileId, const char* buffer,
                                      UINT32 length, UINT32 offset);
typedef UINT (*psRdpdrDriveCloseFile)(RdpdrServerContext* context, void* callbackData,
                                      UINT32 deviceId, UINT32 fileId);
typedef UINT (*psRdpdrDriveDeleteFile)(RdpdrServerContext* context, void* callbackData,
                                       UINT32 deviceId, const char* path);
typedef UINT (*psRdpdrDriveRenameFile)(RdpdrServerContext* context, void* callbackData,
                                       UINT32 deviceId, const char* oldPath, const char* newPath);

typedef void (*psRdpdrOnDriveCreate)(RdpdrServerContext* context, UINT32 deviceId,
                                     const char* name);
typedef void (*psRdpdrOnDriveDelete)(RdpdrServerContext* context, UINT32 deviceId);
typedef void (*psRdpdrOnDriveCreateDirectoryComplete)(RdpdrServerContext* context,
                                                      void* callbackData, UINT32 ioStatus);
typedef void (*psRdpdrOnDriveDeleteDirectoryComplete)(RdpdrServerContext* context,
                                                      void* callbackData, UINT32 ioStatus);
typedef void (*psRdpdrOnDriveQueryDirectoryComplete)(RdpdrServerContext* context,
                                                     void* callbackData, UINT32 ioStatus,
                                                     FILE_DIRECTORY_INFORMATION* fdi);
typedef void (*psRdpdrOnDriveOpenFileComplete)(RdpdrServerContext* context, void* callbackData,
                                               UINT32 ioStatus, UINT32 deviceId, UINT32 fileId);
typedef void (*psRdpdrOnDriveReadFileComplete)(RdpdrServerContext* context, void* callbackData,
                                               UINT32 ioStatus, const char* buffer, UINT32 length);
typedef void (*psRdpdrOnDriveWriteFileComplete)(RdpdrServerContext* context, void* callbackData,
                                                UINT32 ioStatus, UINT32 bytesWritten);
typedef void (*psRdpdrOnDriveCloseFileComplete)(RdpdrServerContext* context, void* callbackData,
                                                UINT32 ioStatus);
typedef void (*psRdpdrOnDriveDeleteFileComplete)(RdpdrServerContext* context, void* callbackData,
                                                 UINT32 ioStatus);
typedef void (*psRdpdrOnDriveRenameFileComplete)(RdpdrServerContext* context, void* callbackData,
                                                 UINT32 ioStatus);

typedef void (*psRdpdrOnPortCreate)(RdpdrServerContext* context, UINT32 deviceId, const char* name);
typedef void (*psRdpdrOnPortDelete)(RdpdrServerContext* context, UINT32 deviceId);

typedef void (*psRdpdrOnPrinterCreate)(RdpdrServerContext* context, UINT32 deviceId,
                                       const char* name);
typedef void (*psRdpdrOnPrinterDelete)(RdpdrServerContext* context, UINT32 deviceId);

typedef void (*psRdpdrOnSmartcardCreate)(RdpdrServerContext* context, UINT32 deviceId,
                                         const char* name);
typedef void (*psRdpdrOnSmartcardDelete)(RdpdrServerContext* context, UINT32 deviceId);

struct _rdpdr_server_context
{
	HANDLE vcm;

	psRdpdrStart Start;
	psRdpdrStop Stop;

	RdpdrServerPrivate* priv;

	/* Server self-defined pointer. */
	void* data;

	/* Server supported redirections. Set by server. */
	BOOL supportsDrives;
	BOOL supportsPorts;
	BOOL supportsPrinters;
	BOOL supportsSmartcards;

	/*** Drive APIs called by the server. ***/
	psRdpdrDriveCreateDirectory DriveCreateDirectory;
	psRdpdrDriveDeleteDirectory DriveDeleteDirectory;
	psRdpdrDriveQueryDirectory DriveQueryDirectory;
	psRdpdrDriveOpenFile DriveOpenFile;
	psRdpdrDriveReadFile DriveReadFile;
	psRdpdrDriveWriteFile DriveWriteFile;
	psRdpdrDriveCloseFile DriveCloseFile;
	psRdpdrDriveDeleteFile DriveDeleteFile;
	psRdpdrDriveRenameFile DriveRenameFile;

	/*** Drive callbacks registered by the server. ***/
	psRdpdrOnDriveCreate OnDriveCreate;
	psRdpdrOnDriveDelete OnDriveDelete;
	psRdpdrOnDriveCreateDirectoryComplete OnDriveCreateDirectoryComplete;
	psRdpdrOnDriveDeleteDirectoryComplete OnDriveDeleteDirectoryComplete;
	psRdpdrOnDriveQueryDirectoryComplete OnDriveQueryDirectoryComplete;
	psRdpdrOnDriveOpenFileComplete OnDriveOpenFileComplete;
	psRdpdrOnDriveReadFileComplete OnDriveReadFileComplete;
	psRdpdrOnDriveWriteFileComplete OnDriveWriteFileComplete;
	psRdpdrOnDriveCloseFileComplete OnDriveCloseFileComplete;
	psRdpdrOnDriveDeleteFileComplete OnDriveDeleteFileComplete;
	psRdpdrOnDriveRenameFileComplete OnDriveRenameFileComplete;

	/*** Port callbacks registered by the server. ***/
	psRdpdrOnPortCreate OnPortCreate;
	psRdpdrOnPortDelete OnPortDelete;

	/*** Printer callbacks registered by the server. ***/
	psRdpdrOnPrinterCreate OnPrinterCreate;
	psRdpdrOnPrinterDelete OnPrinterDelete;

	/*** Smartcard callbacks registered by the server. ***/
	psRdpdrOnSmartcardCreate OnSmartcardCreate;
	psRdpdrOnSmartcardDelete OnSmartcardDelete;

	rdpContext* rdpcontext;
};

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API RdpdrServerContext* rdpdr_server_context_new(HANDLE vcm);
	FREERDP_API void rdpdr_server_context_free(RdpdrServerContext* context);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNEL_RDPDR_SERVER_RDPDR_H */
