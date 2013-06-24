/**
 * WinPR: Windows Portable Runtime
 * Windows Terminal Services API (WTSAPI)
 *
 * Copyright 2013 Mike McDonald <mikem@nogginware.com>
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

#include <winpr/wtsapi.h>

BOOL WINPR_API ProcessIdToSessionId(
	DWORD ProcessId,
	DWORD *pSessionId
)
{
	return FALSE;
}

VOID WINPR_API WTSCloseServer(
	HANDLE hServer
)
{
}

BOOL WINPR_API WTSConnectSession(
	ULONG LoginId,
	ULONG TargetLogonId,
	PSTR pPassword,
	BOOL bWait
)
{
	return FALSE;
}

BOOL WINPR_API WTSCreateListener(
	HANDLE hServer,
	PVOID pReserved,
	DWORD Reserved,
	LPTSTR pListenerName,
	PWTSLISTENERCONFIG pBuffer,
	DWORD flag
)
{
	return FALSE;
}

BOOL WINPR_API WTSDisconnectSession(
	HANDLE hServer,
	DWORD SessionId,
	BOOL bWait
)
{
	return FALSE;
}

BOOL WINPR_API WTSEnableChildSessions(
	BOOL bEnable
)
{
	return FALSE;
}

BOOL WINPR_API WTSEnumerateListenersA(
	HANDLE hServer,
	PVOID pReserved,
	DWORD Reserved,
	PWTSLISTENERNAMEA pListeners,
	DWORD *pCount
)
{
	return FALSE;
}

BOOL WINPR_API WTSEnumerateListenersW(
	HANDLE hServer,
	PVOID pReserved,
	DWORD Reserved,
	PWTSLISTENERNAMEW pListeners,
	DWORD *pCount
)
{
	return FALSE;
}

BOOL WINPR_API WTSEnumerateProcesses(
	HANDLE hServer,
	DWORD Reserved,
	DWORD Version,
	PWTS_PROCESS_INFO *ppProcessInfo,
	DWORD *pCount
)
{
	return FALSE;
}

BOOL WINPR_API WTSEnumerateProcessesEx(
	HANDLE hServer,
	DWORD *pLevel,
	DWORD SessionId,
	LPTSTR *ppProcessInfo,
	DWORD *pCount
)
{
	return FALSE;
}

BOOL WINPR_API WTSEnumerateServers(
	LPTSTR pDomainName,
	DWORD Reserved,
	DWORD Version,
	PWTS_SERVER_INFO *ppServerInfo,
	DWORD *pCount
)
{
	return FALSE;
}

BOOL WINPR_API WTSEnumerateSessionsA(
	HANDLE hServer,
	DWORD Reserved,
	DWORD Version,
	PWTS_SESSION_INFOA *ppSessionInfo,
	DWORD *pCount
)
{
	return FALSE;
}

BOOL WINPR_API WTSEnumerateSessionsW(
	HANDLE hServer,
	DWORD Reserved,
	DWORD Version,
	PWTS_SESSION_INFOW *ppSessionInfo,
	DWORD *pCount
)
{
	return FALSE;
}

BOOL WINPR_API WTSEnumerateSessionsExA(
	HANDLE hServer,
	DWORD *pLevel,
	DWORD Filter,
	PWTS_SESSION_INFO_1A *ppSessionInfo,
	DWORD *pCount
)
{
	return FALSE;
}

BOOL WINPR_API WTSEnumerateSessionsExW(
	HANDLE hServer,
	DWORD *pLevel,
	DWORD Filter,
	PWTS_SESSION_INFO_1W *ppSessionInfo,
	DWORD *pCount
)
{
	return FALSE;
}

VOID WTSFreeMemory(
	PVOID pMemory
)
{
}

BOOL WINPR_API WTSFreeMemoryEx(
	WTS_TYPE_CLASS WTSTypeClass,
	PVOID pMemory,
	ULONG NumberOfEntries
)
{
	return FALSE;
}

DWORD WINPR_API WTSGetActiveConsoleSessionId()
{
	return 0;
}

BOOL WINPR_API WTSGetChildSessionId(
	DWORD *pSessionId
)
{
	return FALSE;
}

BOOL WINPR_API WTSGetListenerSecurityA(
	HANDLE hServer,
	PVOID pReserved,
	DWORD Reserved,
	LPSTR pListenerName,
	SECURITY_INFORMATION SecurityInformation,
	PSECURITY_DESCRIPTOR pSecurityDescriptor,
	DWORD nLength,
	LPDWORD lpnLengthNeeded
)
{
	return FALSE;
}

BOOL WINPR_API WTSGetListenerSecurityW(
	HANDLE hServer,
	PVOID pReserved,
	DWORD Reserved,
	LPWSTR pListenerName,
	SECURITY_INFORMATION SecurityInformation,
	PSECURITY_DESCRIPTOR pSecurityDescriptor,
	DWORD nLength,
	LPDWORD lpnLengthNeeded
)
{
	return FALSE;
}

BOOL WINPR_API WTSIsChildSessionsEnabled(
	BOOL *pbEnabled
)
{
	return FALSE;
}

BOOL WINPR_API WTSLogoffSession(
	HANDLE hServer,
	DWORD SessionId,
	BOOL bWait
)
{
	return FALSE;
}

HANDLE WINPR_API WTSOpenServer(
	LPTSTR pServerName
)
{
	return NULL;
}

HANDLE WINPR_API WTSOpenServerExA(
	LPSTR pServerName
)
{
	return NULL;
}

HANDLE WINPR_API WTSOpenServerExW(
	LPWSTR pServerName
)
{
	return NULL;
}

BOOL WINPR_API WTSQueryListenerConfig(
	HANDLE hServer,
	PVOID pReserved,
	DWORD Reserved,
	LPTSTR pListenerName,
	PWTSLISTENERCONFIG pBuffer
)
{
	return FALSE;
}

BOOL WINPR_API WTSQuerySessionInformation(
	HANDLE hServer,
	DWORD SessionId,
	WTS_INFO_CLASS WTSInfoClass,
	LPTSTR *ppBuffer,
	DWORD *pBytesReturned
)
{
	return FALSE;
}

BOOL WINPR_API WTSQueryUserConfig(
	LPTSTR pServerName,
	LPTSTR pUserName,
	WTS_CONFIG_CLASS WTSConfigClass,
	LPTSTR *ppBuffer,
	DWORD *pBytesReturned
)
{
	return FALSE;
}

BOOL WINPR_API WTSQueryUserToken(
	DWORD SessionId,
	PHANDLE phToken
)
{
	return FALSE;
}

BOOL WINPR_API WTSRegisterSessionNotification(
	HWND hWnd,
	DWORD dwFlags
)
{
	return FALSE;
}

BOOL WINPR_API WTSRegisterSessionNotificationEx(
	HANDLE hServer,
	HWND hWnd,
	DWORD dwFlags
)
{
	return FALSE;
}

BOOL WINPR_API WTSSendMessage(
	HANDLE hServer,
	DWORD SessionId,
	LPTSTR pTitle,
	DWORD TitleLength,
	LPTSTR pMessage,
	DWORD MessageLength,
	DWORD Style,
	DWORD Timeout,
	DWORD *pResponse,
	BOOL bWait
)
{
	return FALSE;
}

BOOL WINPR_API WTSSetListenerSecurity(
	HANDLE hServer,
	PVOID pReserved,
	DWORD Reserved,
	LPTSTR pListenerName,
	SECURITY_INFORMATION SecurityInformation,
	PSECURITY_DESCRIPTOR pSecurityDescriptor
)
{
	return FALSE;
}

BOOL WINPR_API WTSSetUserConfig(
	LPTSTR pServerName,
	LPTSTR pUserName,
	WTS_CONFIG_CLASS WTSConfigClass,
	LPTSTR pBuffer,
	DWORD DataLength
)
{
	return FALSE;
}

BOOL WINPR_API WTSShutdownSystem(
	HANDLE hServer,
	DWORD ShutdownFlag
)
{
	return FALSE;
}

BOOL WINPR_API WTSStartRemoteControlSession(
	LPTSTR pTargetServerName,
	ULONG TargetLogonId,
	BYTE HotkeyVk,
	USHORT HotKeyModifiers
)
{
	return FALSE;
}

BOOL WINPR_API WTSStopRemoteControlSession(
	ULONG LogonId
)
{
	return FALSE;
}

BOOL WINPR_API WTSTerminateProcess(
	HANDLE hServer,
	DWORD ProcessId,
	DWORD ExitCode
)
{
	return FALSE;
}

BOOL WINPR_API WTSUnregisterSessionNotification(
	HWND hWnd
)
{
	return FALSE;
}

BOOL WINPR_API WTSUnregisterSessionNotificationEx(
	HANDLE hServer,
	HWND hWnd
)
{
	return FALSE;
}

BOOL WINPR_API WTSVirtualChannelClose(
	HANDLE hChannelHandle
)
{
	return FALSE;
}

BOOL WINPR_API WTSVirtualChannelOpenA(
	HANDLE hServer,
	DWORD SessionId,
	LPSTR pVirtualName
)
{
	return FALSE;
}

BOOL WINPR_API WTSVirtualChannelOpenW(
	HANDLE hServer,
	DWORD SessionId,
	LPWSTR pVirtualName
)
{
	return FALSE;
}

BOOL WINPR_API WTSVirtualChannelOpenExA(
	DWORD SessionId,
	LPSTR pVirtualName,
	DWORD Flags
)
{
	return FALSE;
}

BOOL WINPR_API WTSVirtualChannelOpenExW(
	DWORD SessionId,
	LPWSTR pVirtualName,
	DWORD Flags
)
{
	return FALSE;
}

BOOL WINPR_API WTSVirtualChannelPurgeInput(
	HANDLE hChannelHandle
)
{
	return FALSE;
}

BOOL WINPR_API WTSVirtualChannelPurgeOutput(
	HANDLE hChannelHandle
)
{
	return FALSE;
}

BOOL WINPR_API WTSVirtualChannelQuery(
	HANDLE hChannelHandle,
	WTS_VIRTUAL_CLASS WTSVirtualClass,
	PVOID *ppBuffer,
	DWORD *pBytesReturned
)
{
	return FALSE;
}

BOOL WINPR_API WTSVirtualChannelRead(
	HANDLE hChannelHandle,
	ULONG TimeOut,
	PCHAR Buffer,
	ULONG BufferSize,
	PULONG pBytesRead
)
{
	return FALSE;
}

BOOL WINPR_API WTSVirtualChannelWrite(
	HANDLE hChannelHandle,
	PCHAR Buffer,
	ULONG BufferSize,
	PULONG pBytesWritten
)
{
	return FALSE;
}

BOOL WINPR_API WTSWaitSystemEvent(
	HANDLE hServer,
	DWORD EventMask,
	DWORD *pEventFlags
)
{
	return FALSE;
}
