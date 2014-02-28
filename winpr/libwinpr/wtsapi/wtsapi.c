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
#include <winpr/synch.h>

#include <winpr/wtsapi.h>

/**
 * Remote Desktop Services API Functions:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/aa383464/
 */

static PWtsApiFunctionTable g_WtsApi = NULL;

BOOL WINAPI WTSStartRemoteControlSessionW(LPWSTR pTargetServerName, ULONG TargetLogonId, BYTE HotkeyVk, USHORT HotkeyModifiers)
{
	if (!g_WtsApi || !g_WtsApi->StartRemoteControlSessionW)
		return FALSE;

	return g_WtsApi->StartRemoteControlSessionW(pTargetServerName, TargetLogonId, HotkeyVk, HotkeyModifiers);
}

BOOL WINAPI WTSStartRemoteControlSessionA(LPSTR pTargetServerName, ULONG TargetLogonId, BYTE HotkeyVk, USHORT HotkeyModifiers)
{
	if (!g_WtsApi || !g_WtsApi->StartRemoteControlSessionA)
		return FALSE;

	return g_WtsApi->StartRemoteControlSessionA(pTargetServerName, TargetLogonId, HotkeyVk, HotkeyModifiers);
}

BOOL WINAPI WTSStopRemoteControlSession(ULONG LogonId)
{
	if (!g_WtsApi || !g_WtsApi->StopRemoteControlSession)
		return FALSE;

	return g_WtsApi->StopRemoteControlSession(LogonId);
}

BOOL WINAPI WTSConnectSessionW(ULONG LogonId, ULONG TargetLogonId, PWSTR pPassword, BOOL bWait)
{
	if (!g_WtsApi || !g_WtsApi->ConnectSessionW)
		return FALSE;

	return g_WtsApi->ConnectSessionW(LogonId, TargetLogonId, pPassword, bWait);
}

BOOL WINAPI WTSConnectSessionA(ULONG LogonId, ULONG TargetLogonId, PSTR pPassword, BOOL bWait)
{
	if (!g_WtsApi || !g_WtsApi->ConnectSessionA)
		return FALSE;

	return g_WtsApi->ConnectSessionA(LogonId, TargetLogonId, pPassword, bWait);
}

BOOL WINAPI WTSEnumerateServersW(LPWSTR pDomainName, DWORD Reserved, DWORD Version, PWTS_SERVER_INFOW* ppServerInfo, DWORD* pCount)
{
	if (!g_WtsApi || !g_WtsApi->EnumerateServersW)
		return FALSE;

	return g_WtsApi->EnumerateServersW(pDomainName, Reserved, Version, ppServerInfo, pCount);
}

BOOL WINAPI WTSEnumerateServersA(LPSTR pDomainName, DWORD Reserved, DWORD Version, PWTS_SERVER_INFOA* ppServerInfo, DWORD* pCount)
{
	if (!g_WtsApi || !g_WtsApi->EnumerateServersA)
		return FALSE;

	return g_WtsApi->EnumerateServersA(pDomainName, Reserved, Version, ppServerInfo, pCount);
}

HANDLE WINAPI WTSOpenServerW(LPWSTR pServerName)
{
	if (!g_WtsApi || !g_WtsApi->OpenServerW)
		return FALSE;

	return g_WtsApi->OpenServerW(pServerName);
}

HANDLE WINAPI WTSOpenServerA(LPSTR pServerName)
{
	if (!g_WtsApi || !g_WtsApi->OpenServerA)
		return FALSE;

	return g_WtsApi->OpenServerA(pServerName);
}

HANDLE WINAPI WTSOpenServerExW(LPWSTR pServerName)
{
	if (!g_WtsApi || !g_WtsApi->OpenServerExW)
		return FALSE;

	return g_WtsApi->OpenServerExW(pServerName);
}

HANDLE WINAPI WTSOpenServerExA(LPSTR pServerName)
{
	if (!g_WtsApi || !g_WtsApi->OpenServerExA)
		return FALSE;

	return g_WtsApi->OpenServerExA(pServerName);
}

VOID WINAPI WTSCloseServer(HANDLE hServer)
{
	if (!g_WtsApi || !g_WtsApi->CloseServer)
		return;

	g_WtsApi->CloseServer(hServer);
}

BOOL WINAPI WTSEnumerateSessionsW(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_SESSION_INFOW* ppSessionInfo, DWORD* pCount)
{
	if (!g_WtsApi || !g_WtsApi->EnumerateSessionsW)
		return FALSE;

	return g_WtsApi->EnumerateSessionsW(hServer, Reserved, Version, ppSessionInfo, pCount);
}

BOOL WINAPI WTSEnumerateSessionsA(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_SESSION_INFOA* ppSessionInfo, DWORD* pCount)
{
	if (!g_WtsApi || !g_WtsApi->EnumerateSessionsA)
		return FALSE;

	return g_WtsApi->EnumerateSessionsA(hServer, Reserved, Version, ppSessionInfo, pCount);
}

BOOL WINAPI WTSEnumerateSessionsExW(HANDLE hServer, DWORD* pLevel, DWORD Filter, PWTS_SESSION_INFO_1W* ppSessionInfo, DWORD* pCount)
{
	if (!g_WtsApi || !g_WtsApi->EnumerateSessionsExW)
		return FALSE;

	return g_WtsApi->EnumerateSessionsExW(hServer, pLevel, Filter, ppSessionInfo, pCount);
}

BOOL WINAPI WTSEnumerateSessionsExA(HANDLE hServer, DWORD* pLevel, DWORD Filter, PWTS_SESSION_INFO_1A* ppSessionInfo, DWORD* pCount)
{
	if (!g_WtsApi || !g_WtsApi->EnumerateSessionsExA)
		return FALSE;

	return g_WtsApi->EnumerateSessionsExA(hServer, pLevel, Filter, ppSessionInfo, pCount);
}

BOOL WINAPI WTSEnumerateProcessesW(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_PROCESS_INFOW* ppProcessInfo, DWORD* pCount)
{
	if (!g_WtsApi || !g_WtsApi->EnumerateProcessesW)
		return FALSE;

	return g_WtsApi->EnumerateProcessesW(hServer, Reserved, Version, ppProcessInfo, pCount);
}

BOOL WINAPI WTSEnumerateProcessesA(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_PROCESS_INFOA* ppProcessInfo, DWORD* pCount)
{
	if (!g_WtsApi || !g_WtsApi->EnumerateProcessesA)
		return FALSE;

	return g_WtsApi->EnumerateProcessesA(hServer, Reserved, Version, ppProcessInfo, pCount);
}

BOOL WINAPI WTSTerminateProcess(HANDLE hServer, DWORD ProcessId, DWORD ExitCode)
{
	if (!g_WtsApi || !g_WtsApi->TerminateProcess)
		return FALSE;

	return g_WtsApi->TerminateProcess(hServer, ProcessId, ExitCode);
}

BOOL WINAPI WTSQuerySessionInformationW(HANDLE hServer, DWORD SessionId, WTS_INFO_CLASS WTSInfoClass, LPWSTR* ppBuffer, DWORD* pBytesReturned)
{
	if (!g_WtsApi || !g_WtsApi->QuerySessionInformationW)
		return FALSE;

	return g_WtsApi->QuerySessionInformationW(hServer, SessionId, WTSInfoClass, ppBuffer, pBytesReturned);
}

BOOL WINAPI WTSQuerySessionInformationA(HANDLE hServer, DWORD SessionId, WTS_INFO_CLASS WTSInfoClass, LPSTR* ppBuffer, DWORD* pBytesReturned)
{
	if (!g_WtsApi || !g_WtsApi->QuerySessionInformationA)
		return FALSE;

	return g_WtsApi->QuerySessionInformationA(hServer, SessionId, WTSInfoClass, ppBuffer, pBytesReturned);
}

BOOL WINAPI WTSQueryUserConfigW(LPWSTR pServerName, LPWSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPWSTR* ppBuffer, DWORD* pBytesReturned)
{
	if (!g_WtsApi || !g_WtsApi->QueryUserConfigW)
		return FALSE;

	return g_WtsApi->QueryUserConfigW(pServerName, pUserName, WTSConfigClass, ppBuffer, pBytesReturned);
}

BOOL WINAPI WTSQueryUserConfigA(LPSTR pServerName, LPSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPSTR* ppBuffer, DWORD* pBytesReturned)
{
	if (!g_WtsApi || !g_WtsApi->QueryUserConfigA)
		return FALSE;

	return g_WtsApi->QueryUserConfigA(pServerName, pUserName, WTSConfigClass, ppBuffer, pBytesReturned);
}

BOOL WINAPI WTSSetUserConfigW(LPWSTR pServerName, LPWSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPWSTR pBuffer, DWORD DataLength)
{
	if (!g_WtsApi || !g_WtsApi->SetUserConfigW)
		return FALSE;

	return g_WtsApi->SetUserConfigW(pServerName, pUserName, WTSConfigClass, pBuffer, DataLength);
}

BOOL WINAPI WTSSetUserConfigA(LPSTR pServerName, LPSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPSTR pBuffer, DWORD DataLength)
{
	if (!g_WtsApi || !g_WtsApi->SetUserConfigA)
		return FALSE;

	return g_WtsApi->SetUserConfigA(pServerName, pUserName, WTSConfigClass, pBuffer, DataLength);
}

BOOL WINAPI WTSSendMessageW(HANDLE hServer, DWORD SessionId, LPWSTR pTitle, DWORD TitleLength,
		LPWSTR pMessage, DWORD MessageLength, DWORD Style, DWORD Timeout, DWORD* pResponse, BOOL bWait)
{
	if (!g_WtsApi || !g_WtsApi->SendMessageW)
		return FALSE;

	return g_WtsApi->SendMessageW(hServer, SessionId, pTitle, TitleLength,
			pMessage, MessageLength, Style, Timeout, pResponse, bWait);
}

BOOL WINAPI WTSSendMessageA(HANDLE hServer, DWORD SessionId, LPSTR pTitle, DWORD TitleLength,
		LPSTR pMessage, DWORD MessageLength, DWORD Style, DWORD Timeout, DWORD* pResponse, BOOL bWait)
{
	if (!g_WtsApi || !g_WtsApi->SendMessageA)
		return FALSE;

	return g_WtsApi->SendMessageA(hServer, SessionId, pTitle, TitleLength,
			pMessage, MessageLength, Style, Timeout, pResponse, bWait);
}

BOOL WINAPI WTSDisconnectSession(HANDLE hServer, DWORD SessionId, BOOL bWait)
{
	if (!g_WtsApi || !g_WtsApi->DisconnectSession)
		return FALSE;

	return g_WtsApi->DisconnectSession(hServer, SessionId, bWait);
}

BOOL WINAPI WTSLogoffSession(HANDLE hServer, DWORD SessionId, BOOL bWait)
{
	if (!g_WtsApi || !g_WtsApi->LogoffSession)
		return FALSE;

	return g_WtsApi->LogoffSession(hServer, SessionId, bWait);
}

BOOL WINAPI WTSShutdownSystem(HANDLE hServer, DWORD ShutdownFlag)
{
	if (!g_WtsApi || !g_WtsApi->ShutdownSystem)
		return FALSE;

	return g_WtsApi->ShutdownSystem(hServer, ShutdownFlag);
}

BOOL WINAPI WTSWaitSystemEvent(HANDLE hServer, DWORD EventMask, DWORD* pEventFlags)
{
	if (!g_WtsApi || !g_WtsApi->WaitSystemEvent)
		return FALSE;

	return g_WtsApi->WaitSystemEvent(hServer, EventMask, pEventFlags);
}

HANDLE WINAPI WTSVirtualChannelOpen(HANDLE hServer, DWORD SessionId, LPSTR pVirtualName)
{
	if (!g_WtsApi || !g_WtsApi->VirtualChannelOpen)
		return FALSE;

	return g_WtsApi->VirtualChannelOpen(hServer, SessionId, pVirtualName);
}

HANDLE WINAPI WTSVirtualChannelOpenEx(DWORD SessionId, LPSTR pVirtualName, DWORD flags)
{
	if (!g_WtsApi || !g_WtsApi->VirtualChannelOpenEx)
		return FALSE;

	return g_WtsApi->VirtualChannelOpenEx(SessionId, pVirtualName, flags);
}

BOOL WINAPI WTSVirtualChannelClose(HANDLE hChannelHandle)
{
	if (!g_WtsApi || !g_WtsApi->VirtualChannelClose)
		return FALSE;

	return g_WtsApi->VirtualChannelClose(hChannelHandle);
}

BOOL WINAPI WTSVirtualChannelRead(HANDLE hChannelHandle, ULONG TimeOut, PCHAR Buffer, ULONG BufferSize, PULONG pBytesRead)
{
	if (!g_WtsApi || !g_WtsApi->VirtualChannelRead)
		return FALSE;

	return g_WtsApi->VirtualChannelRead(hChannelHandle, TimeOut, Buffer, BufferSize, pBytesRead);
}

BOOL WINAPI WTSVirtualChannelWrite(HANDLE hChannelHandle, PCHAR Buffer, ULONG Length, PULONG pBytesWritten)
{
	if (!g_WtsApi || !g_WtsApi->VirtualChannelWrite)
		return FALSE;

	return g_WtsApi->VirtualChannelWrite(hChannelHandle, Buffer, Length, pBytesWritten);
}

BOOL WINAPI WTSVirtualChannelPurgeInput(HANDLE hChannelHandle)
{
	if (!g_WtsApi || !g_WtsApi->VirtualChannelPurgeInput)
		return FALSE;

	return g_WtsApi->VirtualChannelPurgeInput(hChannelHandle);
}

BOOL WINAPI WTSVirtualChannelPurgeOutput(HANDLE hChannelHandle)
{
	if (!g_WtsApi || !g_WtsApi->VirtualChannelPurgeOutput)
		return FALSE;

	return g_WtsApi->VirtualChannelPurgeOutput(hChannelHandle);
}

BOOL WINAPI WTSVirtualChannelQuery(HANDLE hChannelHandle, WTS_VIRTUAL_CLASS WtsVirtualClass, PVOID* ppBuffer, DWORD* pBytesReturned)
{
	if (!g_WtsApi || !g_WtsApi->VirtualChannelQuery)
		return FALSE;

	return g_WtsApi->VirtualChannelQuery(hChannelHandle, WtsVirtualClass, ppBuffer, pBytesReturned);
}

VOID WINAPI WTSFreeMemory(PVOID pMemory)
{
	if (!g_WtsApi || !g_WtsApi->FreeMemory)
		return;

	g_WtsApi->FreeMemory(pMemory);
}

BOOL WINAPI WTSFreeMemoryExW(WTS_TYPE_CLASS WTSTypeClass, PVOID pMemory, ULONG NumberOfEntries)
{
	if (!g_WtsApi || !g_WtsApi->FreeMemoryExW)
		return FALSE;

	return g_WtsApi->FreeMemoryExW(WTSTypeClass, pMemory, NumberOfEntries);
}

BOOL WINAPI WTSFreeMemoryExA(WTS_TYPE_CLASS WTSTypeClass, PVOID pMemory, ULONG NumberOfEntries)
{
	if (!g_WtsApi || !g_WtsApi->FreeMemoryExA)
		return FALSE;

	return g_WtsApi->FreeMemoryExA(WTSTypeClass, pMemory, NumberOfEntries);
}

BOOL WINAPI WTSRegisterSessionNotification(HWND hWnd, DWORD dwFlags)
{
	if (!g_WtsApi || !g_WtsApi->RegisterSessionNotification)
		return FALSE;

	return g_WtsApi->RegisterSessionNotification(hWnd, dwFlags);
}

BOOL WINAPI WTSUnRegisterSessionNotification(HWND hWnd)
{
	if (!g_WtsApi || !g_WtsApi->UnRegisterSessionNotification)
		return FALSE;

	return g_WtsApi->UnRegisterSessionNotification(hWnd);
}

BOOL WINAPI WTSRegisterSessionNotificationEx(HANDLE hServer, HWND hWnd, DWORD dwFlags)
{
	if (!g_WtsApi || !g_WtsApi->RegisterSessionNotificationEx)
		return FALSE;

	return g_WtsApi->RegisterSessionNotificationEx(hServer, hWnd, dwFlags);
}

BOOL WINAPI WTSUnRegisterSessionNotificationEx(HANDLE hServer, HWND hWnd)
{
	if (!g_WtsApi || !g_WtsApi->UnRegisterSessionNotificationEx)
		return FALSE;

	return g_WtsApi->UnRegisterSessionNotificationEx(hServer, hWnd);
}

BOOL WINAPI WTSQueryUserToken(ULONG SessionId, PHANDLE phToken)
{
	if (!g_WtsApi || !g_WtsApi->QueryUserToken)
		return FALSE;

	return g_WtsApi->QueryUserToken(SessionId, phToken);
}

BOOL WINAPI WTSEnumerateProcessesExW(HANDLE hServer, DWORD* pLevel, DWORD SessionId, LPWSTR* ppProcessInfo, DWORD* pCount)
{
	if (!g_WtsApi || !g_WtsApi->EnumerateProcessesExW)
		return FALSE;

	return g_WtsApi->EnumerateProcessesExW(hServer, pLevel, SessionId, ppProcessInfo, pCount);
}

BOOL WINAPI WTSEnumerateProcessesExA(HANDLE hServer, DWORD* pLevel, DWORD SessionId, LPSTR* ppProcessInfo, DWORD* pCount)
{
	if (!g_WtsApi || !g_WtsApi->EnumerateProcessesExA)
		return FALSE;

	return g_WtsApi->EnumerateProcessesExA(hServer, pLevel, SessionId, ppProcessInfo, pCount);
}

BOOL WINAPI WTSEnumerateListenersW(HANDLE hServer, PVOID pReserved, DWORD Reserved, PWTSLISTENERNAMEW pListeners, DWORD* pCount)
{
	if (!g_WtsApi || !g_WtsApi->EnumerateListenersW)
		return FALSE;

	return g_WtsApi->EnumerateListenersW(hServer, pReserved, Reserved, pListeners, pCount);
}

BOOL WINAPI WTSEnumerateListenersA(HANDLE hServer, PVOID pReserved, DWORD Reserved, PWTSLISTENERNAMEA pListeners, DWORD* pCount)
{
	if (!g_WtsApi || !g_WtsApi->EnumerateListenersA)
		return FALSE;

	return g_WtsApi->EnumerateListenersA(hServer, pReserved, Reserved, pListeners, pCount);
}

BOOL WINAPI WTSQueryListenerConfigW(HANDLE hServer, PVOID pReserved, DWORD Reserved, LPWSTR pListenerName, PWTSLISTENERCONFIGW pBuffer)
{
	if (!g_WtsApi || !g_WtsApi->QueryListenerConfigW)
		return FALSE;

	return g_WtsApi->QueryListenerConfigW(hServer, pReserved, Reserved, pListenerName, pBuffer);
}

BOOL WINAPI WTSQueryListenerConfigA(HANDLE hServer, PVOID pReserved, DWORD Reserved, LPSTR pListenerName, PWTSLISTENERCONFIGA pBuffer)
{
	if (!g_WtsApi || !g_WtsApi->QueryListenerConfigA)
		return FALSE;

	return g_WtsApi->QueryListenerConfigA(hServer, pReserved, Reserved, pListenerName, pBuffer);
}

BOOL WINAPI WTSCreateListenerW(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPWSTR pListenerName, PWTSLISTENERCONFIGW pBuffer, DWORD flag)
{
	if (!g_WtsApi || !g_WtsApi->CreateListenerW)
		return FALSE;

	return g_WtsApi->CreateListenerW(hServer, pReserved, Reserved, pListenerName, pBuffer, flag);
}

BOOL WINAPI WTSCreateListenerA(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPSTR pListenerName, PWTSLISTENERCONFIGA pBuffer, DWORD flag)
{
	if (!g_WtsApi || !g_WtsApi->CreateListenerA)
		return FALSE;

	return g_WtsApi->CreateListenerA(hServer, pReserved, Reserved, pListenerName, pBuffer, flag);
}

BOOL WINAPI WTSSetListenerSecurityW(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPWSTR pListenerName, SECURITY_INFORMATION SecurityInformation,
		PSECURITY_DESCRIPTOR pSecurityDescriptor)
{
	if (!g_WtsApi || !g_WtsApi->SetListenerSecurityW)
		return FALSE;

	return g_WtsApi->SetListenerSecurityW(hServer, pReserved, Reserved,
			pListenerName, SecurityInformation, pSecurityDescriptor);
}

BOOL WINAPI WTSSetListenerSecurityA(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPSTR pListenerName, SECURITY_INFORMATION SecurityInformation,
		PSECURITY_DESCRIPTOR pSecurityDescriptor)
{
	if (!g_WtsApi || !g_WtsApi->SetListenerSecurityA)
		return FALSE;

	return g_WtsApi->SetListenerSecurityA(hServer, pReserved, Reserved,
			pListenerName, SecurityInformation, pSecurityDescriptor);
}

BOOL WINAPI WTSGetListenerSecurityW(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPWSTR pListenerName, SECURITY_INFORMATION SecurityInformation,
		PSECURITY_DESCRIPTOR pSecurityDescriptor, DWORD nLength, LPDWORD lpnLengthNeeded)
{
	if (!g_WtsApi || !g_WtsApi->GetListenerSecurityW)
		return FALSE;

	return g_WtsApi->GetListenerSecurityW(hServer, pReserved, Reserved, pListenerName,
			SecurityInformation, pSecurityDescriptor, nLength, lpnLengthNeeded);
}

BOOL WINAPI WTSGetListenerSecurityA(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPSTR pListenerName, SECURITY_INFORMATION SecurityInformation,
		PSECURITY_DESCRIPTOR pSecurityDescriptor, DWORD nLength, LPDWORD lpnLengthNeeded)
{
	if (!g_WtsApi || !g_WtsApi->GetListenerSecurityA)
		return FALSE;

	return g_WtsApi->GetListenerSecurityA(hServer, pReserved, Reserved, pListenerName,
			SecurityInformation, pSecurityDescriptor, nLength, lpnLengthNeeded);
}

BOOL CDECL WTSEnableChildSessions(BOOL bEnable)
{
	if (!g_WtsApi || !g_WtsApi->EnableChildSessions)
		return FALSE;

	return g_WtsApi->EnableChildSessions(bEnable);
}

BOOL CDECL WTSIsChildSessionsEnabled(PBOOL pbEnabled)
{
	if (!g_WtsApi || !g_WtsApi->IsChildSessionsEnabled)
		return FALSE;

	return g_WtsApi->IsChildSessionsEnabled(pbEnabled);
}

BOOL CDECL WTSGetChildSessionId(PULONG pSessionId)
{
	if (!g_WtsApi || !g_WtsApi->GetChildSessionId)
		return FALSE;

	return g_WtsApi->GetChildSessionId(pSessionId);
}

#ifndef _WIN32

/**
 * WTSGetActiveConsoleSessionId is declared in WinBase.h and exported by kernel32.dll
 */

DWORD WINAPI WTSGetActiveConsoleSessionId(void)
{
	if (!g_WtsApi || !g_WtsApi->GetActiveConsoleSessionId)
		return 0xFFFFFFFF;

	return g_WtsApi->GetActiveConsoleSessionId();
}

#endif

BOOL WTSRegisterWtsApiFunctionTable(PWtsApiFunctionTable table)
{
	g_WtsApi = table;
	return TRUE;
}
