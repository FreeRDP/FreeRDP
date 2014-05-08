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
#include <winpr/ini.h>
#include <winpr/path.h>
#include <winpr/synch.h>
#include <winpr/library.h>
#include <winpr/environment.h>

#include <winpr/wtsapi.h>

#include "wtsapi.h"

/**
 * Remote Desktop Services API Functions:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/aa383464/
 */

void InitializeWtsApiStubs(void);

static BOOL g_Initialized = FALSE;
static HMODULE g_WtsApiModule = NULL;

static PWtsApiFunctionTable g_WtsApi = NULL;

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

BOOL WTSRegisterWtsApiFunctionTable(PWtsApiFunctionTable table)
{
	g_WtsApi = table;
	return TRUE;
}

static BOOL LoadAndInitialize(char *library)
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
	char* prefix;
	char* libdir;
	wIniFile* ini;
	
	if (g_WtsApi)
		return;
	
	ini = IniFile_New();

	if (IniFile_Parse(ini, "/var/run/freerds.instance") < 0)
	{
		IniFile_Free(ini);
		fprintf(stderr, "failed to parse freerds.instance\n");
		LoadAndInitialize(FREERDS_LIBRARY_NAME);
		return;
	}
	
	prefix = IniFile_GetKeyValueString(ini, "FreeRDS", "prefix");
	libdir = IniFile_GetKeyValueString(ini, "FreeRDS", "libdir");
	
	fprintf(stderr, "FreeRDS (prefix / libdir): %s / %s\n", prefix, libdir);
	
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
	
	if (!g_WtsApi)
		InitializeWtsApiStubs_FreeRDS();

	return;
}
