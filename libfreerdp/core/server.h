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
#include <freerdp/utils/debug.h>
#include <freerdp/channels/wtsvc.h>

#include <winpr/synch.h>
#include <winpr/stream.h>
#include <winpr/collections.h>

typedef struct WTSVirtualChannelManager WTSVirtualChannelManager;

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

BOOL WINAPI FreeRDP_WTSStartRemoteControlSessionW(LPWSTR pTargetServerName, ULONG TargetLogonId, BYTE HotkeyVk, USHORT HotkeyModifiers);
BOOL WINAPI FreeRDP_WTSStartRemoteControlSessionA(LPSTR pTargetServerName, ULONG TargetLogonId, BYTE HotkeyVk, USHORT HotkeyModifiers);
BOOL WINAPI FreeRDP_WTSStopRemoteControlSession(ULONG LogonId);
BOOL WINAPI FreeRDP_WTSConnectSessionW(ULONG LogonId, ULONG TargetLogonId, PWSTR pPassword, BOOL bWait);
BOOL WINAPI FreeRDP_WTSConnectSessionA(ULONG LogonId, ULONG TargetLogonId, PSTR pPassword, BOOL bWait);
BOOL WINAPI FreeRDP_WTSEnumerateServersW(LPWSTR pDomainName, DWORD Reserved, DWORD Version, PWTS_SERVER_INFOW* ppServerInfo, DWORD* pCount);
BOOL WINAPI FreeRDP_WTSEnumerateServersA(LPSTR pDomainName, DWORD Reserved, DWORD Version, PWTS_SERVER_INFOA* ppServerInfo, DWORD* pCount);
HANDLE WINAPI FreeRDP_WTSOpenServerW(LPWSTR pServerName);
HANDLE WINAPI FreeRDP_WTSOpenServerA(LPSTR pServerName);
HANDLE WINAPI FreeRDP_WTSOpenServerExW(LPWSTR pServerName);
HANDLE WINAPI FreeRDP_WTSOpenServerExA(LPSTR pServerName);
VOID WINAPI FreeRDP_WTSCloseServer(HANDLE hServer);
BOOL WINAPI FreeRDP_WTSEnumerateSessionsW(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_SESSION_INFOW* ppSessionInfo, DWORD* pCount);
BOOL WINAPI FreeRDP_WTSEnumerateSessionsA(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_SESSION_INFOA* ppSessionInfo, DWORD* pCount);
BOOL WINAPI FreeRDP_WTSEnumerateSessionsExW(HANDLE hServer, DWORD* pLevel, DWORD Filter, PWTS_SESSION_INFO_1W* ppSessionInfo, DWORD* pCount);
BOOL WINAPI FreeRDP_WTSEnumerateSessionsExA(HANDLE hServer, DWORD* pLevel, DWORD Filter, PWTS_SESSION_INFO_1A* ppSessionInfo, DWORD* pCount);
BOOL WINAPI FreeRDP_WTSEnumerateProcessesW(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_PROCESS_INFOW* ppProcessInfo, DWORD* pCount);
BOOL WINAPI FreeRDP_WTSEnumerateProcessesA(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_PROCESS_INFOA* ppProcessInfo, DWORD* pCount);
BOOL WINAPI FreeRDP_WTSTerminateProcess(HANDLE hServer, DWORD ProcessId, DWORD ExitCode);
BOOL WINAPI FreeRDP_WTSQuerySessionInformationW(HANDLE hServer, DWORD SessionId, WTS_INFO_CLASS WTSInfoClass, LPWSTR* ppBuffer, DWORD* pBytesReturned);
BOOL WINAPI FreeRDP_WTSQuerySessionInformationA(HANDLE hServer, DWORD SessionId, WTS_INFO_CLASS WTSInfoClass, LPSTR* ppBuffer, DWORD* pBytesReturned);
BOOL WINAPI FreeRDP_WTSQueryUserConfigW(LPWSTR pServerName, LPWSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPWSTR* ppBuffer, DWORD* pBytesReturned);
BOOL WINAPI FreeRDP_WTSQueryUserConfigA(LPSTR pServerName, LPSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPSTR* ppBuffer, DWORD* pBytesReturned);
BOOL WINAPI FreeRDP_WTSSetUserConfigW(LPWSTR pServerName, LPWSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPWSTR pBuffer, DWORD DataLength);
BOOL WINAPI FreeRDP_WTSSetUserConfigA(LPSTR pServerName, LPSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPSTR pBuffer, DWORD DataLength);
BOOL WINAPI FreeRDP_WTSSendMessageW(HANDLE hServer, DWORD SessionId, LPWSTR pTitle, DWORD TitleLength,
		LPWSTR pMessage, DWORD MessageLength, DWORD Style, DWORD Timeout, DWORD* pResponse, BOOL bWait);
BOOL WINAPI FreeRDP_WTSSendMessageA(HANDLE hServer, DWORD SessionId, LPSTR pTitle, DWORD TitleLength,
		LPSTR pMessage, DWORD MessageLength, DWORD Style, DWORD Timeout, DWORD* pResponse, BOOL bWait);
BOOL WINAPI FreeRDP_WTSDisconnectSession(HANDLE hServer, DWORD SessionId, BOOL bWait);
BOOL WINAPI FreeRDP_WTSLogoffSession(HANDLE hServer, DWORD SessionId, BOOL bWait);
BOOL WINAPI FreeRDP_WTSShutdownSystem(HANDLE hServer, DWORD ShutdownFlag);
BOOL WINAPI FreeRDP_WTSWaitSystemEvent(HANDLE hServer, DWORD EventMask, DWORD* pEventFlags);
HANDLE WINAPI FreeRDP_WTSVirtualChannelOpen(HANDLE hServer, DWORD SessionId, LPSTR pVirtualName);
HANDLE WINAPI FreeRDP_WTSVirtualChannelOpenEx(DWORD SessionId, LPSTR pVirtualName, DWORD flags);
BOOL WINAPI FreeRDP_WTSVirtualChannelClose(HANDLE hChannelHandle);
BOOL WINAPI FreeRDP_WTSVirtualChannelRead(HANDLE hChannelHandle, ULONG TimeOut, PCHAR Buffer, ULONG BufferSize, PULONG pBytesRead);
BOOL WINAPI FreeRDP_WTSVirtualChannelWrite(HANDLE hChannelHandle, PCHAR Buffer, ULONG Length, PULONG pBytesWritten);
BOOL WINAPI FreeRDP_WTSVirtualChannelPurgeInput(HANDLE hChannelHandle);
BOOL WINAPI FreeRDP_WTSVirtualChannelPurgeOutput(HANDLE hChannelHandle);
BOOL WINAPI FreeRDP_WTSVirtualChannelQuery(HANDLE hChannelHandle, WTS_VIRTUAL_CLASS WtsVirtualClass, PVOID* ppBuffer, DWORD* pBytesReturned);
VOID WINAPI FreeRDP_WTSFreeMemory(PVOID pMemory);
BOOL WINAPI FreeRDP_WTSFreeMemoryExW(WTS_TYPE_CLASS WTSTypeClass, PVOID pMemory, ULONG NumberOfEntries);
BOOL WINAPI FreeRDP_WTSFreeMemoryExA(WTS_TYPE_CLASS WTSTypeClass, PVOID pMemory, ULONG NumberOfEntries);
BOOL WINAPI FreeRDP_WTSRegisterSessionNotification(HWND hWnd, DWORD dwFlags);
BOOL WINAPI FreeRDP_WTSUnRegisterSessionNotification(HWND hWnd);
BOOL WINAPI FreeRDP_WTSRegisterSessionNotificationEx(HANDLE hServer, HWND hWnd, DWORD dwFlags);
BOOL WINAPI FreeRDP_WTSUnRegisterSessionNotificationEx(HANDLE hServer, HWND hWnd);
BOOL WINAPI FreeRDP_WTSQueryUserToken(ULONG SessionId, PHANDLE phToken);
BOOL WINAPI FreeRDP_WTSEnumerateProcessesExW(HANDLE hServer, DWORD* pLevel, DWORD SessionId, LPWSTR* ppProcessInfo, DWORD* pCount);
BOOL WINAPI FreeRDP_WTSEnumerateProcessesExA(HANDLE hServer, DWORD* pLevel, DWORD SessionId, LPSTR* ppProcessInfo, DWORD* pCount);
BOOL WINAPI FreeRDP_WTSEnumerateListenersW(HANDLE hServer, PVOID pReserved, DWORD Reserved, PWTSLISTENERNAMEW pListeners, DWORD* pCount);
BOOL WINAPI FreeRDP_WTSEnumerateListenersA(HANDLE hServer, PVOID pReserved, DWORD Reserved, PWTSLISTENERNAMEA pListeners, DWORD* pCount);
BOOL WINAPI FreeRDP_WTSQueryListenerConfigW(HANDLE hServer, PVOID pReserved, DWORD Reserved, LPWSTR pListenerName, PWTSLISTENERCONFIGW pBuffer);
BOOL WINAPI FreeRDP_WTSQueryListenerConfigA(HANDLE hServer, PVOID pReserved, DWORD Reserved, LPSTR pListenerName, PWTSLISTENERCONFIGA pBuffer);
BOOL WINAPI FreeRDP_WTSCreateListenerW(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPWSTR pListenerName, PWTSLISTENERCONFIGW pBuffer, DWORD flag);
BOOL WINAPI FreeRDP_WTSCreateListenerA(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPSTR pListenerName, PWTSLISTENERCONFIGA pBuffer, DWORD flag);
BOOL WINAPI FreeRDP_WTSSetListenerSecurityW(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPWSTR pListenerName, SECURITY_INFORMATION SecurityInformation,
		PSECURITY_DESCRIPTOR pSecurityDescriptor);
BOOL WINAPI FreeRDP_WTSSetListenerSecurityA(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPSTR pListenerName, SECURITY_INFORMATION SecurityInformation,
		PSECURITY_DESCRIPTOR pSecurityDescriptor);
BOOL WINAPI FreeRDP_WTSGetListenerSecurityW(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPWSTR pListenerName, SECURITY_INFORMATION SecurityInformation,
		PSECURITY_DESCRIPTOR pSecurityDescriptor, DWORD nLength, LPDWORD lpnLengthNeeded);
BOOL WINAPI FreeRDP_WTSGetListenerSecurityA(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPSTR pListenerName, SECURITY_INFORMATION SecurityInformation,
		PSECURITY_DESCRIPTOR pSecurityDescriptor, DWORD nLength, LPDWORD lpnLengthNeeded);

BOOL CDECL FreeRDP_WTSEnableChildSessions(BOOL bEnable);
BOOL CDECL FreeRDP_WTSIsChildSessionsEnabled(PBOOL pbEnabled);
BOOL CDECL FreeRDP_WTSGetChildSessionId(PULONG pSessionId);

DWORD WINAPI FreeRDP_WTSGetActiveConsoleSessionId(void);

#endif /* FREERDP_CORE_SERVER_H */
