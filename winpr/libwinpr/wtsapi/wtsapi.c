/**
 * WinPR: Windows Portable Runtime
 * Windows Terminal Services API
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/ini.h>
#include <winpr/path.h>
#include <winpr/synch.h>
#include <winpr/library.h>
#include <winpr/environment.h>

#include <winpr/wtsapi.h>

#include "wtsapi.h"

#ifdef _WIN32
#include "wtsapi_win32.h"
#endif

#include "../log.h"
#define TAG WINPR_TAG("wtsapi")

/**
 * Remote Desktop Services API Functions:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/aa383464/
 */

void InitializeWtsApiStubs(void);

static BOOL g_Initialized = FALSE;
static HMODULE g_WtsApiModule = NULL;

static PWtsApiFunctionTable g_WtsApi = NULL;

static HMODULE g_WtsApi32Module = NULL;

static WtsApiFunctionTable WtsApi32_WtsApiFunctionTable =
{
	0, /* dwVersion */
	0, /* dwFlags */

	NULL, /* StopRemoteControlSession */
	NULL, /* StartRemoteControlSessionW */
	NULL, /* StartRemoteControlSessionA */
	NULL, /* ConnectSessionW */
	NULL, /* ConnectSessionA */
	NULL, /* EnumerateServersW */
	NULL, /* EnumerateServersA */
	NULL, /* OpenServerW */
	NULL, /* OpenServerA */
	NULL, /* OpenServerExW */
	NULL, /* OpenServerExA */
	NULL, /* CloseServer */
	NULL, /* EnumerateSessionsW */
	NULL, /* EnumerateSessionsA */
	NULL, /* EnumerateSessionsExW */
	NULL, /* EnumerateSessionsExA */
	NULL, /* EnumerateProcessesW */
	NULL, /* EnumerateProcessesA */
	NULL, /* TerminateProcess */
	NULL, /* QuerySessionInformationW */
	NULL, /* QuerySessionInformationA */
	NULL, /* QueryUserConfigW */
	NULL, /* QueryUserConfigA */
	NULL, /* SetUserConfigW */
	NULL, /* SetUserConfigA */
	NULL, /* SendMessageW */
	NULL, /* SendMessageA */
	NULL, /* DisconnectSession */
	NULL, /* LogoffSession */
	NULL, /* ShutdownSystem */
	NULL, /* WaitSystemEvent */
	NULL, /* VirtualChannelOpen */
	NULL, /* VirtualChannelOpenEx */
	NULL, /* VirtualChannelClose */
	NULL, /* VirtualChannelRead */
	NULL, /* VirtualChannelWrite */
	NULL, /* VirtualChannelPurgeInput */
	NULL, /* VirtualChannelPurgeOutput */
	NULL, /* VirtualChannelQuery */
	NULL, /* FreeMemory */
	NULL, /* RegisterSessionNotification */
	NULL, /* UnRegisterSessionNotification */
	NULL, /* RegisterSessionNotificationEx */
	NULL, /* UnRegisterSessionNotificationEx */
	NULL, /* QueryUserToken */
	NULL, /* FreeMemoryExW */
	NULL, /* FreeMemoryExA */
	NULL, /* EnumerateProcessesExW */
	NULL, /* EnumerateProcessesExA */
	NULL, /* EnumerateListenersW */
	NULL, /* EnumerateListenersA */
	NULL, /* QueryListenerConfigW */
	NULL, /* QueryListenerConfigA */
	NULL, /* CreateListenerW */
	NULL, /* CreateListenerA */
	NULL, /* SetListenerSecurityW */
	NULL, /* SetListenerSecurityA */
	NULL, /* GetListenerSecurityW */
	NULL, /* GetListenerSecurityA */
	NULL, /* EnableChildSessions */
	NULL, /* IsChildSessionsEnabled */
	NULL, /* GetChildSessionId */
	NULL  /* GetActiveConsoleSessionId */
};

#define WTSAPI32_LOAD_PROC(_name, _type) \
	WtsApi32_WtsApiFunctionTable.p ## _name = (## _type) GetProcAddress(g_WtsApi32Module, "WTS" #_name);

int WtsApi32_InitializeWtsApi(void)
{
	g_WtsApi32Module = LoadLibraryA("wtsapi32.dll");

	if (!g_WtsApi32Module)
		return -1;

#ifdef _WIN32
	WTSAPI32_LOAD_PROC(StopRemoteControlSession, WTS_STOP_REMOTE_CONTROL_SESSION_FN);
	WTSAPI32_LOAD_PROC(StartRemoteControlSessionW, WTS_START_REMOTE_CONTROL_SESSION_FN_W);
	WTSAPI32_LOAD_PROC(StartRemoteControlSessionA, WTS_START_REMOTE_CONTROL_SESSION_FN_A);
	WTSAPI32_LOAD_PROC(ConnectSessionW, WTS_CONNECT_SESSION_FN_W);
	WTSAPI32_LOAD_PROC(ConnectSessionA, WTS_CONNECT_SESSION_FN_A);
	WTSAPI32_LOAD_PROC(EnumerateServersW, WTS_ENUMERATE_SERVERS_FN_W);
	WTSAPI32_LOAD_PROC(EnumerateServersA, WTS_ENUMERATE_SERVERS_FN_A);
	WTSAPI32_LOAD_PROC(OpenServerW, WTS_OPEN_SERVER_FN_W);
	WTSAPI32_LOAD_PROC(OpenServerA, WTS_OPEN_SERVER_FN_A);
	WTSAPI32_LOAD_PROC(OpenServerExW, WTS_OPEN_SERVER_EX_FN_W);
	WTSAPI32_LOAD_PROC(OpenServerExA, WTS_OPEN_SERVER_EX_FN_A);
	WTSAPI32_LOAD_PROC(CloseServer, WTS_CLOSE_SERVER_FN);
	WTSAPI32_LOAD_PROC(EnumerateSessionsW, WTS_ENUMERATE_SESSIONS_FN_W);
	WTSAPI32_LOAD_PROC(EnumerateSessionsA, WTS_ENUMERATE_SESSIONS_FN_A);
	WTSAPI32_LOAD_PROC(EnumerateSessionsExW, WTS_ENUMERATE_SESSIONS_EX_FN_W);
	WTSAPI32_LOAD_PROC(EnumerateSessionsExA, WTS_ENUMERATE_SESSIONS_EX_FN_A);
	WTSAPI32_LOAD_PROC(EnumerateProcessesW, WTS_ENUMERATE_PROCESSES_FN_W);
	WTSAPI32_LOAD_PROC(EnumerateProcessesA, WTS_ENUMERATE_PROCESSES_FN_A);
	WTSAPI32_LOAD_PROC(TerminateProcess, WTS_TERMINATE_PROCESS_FN);
	WTSAPI32_LOAD_PROC(QuerySessionInformationW, WTS_QUERY_SESSION_INFORMATION_FN_W);
	WTSAPI32_LOAD_PROC(QuerySessionInformationA, WTS_QUERY_SESSION_INFORMATION_FN_A);
	WTSAPI32_LOAD_PROC(QueryUserConfigW, WTS_QUERY_USER_CONFIG_FN_W);
	WTSAPI32_LOAD_PROC(QueryUserConfigA, WTS_QUERY_USER_CONFIG_FN_A);
	WTSAPI32_LOAD_PROC(SetUserConfigW, WTS_SET_USER_CONFIG_FN_W);
	WTSAPI32_LOAD_PROC(SetUserConfigA, WTS_SET_USER_CONFIG_FN_A);
	WTSAPI32_LOAD_PROC(SendMessageW, WTS_SEND_MESSAGE_FN_W);
	WTSAPI32_LOAD_PROC(SendMessageA, WTS_SEND_MESSAGE_FN_A);
	WTSAPI32_LOAD_PROC(DisconnectSession, WTS_DISCONNECT_SESSION_FN);
	WTSAPI32_LOAD_PROC(LogoffSession, WTS_LOGOFF_SESSION_FN);
	WTSAPI32_LOAD_PROC(ShutdownSystem, WTS_SHUTDOWN_SYSTEM_FN);
	WTSAPI32_LOAD_PROC(WaitSystemEvent, WTS_WAIT_SYSTEM_EVENT_FN);
	WTSAPI32_LOAD_PROC(VirtualChannelOpen, WTS_VIRTUAL_CHANNEL_OPEN_FN);
	WTSAPI32_LOAD_PROC(VirtualChannelOpenEx, WTS_VIRTUAL_CHANNEL_OPEN_EX_FN);
	WTSAPI32_LOAD_PROC(VirtualChannelClose, WTS_VIRTUAL_CHANNEL_CLOSE_FN);
	WTSAPI32_LOAD_PROC(VirtualChannelRead, WTS_VIRTUAL_CHANNEL_READ_FN);
	WTSAPI32_LOAD_PROC(VirtualChannelWrite, WTS_VIRTUAL_CHANNEL_WRITE_FN);
	WTSAPI32_LOAD_PROC(VirtualChannelPurgeInput, WTS_VIRTUAL_CHANNEL_PURGE_INPUT_FN);
	WTSAPI32_LOAD_PROC(VirtualChannelPurgeOutput, WTS_VIRTUAL_CHANNEL_PURGE_OUTPUT_FN);
	WTSAPI32_LOAD_PROC(VirtualChannelQuery, WTS_VIRTUAL_CHANNEL_QUERY_FN);
	WTSAPI32_LOAD_PROC(FreeMemory, WTS_FREE_MEMORY_FN);
	WTSAPI32_LOAD_PROC(RegisterSessionNotification, WTS_REGISTER_SESSION_NOTIFICATION_FN);
	WTSAPI32_LOAD_PROC(UnRegisterSessionNotification, WTS_UNREGISTER_SESSION_NOTIFICATION_FN);
	WTSAPI32_LOAD_PROC(RegisterSessionNotificationEx, WTS_REGISTER_SESSION_NOTIFICATION_EX_FN);
	WTSAPI32_LOAD_PROC(UnRegisterSessionNotificationEx, WTS_UNREGISTER_SESSION_NOTIFICATION_EX_FN);
	WTSAPI32_LOAD_PROC(QueryUserToken, WTS_QUERY_USER_TOKEN_FN);
	WTSAPI32_LOAD_PROC(FreeMemoryExW, WTS_FREE_MEMORY_EX_FN_W);
	WTSAPI32_LOAD_PROC(FreeMemoryExA, WTS_FREE_MEMORY_EX_FN_A);
	WTSAPI32_LOAD_PROC(EnumerateProcessesExW, WTS_ENUMERATE_PROCESSES_EX_FN_W);
	WTSAPI32_LOAD_PROC(EnumerateProcessesExA, WTS_ENUMERATE_PROCESSES_EX_FN_A);
	WTSAPI32_LOAD_PROC(EnumerateListenersW, WTS_ENUMERATE_LISTENERS_FN_W);
	WTSAPI32_LOAD_PROC(EnumerateListenersA, WTS_ENUMERATE_LISTENERS_FN_A);
	WTSAPI32_LOAD_PROC(QueryListenerConfigW, WTS_QUERY_LISTENER_CONFIG_FN_W);
	WTSAPI32_LOAD_PROC(QueryListenerConfigA, WTS_QUERY_LISTENER_CONFIG_FN_A);
	WTSAPI32_LOAD_PROC(CreateListenerW, WTS_CREATE_LISTENER_FN_W);
	WTSAPI32_LOAD_PROC(CreateListenerA, WTS_CREATE_LISTENER_FN_A);
	WTSAPI32_LOAD_PROC(SetListenerSecurityW, WTS_SET_LISTENER_SECURITY_FN_W);
	WTSAPI32_LOAD_PROC(SetListenerSecurityA, WTS_SET_LISTENER_SECURITY_FN_A);
	WTSAPI32_LOAD_PROC(GetListenerSecurityW, WTS_GET_LISTENER_SECURITY_FN_W);
	WTSAPI32_LOAD_PROC(GetListenerSecurityA, WTS_GET_LISTENER_SECURITY_FN_A);
	WTSAPI32_LOAD_PROC(EnableChildSessions, WTS_ENABLE_CHILD_SESSIONS_FN);
	WTSAPI32_LOAD_PROC(IsChildSessionsEnabled, WTS_IS_CHILD_SESSIONS_ENABLED_FN);
	WTSAPI32_LOAD_PROC(GetChildSessionId, WTS_GET_CHILD_SESSION_ID_FN);
	WTSAPI32_LOAD_PROC(GetActiveConsoleSessionId, WTS_GET_ACTIVE_CONSOLE_SESSION_ID_FN);

	Win32_InitializeWinSta(&WtsApi32_WtsApiFunctionTable);
#endif

	g_WtsApi = &WtsApi32_WtsApiFunctionTable;

	return 1;
}

/* WtsApi Functions */

BOOL WINAPI WTSStartRemoteControlSessionW(LPWSTR pTargetServerName, ULONG TargetLogonId, BYTE HotkeyVk, USHORT HotkeyModifiers)
{
	WTSAPI_STUB_CALL_BOOL(StartRemoteControlSessionW, pTargetServerName, TargetLogonId, HotkeyVk, HotkeyModifiers);
}

BOOL WINAPI WTSStartRemoteControlSessionA(LPSTR pTargetServerName, ULONG TargetLogonId, BYTE HotkeyVk, USHORT HotkeyModifiers)
{
	WTSAPI_STUB_CALL_BOOL(StartRemoteControlSessionA, pTargetServerName, TargetLogonId, HotkeyVk, HotkeyModifiers);
}

BOOL WINAPI WTSStopRemoteControlSession(ULONG LogonId)
{
	WTSAPI_STUB_CALL_BOOL(StopRemoteControlSession, LogonId);
}

BOOL WINAPI WTSConnectSessionW(ULONG LogonId, ULONG TargetLogonId, PWSTR pPassword, BOOL bWait)
{
	WTSAPI_STUB_CALL_BOOL(ConnectSessionW, LogonId, TargetLogonId, pPassword, bWait);
}

BOOL WINAPI WTSConnectSessionA(ULONG LogonId, ULONG TargetLogonId, PSTR pPassword, BOOL bWait)
{
	WTSAPI_STUB_CALL_BOOL(ConnectSessionA, LogonId, TargetLogonId, pPassword, bWait);
}

BOOL WINAPI WTSEnumerateServersW(LPWSTR pDomainName, DWORD Reserved, DWORD Version, PWTS_SERVER_INFOW* ppServerInfo, DWORD* pCount)
{
	WTSAPI_STUB_CALL_BOOL(EnumerateServersW, pDomainName, Reserved, Version, ppServerInfo, pCount);
}

BOOL WINAPI WTSEnumerateServersA(LPSTR pDomainName, DWORD Reserved, DWORD Version, PWTS_SERVER_INFOA* ppServerInfo, DWORD* pCount)
{
	WTSAPI_STUB_CALL_BOOL(EnumerateServersA, pDomainName, Reserved, Version, ppServerInfo, pCount);
}

HANDLE WINAPI WTSOpenServerW(LPWSTR pServerName)
{
	WTSAPI_STUB_CALL_HANDLE(OpenServerW, pServerName);
}

HANDLE WINAPI WTSOpenServerA(LPSTR pServerName)
{
	WTSAPI_STUB_CALL_HANDLE(OpenServerA, pServerName);
}

HANDLE WINAPI WTSOpenServerExW(LPWSTR pServerName)
{
	WTSAPI_STUB_CALL_HANDLE(OpenServerExW, pServerName);
}

HANDLE WINAPI WTSOpenServerExA(LPSTR pServerName)
{
	WTSAPI_STUB_CALL_HANDLE(OpenServerExA, pServerName);
}

VOID WINAPI WTSCloseServer(HANDLE hServer)
{
	WTSAPI_STUB_CALL_VOID(CloseServer, hServer);
}

BOOL WINAPI WTSEnumerateSessionsW(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_SESSION_INFOW* ppSessionInfo, DWORD* pCount)
{
	WTSAPI_STUB_CALL_BOOL(EnumerateSessionsW, hServer, Reserved, Version, ppSessionInfo, pCount);
}

BOOL WINAPI WTSEnumerateSessionsA(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_SESSION_INFOA* ppSessionInfo, DWORD* pCount)
{
	WTSAPI_STUB_CALL_BOOL(EnumerateSessionsA, hServer, Reserved, Version, ppSessionInfo, pCount);
}

BOOL WINAPI WTSEnumerateSessionsExW(HANDLE hServer, DWORD* pLevel, DWORD Filter, PWTS_SESSION_INFO_1W* ppSessionInfo, DWORD* pCount)
{
	WTSAPI_STUB_CALL_BOOL(EnumerateSessionsExW, hServer, pLevel, Filter, ppSessionInfo, pCount);
}

BOOL WINAPI WTSEnumerateSessionsExA(HANDLE hServer, DWORD* pLevel, DWORD Filter, PWTS_SESSION_INFO_1A* ppSessionInfo, DWORD* pCount)
{
	WTSAPI_STUB_CALL_BOOL(EnumerateSessionsExA, hServer, pLevel, Filter, ppSessionInfo, pCount);
}

BOOL WINAPI WTSEnumerateProcessesW(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_PROCESS_INFOW* ppProcessInfo, DWORD* pCount)
{
	WTSAPI_STUB_CALL_BOOL(EnumerateProcessesW, hServer, Reserved, Version, ppProcessInfo, pCount);
}

BOOL WINAPI WTSEnumerateProcessesA(HANDLE hServer, DWORD Reserved, DWORD Version, PWTS_PROCESS_INFOA* ppProcessInfo, DWORD* pCount)
{
	WTSAPI_STUB_CALL_BOOL(EnumerateProcessesA, hServer, Reserved, Version, ppProcessInfo, pCount);
}

BOOL WINAPI WTSTerminateProcess(HANDLE hServer, DWORD ProcessId, DWORD ExitCode)
{
	WTSAPI_STUB_CALL_BOOL(TerminateProcess, hServer, ProcessId, ExitCode);
}

BOOL WINAPI WTSQuerySessionInformationW(HANDLE hServer, DWORD SessionId, WTS_INFO_CLASS WTSInfoClass, LPWSTR* ppBuffer, DWORD* pBytesReturned)
{
	WTSAPI_STUB_CALL_BOOL(QuerySessionInformationW, hServer, SessionId, WTSInfoClass, ppBuffer, pBytesReturned);
}

BOOL WINAPI WTSQuerySessionInformationA(HANDLE hServer, DWORD SessionId, WTS_INFO_CLASS WTSInfoClass, LPSTR* ppBuffer, DWORD* pBytesReturned)
{
	WTSAPI_STUB_CALL_BOOL(QuerySessionInformationA, hServer, SessionId, WTSInfoClass, ppBuffer, pBytesReturned);
}

BOOL WINAPI WTSQueryUserConfigW(LPWSTR pServerName, LPWSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPWSTR* ppBuffer, DWORD* pBytesReturned)
{
	WTSAPI_STUB_CALL_BOOL(QueryUserConfigW, pServerName, pUserName, WTSConfigClass, ppBuffer, pBytesReturned);
}

BOOL WINAPI WTSQueryUserConfigA(LPSTR pServerName, LPSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPSTR* ppBuffer, DWORD* pBytesReturned)
{
	WTSAPI_STUB_CALL_BOOL(QueryUserConfigA, pServerName, pUserName, WTSConfigClass, ppBuffer, pBytesReturned);
}

BOOL WINAPI WTSSetUserConfigW(LPWSTR pServerName, LPWSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPWSTR pBuffer, DWORD DataLength)
{
	WTSAPI_STUB_CALL_BOOL(SetUserConfigW, pServerName, pUserName, WTSConfigClass, pBuffer, DataLength);
}

BOOL WINAPI WTSSetUserConfigA(LPSTR pServerName, LPSTR pUserName, WTS_CONFIG_CLASS WTSConfigClass, LPSTR pBuffer, DWORD DataLength)
{
	WTSAPI_STUB_CALL_BOOL(SetUserConfigA, pServerName, pUserName, WTSConfigClass, pBuffer, DataLength);
}

BOOL WINAPI WTSSendMessageW(HANDLE hServer, DWORD SessionId, LPWSTR pTitle, DWORD TitleLength,
							LPWSTR pMessage, DWORD MessageLength, DWORD Style, DWORD Timeout, DWORD* pResponse, BOOL bWait)
{
	WTSAPI_STUB_CALL_BOOL(SendMessageW, hServer, SessionId, pTitle, TitleLength,
						  pMessage, MessageLength, Style, Timeout, pResponse, bWait);
}

BOOL WINAPI WTSSendMessageA(HANDLE hServer, DWORD SessionId, LPSTR pTitle, DWORD TitleLength,
							LPSTR pMessage, DWORD MessageLength, DWORD Style, DWORD Timeout, DWORD* pResponse, BOOL bWait)
{
	WTSAPI_STUB_CALL_BOOL(SendMessageA, hServer, SessionId, pTitle, TitleLength,
						  pMessage, MessageLength, Style, Timeout, pResponse, bWait);
}

BOOL WINAPI WTSDisconnectSession(HANDLE hServer, DWORD SessionId, BOOL bWait)
{
	WTSAPI_STUB_CALL_BOOL(DisconnectSession, hServer, SessionId, bWait);
}

BOOL WINAPI WTSLogoffSession(HANDLE hServer, DWORD SessionId, BOOL bWait)
{
	WTSAPI_STUB_CALL_BOOL(LogoffSession, hServer, SessionId, bWait);
}

BOOL WINAPI WTSShutdownSystem(HANDLE hServer, DWORD ShutdownFlag)
{
	WTSAPI_STUB_CALL_BOOL(ShutdownSystem, hServer, ShutdownFlag);
}

BOOL WINAPI WTSWaitSystemEvent(HANDLE hServer, DWORD EventMask, DWORD* pEventFlags)
{
	WTSAPI_STUB_CALL_BOOL(WaitSystemEvent, hServer, EventMask, pEventFlags);
}

HANDLE WINAPI WTSVirtualChannelOpen(HANDLE hServer, DWORD SessionId, LPSTR pVirtualName)
{
	WTSAPI_STUB_CALL_HANDLE(VirtualChannelOpen, hServer, SessionId, pVirtualName);
}

HANDLE WINAPI WTSVirtualChannelOpenEx(DWORD SessionId, LPSTR pVirtualName, DWORD flags)
{
	WTSAPI_STUB_CALL_HANDLE(VirtualChannelOpenEx, SessionId, pVirtualName, flags);
}

BOOL WINAPI WTSVirtualChannelClose(HANDLE hChannelHandle)
{
	WTSAPI_STUB_CALL_BOOL(VirtualChannelClose, hChannelHandle);
}

BOOL WINAPI WTSVirtualChannelRead(HANDLE hChannelHandle, ULONG TimeOut, PCHAR Buffer, ULONG BufferSize, PULONG pBytesRead)
{
	WTSAPI_STUB_CALL_BOOL(VirtualChannelRead, hChannelHandle, TimeOut, Buffer, BufferSize, pBytesRead);
}

BOOL WINAPI WTSVirtualChannelWrite(HANDLE hChannelHandle, PCHAR Buffer, ULONG Length, PULONG pBytesWritten)
{
	WTSAPI_STUB_CALL_BOOL(VirtualChannelWrite, hChannelHandle, Buffer, Length, pBytesWritten);
}

BOOL WINAPI WTSVirtualChannelPurgeInput(HANDLE hChannelHandle)
{
	WTSAPI_STUB_CALL_BOOL(VirtualChannelPurgeInput, hChannelHandle);
}

BOOL WINAPI WTSVirtualChannelPurgeOutput(HANDLE hChannelHandle)
{
	WTSAPI_STUB_CALL_BOOL(VirtualChannelPurgeOutput, hChannelHandle);
}

BOOL WINAPI WTSVirtualChannelQuery(HANDLE hChannelHandle, WTS_VIRTUAL_CLASS WtsVirtualClass, PVOID* ppBuffer, DWORD* pBytesReturned)
{
	WTSAPI_STUB_CALL_BOOL(VirtualChannelQuery, hChannelHandle, WtsVirtualClass, ppBuffer, pBytesReturned);
}

VOID WINAPI WTSFreeMemory(PVOID pMemory)
{
	WTSAPI_STUB_CALL_VOID(FreeMemory, pMemory);
}

BOOL WINAPI WTSFreeMemoryExW(WTS_TYPE_CLASS WTSTypeClass, PVOID pMemory, ULONG NumberOfEntries)
{
	WTSAPI_STUB_CALL_BOOL(FreeMemoryExW, WTSTypeClass, pMemory, NumberOfEntries);
}

BOOL WINAPI WTSFreeMemoryExA(WTS_TYPE_CLASS WTSTypeClass, PVOID pMemory, ULONG NumberOfEntries)
{
	WTSAPI_STUB_CALL_BOOL(FreeMemoryExA, WTSTypeClass, pMemory, NumberOfEntries);
}

BOOL WINAPI WTSRegisterSessionNotification(HWND hWnd, DWORD dwFlags)
{
	WTSAPI_STUB_CALL_BOOL(RegisterSessionNotification, hWnd, dwFlags);
}

BOOL WINAPI WTSUnRegisterSessionNotification(HWND hWnd)
{
	WTSAPI_STUB_CALL_BOOL(UnRegisterSessionNotification, hWnd);
}

BOOL WINAPI WTSRegisterSessionNotificationEx(HANDLE hServer, HWND hWnd, DWORD dwFlags)
{
	WTSAPI_STUB_CALL_BOOL(RegisterSessionNotificationEx, hServer, hWnd, dwFlags);
}

BOOL WINAPI WTSUnRegisterSessionNotificationEx(HANDLE hServer, HWND hWnd)
{
	WTSAPI_STUB_CALL_BOOL(UnRegisterSessionNotificationEx, hServer, hWnd);
}

BOOL WINAPI WTSQueryUserToken(ULONG SessionId, PHANDLE phToken)
{
	WTSAPI_STUB_CALL_BOOL(QueryUserToken, SessionId, phToken);
}

BOOL WINAPI WTSEnumerateProcessesExW(HANDLE hServer, DWORD* pLevel, DWORD SessionId, LPWSTR* ppProcessInfo, DWORD* pCount)
{
	WTSAPI_STUB_CALL_BOOL(EnumerateProcessesExW, hServer, pLevel, SessionId, ppProcessInfo, pCount);
}

BOOL WINAPI WTSEnumerateProcessesExA(HANDLE hServer, DWORD* pLevel, DWORD SessionId, LPSTR* ppProcessInfo, DWORD* pCount)
{
	WTSAPI_STUB_CALL_BOOL(EnumerateProcessesExA, hServer, pLevel, SessionId, ppProcessInfo, pCount);
}

BOOL WINAPI WTSEnumerateListenersW(HANDLE hServer, PVOID pReserved, DWORD Reserved, PWTSLISTENERNAMEW pListeners, DWORD* pCount)
{
	WTSAPI_STUB_CALL_BOOL(EnumerateListenersW, hServer, pReserved, Reserved, pListeners, pCount);
}

BOOL WINAPI WTSEnumerateListenersA(HANDLE hServer, PVOID pReserved, DWORD Reserved, PWTSLISTENERNAMEA pListeners, DWORD* pCount)
{
	WTSAPI_STUB_CALL_BOOL(EnumerateListenersA, hServer, pReserved, Reserved, pListeners, pCount);
}

BOOL WINAPI WTSQueryListenerConfigW(HANDLE hServer, PVOID pReserved, DWORD Reserved, LPWSTR pListenerName, PWTSLISTENERCONFIGW pBuffer)
{
	WTSAPI_STUB_CALL_BOOL(QueryListenerConfigW, hServer, pReserved, Reserved, pListenerName, pBuffer);
}

BOOL WINAPI WTSQueryListenerConfigA(HANDLE hServer, PVOID pReserved, DWORD Reserved, LPSTR pListenerName, PWTSLISTENERCONFIGA pBuffer)
{
	WTSAPI_STUB_CALL_BOOL(QueryListenerConfigA, hServer, pReserved, Reserved, pListenerName, pBuffer);
}

BOOL WINAPI WTSCreateListenerW(HANDLE hServer, PVOID pReserved, DWORD Reserved,
							   LPWSTR pListenerName, PWTSLISTENERCONFIGW pBuffer, DWORD flag)
{
	WTSAPI_STUB_CALL_BOOL(CreateListenerW, hServer, pReserved, Reserved, pListenerName, pBuffer, flag);
}

BOOL WINAPI WTSCreateListenerA(HANDLE hServer, PVOID pReserved, DWORD Reserved,
							   LPSTR pListenerName, PWTSLISTENERCONFIGA pBuffer, DWORD flag)
{
	WTSAPI_STUB_CALL_BOOL(CreateListenerA, hServer, pReserved, Reserved, pListenerName, pBuffer, flag);
}

BOOL WINAPI WTSSetListenerSecurityW(HANDLE hServer, PVOID pReserved, DWORD Reserved,
									LPWSTR pListenerName, SECURITY_INFORMATION SecurityInformation,
									PSECURITY_DESCRIPTOR pSecurityDescriptor)
{
	WTSAPI_STUB_CALL_BOOL(SetListenerSecurityW, hServer, pReserved, Reserved,
						  pListenerName, SecurityInformation, pSecurityDescriptor);
}

BOOL WINAPI WTSSetListenerSecurityA(HANDLE hServer, PVOID pReserved, DWORD Reserved,
									LPSTR pListenerName, SECURITY_INFORMATION SecurityInformation,
									PSECURITY_DESCRIPTOR pSecurityDescriptor)
{
	WTSAPI_STUB_CALL_BOOL(SetListenerSecurityA, hServer, pReserved, Reserved,
						  pListenerName, SecurityInformation, pSecurityDescriptor);
}

BOOL WINAPI WTSGetListenerSecurityW(HANDLE hServer, PVOID pReserved, DWORD Reserved,
									LPWSTR pListenerName, SECURITY_INFORMATION SecurityInformation,
									PSECURITY_DESCRIPTOR pSecurityDescriptor, DWORD nLength, LPDWORD lpnLengthNeeded)
{
	WTSAPI_STUB_CALL_BOOL(GetListenerSecurityW, hServer, pReserved, Reserved, pListenerName,
						  SecurityInformation, pSecurityDescriptor, nLength, lpnLengthNeeded);
}

BOOL WINAPI WTSGetListenerSecurityA(HANDLE hServer, PVOID pReserved, DWORD Reserved,
									LPSTR pListenerName, SECURITY_INFORMATION SecurityInformation,
									PSECURITY_DESCRIPTOR pSecurityDescriptor, DWORD nLength, LPDWORD lpnLengthNeeded)
{
	WTSAPI_STUB_CALL_BOOL(GetListenerSecurityA, hServer, pReserved, Reserved, pListenerName,
						  SecurityInformation, pSecurityDescriptor, nLength, lpnLengthNeeded);
}

BOOL CDECL WTSEnableChildSessions(BOOL bEnable)
{
	WTSAPI_STUB_CALL_BOOL(EnableChildSessions, bEnable);
}

BOOL CDECL WTSIsChildSessionsEnabled(PBOOL pbEnabled)
{
	WTSAPI_STUB_CALL_BOOL(IsChildSessionsEnabled, pbEnabled);
}

BOOL CDECL WTSGetChildSessionId(PULONG pSessionId)
{
	WTSAPI_STUB_CALL_BOOL(GetChildSessionId, pSessionId);
}

BOOL CDECL WTSLogonUser(HANDLE hServer, LPCSTR username, LPCSTR password, LPCSTR domain)
{
	WTSAPI_STUB_CALL_BOOL(LogonUser, hServer, username, password, domain);
}

BOOL CDECL WTSLogoffUser(HANDLE hServer)
{
	WTSAPI_STUB_CALL_BOOL(LogoffUser, hServer);
}

#ifndef _WIN32

/**
 * WTSGetActiveConsoleSessionId is declared in WinBase.h and exported by kernel32.dll
 */

DWORD WINAPI WTSGetActiveConsoleSessionId(void)
{
	if (!g_Initialized)
		InitializeWtsApiStubs();

	if (!g_WtsApi || !g_WtsApi->pGetActiveConsoleSessionId)
		return 0xFFFFFFFF;

	return g_WtsApi->pGetActiveConsoleSessionId();
}

#endif

const CHAR* WTSErrorToString(UINT error)
{
	switch(error)
	{
	case CHANNEL_RC_OK:
		return "CHANNEL_RC_OK";

	case CHANNEL_RC_ALREADY_INITIALIZED:
		return "CHANNEL_RC_ALREADY_INITIALIZED";

	case CHANNEL_RC_NOT_INITIALIZED:
		return "CHANNEL_RC_NOT_INITIALIZED";

	case CHANNEL_RC_ALREADY_CONNECTED:
		return "CHANNEL_RC_ALREADY_CONNECTED";

	case CHANNEL_RC_NOT_CONNECTED:
		return "CHANNEL_RC_NOT_CONNECTED";

	case CHANNEL_RC_TOO_MANY_CHANNELS:
		return "CHANNEL_RC_TOO_MANY_CHANNELS";

	case CHANNEL_RC_BAD_CHANNEL:
		return "CHANNEL_RC_BAD_CHANNEL";

	case CHANNEL_RC_BAD_CHANNEL_HANDLE:
		return "CHANNEL_RC_BAD_CHANNEL_HANDLE";

	case CHANNEL_RC_NO_BUFFER:
		return "CHANNEL_RC_NO_BUFFER";

	case CHANNEL_RC_BAD_INIT_HANDLE:
		return "CHANNEL_RC_BAD_INIT_HANDLE";

	case CHANNEL_RC_NOT_OPEN:
		return "CHANNEL_RC_NOT_OPEN";

	case CHANNEL_RC_BAD_PROC:
		return "CHANNEL_RC_BAD_PROC";

	case CHANNEL_RC_NO_MEMORY:
		return "CHANNEL_RC_NO_MEMORY";

	case CHANNEL_RC_UNKNOWN_CHANNEL_NAME:
		return "CHANNEL_RC_UNKNOWN_CHANNEL_NAME";

	case CHANNEL_RC_ALREADY_OPEN:
		return "CHANNEL_RC_ALREADY_OPEN";

	case CHANNEL_RC_NOT_IN_VIRTUALCHANNELENTRY:
		return "CHANNEL_RC_NOT_IN_VIRTUALCHANNELENTRY";

	case CHANNEL_RC_NULL_DATA:
		return "CHANNEL_RC_NULL_DATA";

	case CHANNEL_RC_ZERO_LENGTH:
		return "CHANNEL_RC_ZERO_LENGTH";

	case CHANNEL_RC_INVALID_INSTANCE:
		return "CHANNEL_RC_INVALID_INSTANCE";

	case CHANNEL_RC_UNSUPPORTED_VERSION:
		return "CHANNEL_RC_UNSUPPORTED_VERSION";

	case CHANNEL_RC_INITIALIZATION_ERROR:
		return "CHANNEL_RC_INITIALIZATION_ERROR";

	default:
		return "UNKNOWN";
	}
}

BOOL WTSRegisterWtsApiFunctionTable(PWtsApiFunctionTable table)
{
	g_WtsApi = table;
	g_Initialized = TRUE;
	return TRUE;
}

static BOOL LoadAndInitialize(char* library)
{
	INIT_WTSAPI_FN pInitWtsApi;
	g_WtsApiModule = LoadLibraryA(library);

	if (!g_WtsApiModule)
		return FALSE;

	pInitWtsApi = (INIT_WTSAPI_FN) GetProcAddress(g_WtsApiModule, "InitWtsApi");

	if (!pInitWtsApi)
	{
		return FALSE;
	}

	g_WtsApi = pInitWtsApi();
	return TRUE;
}

void InitializeWtsApiStubs_Env()
{
	DWORD nSize;
	char* env = NULL;

	if (g_WtsApi)
		return;

	nSize = GetEnvironmentVariableA("WTSAPI_LIBRARY", NULL, 0);

	if (!nSize)
	{
		return;
	}

	env = (LPSTR) malloc(nSize);
	nSize = GetEnvironmentVariableA("WTSAPI_LIBRARY", env, nSize);

	if (env)
		LoadAndInitialize(env);
}

#define FREERDS_LIBRARY_NAME "libfreerds-fdsapi.so"

void InitializeWtsApiStubs_FreeRDS()
{
	wIniFile* ini;
	const char* prefix;
	const char* libdir;

	if (g_WtsApi)
		return;

	ini = IniFile_New();

	if (IniFile_ReadFile(ini, "/var/run/freerds.instance") < 0)
	{
		IniFile_Free(ini);
		WLog_ERR(TAG, "failed to parse freerds.instance");
		LoadAndInitialize(FREERDS_LIBRARY_NAME);
		return;
	}

	prefix = IniFile_GetKeyValueString(ini, "FreeRDS", "prefix");
	libdir = IniFile_GetKeyValueString(ini, "FreeRDS", "libdir");
	WLog_INFO(TAG, "FreeRDS (prefix / libdir): %s / %s", prefix, libdir);

	if (prefix && libdir)
	{
		char* prefix_libdir;
		char* wtsapi_library;
		prefix_libdir = GetCombinedPath(prefix, libdir);
		wtsapi_library = GetCombinedPath(prefix_libdir, FREERDS_LIBRARY_NAME);

		if (wtsapi_library)
		{
			LoadAndInitialize(wtsapi_library);
		}

		free(prefix_libdir);
		free(wtsapi_library);
	}

	IniFile_Free(ini);
}

void InitializeWtsApiStubs(void)
{
	if (g_Initialized)
		return;

	g_Initialized = TRUE;
	InitializeWtsApiStubs_Env();

#ifdef _WIN32
	WtsApi32_InitializeWtsApi();
#endif

	if (!g_WtsApi)
		InitializeWtsApiStubs_FreeRDS();

	return;
}
