/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * rdp2tcp Virtual Channel Extension
 *
 * Copyright 2017 Artur Zaprzala
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

#include <stdio.h>
#include <winpr/assert.h>

#include <winpr/file.h>
#include <winpr/pipe.h>
#include <winpr/thread.h>

#include <freerdp/freerdp.h>
#include <freerdp/svc.h>
#include <freerdp/channels/rdp2tcp.h>

#include <freerdp/log.h>
#define TAG CLIENT_TAG(RDP2TCP_DVC_CHANNEL_NAME)

typedef struct
{
	HANDLE hStdOutputRead;
	HANDLE hStdInputWrite;
	HANDLE hProcess;
	HANDLE copyThread;
	HANDLE writeComplete;
	DWORD openHandle;
	void* initHandle;
	CHANNEL_ENTRY_POINTS_FREERDP_EX channelEntryPoints;
	char buffer[16 * 1024];
	char* commandline;
} Plugin;

static int init_external_addin(Plugin* plugin)
{
	int rc = -1;
	SECURITY_ATTRIBUTES saAttr = { 0 };
	STARTUPINFOA siStartInfo = { 0 }; /* Using ANSI type to match CreateProcessA */
	PROCESS_INFORMATION procInfo = { 0 };

	WINPR_ASSERT(plugin);

	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;
	siStartInfo.cb = sizeof(STARTUPINFO);
	siStartInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
	siStartInfo.dwFlags = STARTF_USESTDHANDLES;

	// Create pipes
	if (!CreatePipe(&plugin->hStdOutputRead, &siStartInfo.hStdOutput, &saAttr, 0))
	{
		WLog_ERR(TAG, "stdout CreatePipe");
		goto fail;
	}

	if (!SetHandleInformation(plugin->hStdOutputRead, HANDLE_FLAG_INHERIT, 0))
	{
		WLog_ERR(TAG, "stdout SetHandleInformation");
		goto fail;
	}

	if (!CreatePipe(&siStartInfo.hStdInput, &plugin->hStdInputWrite, &saAttr, 0))
	{
		WLog_ERR(TAG, "stdin CreatePipe");
		goto fail;
	}

	if (!SetHandleInformation(plugin->hStdInputWrite, HANDLE_FLAG_INHERIT, 0))
	{
		WLog_ERR(TAG, "stdin SetHandleInformation");
		goto fail;
	}

	// Execute plugin
	const ADDIN_ARGV* args = (const ADDIN_ARGV*)plugin->channelEntryPoints.pExtendedData;
	if (!args || (args->argc < 2))
	{
		WLog_ERR(TAG, "missing command line options");
		goto fail;
	}

	plugin->commandline = _strdup(args->argv[1]);
	if (!CreateProcessA(NULL,
	                    plugin->commandline, // command line
	                    NULL,                // process security attributes
	                    NULL,                // primary thread security attributes
	                    TRUE,                // handles are inherited
	                    0,                   // creation flags
	                    NULL,                // use parent's environment
	                    NULL,                // use parent's current directory
	                    &siStartInfo,        // STARTUPINFO pointer
	                    &procInfo            // receives PROCESS_INFORMATION
	                    ))
	{
		WLog_ERR(TAG, "fork for addin");
		goto fail;
	}

	plugin->hProcess = procInfo.hProcess;

	rc = 0;
fail:
	(void)CloseHandle(procInfo.hThread);
	(void)CloseHandle(siStartInfo.hStdOutput);
	(void)CloseHandle(siStartInfo.hStdInput);
	return rc;
}

static DWORD WINAPI copyThread(void* data)
{
	DWORD status = WAIT_OBJECT_0;
	Plugin* plugin = (Plugin*)data;
	size_t const bufsize = 16ULL * 1024ULL;

	WINPR_ASSERT(plugin);

	while (status == WAIT_OBJECT_0)
	{
		(void)ResetEvent(plugin->writeComplete);

		DWORD dwRead = 0;
		char* buffer = calloc(bufsize, sizeof(char));

		if (!buffer)
		{
			(void)fprintf(stderr, "rdp2tcp copyThread: malloc failed\n");
			goto fail;
		}

		// if (!ReadFile(plugin->hStdOutputRead, plugin->buffer, sizeof plugin->buffer, &dwRead,
		// NULL))
		if (!ReadFile(plugin->hStdOutputRead, buffer, bufsize, &dwRead, NULL))
		{
			free(buffer);
			goto fail;
		}

		if (plugin->channelEntryPoints.pVirtualChannelWriteEx(
		        plugin->initHandle, plugin->openHandle, buffer, dwRead, buffer) != CHANNEL_RC_OK)
		{
			free(buffer);
			(void)fprintf(stderr, "rdp2tcp copyThread failed %i\n", (int)dwRead);
			goto fail;
		}

		HANDLE handles[] = { plugin->writeComplete,
			                 freerdp_abort_event(plugin->channelEntryPoints.context) };
		status = WaitForMultipleObjects(ARRAYSIZE(handles), handles, FALSE, INFINITE);
	}

fail:
	ExitThread(0);
	return 0;
}

static void closeChannel(Plugin* plugin)
{
	WINPR_ASSERT(plugin);
	WINPR_ASSERT(plugin->channelEntryPoints.pVirtualChannelCloseEx);
	plugin->channelEntryPoints.pVirtualChannelCloseEx(plugin->initHandle, plugin->openHandle);
}

static void dataReceived(Plugin* plugin, void* pData, UINT32 dataLength, UINT32 totalLength,
                         UINT32 dataFlags)
{
	DWORD dwWritten = 0;

	WINPR_ASSERT(plugin);

	if (dataFlags & CHANNEL_FLAG_SUSPEND)
		return;

	if (dataFlags & CHANNEL_FLAG_RESUME)
		return;

	if (dataFlags & CHANNEL_FLAG_FIRST)
	{
		if (!WriteFile(plugin->hStdInputWrite, &totalLength, sizeof(totalLength), &dwWritten, NULL))
			closeChannel(plugin);
	}

	if (!WriteFile(plugin->hStdInputWrite, pData, dataLength, &dwWritten, NULL))
		closeChannel(plugin);
}

static void VCAPITYPE VirtualChannelOpenEventEx(LPVOID lpUserParam,
                                                WINPR_ATTR_UNUSED DWORD openHandle, UINT event,
                                                LPVOID pData, UINT32 dataLength, UINT32 totalLength,
                                                UINT32 dataFlags)
{
	Plugin* plugin = (Plugin*)lpUserParam;

	WINPR_ASSERT(plugin);
	switch (event)
	{
		case CHANNEL_EVENT_DATA_RECEIVED:
			dataReceived(plugin, pData, dataLength, totalLength, dataFlags);
			break;

		case CHANNEL_EVENT_WRITE_CANCELLED:
			free(pData);
			break;
		case CHANNEL_EVENT_WRITE_COMPLETE:
			(void)SetEvent(plugin->writeComplete);
			free(pData);
			break;
		default:
			break;
	}
}

static void channel_terminated(Plugin* plugin)
{
	if (!plugin)
		return;

	if (plugin->copyThread)
		(void)CloseHandle(plugin->copyThread);
	if (plugin->writeComplete)
		(void)CloseHandle(plugin->writeComplete);

	(void)CloseHandle(plugin->hStdInputWrite);
	(void)CloseHandle(plugin->hStdOutputRead);
	TerminateProcess(plugin->hProcess, 0);
	(void)CloseHandle(plugin->hProcess);
	free(plugin->commandline);
	free(plugin);
}

static void channel_initialized(Plugin* plugin)
{
	WINPR_ASSERT(plugin);
	WINPR_ASSERT(!plugin->writeComplete);
	plugin->writeComplete = CreateEvent(NULL, TRUE, FALSE, NULL);

	WINPR_ASSERT(!plugin->copyThread);
	plugin->copyThread = CreateThread(NULL, 0, copyThread, plugin, 0, NULL);
}

static VOID VCAPITYPE VirtualChannelInitEventEx(LPVOID lpUserParam, LPVOID pInitHandle, UINT event,
                                                WINPR_ATTR_UNUSED LPVOID pData,
                                                WINPR_ATTR_UNUSED UINT dataLength)
{
	Plugin* plugin = (Plugin*)lpUserParam;

	WINPR_ASSERT(plugin);

	switch (event)
	{
		case CHANNEL_EVENT_INITIALIZED:
			channel_initialized(plugin);
			break;

		case CHANNEL_EVENT_CONNECTED:
			WINPR_ASSERT(plugin);
			WINPR_ASSERT(plugin->channelEntryPoints.pVirtualChannelOpenEx);
			if (plugin->channelEntryPoints.pVirtualChannelOpenEx(
			        pInitHandle, &plugin->openHandle, RDP2TCP_DVC_CHANNEL_NAME,
			        VirtualChannelOpenEventEx) != CHANNEL_RC_OK)
				return;

			break;

		case CHANNEL_EVENT_DISCONNECTED:
			closeChannel(plugin);
			break;

		case CHANNEL_EVENT_TERMINATED:
			channel_terminated(plugin);
			break;
		default:
			break;
	}
}

#define VirtualChannelEntryEx rdp2tcp_VirtualChannelEntryEx
FREERDP_ENTRY_POINT(BOOL VCAPITYPE VirtualChannelEntryEx(PCHANNEL_ENTRY_POINTS_EX pEntryPoints,
                                                         PVOID pInitHandle))
{
	CHANNEL_ENTRY_POINTS_FREERDP_EX* pEntryPointsEx =
	    (CHANNEL_ENTRY_POINTS_FREERDP_EX*)pEntryPoints;
	WINPR_ASSERT(pEntryPointsEx);
	WINPR_ASSERT(pEntryPointsEx->cbSize >= sizeof(CHANNEL_ENTRY_POINTS_FREERDP_EX) &&
	             pEntryPointsEx->MagicNumber == FREERDP_CHANNEL_MAGIC_NUMBER);

	Plugin* plugin = (Plugin*)calloc(1, sizeof(Plugin));

	if (!plugin)
		return FALSE;

	plugin->initHandle = pInitHandle;
	plugin->channelEntryPoints = *pEntryPointsEx;

	if (init_external_addin(plugin) < 0)
	{
		channel_terminated(plugin);
		return FALSE;
	}

	CHANNEL_DEF channelDef = { 0 };
	strncpy(channelDef.name, RDP2TCP_DVC_CHANNEL_NAME, sizeof(channelDef.name));
	channelDef.options =
	    CHANNEL_OPTION_INITIALIZED | CHANNEL_OPTION_ENCRYPT_RDP | CHANNEL_OPTION_COMPRESS_RDP;

	if (pEntryPointsEx->pVirtualChannelInitEx(plugin, NULL, pInitHandle, &channelDef, 1,
	                                          VIRTUAL_CHANNEL_VERSION_WIN2000,
	                                          VirtualChannelInitEventEx) != CHANNEL_RC_OK)
	{
		channel_terminated(plugin);
		return FALSE;
	}

	return TRUE;
}
