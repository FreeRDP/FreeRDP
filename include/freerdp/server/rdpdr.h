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

typedef struct s_rdpdr_server_context RdpdrServerContext;
typedef struct s_rdpdr_server_private RdpdrServerPrivate;

typedef struct
{
	UINT16 Component;
	UINT16 PacketId;
} RDPDR_HEADER;

typedef struct
{
	UINT16 CapabilityType;
	UINT16 CapabilityLength;
	UINT32 Version;
} RDPDR_CAPABILITY_HEADER;

#ifndef __MINGW32__
typedef struct
{
	UINT32 NextEntryOffset;
	UINT32 FileIndex;
	LARGE_INTEGER CreationTime;
	LARGE_INTEGER LastAccessTime;
	LARGE_INTEGER LastWriteTime;
	LARGE_INTEGER ChangeTime;
	LARGE_INTEGER EndOfFile;
	LARGE_INTEGER AllocationSize;
	UINT32 FileAttributes;
	char FileName[512];
} FILE_DIRECTORY_INFORMATION;
#endif

typedef struct
{
	UINT32 DeviceType;
	UINT32 DeviceId;
	char PreferredDosName[9];
	UINT32 DeviceDataLength;
	BYTE* DeviceData;
} RdpdrDevice;

typedef UINT (*psRdpdrStart)(RdpdrServerContext* context);
typedef UINT (*psRdpdrStop)(RdpdrServerContext* context);

typedef UINT (*psRdpdrCapablityPDU)(RdpdrServerContext* context,
                                    const RDPDR_CAPABILITY_HEADER* header, size_t size,
                                    const BYTE* data);
typedef UINT (*psRdpdrReceivePDU)(RdpdrServerContext* context, const RDPDR_HEADER* header,
                                  UINT error);
typedef UINT (*psRdpdrReceiveAnnounceResponse)(RdpdrServerContext* context, UINT16 VersionMajor,
                                               UINT16 VersionMinor, UINT32 ClientId);
typedef UINT (*psRdpdrSendServerAnnounce)(RdpdrServerContext* context);
typedef UINT (*psRdpdrReceiveDeviceAnnounce)(RdpdrServerContext* context,
                                             const RdpdrDevice* device);
typedef UINT (*psRdpdrReceiveDeviceRemove)(RdpdrServerContext* context, UINT32 deviceId,
                                           const RdpdrDevice* device);
typedef UINT (*psRdpdrReceiveClientNameRequest)(RdpdrServerContext* context, size_t ComputerNameLen,
                                                const char* name);

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

typedef UINT (*psRdpdrOnDeviceCreate)(RdpdrServerContext* context, const RdpdrDevice* device);
typedef UINT (*psRdpdrOnDeviceDelete)(RdpdrServerContext* context, UINT32 deviceId);

struct s_rdpdr_server_context
{
	HANDLE vcm;

	psRdpdrStart Start;
	psRdpdrStop Stop;

	RdpdrServerPrivate* priv;

	/* Server self-defined pointer. */
	void* data;

	/**< Server supported redirections.
	 * initially used to determine which redirections are supported by the
	 * server in the server capability, later on updated with what the client
	 * actually wants to have supported.
	 *
	 * Use the \b RDPDR_DTYP_* defines as a mask to check.
	 */
	UINT16 supported;

	/*** RDPDR message intercept callbacks */
	psRdpdrCapablityPDU ReceiveCaps; /**< Called for each received capability */
	psRdpdrCapablityPDU SendCaps;    /**< Called for each capability to be sent */
	psRdpdrReceivePDU ReceivePDU;    /**< Called after a RDPDR pdu was received and parsed */
	psRdpdrSendServerAnnounce
	    SendServerAnnounce; /**< Called before the server sends the announce message */
	psRdpdrReceiveAnnounceResponse
	    ReceiveAnnounceResponse; /**< Called after the client announce response is received */
	psRdpdrReceiveClientNameRequest
	    ReceiveClientNameRequest; /**< Called after a client name request is received */
	psRdpdrReceiveDeviceAnnounce
	    ReceiveDeviceAnnounce; /** < Called after a new device request was received but before the
	                              device is added */
	psRdpdrReceiveDeviceRemove ReceiveDeviceRemove; /**< Called after a new device request was
	                                                   received, but before it is removed */

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
	psRdpdrOnDeviceCreate OnDriveCreate; /**< Called for devices of type \b RDPDR_DTYP_FILESYSTEM
	                                        after \b ReceiveDeviceAnnounce */
	psRdpdrOnDeviceDelete OnDriveDelete; /**< Called for devices of type \b RDPDR_DTYP_FILESYSTEM
	                                        after \b ReceiveDeviceRemove */
	psRdpdrOnDriveCreateDirectoryComplete OnDriveCreateDirectoryComplete;
	psRdpdrOnDriveDeleteDirectoryComplete OnDriveDeleteDirectoryComplete;
	psRdpdrOnDriveQueryDirectoryComplete OnDriveQueryDirectoryComplete;
	psRdpdrOnDriveOpenFileComplete OnDriveOpenFileComplete;
	psRdpdrOnDriveReadFileComplete OnDriveReadFileComplete;
	psRdpdrOnDriveWriteFileComplete OnDriveWriteFileComplete;
	psRdpdrOnDriveCloseFileComplete OnDriveCloseFileComplete;
	psRdpdrOnDriveDeleteFileComplete OnDriveDeleteFileComplete;
	psRdpdrOnDriveRenameFileComplete OnDriveRenameFileComplete;

	/*** Serial Port callbacks registered by the server. ***/
	psRdpdrOnDeviceCreate OnSerialPortCreate; /**< Called for devices of type \b RDPDR_DTYP_SERIAL
	                                       after \b ReceiveDeviceAnnounce */
	psRdpdrOnDeviceDelete OnSerialPortDelete; /**< Called for devices of type \b RDPDR_DTYP_SERIAL
	                                       after \b ReceiveDeviceRemove */

	/*** Parallel Port callbacks registered by the server. ***/
	psRdpdrOnDeviceCreate OnParallelPortCreate; /**< Called for devices of type \b
	                                       RDPDR_DTYP_PARALLEL after \b ReceiveDeviceAnnounce */
	psRdpdrOnDeviceDelete OnParallelPortDelete; /**< Called for devices of type \b
	                                       RDPDR_DTYP_PARALLEL after \b ReceiveDeviceRemove */

	/*** Printer callbacks registered by the server. ***/
	psRdpdrOnDeviceCreate OnPrinterCreate; /**< Called for devices of type RDPDR_DTYP_PRINT after \b
	                                          ReceiveDeviceAnnounce */
	psRdpdrOnDeviceDelete OnPrinterDelete; /**< Called for devices of type RDPDR_DTYP_PRINT after \b
	                                          ReceiveDeviceRemove */

	/*** Smartcard callbacks registered by the server. ***/
	psRdpdrOnDeviceCreate OnSmartcardCreate; /**< Called for devices of type RDPDR_DTYP_SMARTCARD
	                                            after                    \b ReceiveDeviceAnnounce */
	psRdpdrOnDeviceDelete OnSmartcardDelete; /**< Called for devices of type RDPDR_DTYP_SMARTCARD
	                                            after \b                ReceiveDeviceRemove */

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
