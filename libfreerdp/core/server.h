/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Server Channels
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CORE_SERVER_H
#define FREERDP_CORE_SERVER_H

#include <freerdp/freerdp.h>
#include <freerdp/utils/list.h>
#include <freerdp/utils/debug.h>
#include <freerdp/channels/wtsvc.h>

#include <winpr/synch.h>
#include <winpr/stream.h>
#include <winpr/collections.h>

#include "rdp.h"

#define CREATE_REQUEST_PDU			0x01
#define DATA_FIRST_PDU				0x02
#define DATA_PDU				0x03
#define CLOSE_REQUEST_PDU			0x04
#define CAPABILITY_REQUEST_PDU			0x05

enum
{
	RDP_PEER_CHANNEL_TYPE_SVC = 0,
	RDP_PEER_CHANNEL_TYPE_DVC = 1
};

enum
{
	DRDYNVC_STATE_NONE = 0,
	DRDYNVC_STATE_INITIALIZED = 1,
	DRDYNVC_STATE_READY = 2
};

enum
{
	DVC_OPEN_STATE_NONE = 0,
	DVC_OPEN_STATE_SUCCEEDED = 1,
	DVC_OPEN_STATE_FAILED = 2,
	DVC_OPEN_STATE_CLOSED = 3
};

typedef struct rdp_peer_channel rdpPeerChannel;

struct rdp_peer_channel
{
	WTSVirtualChannelManager* vcm;
	freerdp_peer* client;

	UINT32 channelId;
	UINT16 channelType;
	UINT16 index;

	wStream* receiveData;
	wMessageQueue* queue;

	BYTE dvc_open_state;
	UINT32 dvc_total_length;
};

struct WTSVirtualChannelManager
{
	rdpRdp* rdp;
	freerdp_peer* client;

	DWORD SessionId;
	wMessageQueue* queue;

	rdpPeerChannel* drdynvc_channel;
	BYTE drdynvc_state;
	UINT32 dvc_channel_id_seq;

	wArrayList* dynamicVirtualChannels;
};

BOOL FreeRDP_WTSStartRemoteControlSessionW(LPWSTR pTargetServerName, ULONG TargetLogonId, BYTE HotkeyVk, USHORT HotkeyModifiers);
BOOL FreeRDP_WTSStartRemoteControlSessionA(LPSTR pTargetServerName, ULONG TargetLogonId, BYTE HotkeyVk, USHORT HotkeyModifiers);
BOOL FreeRDP_WTSStopRemoteControlSession(ULONG LogonId);
BOOL FreeRDP_WTSConnectSessionW(ULONG LogonId, ULONG TargetLogonId, PWSTR pPassword, BOOL bWait);
BOOL FreeRDP_WTSConnectSessionA(ULONG LogonId, ULONG TargetLogonId, PSTR pPassword, BOOL bWait);
BOOL FreeRDP_WTSEnumerateServersW(LPWSTR pDomainName, DWORD Reserved, DWORD Version, PWTS_SERVER_INFOW* ppServerInfo, DWORD* pCount);
BOOL FreeRDP_WTSEnumerateServersA(LPSTR pDomainName, DWORD Reserved, DWORD Version, PWTS_SERVER_INFOA* ppServerInfo, DWORD* pCount);
HANDLE FreeRDP_WTSOpenServerW(LPWSTR pServerName);
HANDLE FreeRDP_WTSOpenServerA(LPSTR pServerName);
HANDLE FreeRDP_WTSOpenServerExW(LPWSTR pServerName);
HANDLE FreeRDP_WTSOpenServerExA(LPSTR pServerName);
VOID FreeRDP_WTSCloseServer(HANDLE hServer);
BOOL FreeRDP_WTSEnumerateSessionsW(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_SESSION_INFOW* ppSessionInfo, DWORD* pCount);
BOOL FreeRDP_WTSEnumerateSessionsA(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_SESSION_INFOA* ppSessionInfo, DWORD* pCount);
BOOL FreeRDP_WTSEnumerateSessionsExW(HANDLE hServer, DWORD* pLevel, DWORD Filter, PWTS_SESSION_INFO_1W* ppSessionInfo, DWORD* pCount);
BOOL FreeRDP_WTSEnumerateSessionsExA(HANDLE hServer, DWORD* pLevel, DWORD Filter, PWTS_SESSION_INFO_1A* ppSessionInfo, DWORD* pCount);
BOOL FreeRDP_WTSEnumerateProcessesW(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_PROCESS_INFOW* ppProcessInfo, DWORD* pCount);
BOOL FreeRDP_WTSEnumerateProcessesA(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_PROCESS_INFOA* ppProcessInfo, DWORD* pCount);
BOOL FreeRDP_WTSTerminateProcess(HANDLE hServer, DWORD ProcessId, DWORD ExitCode);
BOOL FreeRDP_WTSQuerySessionInformationW(HANDLE hServer, DWORD SessionId, WTS_INFO_CLASS WTSInfoClass, LPWSTR* ppBuffer, DWORD* pBytesReturned);
BOOL FreeRDP_WTSQuerySessionInformationA(HANDLE hServer, DWORD SessionId, WTS_INFO_CLASS WTSInfoClass, LPSTR* ppBuffer, DWORD* pBytesReturned);
BOOL FreeRDP_WTSQueryUserConfigW(LPWSTR pServerName, LPWSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPWSTR* ppBuffer, DWORD* pBytesReturned);
BOOL FreeRDP_WTSQueryUserConfigA(LPSTR pServerName, LPSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPSTR* ppBuffer, DWORD* pBytesReturned);
BOOL FreeRDP_WTSSetUserConfigW(LPWSTR pServerName, LPWSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPWSTR pBuffer, DWORD DataLength);
BOOL FreeRDP_WTSSetUserConfigA(LPSTR pServerName, LPSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPSTR pBuffer, DWORD DataLength);
BOOL FreeRDP_WTSSendMessageW(HANDLE hServer, DWORD SessionId, LPWSTR pTitle, DWORD TitleLength,
		LPWSTR pMessage, DWORD MessageLength, DWORD Style, DWORD Timeout, DWORD* pResponse, BOOL bWait);
BOOL FreeRDP_WTSSendMessageA(HANDLE hServer, DWORD SessionId, LPSTR pTitle, DWORD TitleLength,
		LPSTR pMessage, DWORD MessageLength, DWORD Style, DWORD Timeout, DWORD* pResponse, BOOL bWait);
BOOL FreeRDP_WTSDisconnectSession(HANDLE hServer, DWORD SessionId, BOOL bWait);
BOOL FreeRDP_WTSLogoffSession(HANDLE hServer, DWORD SessionId, BOOL bWait);
BOOL FreeRDP_WTSShutdownSystem(HANDLE hServer, DWORD ShutdownFlag);
BOOL FreeRDP_WTSWaitSystemEvent(HANDLE hServer, DWORD EventMask, DWORD* pEventFlags);
HANDLE FreeRDP_WTSVirtualChannelOpen(HANDLE hServer, DWORD SessionId, LPSTR pVirtualName);
HANDLE FreeRDP_WTSVirtualChannelOpenEx(DWORD SessionId, LPSTR pVirtualName, DWORD flags);
BOOL FreeRDP_WTSVirtualChannelClose(HANDLE hChannelHandle);
BOOL FreeRDP_WTSVirtualChannelRead(HANDLE hChannelHandle, ULONG TimeOut, PCHAR Buffer, ULONG BufferSize, PULONG pBytesRead);
BOOL FreeRDP_WTSVirtualChannelWrite(HANDLE hChannelHandle, PCHAR Buffer, ULONG Length, PULONG pBytesWritten);
BOOL FreeRDP_WTSVirtualChannelPurgeInput(HANDLE hChannelHandle);
BOOL FreeRDP_WTSVirtualChannelPurgeOutput(HANDLE hChannelHandle);
BOOL FreeRDP_WTSVirtualChannelQuery(HANDLE hChannelHandle, WTS_VIRTUAL_CLASS WtsVirtualClass, PVOID* ppBuffer, DWORD* pBytesReturned);
VOID FreeRDP_WTSFreeMemory(PVOID pMemory);
BOOL FreeRDP_WTSFreeMemoryExW(WTS_TYPE_CLASS WTSTypeClass, PVOID pMemory, ULONG NumberOfEntries);
BOOL FreeRDP_WTSFreeMemoryExA(WTS_TYPE_CLASS WTSTypeClass, PVOID pMemory, ULONG NumberOfEntries);
BOOL FreeRDP_WTSRegisterSessionNotification(HWND hWnd, DWORD dwFlags);
BOOL FreeRDP_WTSUnRegisterSessionNotification(HWND hWnd);
BOOL FreeRDP_WTSRegisterSessionNotificationEx(HANDLE hServer, HWND hWnd, DWORD dwFlags);
BOOL FreeRDP_WTSUnRegisterSessionNotificationEx(HANDLE hServer, HWND hWnd);
BOOL FreeRDP_WTSQueryUserToken(ULONG SessionId, PHANDLE phToken);
BOOL FreeRDP_WTSEnumerateProcessesExW(HANDLE hServer, DWORD* pLevel, DWORD SessionId, LPWSTR* ppProcessInfo, DWORD* pCount);
BOOL FreeRDP_WTSEnumerateProcessesExA(HANDLE hServer, DWORD* pLevel, DWORD SessionId, LPSTR* ppProcessInfo, DWORD* pCount);
BOOL FreeRDP_WTSEnumerateListenersW(HANDLE hServer, PVOID pReserved, DWORD Reserved, PWTSLISTENERNAMEW pListeners, DWORD* pCount);
BOOL FreeRDP_WTSEnumerateListenersA(HANDLE hServer, PVOID pReserved, DWORD Reserved, PWTSLISTENERNAMEA pListeners, DWORD* pCount);
BOOL FreeRDP_WTSQueryListenerConfigW(HANDLE hServer, PVOID pReserved, DWORD Reserved, LPWSTR pListenerName, PWTSLISTENERCONFIGW pBuffer);
BOOL FreeRDP_WTSQueryListenerConfigA(HANDLE hServer, PVOID pReserved, DWORD Reserved, LPSTR pListenerName, PWTSLISTENERCONFIGA pBuffer);
BOOL FreeRDP_WTSCreateListenerW(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPWSTR pListenerName, PWTSLISTENERCONFIGW pBuffer, DWORD flag);
BOOL FreeRDP_WTSCreateListenerA(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPSTR pListenerName, PWTSLISTENERCONFIGA pBuffer, DWORD flag);
BOOL FreeRDP_WTSSetListenerSecurityW(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPWSTR pListenerName, SECURITY_INFORMATION SecurityInformation,
		PSECURITY_DESCRIPTOR pSecurityDescriptor);
BOOL FreeRDP_WTSSetListenerSecurityA(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPSTR pListenerName, SECURITY_INFORMATION SecurityInformation,
		PSECURITY_DESCRIPTOR pSecurityDescriptor);
BOOL FreeRDP_WTSGetListenerSecurityW(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPWSTR pListenerName, SECURITY_INFORMATION SecurityInformation,
		PSECURITY_DESCRIPTOR pSecurityDescriptor, DWORD nLength, LPDWORD lpnLengthNeeded);
BOOL FreeRDP_WTSGetListenerSecurityA(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPSTR pListenerName, SECURITY_INFORMATION SecurityInformation,
		PSECURITY_DESCRIPTOR pSecurityDescriptor, DWORD nLength, LPDWORD lpnLengthNeeded);
BOOL FreeRDP_WTSEnableChildSessions(BOOL bEnable);
BOOL FreeRDP_WTSIsChildSessionsEnabled(PBOOL pbEnabled);
BOOL FreeRDP_WTSGetChildSessionId(PULONG pSessionId);
DWORD FreeRDP_WTSGetActiveConsoleSessionId(void);

#endif /* FREERDP_CORE_SERVER_H */
