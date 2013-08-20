/**
 * WinPR: Windows Portable Runtime
 * Windows Terminal Services API
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/crt.h>

#include <winpr/wtsapi.h>

#ifndef _WIN32

BOOL WTSStopRemoteControlSession(ULONG LogonId)
{
	return TRUE;
}

BOOL WTSStartRemoteControlSessionW(LPWSTR pTargetServerName, ULONG TargetLogonId, BYTE HotkeyVk, USHORT HotkeyModifiers)
{
	return TRUE;
}

BOOL WTSStartRemoteControlSessionA(LPSTR pTargetServerName, ULONG TargetLogonId, BYTE HotkeyVk, USHORT HotkeyModifiers)
{
	return TRUE;
}

BOOL WTSConnectSessionA(ULONG LogonId, ULONG TargetLogonId, PSTR pPassword, BOOL bWait)
{
	return TRUE;
}

BOOL WTSConnectSessionW(ULONG LogonId, ULONG TargetLogonId, PWSTR pPassword, BOOL bWait)
{
	return TRUE;
}

BOOL WTSEnumerateServersW(LPWSTR pDomainName, DWORD Reserved, DWORD Version, PWTS_SERVER_INFOW* ppServerInfo, DWORD* pCount)
{
	return TRUE;
}

BOOL WTSEnumerateServersA(LPSTR pDomainName, DWORD Reserved, DWORD Version, PWTS_SERVER_INFOA* ppServerInfo, DWORD* pCount)
{
	return TRUE;
}

HANDLE WTSOpenServerW(LPWSTR pServerName)
{
	return NULL;
}

HANDLE WTSOpenServerA(LPSTR pServerName)
{
	return NULL;
}

HANDLE WTSOpenServerExW(LPWSTR pServerName)
{
	return NULL;
}

HANDLE WTSOpenServerExA(LPSTR pServerName)
{
	return NULL;
}

VOID WTSCloseServer(HANDLE hServer)
{

}

BOOL WTSEnumerateSessionsW(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_SESSION_INFOW* ppSessionInfo, DWORD* pCount)
{
	return TRUE;
}

BOOL WTSEnumerateSessionsA(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_SESSION_INFOA* ppSessionInfo, DWORD* pCount)
{
	return TRUE;
}

BOOL WTSEnumerateSessionsExW(HANDLE hServer, DWORD* pLevel, DWORD Filter, PWTS_SESSION_INFO_1W* ppSessionInfo, DWORD* pCount)
{
	return TRUE;
}

BOOL WTSEnumerateSessionsExA(HANDLE hServer, DWORD* pLevel, DWORD Filter, PWTS_SESSION_INFO_1A* ppSessionInfo, DWORD* pCount)
{
	return TRUE;
}

BOOL WTSEnumerateProcessesW(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_PROCESS_INFOW* ppProcessInfo, DWORD* pCount)
{
	return TRUE;
}

BOOL WTSEnumerateProcessesA(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_PROCESS_INFOA* ppProcessInfo, DWORD* pCount)
{
	return TRUE;
}

BOOL WTSTerminateProcess(HANDLE hServer, DWORD ProcessId, DWORD ExitCode)
{
	return TRUE;
}

BOOL WTSQuerySessionInformationW(HANDLE hServer, DWORD SessionId, WTS_INFO_CLASS WTSInfoClass, LPWSTR* ppBuffer, DWORD* pBytesReturned)
{
	return TRUE;
}

BOOL WTSQuerySessionInformationA(HANDLE hServer, DWORD SessionId, WTS_INFO_CLASS WTSInfoClass, LPSTR* ppBuffer, DWORD* pBytesReturned)
{
	return TRUE;
}

BOOL WTSQueryUserConfigW(LPWSTR pServerName, LPWSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPWSTR* ppBuffer, DWORD* pBytesReturned)
{
	return TRUE;
}

BOOL WTSQueryUserConfigA(LPSTR pServerName, LPSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPSTR* ppBuffer, DWORD* pBytesReturned)
{
	return TRUE;
}

BOOL WTSSetUserConfigW(LPWSTR pServerName, LPWSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPWSTR pBuffer, DWORD DataLength)
{
	return TRUE;
}

BOOL WTSSetUserConfigA(LPSTR pServerName, LPSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPSTR pBuffer, DWORD DataLength)
{
	return TRUE;
}

BOOL WTSSendMessageW(HANDLE hServer, DWORD SessionId, LPWSTR pTitle, DWORD TitleLength,
		LPWSTR pMessage, DWORD MessageLength, DWORD Style, DWORD Timeout, DWORD* pResponse, BOOL bWait)
{
	return TRUE;
}

BOOL WTSSendMessageA(HANDLE hServer, DWORD SessionId, LPSTR pTitle, DWORD TitleLength,
		LPSTR pMessage, DWORD MessageLength, DWORD Style, DWORD Timeout, DWORD* pResponse, BOOL bWait)
{
	return TRUE;
}

BOOL WTSDisconnectSession(HANDLE hServer, DWORD SessionId, BOOL bWait)
{
	return TRUE;
}

BOOL WTSLogoffSession(HANDLE hServer, DWORD SessionId, BOOL bWait)
{
	return TRUE;
}

BOOL WTSShutdownSystem(HANDLE hServer, DWORD ShutdownFlag)
{
	return TRUE;
}

BOOL WTSWaitSystemEvent(HANDLE hServer, DWORD EventMask, DWORD* pEventFlags)
{
	return TRUE;
}

HANDLE WTSVirtualChannelOpen(HANDLE hServer, DWORD SessionId, LPSTR pVirtualName)
{
	return NULL;
}

HANDLE WTSVirtualChannelOpenEx(DWORD SessionId, LPSTR pVirtualName, DWORD flags)
{
	return NULL;
}

BOOL WTSVirtualChannelClose(HANDLE hChannelHandle)
{
	return TRUE;
}

BOOL WTSVirtualChannelRead(HANDLE hChannelHandle, ULONG TimeOut, PCHAR Buffer, ULONG BufferSize, PULONG pBytesRead)
{
	return TRUE;
}

BOOL WTSVirtualChannelWrite(HANDLE hChannelHandle, PCHAR Buffer, ULONG Length, PULONG pBytesWritten)
{
	return TRUE;
}

BOOL WTSVirtualChannelPurgeInput(HANDLE hChannelHandle)
{
	return TRUE;
}

BOOL WTSVirtualChannelPurgeOutput(HANDLE hChannelHandle)
{
	return TRUE;
}

BOOL WTSVirtualChannelQuery(HANDLE hChannelHandle, WTS_VIRTUAL_CLASS WtsVirtualClass, PVOID* ppBuffer, DWORD* pBytesReturned)
{
	return TRUE;
}

VOID WTSFreeMemory(PVOID pMemory)
{

}

BOOL WTSRegisterSessionNotification(HWND hWnd, DWORD dwFlags)
{
	return TRUE;
}

BOOL WTSUnRegisterSessionNotification(HWND hWnd)
{
	return TRUE;
}

BOOL WTSRegisterSessionNotificationEx(HANDLE hServer, HWND hWnd, DWORD dwFlags)
{
	return TRUE;
}

BOOL WTSUnRegisterSessionNotificationEx(HANDLE hServer, HWND hWnd)
{
	return TRUE;
}

BOOL WTSQueryUserToken(ULONG SessionId, PHANDLE phToken)
{
	return TRUE;
}

BOOL WTSFreeMemoryExW(WTS_TYPE_CLASS WTSTypeClass, PVOID pMemory, ULONG NumberOfEntries)
{
	return TRUE;
}

BOOL WTSFreeMemoryExA(WTS_TYPE_CLASS WTSTypeClass, PVOID pMemory, ULONG NumberOfEntries)
{
	return TRUE;
}

BOOL WTSEnumerateProcessesExW(HANDLE hServer, DWORD* pLevel, DWORD SessionId, LPWSTR* ppProcessInfo, DWORD* pCount)
{
	return TRUE;
}

BOOL WTSEnumerateProcessesExA(HANDLE hServer, DWORD* pLevel, DWORD SessionId, LPSTR* ppProcessInfo, DWORD* pCount)
{
	return TRUE;
}

BOOL WTSEnumerateListenersW(HANDLE hServer, PVOID pReserved, DWORD Reserved, PWTSLISTENERNAMEW pListeners, DWORD* pCount)
{
	return TRUE;
}

BOOL WTSEnumerateListenersA(HANDLE hServer, PVOID pReserved, DWORD Reserved, PWTSLISTENERNAMEA pListeners, DWORD* pCount)
{
	return TRUE;
}

BOOL WTSQueryListenerConfigW(HANDLE hServer, PVOID pReserved, DWORD Reserved, LPWSTR pListenerName, PWTSLISTENERCONFIGW pBuffer)
{
	return TRUE;
}

BOOL WTSQueryListenerConfigA(HANDLE hServer, PVOID pReserved, DWORD Reserved, LPSTR pListenerName, PWTSLISTENERCONFIGA pBuffer)
{
	return TRUE;
}

BOOL WTSCreateListenerW(HANDLE hServer, PVOID pReserved, DWORD Reserved, LPWSTR pListenerName, PWTSLISTENERCONFIGW pBuffer, DWORD flag)
{
	return TRUE;
}

BOOL WTSCreateListenerA(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPSTR pListenerName, PWTSLISTENERCONFIGA pBuffer, DWORD flag)
{
	return TRUE;
}

BOOL WTSSetListenerSecurityW(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPWSTR pListenerName, SECURITY_INFORMATION SecurityInformation,
		PSECURITY_DESCRIPTOR pSecurityDescriptor)
{
	return TRUE;
}

BOOL WTSSetListenerSecurityA(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPSTR pListenerName, SECURITY_INFORMATION SecurityInformation,
		PSECURITY_DESCRIPTOR pSecurityDescriptor)
{
	return TRUE;
}

BOOL WTSGetListenerSecurityW(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPWSTR pListenerName, SECURITY_INFORMATION SecurityInformation,
		PSECURITY_DESCRIPTOR pSecurityDescriptor, DWORD nLength, LPDWORD lpnLengthNeeded)
{
	return TRUE;
}

BOOL WTSGetListenerSecurityA(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPSTR pListenerName, SECURITY_INFORMATION SecurityInformation,
		PSECURITY_DESCRIPTOR pSecurityDescriptor, DWORD nLength, LPDWORD lpnLengthNeeded)
{
	return TRUE;
}

BOOL WTSEnableChildSessions(BOOL bEnable)
{
	return TRUE;
}

BOOL WTSIsChildSessionsEnabled(PBOOL pbEnabled)
{
	return TRUE;
}

BOOL WTSGetChildSessionId(PULONG pSessionId)
{
	return TRUE;
}

#endif
