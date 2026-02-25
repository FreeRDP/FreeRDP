/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Server Channels
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Copyright 2015 Thincast Technologies GmbH
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

#ifndef FREERDP_LIB_CORE_SERVER_H
#define FREERDP_LIB_CORE_SERVER_H

#include <freerdp/freerdp.h>
#include <freerdp/api.h>
#include <freerdp/channels/wtsvc.h>

#include <winpr/synch.h>
#include <winpr/stream.h>
#include <winpr/collections.h>

typedef struct rdp_peer_channel rdpPeerChannel;
typedef struct WTSVirtualChannelManager WTSVirtualChannelManager;

#include "rdp.h"
#include "mcs.h"

enum
{
	RDP_PEER_CHANNEL_TYPE_SVC = 0,
	RDP_PEER_CHANNEL_TYPE_DVC = 1
};

enum
{
	DVC_OPEN_STATE_NONE = 0,
	DVC_OPEN_STATE_SUCCEEDED = 1,
	DVC_OPEN_STATE_FAILED = 2,
	DVC_OPEN_STATE_CLOSED = 3
};

struct rdp_peer_channel
{
	WTSVirtualChannelManager* vcm;
	freerdp_peer* client;

	void* extra;
	UINT16 index;
	UINT32 channelId;
	UINT16 channelType;
	UINT32 channelFlags;

	wStream* receiveData;
	wMessageQueue* queue;

	BYTE dvc_open_state;
	INT32 creationStatus;
	UINT32 dvc_total_length;
	rdpMcsChannel* mcsChannel;

	char channelName[128];
	CRITICAL_SECTION writeLock;
};

struct WTSVirtualChannelManager
{
	rdpRdp* rdp;
	freerdp_peer* client;

	DWORD SessionId;
	wMessageQueue* queue;

	rdpPeerChannel* drdynvc_channel;
	BYTE drdynvc_state;
	LONG dvc_channel_id_seq;
	UINT16 dvc_spoken_version;

	WINPR_ATTR_NODISCARD psDVCCreationStatusCallback dvc_creation_status;
	void* dvc_creation_status_userdata;

	wHashTable* dynamicVirtualChannels;
};

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSStartRemoteControlSessionW(LPWSTR pTargetServerName,
                                                                ULONG TargetLogonId, BYTE HotkeyVk,
                                                                USHORT HotkeyModifiers);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSStartRemoteControlSessionA(LPSTR pTargetServerName,
                                                                ULONG TargetLogonId, BYTE HotkeyVk,
                                                                USHORT HotkeyModifiers);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSStartRemoteControlSessionExW(LPWSTR pTargetServerName,
                                                                  ULONG TargetLogonId,
                                                                  BYTE HotkeyVk,
                                                                  USHORT HotkeyModifiers,
                                                                  DWORD flags);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSStartRemoteControlSessionExA(LPSTR pTargetServerName,
                                                                  ULONG TargetLogonId,
                                                                  BYTE HotkeyVk,
                                                                  USHORT HotkeyModifiers,
                                                                  DWORD flags);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSStopRemoteControlSession(ULONG LogonId);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSConnectSessionW(ULONG LogonId, ULONG TargetLogonId,
                                                     PWSTR pPassword, BOOL bWait);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSConnectSessionA(ULONG LogonId, ULONG TargetLogonId,
                                                     PSTR pPassword, BOOL bWait);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSEnumerateServersW(LPWSTR pDomainName, DWORD Reserved,
                                                       DWORD Version,
                                                       PWTS_SERVER_INFOW* ppServerInfo,
                                                       DWORD* pCount);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSEnumerateServersA(LPSTR pDomainName, DWORD Reserved,
                                                       DWORD Version,
                                                       PWTS_SERVER_INFOA* ppServerInfo,
                                                       DWORD* pCount);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL HANDLE WINAPI FreeRDP_WTSOpenServerW(LPWSTR pServerName);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL HANDLE WINAPI FreeRDP_WTSOpenServerA(LPSTR pServerName);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL HANDLE WINAPI FreeRDP_WTSOpenServerExW(LPWSTR pServerName);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL HANDLE WINAPI FreeRDP_WTSOpenServerExA(LPSTR pServerName);

FREERDP_LOCAL VOID WINAPI FreeRDP_WTSCloseServer(HANDLE hServer);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSEnumerateSessionsW(HANDLE hServer, DWORD Reserved,
                                                        DWORD Version,
                                                        PWTS_SESSION_INFOW* ppSessionInfo,
                                                        DWORD* pCount);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSEnumerateSessionsA(HANDLE hServer, DWORD Reserved,
                                                        DWORD Version,
                                                        PWTS_SESSION_INFOA* ppSessionInfo,
                                                        DWORD* pCount);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSEnumerateSessionsExW(HANDLE hServer, DWORD* pLevel,
                                                          DWORD Filter,
                                                          PWTS_SESSION_INFO_1W* ppSessionInfo,
                                                          DWORD* pCount);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSEnumerateSessionsExA(HANDLE hServer, DWORD* pLevel,
                                                          DWORD Filter,
                                                          PWTS_SESSION_INFO_1A* ppSessionInfo,
                                                          DWORD* pCount);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSEnumerateProcessesW(HANDLE hServer, DWORD Reserved,
                                                         DWORD Version,
                                                         PWTS_PROCESS_INFOW* ppProcessInfo,
                                                         DWORD* pCount);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSEnumerateProcessesA(HANDLE hServer, DWORD Reserved,
                                                         DWORD Version,
                                                         PWTS_PROCESS_INFOA* ppProcessInfo,
                                                         DWORD* pCount);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSTerminateProcess(HANDLE hServer, DWORD ProcessId,
                                                      DWORD ExitCode);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSQuerySessionInformationW(HANDLE hServer, DWORD SessionId,
                                                              WTS_INFO_CLASS WTSInfoClass,
                                                              LPWSTR* ppBuffer,
                                                              DWORD* pBytesReturned);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSQuerySessionInformationA(HANDLE hServer, DWORD SessionId,
                                                              WTS_INFO_CLASS WTSInfoClass,
                                                              LPSTR* ppBuffer,
                                                              DWORD* pBytesReturned);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSQueryUserConfigW(LPWSTR pServerName, LPWSTR pUserName,
                                                      WTS_CONFIG_CLASS WTSConfigClass,
                                                      LPWSTR* ppBuffer, DWORD* pBytesReturned);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSQueryUserConfigA(LPSTR pServerName, LPSTR pUserName,
                                                      WTS_CONFIG_CLASS WTSConfigClass,
                                                      LPSTR* ppBuffer, DWORD* pBytesReturned);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSSetUserConfigW(LPWSTR pServerName, LPWSTR pUserName,
                                                    WTS_CONFIG_CLASS WTSConfigClass, LPWSTR pBuffer,
                                                    DWORD DataLength);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSSetUserConfigA(LPSTR pServerName, LPSTR pUserName,
                                                    WTS_CONFIG_CLASS WTSConfigClass, LPSTR pBuffer,
                                                    DWORD DataLength);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSSendMessageW(HANDLE hServer, DWORD SessionId, LPWSTR pTitle,
                                                  DWORD TitleLength, LPWSTR pMessage,
                                                  DWORD MessageLength, DWORD Style, DWORD Timeout,
                                                  DWORD* pResponse, BOOL bWait);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSSendMessageA(HANDLE hServer, DWORD SessionId, LPSTR pTitle,
                                                  DWORD TitleLength, LPSTR pMessage,
                                                  DWORD MessageLength, DWORD Style, DWORD Timeout,
                                                  DWORD* pResponse, BOOL bWait);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSDisconnectSession(HANDLE hServer, DWORD SessionId, BOOL bWait);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSLogoffSession(HANDLE hServer, DWORD SessionId, BOOL bWait);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSShutdownSystem(HANDLE hServer, DWORD ShutdownFlag);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSWaitSystemEvent(HANDLE hServer, DWORD EventMask,
                                                     DWORD* pEventFlags);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL HANDLE WINAPI FreeRDP_WTSVirtualChannelOpen(HANDLE hServer, DWORD SessionId,
                                                          LPSTR pVirtualName);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL HANDLE WINAPI FreeRDP_WTSVirtualChannelOpenEx(DWORD SessionId, LPSTR pVirtualName,
                                                            DWORD flags);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSVirtualChannelClose(HANDLE hChannelHandle);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSVirtualChannelRead(HANDLE hChannelHandle, ULONG TimeOut,
                                                        PCHAR Buffer, ULONG BufferSize,
                                                        PULONG pBytesRead);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSVirtualChannelWrite(HANDLE hChannelHandle, PCHAR Buffer,
                                                         ULONG Length, PULONG pBytesWritten);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSVirtualChannelPurgeInput(HANDLE hChannelHandle);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSVirtualChannelPurgeOutput(HANDLE hChannelHandle);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSVirtualChannelQuery(HANDLE hChannelHandle,
                                                         WTS_VIRTUAL_CLASS WtsVirtualClass,
                                                         PVOID* ppBuffer, DWORD* pBytesReturned);

FREERDP_LOCAL VOID WINAPI FreeRDP_WTSFreeMemory(PVOID pMemory);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSFreeMemoryExW(WTS_TYPE_CLASS WTSTypeClass, PVOID pMemory,
                                                   ULONG NumberOfEntries);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSFreeMemoryExA(WTS_TYPE_CLASS WTSTypeClass, PVOID pMemory,
                                                   ULONG NumberOfEntries);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSRegisterSessionNotification(HWND hWnd, DWORD dwFlags);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSUnRegisterSessionNotification(HWND hWnd);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSRegisterSessionNotificationEx(HANDLE hServer, HWND hWnd,
                                                                   DWORD dwFlags);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSUnRegisterSessionNotificationEx(HANDLE hServer, HWND hWnd);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSQueryUserToken(ULONG SessionId, PHANDLE phToken);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSEnumerateProcessesExW(HANDLE hServer, DWORD* pLevel,
                                                           DWORD SessionId, LPWSTR* ppProcessInfo,
                                                           DWORD* pCount);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSEnumerateProcessesExA(HANDLE hServer, DWORD* pLevel,
                                                           DWORD SessionId, LPSTR* ppProcessInfo,
                                                           DWORD* pCount);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSEnumerateListenersW(HANDLE hServer, PVOID pReserved,
                                                         DWORD Reserved,
                                                         PWTSLISTENERNAMEW pListeners,
                                                         DWORD* pCount);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSEnumerateListenersA(HANDLE hServer, PVOID pReserved,
                                                         DWORD Reserved,
                                                         PWTSLISTENERNAMEA pListeners,
                                                         DWORD* pCount);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSQueryListenerConfigW(HANDLE hServer, PVOID pReserved,
                                                          DWORD Reserved, LPWSTR pListenerName,
                                                          PWTSLISTENERCONFIGW pBuffer);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSQueryListenerConfigA(HANDLE hServer, PVOID pReserved,
                                                          DWORD Reserved, LPSTR pListenerName,
                                                          PWTSLISTENERCONFIGA pBuffer);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSCreateListenerW(HANDLE hServer, PVOID pReserved,
                                                     DWORD Reserved, LPWSTR pListenerName,
                                                     PWTSLISTENERCONFIGW pBuffer, DWORD flag);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSCreateListenerA(HANDLE hServer, PVOID pReserved,
                                                     DWORD Reserved, LPSTR pListenerName,
                                                     PWTSLISTENERCONFIGA pBuffer, DWORD flag);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSSetListenerSecurityW(HANDLE hServer, PVOID pReserved,
                                                          DWORD Reserved, LPWSTR pListenerName,
                                                          SECURITY_INFORMATION SecurityInformation,
                                                          PSECURITY_DESCRIPTOR pSecurityDescriptor);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSSetListenerSecurityA(HANDLE hServer, PVOID pReserved,
                                                          DWORD Reserved, LPSTR pListenerName,
                                                          SECURITY_INFORMATION SecurityInformation,
                                                          PSECURITY_DESCRIPTOR pSecurityDescriptor);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSGetListenerSecurityW(HANDLE hServer, PVOID pReserved,
                                                          DWORD Reserved, LPWSTR pListenerName,
                                                          SECURITY_INFORMATION SecurityInformation,
                                                          PSECURITY_DESCRIPTOR pSecurityDescriptor,
                                                          DWORD nLength, LPDWORD lpnLengthNeeded);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSGetListenerSecurityA(HANDLE hServer, PVOID pReserved,
                                                          DWORD Reserved, LPSTR pListenerName,
                                                          SECURITY_INFORMATION SecurityInformation,
                                                          PSECURITY_DESCRIPTOR pSecurityDescriptor,
                                                          DWORD nLength, LPDWORD lpnLengthNeeded);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL CDECL FreeRDP_WTSEnableChildSessions(BOOL bEnable);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL CDECL FreeRDP_WTSIsChildSessionsEnabled(PBOOL pbEnabled);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL CDECL FreeRDP_WTSGetChildSessionId(PULONG pSessionId);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL DWORD WINAPI FreeRDP_WTSGetActiveConsoleSessionId(void);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSLogoffUser(HANDLE hServer);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL WINAPI FreeRDP_WTSLogonUser(HANDLE hServer, LPCSTR username, LPCSTR password,
                                               LPCSTR domain);

FREERDP_LOCAL void server_channel_common_free(rdpPeerChannel*);

WINPR_ATTR_MALLOC(server_channel_common_free, 1)
WINPR_ATTR_NODISCARD
FREERDP_LOCAL rdpPeerChannel* server_channel_common_new(freerdp_peer* client, UINT16 index,
                                                        UINT32 channelId, size_t chunkSize,
                                                        const wObject* callback, const char* name);

#endif /* FREERDP_LIB_CORE_SERVER_H */
