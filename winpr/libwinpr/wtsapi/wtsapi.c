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
#include <winpr/library.h>

/**
 * Remote Desktop Services API Functions:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/aa383464/
 */

#ifndef _WIN32

#include "wtsrpc_c.h"

#define CHECKANDLOADDLL() if(!gTriedLoadingSO) {loadWTSApiDLL("libfreerds-fdsapi.so");gTriedLoadingSO = TRUE;}

extern WTSFunctionTable WTSApiSTdFunctionTable;

WTSFunctionTable * gWTSApiFunctionTable;
BOOL gTriedLoadingSO = FALSE;

void loadWTSApiDLL(char * wtsApiDLL) {
	HMODULE hLib;
	pWTSApiEntry entry;

	hLib = LoadLibrary(wtsApiDLL);

	if (!hLib)
	{
		gWTSApiFunctionTable = &WTSApiSTdFunctionTable;
		return;
	}
	// get the exports
	entry = (pWTSApiEntry) GetProcAddress(hLib, "FDSApiEntry");

	if (entry != NULL) {
		// found entrypoint
		gWTSApiFunctionTable =  entry();
	} else {
		gWTSApiFunctionTable = &WTSApiSTdFunctionTable;
	}
	return;
};



BOOL WTSStartRemoteControlSessionW(LPWSTR pTargetServerName, ULONG TargetLogonId, BYTE HotkeyVk, USHORT HotkeyModifiers)
{
	CHECKANDLOADDLL();
	return gWTSApiFunctionTable->rpcStartRemoteControlSession(NULL, pTargetServerName, TargetLogonId, HotkeyVk, HotkeyModifiers);
}

BOOL WTSStartRemoteControlSessionA(LPSTR pTargetServerName, ULONG TargetLogonId, BYTE HotkeyVk, USHORT HotkeyModifiers)
{
	CHECKANDLOADDLL();
	return TRUE;
}

BOOL WTSStopRemoteControlSession(ULONG LogonId)
{
	CHECKANDLOADDLL();
	return gWTSApiFunctionTable->rpcStopRemoteControlSession(NULL, LogonId);
}

BOOL WTSConnectSessionW(ULONG LogonId, ULONG TargetLogonId, PWSTR pPassword, BOOL bWait)
{
	CHECKANDLOADDLL();
	return gWTSApiFunctionTable->rpcConnectSession(NULL, LogonId, TargetLogonId, pPassword, bWait);
}

BOOL WTSConnectSessionA(ULONG LogonId, ULONG TargetLogonId, PSTR pPassword, BOOL bWait)
{
	return TRUE;
}

BOOL WTSEnumerateServersW(LPWSTR pDomainName, DWORD Reserved, DWORD Version, PWTS_SERVER_INFOW* ppServerInfo, DWORD* pCount)
{
	CHECKANDLOADDLL();
	return gWTSApiFunctionTable->rpcEnumerateServers(NULL, pDomainName, Reserved, Version, ppServerInfo, pCount);
}

BOOL WTSEnumerateServersA(LPSTR pDomainName, DWORD Reserved, DWORD Version, PWTS_SERVER_INFOA* ppServerInfo, DWORD* pCount)
{
	return FALSE;
}

HANDLE WTSOpenServerW(LPWSTR pServerName)
{
	CHECKANDLOADDLL();
	return gWTSApiFunctionTable->rpcOpenServer(NULL, pServerName);
}

HANDLE WTSOpenServerA(LPSTR pServerName)
{
	return NULL;
}

HANDLE WTSOpenServerExW(LPWSTR pServerName)
{
	CHECKANDLOADDLL();
	return gWTSApiFunctionTable->rpcOpenServerEx(NULL, pServerName);
}

HANDLE WTSOpenServerExA(LPSTR pServerName)
{
	return NULL;
}

VOID WTSCloseServer(HANDLE hServer)
{
	CHECKANDLOADDLL();
	gWTSApiFunctionTable->rpcCloseServer(NULL, hServer);
}

BOOL WTSEnumerateSessionsW(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_SESSION_INFOW* ppSessionInfo, DWORD* pCount)
{
	CHECKANDLOADDLL();
	return gWTSApiFunctionTable->rpcEnumerateSessions(NULL, hServer, Reserved, Version, ppSessionInfo, pCount);
}

BOOL WTSEnumerateSessionsA(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_SESSION_INFOA* ppSessionInfo, DWORD* pCount)
{
	return FALSE;
}

BOOL WTSEnumerateSessionsExW(HANDLE hServer, DWORD* pLevel, DWORD Filter, PWTS_SESSION_INFO_1W* ppSessionInfo, DWORD* pCount)
{
	CHECKANDLOADDLL();
	return gWTSApiFunctionTable->rpcEnumerateSessionsEx(NULL, hServer, pLevel, Filter, ppSessionInfo, pCount);
}

BOOL WTSEnumerateSessionsExA(HANDLE hServer, DWORD* pLevel, DWORD Filter, PWTS_SESSION_INFO_1A* ppSessionInfo, DWORD* pCount)
{
	return FALSE;
}

BOOL WTSEnumerateProcessesW(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_PROCESS_INFOW* ppProcessInfo, DWORD* pCount)
{
	CHECKANDLOADDLL();
	return gWTSApiFunctionTable->rpcEnumerateProcesses(NULL, hServer, Reserved, Version, ppProcessInfo, pCount);
}

BOOL WTSEnumerateProcessesA(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_PROCESS_INFOA* ppProcessInfo, DWORD* pCount)
{
	return FALSE;
}

BOOL WTSTerminateProcess(HANDLE hServer, DWORD ProcessId, DWORD ExitCode)
{
	CHECKANDLOADDLL();
	return gWTSApiFunctionTable->rpcTerminateProcess(NULL, hServer, ProcessId, ExitCode);
}

BOOL WTSQuerySessionInformationW(HANDLE hServer, DWORD SessionId, WTS_INFO_CLASS WTSInfoClass, LPWSTR* ppBuffer, DWORD* pBytesReturned)
{
	CHECKANDLOADDLL();
	return gWTSApiFunctionTable->rpcQuerySessionInformation(NULL, hServer, SessionId, WTSInfoClass, ppBuffer, pBytesReturned);
}

BOOL WTSQuerySessionInformationA(HANDLE hServer, DWORD SessionId, WTS_INFO_CLASS WTSInfoClass, LPSTR* ppBuffer, DWORD* pBytesReturned)
{
	return FALSE;
}

BOOL WTSQueryUserConfigW(LPWSTR pServerName, LPWSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPWSTR* ppBuffer, DWORD* pBytesReturned)
{
	CHECKANDLOADDLL();
	return gWTSApiFunctionTable->rpcQueryUserConfig(NULL, pServerName, pUserName, WTSConfigClass, ppBuffer, pBytesReturned);
}

BOOL WTSQueryUserConfigA(LPSTR pServerName, LPSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPSTR* ppBuffer, DWORD* pBytesReturned)
{
	return FALSE;
}

BOOL WTSSetUserConfigW(LPWSTR pServerName, LPWSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPWSTR pBuffer, DWORD DataLength)
{
	CHECKANDLOADDLL();
	return gWTSApiFunctionTable->rpcSetUserConfig(NULL, pServerName, pUserName, WTSConfigClass, pBuffer, DataLength);
}

BOOL WTSSetUserConfigA(LPSTR pServerName, LPSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPSTR pBuffer, DWORD DataLength)
{
	return FALSE;
}

BOOL WTSSendMessageW(HANDLE hServer, DWORD SessionId, LPWSTR pTitle, DWORD TitleLength,
		LPWSTR pMessage, DWORD MessageLength, DWORD Style, DWORD Timeout, DWORD* pResponse, BOOL bWait)
{
	CHECKANDLOADDLL();
	return gWTSApiFunctionTable->rpcSendMessage(NULL, hServer, SessionId, pTitle, TitleLength,
			pMessage, MessageLength, Style, Timeout, pResponse, bWait);
}

BOOL WTSSendMessageA(HANDLE hServer, DWORD SessionId, LPSTR pTitle, DWORD TitleLength,
		LPSTR pMessage, DWORD MessageLength, DWORD Style, DWORD Timeout, DWORD* pResponse, BOOL bWait)
{
	return FALSE;
}

BOOL WTSDisconnectSession(HANDLE hServer, DWORD SessionId, BOOL bWait)
{
	CHECKANDLOADDLL();
	return gWTSApiFunctionTable->rpcDisconnectSession(NULL, hServer, SessionId, bWait);
}

BOOL WTSLogoffSession(HANDLE hServer, DWORD SessionId, BOOL bWait)
{
	CHECKANDLOADDLL();
	return gWTSApiFunctionTable->rpcLogoffSession(NULL, hServer, SessionId, bWait);
}

BOOL WTSShutdownSystem(HANDLE hServer, DWORD ShutdownFlag)
{
	CHECKANDLOADDLL();
	return gWTSApiFunctionTable->rpcShutdownSystem(NULL, hServer, ShutdownFlag);
}

BOOL WTSWaitSystemEvent(HANDLE hServer, DWORD EventMask, DWORD* pEventFlags)
{
	return FALSE;
}

HANDLE WTSVirtualChannelOpen(HANDLE hServer, DWORD SessionId, LPSTR pVirtualName)
{
	CHECKANDLOADDLL();
	if (hServer != WTS_CURRENT_SERVER_HANDLE)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return NULL;
	}
	return gWTSApiFunctionTable->rpcVirtualChannelOpen(SessionId,pVirtualName);

}

HANDLE WTSVirtualChannelOpenEx(DWORD SessionId, LPSTR pVirtualName, DWORD flags)
{

	HANDLE handle = NULL;

	if (!flags)
		handle = WTSVirtualChannelOpen(WTS_CURRENT_SERVER_HANDLE, SessionId, pVirtualName);

	return handle;
}

BOOL WTSVirtualChannelClose(HANDLE hChannelHandle)
{
	CHECKANDLOADDLL();

	if (!hChannelHandle || (hChannelHandle == INVALID_HANDLE_VALUE))
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}


	/* TODO: properly close handle */

	return TRUE;
}

BOOL WTSVirtualChannelRead(HANDLE hChannelHandle, ULONG TimeOut, PCHAR Buffer, ULONG BufferSize, PULONG pBytesRead)
{
	OVERLAPPED overlapped;

	if (!hChannelHandle || (hChannelHandle == INVALID_HANDLE_VALUE))
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	overlapped.hEvent = 0;
	overlapped.Offset = 0;
	overlapped.OffsetHigh = 0;

	if (ReadFile(hChannelHandle, Buffer, BufferSize, pBytesRead, &overlapped))
		return TRUE;

	if (GetLastError() != ERROR_IO_PENDING)
		return FALSE;

	if (!TimeOut)
	{
		CancelIo(hChannelHandle);
		*pBytesRead = 0;
		return TRUE;
	}

	if (WaitForSingleObject(hChannelHandle, TimeOut) == WAIT_TIMEOUT)
	{
		CancelIo(hChannelHandle);
		SetLastError(ERROR_IO_INCOMPLETE);
		return FALSE;
	}

	return GetOverlappedResult(hChannelHandle, &overlapped, pBytesRead, 0);
}

BOOL WTSVirtualChannelWrite(HANDLE hChannelHandle, PCHAR Buffer, ULONG Length, PULONG pBytesWritten)
{
	OVERLAPPED overlapped;

	if (!hChannelHandle || (hChannelHandle == INVALID_HANDLE_VALUE))
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	overlapped.hEvent = 0;
	overlapped.Offset = 0;
	overlapped.OffsetHigh = 0;

	if (WriteFile(hChannelHandle, Buffer, Length, pBytesWritten, &overlapped))
		return TRUE;

	if (GetLastError() == ERROR_IO_PENDING)
		return GetOverlappedResult(hChannelHandle, &overlapped, pBytesWritten, 1);

	return FALSE;
}

BOOL VirtualChannelIoctl(HANDLE hChannelHandle, ULONG IoControlCode)
{
	DWORD error;
	NTSTATUS ntstatus;
	IO_STATUS_BLOCK ioStatusBlock;

	if (!hChannelHandle || (hChannelHandle == INVALID_HANDLE_VALUE))
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	ntstatus = _NtDeviceIoControlFile(hChannelHandle, 0, 0, 0, &ioStatusBlock, IoControlCode, 0, 0, 0, 0);

	if (ntstatus == STATUS_PENDING)
	{
		ntstatus = _NtWaitForSingleObject(hChannelHandle, 0, 0);

		if (ntstatus >= 0)
			ntstatus = ioStatusBlock.status;
	}

	if (ntstatus == STATUS_BUFFER_OVERFLOW)
	{
		ntstatus = STATUS_BUFFER_TOO_SMALL;
		error = _RtlNtStatusToDosError(ntstatus);
		SetLastError(error);
		return FALSE;
	}

	if (ntstatus < 0)
	{
		error = _RtlNtStatusToDosError(ntstatus);
		SetLastError(error);
		return FALSE;
	}

	return TRUE;
}

#define FILE_DEVICE_TERMSRV		0x00000038

BOOL WTSVirtualChannelPurgeInput(HANDLE hChannelHandle)
{
	return VirtualChannelIoctl(hChannelHandle, (FILE_DEVICE_TERMSRV << 16) | 0x0107);
}

BOOL WTSVirtualChannelPurgeOutput(HANDLE hChannelHandle)
{
	return VirtualChannelIoctl(hChannelHandle, (FILE_DEVICE_TERMSRV << 16) | 0x010B);
}

BOOL WTSVirtualChannelQuery(HANDLE hChannelHandle, WTS_VIRTUAL_CLASS WtsVirtualClass, PVOID* ppBuffer, DWORD* pBytesReturned)
{
	if (!hChannelHandle || (hChannelHandle == INVALID_HANDLE_VALUE))
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	if (WtsVirtualClass == WTSVirtualFileHandle)
	{
		*ppBuffer = malloc(sizeof(void*));
		CopyMemory(*ppBuffer, &hChannelHandle, sizeof(void*));
		*pBytesReturned = sizeof(void*);
		return TRUE;
	}

	return FALSE;
}

VOID WTSFreeMemory(PVOID pMemory)
{
	free(pMemory);
}

BOOL WTSFreeMemoryExW(WTS_TYPE_CLASS WTSTypeClass, PVOID pMemory, ULONG NumberOfEntries)
{
	BOOL status = TRUE;

	switch (WTSTypeClass)
	{
		case WTSTypeProcessInfoLevel0:
			break;

		case WTSTypeProcessInfoLevel1:
			break;

		case WTSTypeSessionInfoLevel1:
			break;

		default:
			status = FALSE;
			break;
	}

	return status;
}

BOOL WTSFreeMemoryExA(WTS_TYPE_CLASS WTSTypeClass, PVOID pMemory, ULONG NumberOfEntries)
{
	BOOL status = TRUE;

	switch (WTSTypeClass)
	{
		case WTSTypeProcessInfoLevel0:
			break;

		case WTSTypeProcessInfoLevel1:
			break;

		case WTSTypeSessionInfoLevel1:
			break;

		default:
			status = FALSE;
			break;
	}

	return status;
}

BOOL WTSRegisterSessionNotification(HWND hWnd, DWORD dwFlags)
{
	CHECKANDLOADDLL();
	return gWTSApiFunctionTable->rpcRegisterSessionNotification(NULL, hWnd, dwFlags);
}

BOOL WTSUnRegisterSessionNotification(HWND hWnd)
{
	CHECKANDLOADDLL();
	return gWTSApiFunctionTable->rpcUnRegisterSessionNotification(NULL, hWnd);
}

BOOL WTSRegisterSessionNotificationEx(HANDLE hServer, HWND hWnd, DWORD dwFlags)
{
	CHECKANDLOADDLL();
	return gWTSApiFunctionTable->rpcRegisterSessionNotificationEx(NULL, hServer, hWnd, dwFlags);
}

BOOL WTSUnRegisterSessionNotificationEx(HANDLE hServer, HWND hWnd)
{
	CHECKANDLOADDLL();
	return gWTSApiFunctionTable->rpcUnRegisterSessionNotificationEx(NULL, hServer, hWnd);
}

BOOL WTSQueryUserToken(ULONG SessionId, PHANDLE phToken)
{
	return FALSE;
}

BOOL WTSEnumerateProcessesExW(HANDLE hServer, DWORD* pLevel, DWORD SessionId, LPWSTR* ppProcessInfo, DWORD* pCount)
{
	return FALSE;
}

BOOL WTSEnumerateProcessesExA(HANDLE hServer, DWORD* pLevel, DWORD SessionId, LPSTR* ppProcessInfo, DWORD* pCount)
{
	return FALSE;
}

BOOL WTSEnumerateListenersW(HANDLE hServer, PVOID pReserved, DWORD Reserved, PWTSLISTENERNAMEW pListeners, DWORD* pCount)
{
	return FALSE;
}

BOOL WTSEnumerateListenersA(HANDLE hServer, PVOID pReserved, DWORD Reserved, PWTSLISTENERNAMEA pListeners, DWORD* pCount)
{
	return FALSE;
}

BOOL WTSQueryListenerConfigW(HANDLE hServer, PVOID pReserved, DWORD Reserved, LPWSTR pListenerName, PWTSLISTENERCONFIGW pBuffer)
{
	return FALSE;
}

BOOL WTSQueryListenerConfigA(HANDLE hServer, PVOID pReserved, DWORD Reserved, LPSTR pListenerName, PWTSLISTENERCONFIGA pBuffer)
{
	return FALSE;
}

BOOL WTSCreateListenerW(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPWSTR pListenerName, PWTSLISTENERCONFIGW pBuffer, DWORD flag)
{
	return FALSE;
}

BOOL WTSCreateListenerA(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPSTR pListenerName, PWTSLISTENERCONFIGA pBuffer, DWORD flag)
{
	return FALSE;
}

BOOL WTSSetListenerSecurityW(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPWSTR pListenerName, SECURITY_INFORMATION SecurityInformation,
		PSECURITY_DESCRIPTOR pSecurityDescriptor)
{
	return FALSE;
}

BOOL WTSSetListenerSecurityA(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPSTR pListenerName, SECURITY_INFORMATION SecurityInformation,
		PSECURITY_DESCRIPTOR pSecurityDescriptor)
{
	return FALSE;
}

BOOL WTSGetListenerSecurityW(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPWSTR pListenerName, SECURITY_INFORMATION SecurityInformation,
		PSECURITY_DESCRIPTOR pSecurityDescriptor, DWORD nLength, LPDWORD lpnLengthNeeded)
{
	return FALSE;
}

BOOL WTSGetListenerSecurityA(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPSTR pListenerName, SECURITY_INFORMATION SecurityInformation,
		PSECURITY_DESCRIPTOR pSecurityDescriptor, DWORD nLength, LPDWORD lpnLengthNeeded)
{
	return FALSE;
}

BOOL WTSEnableChildSessions(BOOL bEnable)
{
	CHECKANDLOADDLL();
	return gWTSApiFunctionTable->rpcEnableChildSessions(NULL, bEnable);
}

BOOL WTSIsChildSessionsEnabled(PBOOL pbEnabled)
{
	CHECKANDLOADDLL();
	return gWTSApiFunctionTable->rpcIsChildSessionsEnabled(NULL, pbEnabled);
}

BOOL WTSGetChildSessionId(PULONG pSessionId)
{
	CHECKANDLOADDLL();
	return gWTSApiFunctionTable->rpcGetChildSessionId(NULL, pSessionId);
}

#endif
