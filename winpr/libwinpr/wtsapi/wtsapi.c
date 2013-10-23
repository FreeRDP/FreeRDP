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

#ifndef _WIN32

#include "wtsrpc_c.h"

BOOL WTSStartRemoteControlSessionW(LPWSTR pTargetServerName, ULONG TargetLogonId, BYTE HotkeyVk, USHORT HotkeyModifiers)
{
	return RpcStartRemoteControlSession(NULL, pTargetServerName, TargetLogonId, HotkeyVk, HotkeyModifiers);
}

BOOL WTSStartRemoteControlSessionA(LPSTR pTargetServerName, ULONG TargetLogonId, BYTE HotkeyVk, USHORT HotkeyModifiers)
{
	return TRUE;
}

BOOL WTSStopRemoteControlSession(ULONG LogonId)
{
	return RpcStopRemoteControlSession(NULL, LogonId);
}

BOOL WTSConnectSessionW(ULONG LogonId, ULONG TargetLogonId, PWSTR pPassword, BOOL bWait)
{
	return RpcConnectSession(NULL, LogonId, TargetLogonId, pPassword, bWait);
}

BOOL WTSConnectSessionA(ULONG LogonId, ULONG TargetLogonId, PSTR pPassword, BOOL bWait)
{
	return TRUE;
}

BOOL WTSEnumerateServersW(LPWSTR pDomainName, DWORD Reserved, DWORD Version, PWTS_SERVER_INFOW* ppServerInfo, DWORD* pCount)
{
	return RpcEnumerateServers(NULL, pDomainName, Reserved, Version, ppServerInfo, pCount);
}

BOOL WTSEnumerateServersA(LPSTR pDomainName, DWORD Reserved, DWORD Version, PWTS_SERVER_INFOA* ppServerInfo, DWORD* pCount)
{
	return TRUE;
}

HANDLE WTSOpenServerW(LPWSTR pServerName)
{
	return RpcOpenServer(NULL, pServerName);
}

HANDLE WTSOpenServerA(LPSTR pServerName)
{
	return NULL;
}

HANDLE WTSOpenServerExW(LPWSTR pServerName)
{
	return RpcOpenServerEx(NULL, pServerName);
}

HANDLE WTSOpenServerExA(LPSTR pServerName)
{
	return NULL;
}

VOID WTSCloseServer(HANDLE hServer)
{
	RpcCloseServer(NULL, hServer);
}

BOOL WTSEnumerateSessionsW(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_SESSION_INFOW* ppSessionInfo, DWORD* pCount)
{
	return RpcEnumerateSessions(NULL, hServer, Reserved, Version, ppSessionInfo, pCount);
}

BOOL WTSEnumerateSessionsA(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_SESSION_INFOA* ppSessionInfo, DWORD* pCount)
{
	return TRUE;
}

BOOL WTSEnumerateSessionsExW(HANDLE hServer, DWORD* pLevel, DWORD Filter, PWTS_SESSION_INFO_1W* ppSessionInfo, DWORD* pCount)
{
	return RpcEnumerateSessionsEx(NULL, hServer, pLevel, Filter, ppSessionInfo, pCount);
}

BOOL WTSEnumerateSessionsExA(HANDLE hServer, DWORD* pLevel, DWORD Filter, PWTS_SESSION_INFO_1A* ppSessionInfo, DWORD* pCount)
{
	return TRUE;
}

BOOL WTSEnumerateProcessesW(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_PROCESS_INFOW* ppProcessInfo, DWORD* pCount)
{
	return RpcEnumerateProcesses(NULL, hServer, Reserved, Version, ppProcessInfo, pCount);
}

BOOL WTSEnumerateProcessesA(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_PROCESS_INFOA* ppProcessInfo, DWORD* pCount)
{
	return TRUE;
}

BOOL WTSTerminateProcess(HANDLE hServer, DWORD ProcessId, DWORD ExitCode)
{
	return RpcTerminateProcess(NULL, hServer, ProcessId, ExitCode);
}

BOOL WTSQuerySessionInformationW(HANDLE hServer, DWORD SessionId, WTS_INFO_CLASS WTSInfoClass, LPWSTR* ppBuffer, DWORD* pBytesReturned)
{
	return RpcQuerySessionInformation(NULL, hServer, SessionId, WTSInfoClass, ppBuffer, pBytesReturned);
}

BOOL WTSQuerySessionInformationA(HANDLE hServer, DWORD SessionId, WTS_INFO_CLASS WTSInfoClass, LPSTR* ppBuffer, DWORD* pBytesReturned)
{
	return TRUE;
}

BOOL WTSQueryUserConfigW(LPWSTR pServerName, LPWSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPWSTR* ppBuffer, DWORD* pBytesReturned)
{
	return RpcQueryUserConfig(NULL, pServerName, pUserName, WTSConfigClass, ppBuffer, pBytesReturned);
}

BOOL WTSQueryUserConfigA(LPSTR pServerName, LPSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPSTR* ppBuffer, DWORD* pBytesReturned)
{
	return TRUE;
}

BOOL WTSSetUserConfigW(LPWSTR pServerName, LPWSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPWSTR pBuffer, DWORD DataLength)
{
	return RpcSetUserConfig(NULL, pServerName, pUserName, WTSConfigClass, pBuffer, DataLength);
}

BOOL WTSSetUserConfigA(LPSTR pServerName, LPSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPSTR pBuffer, DWORD DataLength)
{
	return TRUE;
}

BOOL WTSSendMessageW(HANDLE hServer, DWORD SessionId, LPWSTR pTitle, DWORD TitleLength,
		LPWSTR pMessage, DWORD MessageLength, DWORD Style, DWORD Timeout, DWORD* pResponse, BOOL bWait)
{
	return RpcSendMessage(NULL, hServer, SessionId, pTitle, TitleLength,
			pMessage, MessageLength, Style, Timeout, pResponse, bWait);
}

BOOL WTSSendMessageA(HANDLE hServer, DWORD SessionId, LPSTR pTitle, DWORD TitleLength,
		LPSTR pMessage, DWORD MessageLength, DWORD Style, DWORD Timeout, DWORD* pResponse, BOOL bWait)
{
	return TRUE;
}

BOOL WTSDisconnectSession(HANDLE hServer, DWORD SessionId, BOOL bWait)
{
	return RpcDisconnectSession(NULL, hServer, SessionId, bWait);
}

BOOL WTSLogoffSession(HANDLE hServer, DWORD SessionId, BOOL bWait)
{
	return RpcLogoffSession(NULL, hServer, SessionId, bWait);
}

BOOL WTSShutdownSystem(HANDLE hServer, DWORD ShutdownFlag)
{
	return RpcShutdownSystem(NULL, hServer, ShutdownFlag);
}

BOOL WTSWaitSystemEvent(HANDLE hServer, DWORD EventMask, DWORD* pEventFlags)
{
	return TRUE;
}

HANDLE WTSVirtualChannelOpen(HANDLE hServer, DWORD SessionId, LPSTR pVirtualName)
{
	HANDLE handle = NULL;

	if (hServer != WTS_CURRENT_SERVER_HANDLE)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return NULL;
	}

	return handle;
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
	return RpcRegisterSessionNotification(NULL, hWnd, dwFlags);
}

BOOL WTSUnRegisterSessionNotification(HWND hWnd)
{
	return RpcUnRegisterSessionNotification(NULL, hWnd);
}

BOOL WTSRegisterSessionNotificationEx(HANDLE hServer, HWND hWnd, DWORD dwFlags)
{
	return RpcRegisterSessionNotificationEx(NULL, hServer, hWnd, dwFlags);
}

BOOL WTSUnRegisterSessionNotificationEx(HANDLE hServer, HWND hWnd)
{
	return RpcUnRegisterSessionNotificationEx(NULL, hServer, hWnd);
}

BOOL WTSQueryUserToken(ULONG SessionId, PHANDLE phToken)
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

BOOL WTSCreateListenerW(HANDLE hServer, PVOID pReserved, DWORD Reserved,
		LPWSTR pListenerName, PWTSLISTENERCONFIGW pBuffer, DWORD flag)
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
	return RpcEnableChildSessions(NULL, bEnable);
}

BOOL WTSIsChildSessionsEnabled(PBOOL pbEnabled)
{
	return RpcIsChildSessionsEnabled(NULL, pbEnabled);
}

BOOL WTSGetChildSessionId(PULONG pSessionId)
{
	return RpcGetChildSessionId(NULL, pSessionId);
}

#endif
