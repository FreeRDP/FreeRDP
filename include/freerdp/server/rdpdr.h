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
#include <freerdp/config.h>
#include <freerdp/types.h>
#include <freerdp/channels/wtsvc.h>
#include <freerdp/channels/rdpdr.h>
#include <freerdp/channels/scard.h>
#include <freerdp/utils/rdpdr_utils.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/**
	 * Server Interface
	 */

	typedef struct s_rdpdr_server_context RdpdrServerContext;
	typedef struct s_rdpdr_server_private RdpdrServerPrivate;

	/** RDPDR protocol header */
	typedef struct
	{
		UINT16 Component; /**< Component identifier */
		UINT16 PacketId;  /**< Packet identifier */
	} RDPDR_HEADER;

#ifndef __MINGW32__
	/** File directory information structure */
	typedef struct
	{
		UINT32 NextEntryOffset;       /**< Offset to next entry */
		UINT32 FileIndex;             /**< File index */
		LARGE_INTEGER CreationTime;   /**< File creation time */
		LARGE_INTEGER LastAccessTime; /**< Last access time */
		LARGE_INTEGER LastWriteTime;  /**< Last write time */
		LARGE_INTEGER ChangeTime;     /**< Last change time */
		LARGE_INTEGER EndOfFile;      /**< End of file position */
		LARGE_INTEGER AllocationSize; /**< Allocated size */
		UINT32 FileAttributes;        /**< File attributes */
#if defined(WITH_WCHAR_FILE_DIRECTORY_INFORMATION)
		WCHAR FileName[512]; /**< File name (wide char) */
#else
		char FileName[512]; /**< File name */
#endif
	} FILE_DIRECTORY_INFORMATION;
#endif

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
	typedef UINT (*psRdpdrReceiveClientNameRequest)(RdpdrServerContext* context,
	                                                size_t ComputerNameLen, const char* name);

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
	                                     UINT32 deviceId, UINT32 fileId, UINT32 length,
	                                     UINT32 offset);
	typedef UINT (*psRdpdrDriveWriteFile)(RdpdrServerContext* context, void* callbackData,
	                                      UINT32 deviceId, UINT32 fileId, const char* buffer,
	                                      UINT32 length, UINT32 offset);
	typedef UINT (*psRdpdrDriveCloseFile)(RdpdrServerContext* context, void* callbackData,
	                                      UINT32 deviceId, UINT32 fileId);
	typedef UINT (*psRdpdrDriveDeleteFile)(RdpdrServerContext* context, void* callbackData,
	                                       UINT32 deviceId, const char* path);
	typedef UINT (*psRdpdrDriveRenameFile)(RdpdrServerContext* context, void* callbackData,
	                                       UINT32 deviceId, const char* oldPath,
	                                       const char* newPath);

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
	                                               UINT32 ioStatus, const char* buffer,
	                                               UINT32 length);
	typedef void (*psRdpdrOnDriveWriteFileComplete)(RdpdrServerContext* context, void* callbackData,
	                                                UINT32 ioStatus, UINT32 bytesWritten);
	typedef void (*psRdpdrOnDriveCloseFileComplete)(RdpdrServerContext* context, void* callbackData,
	                                                UINT32 ioStatus);
	typedef void (*psRdpdrOnDriveDeleteFileComplete)(RdpdrServerContext* context,
	                                                 void* callbackData, UINT32 ioStatus);
	typedef void (*psRdpdrOnDriveRenameFileComplete)(RdpdrServerContext* context,
	                                                 void* callbackData, UINT32 ioStatus);

	typedef UINT (*psRdpdrOnDeviceCreate)(RdpdrServerContext* context, const RdpdrDevice* device);
	typedef UINT (*psRdpdrOnDeviceDelete)(RdpdrServerContext* context, UINT32 deviceId);

	typedef UINT (*psRdpdrSmartcardEstablishContext)(RdpdrServerContext* context,
	                                                 void* callbackData, UINT32 dwScope,
	                                                 UINT32* completionId);
	typedef UINT (*psRdpdrSmartcardReleaseContext)(RdpdrServerContext* context, void* callbackData,
	                                               const REDIR_SCARDCONTEXT* hContext,
	                                               UINT32* completionId);
	typedef UINT (*psRdpdrSmartcardIsValidContext)(RdpdrServerContext* context, void* callbackData,
	                                               const REDIR_SCARDCONTEXT* hContext,
	                                               UINT32* completionId);
	typedef UINT (*psRdpdrSmartcardListReaderGroups)(RdpdrServerContext* context,
	                                                 void* callbackData,
	                                                 const ListReaderGroups_Call* call,
	                                                 UINT32* completionId);
	typedef UINT (*psRdpdrSmartcardListReaders)(RdpdrServerContext* context, void* callbackData,
	                                            const ListReaders_Call* call, UINT32* completionId);
	typedef UINT (*psRdpdrSmartcardGetStatusChangeA)(RdpdrServerContext* context,
	                                                 void* callbackData,
	                                                 const GetStatusChangeA_Call* call,
	                                                 UINT32* completionId);
	typedef UINT (*psRdpdrSmartcardGetStatusChangeW)(RdpdrServerContext* context,
	                                                 void* callbackData,
	                                                 const GetStatusChangeW_Call* call,
	                                                 UINT32* completionId);
	typedef UINT (*psRdpdrSmartcardCancel)(RdpdrServerContext* context, void* callbackData,
	                                       const REDIR_SCARDCONTEXT* hContext,
	                                       UINT32* completionId);
	typedef UINT (*psRdpdrSmartcardConnectA)(RdpdrServerContext* context, void* callbackData,
	                                         const ConnectA_Call* call, UINT32* completionId);
	typedef UINT (*psRdpdrSmartcardConnectW)(RdpdrServerContext* context, void* callbackData,
	                                         const ConnectW_Call* call, UINT32* completionId);
	typedef UINT (*psRdpdrSmartcardReconnect)(RdpdrServerContext* context, void* callbackData,
	                                          const Reconnect_Call* call, UINT32* completionId);
	typedef UINT (*psRdpdrSmartcardDisconnect)(RdpdrServerContext* context, void* callbackData,
	                                           const HCardAndDisposition_Call* call,
	                                           UINT32* completionId);
	typedef UINT (*psRdpdrSmartcardBeginTransaction)(RdpdrServerContext* context,
	                                                 void* callbackData,
	                                                 const HCardAndDisposition_Call* call,
	                                                 UINT32* completionId);
	typedef UINT (*psRdpdrSmartcardEndTransaction)(RdpdrServerContext* context, void* callbackData,
	                                               const HCardAndDisposition_Call* call,
	                                               UINT32* completionId);
	typedef UINT (*psRdpdrSmartcardStatus)(RdpdrServerContext* context, void* callbackData,
	                                       const Status_Call* call, UINT32* completionId);
	typedef UINT (*psRdpdrSmartcardTransmit)(RdpdrServerContext* context, void* callbackData,
	                                         const Transmit_Call* call, UINT32* completionId);
	typedef UINT (*psRdpdrSmartcardControl)(RdpdrServerContext* context, void* callbackData,
	                                        const Control_Call* call, UINT32* completionId);
	typedef UINT (*psRdpdrSmartcardGetAttrib)(RdpdrServerContext* context, void* callbackData,
	                                          const GetAttrib_Call* call, UINT32* completionId);
	typedef UINT (*psRdpdrSmartcardSetAttrib)(RdpdrServerContext* context, void* callbackData,
	                                          const SetAttrib_Call* call, UINT32* completionId);

	typedef void (*psRdpdrOnSmartcardEstablishContextComplete)(RdpdrServerContext* context,
	                                                           void* callbackData, UINT32 ioStatus,
	                                                           LONG returnCode,
	                                                           const EstablishContext_Return* ret);
	typedef void (*psRdpdrOnSmartcardReleaseContextComplete)(RdpdrServerContext* context,
	                                                         void* callbackData, UINT32 ioStatus,
	                                                         LONG returnCode);
	typedef void (*psRdpdrOnSmartcardIsValidContextComplete)(RdpdrServerContext* context,
	                                                         void* callbackData, UINT32 ioStatus,
	                                                         LONG returnCode);
	typedef void (*psRdpdrOnSmartcardListReaderGroupsComplete)(RdpdrServerContext* context,
	                                                           void* callbackData, UINT32 ioStatus,
	                                                           LONG returnCode,
	                                                           const ListReaderGroups_Return* ret);
	typedef void (*psRdpdrOnSmartcardListReadersComplete)(RdpdrServerContext* context,
	                                                      void* callbackData, UINT32 ioStatus,
	                                                      LONG returnCode,
	                                                      const ListReaders_Return* ret);
	typedef void (*psRdpdrOnSmartcardGetStatusChangeComplete)(RdpdrServerContext* context,
	                                                          void* callbackData, UINT32 ioStatus,
	                                                          LONG returnCode,
	                                                          const GetStatusChange_Return* ret);
	typedef void (*psRdpdrOnSmartcardCancelComplete)(RdpdrServerContext* context,
	                                                 void* callbackData, UINT32 ioStatus,
	                                                 LONG returnCode);
	typedef void (*psRdpdrOnSmartcardConnectComplete)(RdpdrServerContext* context,
	                                                  void* callbackData, UINT32 ioStatus,
	                                                  LONG returnCode, const Connect_Return* ret);
	typedef void (*psRdpdrOnSmartcardReconnectComplete)(RdpdrServerContext* context,
	                                                    void* callbackData, UINT32 ioStatus,
	                                                    LONG returnCode,
	                                                    const Reconnect_Return* ret);
	typedef void (*psRdpdrOnSmartcardDisconnectComplete)(RdpdrServerContext* context,
	                                                     void* callbackData, UINT32 ioStatus,
	                                                     LONG returnCode);
	typedef void (*psRdpdrOnSmartcardBeginTransactionComplete)(RdpdrServerContext* context,
	                                                           void* callbackData, UINT32 ioStatus,
	                                                           LONG returnCode);
	typedef void (*psRdpdrOnSmartcardEndTransactionComplete)(RdpdrServerContext* context,
	                                                         void* callbackData, UINT32 ioStatus,
	                                                         LONG returnCode);
	typedef void (*psRdpdrOnSmartcardStatusComplete)(RdpdrServerContext* context,
	                                                 void* callbackData, UINT32 ioStatus,
	                                                 LONG returnCode, const Status_Return* ret);
	typedef void (*psRdpdrOnSmartcardTransmitComplete)(RdpdrServerContext* context,
	                                                   void* callbackData, UINT32 ioStatus,
	                                                   LONG returnCode, const Transmit_Return* ret);
	typedef void (*psRdpdrOnSmartcardControlComplete)(RdpdrServerContext* context,
	                                                  void* callbackData, UINT32 ioStatus,
	                                                  LONG returnCode, const Control_Return* ret);
	typedef void (*psRdpdrOnSmartcardGetAttribComplete)(RdpdrServerContext* context,
	                                                    void* callbackData, UINT32 ioStatus,
	                                                    LONG returnCode,
	                                                    const GetAttrib_Return* ret);
	typedef void (*psRdpdrOnSmartcardSetAttribComplete)(RdpdrServerContext* context,
	                                                    void* callbackData, UINT32 ioStatus,
	                                                    LONG returnCode);

	struct s_rdpdr_server_context
	{
		HANDLE vcm; /**< Virtual channel manager handle */

		WINPR_ATTR_NODISCARD psRdpdrStart Start; /**< Start the RDPDR server */
		psRdpdrStop Stop;                        /**< Stop the RDPDR server */

		RdpdrServerPrivate* priv; /**< Private server context data */

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
		WINPR_ATTR_NODISCARD psRdpdrCapablityPDU
		    ReceiveCaps; /**< Called for each received capability */
		WINPR_ATTR_NODISCARD psRdpdrCapablityPDU
		    SendCaps; /**< Called for each capability to be sent */
		WINPR_ATTR_NODISCARD psRdpdrReceivePDU
		    ReceivePDU; /**< Called after a RDPDR pdu was received and parsed */
		WINPR_ATTR_NODISCARD psRdpdrSendServerAnnounce
		    SendServerAnnounce; /**< Called before the server sends the announce message */
		WINPR_ATTR_NODISCARD psRdpdrReceiveAnnounceResponse
		    ReceiveAnnounceResponse; /**< Called after the client announce response is received */
		WINPR_ATTR_NODISCARD psRdpdrReceiveClientNameRequest
		    ReceiveClientNameRequest; /**< Called after a client name request is received */
		WINPR_ATTR_NODISCARD psRdpdrReceiveDeviceAnnounce
		    ReceiveDeviceAnnounce; /**< Called after a new device request was received but before
		                              the device is added */
		WINPR_ATTR_NODISCARD psRdpdrReceiveDeviceRemove
		    ReceiveDeviceRemove; /**< Called after a new device request was
		       received, but before it is removed */

		/*** Drive APIs called by the server. ***/
		WINPR_ATTR_NODISCARD psRdpdrDriveCreateDirectory
		    DriveCreateDirectory; /**< Create directory */
		WINPR_ATTR_NODISCARD psRdpdrDriveDeleteDirectory
		    DriveDeleteDirectory; /**< Delete directory */
		WINPR_ATTR_NODISCARD psRdpdrDriveQueryDirectory DriveQueryDirectory; /**< Query directory */
		WINPR_ATTR_NODISCARD psRdpdrDriveOpenFile DriveOpenFile;             /**< Open file */
		WINPR_ATTR_NODISCARD psRdpdrDriveReadFile DriveReadFile;             /**< Read file */
		WINPR_ATTR_NODISCARD psRdpdrDriveWriteFile DriveWriteFile;           /**< Write file */
		WINPR_ATTR_NODISCARD psRdpdrDriveCloseFile DriveCloseFile;           /**< Close file */
		WINPR_ATTR_NODISCARD psRdpdrDriveDeleteFile DriveDeleteFile;         /**< Delete file */
		WINPR_ATTR_NODISCARD psRdpdrDriveRenameFile DriveRenameFile;         /**< Rename file */

		/*** Drive callbacks registered by the server. ***/
		WINPR_ATTR_NODISCARD psRdpdrOnDeviceCreate
		    OnDriveCreate; /**< Called for devices of type \b
		 RDPDR_DTYP_FILESYSTEM after \b ReceiveDeviceAnnounce */
		WINPR_ATTR_NODISCARD psRdpdrOnDeviceDelete
		    OnDriveDelete; /**< Called for devices of type \b
		 RDPDR_DTYP_FILESYSTEM after \b ReceiveDeviceRemove */
		psRdpdrOnDriveCreateDirectoryComplete
		    OnDriveCreateDirectoryComplete; /**< DriveCreateDirectory completion callback */
		psRdpdrOnDriveDeleteDirectoryComplete
		    OnDriveDeleteDirectoryComplete; /**< DriveDeleteDirectory completion callback */
		psRdpdrOnDriveQueryDirectoryComplete
		    OnDriveQueryDirectoryComplete; /**< DriveQueryDirectory completion callback */
		psRdpdrOnDriveOpenFileComplete
		    OnDriveOpenFileComplete; /**< DriveOpenFile completion callback */
		psRdpdrOnDriveReadFileComplete
		    OnDriveReadFileComplete; /**< DriveReadFile completion callback */
		psRdpdrOnDriveWriteFileComplete
		    OnDriveWriteFileComplete; /**< DriveWriteFile completion callback */
		psRdpdrOnDriveCloseFileComplete
		    OnDriveCloseFileComplete; /**< DriveCloseFile completion callback */
		psRdpdrOnDriveDeleteFileComplete
		    OnDriveDeleteFileComplete; /**< DriveDeleteFile completion callback */
		psRdpdrOnDriveRenameFileComplete
		    OnDriveRenameFileComplete; /**< DriveRenameFile completion callback */

		/*** Serial Port callbacks registered by the server. ***/
		WINPR_ATTR_NODISCARD psRdpdrOnDeviceCreate
		    OnSerialPortCreate; /**< Called for devices of type
		\b RDPDR_DTYP_SERIAL after \b ReceiveDeviceAnnounce */
		WINPR_ATTR_NODISCARD psRdpdrOnDeviceDelete
		    OnSerialPortDelete; /**< Called for devices of type
		\b RDPDR_DTYP_SERIAL after \b ReceiveDeviceRemove */

		/*** Parallel Port callbacks registered by the server. ***/
		WINPR_ATTR_NODISCARD psRdpdrOnDeviceCreate
		    OnParallelPortCreate; /**< Called for devices of type
		\b RDPDR_DTYP_PARALLEL after \b ReceiveDeviceAnnounce */
		WINPR_ATTR_NODISCARD psRdpdrOnDeviceDelete
		    OnParallelPortDelete; /**< Called for devices of type
		\b RDPDR_DTYP_PARALLEL after \b ReceiveDeviceRemove */

		/*** Printer callbacks registered by the server. ***/
		WINPR_ATTR_NODISCARD psRdpdrOnDeviceCreate OnPrinterCreate; /**< Called for devices of type
		                                          RDPDR_DTYP_PRINT after \b ReceiveDeviceAnnounce */
		WINPR_ATTR_NODISCARD psRdpdrOnDeviceDelete OnPrinterDelete; /**< Called for devices of type
		                                          RDPDR_DTYP_PRINT after \b ReceiveDeviceRemove */

		/*** Smartcard callbacks registered by the server. ***/
		WINPR_ATTR_NODISCARD psRdpdrOnDeviceCreate
		    OnSmartcardCreate; /**< Called for devices of type RDPDR_DTYP_SMARTCARD
		     after                    \b ReceiveDeviceAnnounce */
		WINPR_ATTR_NODISCARD psRdpdrOnDeviceDelete
		    OnSmartcardDelete; /**< Called for devices of type
		     RDPDR_DTYP_SMARTCARD after \b ReceiveDeviceRemove */

		rdpContext* rdpcontext; /**< RDP context */

		/*** New Smartcard APIs called by the server. ***/
		WINPR_ATTR_NODISCARD psRdpdrSmartcardEstablishContext
		    SmartcardEstablishContext; /**< Send SCardEstablishContext to the client. */
		WINPR_ATTR_NODISCARD psRdpdrSmartcardReleaseContext
		    SmartcardReleaseContext; /**< Send SCardReleaseContext to the client. */
		WINPR_ATTR_NODISCARD psRdpdrSmartcardIsValidContext
		    SmartcardIsValidContext; /**< Send SCardIsValidContext to the client. */
		WINPR_ATTR_NODISCARD psRdpdrSmartcardListReaderGroups
		    SmartcardListReaderGroupsA; /**< Send SCardListReaderGroupsA to the client. */
		WINPR_ATTR_NODISCARD psRdpdrSmartcardListReaderGroups
		    SmartcardListReaderGroupsW; /**< Send SCardListReaderGroupsW to the client. */
		WINPR_ATTR_NODISCARD psRdpdrSmartcardListReaders
		    SmartcardListReadersA; /**< Send SCardListReadersA to the client. */
		WINPR_ATTR_NODISCARD psRdpdrSmartcardListReaders
		    SmartcardListReadersW; /**< Send SCardListReadersW to the client. */
		WINPR_ATTR_NODISCARD psRdpdrSmartcardGetStatusChangeA
		    SmartcardGetStatusChangeA; /**< Send SCardGetStatusChangeA to the client. */
		WINPR_ATTR_NODISCARD psRdpdrSmartcardGetStatusChangeW
		    SmartcardGetStatusChangeW; /**< Send SCardGetStatusChangeW to the client. */
		WINPR_ATTR_NODISCARD psRdpdrSmartcardCancel
		    SmartcardCancel; /**< Send SCardCancel to the client. */
		WINPR_ATTR_NODISCARD psRdpdrSmartcardConnectA
		    SmartcardConnectA; /**< Send SCardConnectA to the client. */
		WINPR_ATTR_NODISCARD psRdpdrSmartcardConnectW
		    SmartcardConnectW; /**< Send SCardConnectW to the client. */
		WINPR_ATTR_NODISCARD psRdpdrSmartcardReconnect
		    SmartcardReconnect; /**< Send SCardReconnect to the client. */
		WINPR_ATTR_NODISCARD psRdpdrSmartcardDisconnect
		    SmartcardDisconnect; /**< Send SCardDisconnect to the client. */
		WINPR_ATTR_NODISCARD psRdpdrSmartcardBeginTransaction
		    SmartcardBeginTransaction; /**< Send SCardBeginTransaction to the client. */
		WINPR_ATTR_NODISCARD psRdpdrSmartcardEndTransaction
		    SmartcardEndTransaction; /**< Send SCardEndTransaction to the client. */
		WINPR_ATTR_NODISCARD psRdpdrSmartcardStatus
		    SmartcardStatusA; /**< Send SCardStatusA to the client. */
		WINPR_ATTR_NODISCARD psRdpdrSmartcardStatus
		    SmartcardStatusW; /**< Send SCardStatusW to the client. */
		WINPR_ATTR_NODISCARD psRdpdrSmartcardTransmit
		    SmartcardTransmit; /**< Send SCardTransmit to the client. */
		WINPR_ATTR_NODISCARD psRdpdrSmartcardControl
		    SmartcardControl; /**< Send SCardControl to the client. */
		WINPR_ATTR_NODISCARD psRdpdrSmartcardGetAttrib
		    SmartcardGetAttrib; /**< Send SCardGetAttrib to the client. */
		WINPR_ATTR_NODISCARD psRdpdrSmartcardSetAttrib
		    SmartcardSetAttrib; /**< Send SCardSetAttrib to the client. */

		/*** New Smartcard callbacks registered by the server. ***/
		/* clang-format off */
		psRdpdrOnSmartcardEstablishContextComplete
		    OnSmartcardEstablishContextComplete; /**< Completion callback for
		                                             SmartcardEstablishContext. */
		psRdpdrOnSmartcardReleaseContextComplete
		    OnSmartcardReleaseContextComplete; /**< Completion callback for
		                                           SmartcardReleaseContext. */
		psRdpdrOnSmartcardIsValidContextComplete
		    OnSmartcardIsValidContextComplete; /**< Completion callback for
		                                           SmartcardIsValidContext. */
		psRdpdrOnSmartcardListReaderGroupsComplete
		    OnSmartcardListReaderGroupsComplete; /**< Completion callback for
		                                             SmartcardListReaderGroups. */
		psRdpdrOnSmartcardListReadersComplete
		    OnSmartcardListReadersComplete; /**< Completion callback for
		                                        SmartcardListReaders. */
		psRdpdrOnSmartcardGetStatusChangeComplete
		    OnSmartcardGetStatusChangeComplete; /**< Completion callback for
		                                            SmartcardGetStatusChange. */
		psRdpdrOnSmartcardCancelComplete
		    OnSmartcardCancelComplete; /**< Completion callback for
		                                   SmartcardCancel. */
		psRdpdrOnSmartcardConnectComplete
		    OnSmartcardConnectComplete; /**< Completion callback for
		                                    SmartcardConnect. */
		psRdpdrOnSmartcardReconnectComplete
		    OnSmartcardReconnectComplete; /**< Completion callback for
		                                      SmartcardReconnect. */
		psRdpdrOnSmartcardDisconnectComplete
		    OnSmartcardDisconnectComplete; /**< Completion callback for
		                                       SmartcardDisconnect. */
		psRdpdrOnSmartcardBeginTransactionComplete
		    OnSmartcardBeginTransactionComplete; /**< Completion callback for
		                                             SmartcardBeginTransaction. */
		psRdpdrOnSmartcardEndTransactionComplete
		    OnSmartcardEndTransactionComplete; /**< Completion callback for
		                                           SmartcardEndTransaction. */
		psRdpdrOnSmartcardStatusComplete
		    OnSmartcardStatusComplete; /**< Completion callback for
		                                   SmartcardStatus. */
		psRdpdrOnSmartcardTransmitComplete
		    OnSmartcardTransmitComplete; /**< Completion callback for
		                                     SmartcardTransmit. */
		psRdpdrOnSmartcardControlComplete
		    OnSmartcardControlComplete; /**< Completion callback for
		                                    SmartcardControl. */
		psRdpdrOnSmartcardGetAttribComplete
		    OnSmartcardGetAttribComplete; /**< Completion callback for
		                                      SmartcardGetAttrib. */
		psRdpdrOnSmartcardSetAttribComplete
		    OnSmartcardSetAttribComplete; /**< Completion callback for
		                                      SmartcardSetAttrib. */
		/* clang-format on */
	};

	/**
	 * @brief Free an RDPDR server context.
	 * @param context The RDPDR server context to free.
	 */
	FREERDP_API void rdpdr_server_context_free(RdpdrServerContext* context);

	/**
	 * @brief Create a new RDPDR server context.
	 * @param vcm Virtual channel manager handle.
	 * @return Newly allocated RDPDR server context, or NULL on failure.
	 */
	WINPR_ATTR_MALLOC(rdpdr_server_context_free, 1)
	FREERDP_API RdpdrServerContext* rdpdr_server_context_new(HANDLE vcm);

	/**
	 * @brief Remove and free a pending IRP request from the context.
	 * @param context The RDPDR server context.
	 * @param completionId The completion ID of the IRP to discard.
	 * @since version 3.28
	 */
	FREERDP_API void rdpdr_server_discard_request(RdpdrServerContext* context, UINT32 completionId);
#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNEL_RDPDR_SERVER_RDPDR_H */
