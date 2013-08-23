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

#ifndef WINPR_WTSAPI_RPC_CLIENT_STUBS_H
#define WINPR_WTSAPI_RPC_CLIENT_STUBS_H

/**
 * RPC Client Stubs
 */

BOOL RpcStartRemoteControlSession(void* context, LPWSTR pTargetServerName,
		ULONG TargetLogonId, BYTE HotkeyVk, USHORT HotkeyModifiers);
BOOL RpcStopRemoteControlSession(void* context, ULONG LogonId);

BOOL RpcConnectSession(void* context, ULONG LogonId, ULONG TargetLogonId, PWSTR pPassword, BOOL bWait);

BOOL RpcEnumerateServers(void* context, LPWSTR pDomainName, DWORD Reserved,
		DWORD Version, PWTS_SERVER_INFOW* ppServerInfo, DWORD* pCount);

HANDLE RpcOpenServer(void* context, LPWSTR pServerName);
HANDLE RpcOpenServerEx(void* context, LPWSTR pServerName);
VOID RpcCloseServer(void* context, HANDLE hServer);

BOOL RpcEnumerateSessions(void* context, HANDLE hServer, DWORD Reserved,
		DWORD Version, PWTS_SESSION_INFOW* ppSessionInfo, DWORD* pCount);
BOOL RpcEnumerateSessionsEx(void* context, HANDLE hServer, DWORD* pLevel,
		DWORD Filter, PWTS_SESSION_INFO_1W* ppSessionInfo, DWORD* pCount);

BOOL RpcEnumerateProcesses(void* context, HANDLE hServer, DWORD Reserved,
		DWORD Version, PWTS_PROCESS_INFOW* ppProcessInfo, DWORD* pCount);
BOOL RpcTerminateProcess(void* context, HANDLE hServer, DWORD ProcessId, DWORD ExitCode);

BOOL RpcQuerySessionInformation(void* context, HANDLE hServer, DWORD SessionId,
		WTS_INFO_CLASS WTSInfoClass, LPWSTR* ppBuffer, DWORD* pBytesReturned);

BOOL RpcQueryUserConfig(void* context, LPWSTR pServerName, LPWSTR pUserName,
		WTS_CONFIG_CLASS WTSConfigClass, LPWSTR* ppBuffer, DWORD* pBytesReturned);
BOOL RpcSetUserConfig(void* context, LPWSTR pServerName, LPWSTR pUserName,
		WTS_CONFIG_CLASS WTSConfigClass, LPWSTR pBuffer, DWORD DataLength);

BOOL RpcSendMessage(void* context, HANDLE hServer, DWORD SessionId, LPWSTR pTitle, DWORD TitleLength,
		LPWSTR pMessage, DWORD MessageLength, DWORD Style, DWORD Timeout, DWORD* pResponse, BOOL bWait);

BOOL RpcDisconnectSession(void* context, HANDLE hServer, DWORD SessionId, BOOL bWait);

BOOL RpcLogoffSession(void* context, HANDLE hServer, DWORD SessionId, BOOL bWait);

BOOL RpcShutdownSystem(void* context, HANDLE hServer, DWORD ShutdownFlag);

BOOL RpcRegisterSessionNotification(void* context, HWND hWnd, DWORD dwFlags);
BOOL RpcUnRegisterSessionNotification(void* context, HWND hWnd);

BOOL RpcRegisterSessionNotificationEx(void* context, HANDLE hServer, HWND hWnd, DWORD dwFlags);
BOOL RpcUnRegisterSessionNotificationEx(void* context, HANDLE hServer, HWND hWnd);

BOOL RpcEnableChildSessions(void* context, BOOL bEnable);
BOOL RpcIsChildSessionsEnabled(void* context, PBOOL pbEnabled);
BOOL RpcGetChildSessionId(void* context, PULONG pSessionId);

#endif /* WINPR_WTSAPI_RPC_CLIENT_STUBS_H */
