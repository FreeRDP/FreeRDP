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
#include <assert.h>

#include <winpr/file.h>
#include <winpr/pipe.h>
#include <winpr/thread.h>

#include <freerdp/svc.h>

#define RDP2TCP_CHAN_NAME "rdp2tcp"

static int const debug = 0;

typedef struct {
	HANDLE hStdOutputRead;
	HANDLE hStdInputWrite;
	HANDLE hProcess;
	HANDLE copyThread;
	HANDLE writeComplete;
	DWORD openHandle;
	void *initHandle;
	CHANNEL_ENTRY_POINTS_FREERDP_EX channelEntryPoints;
	char buffer[16*1024];
} Plugin;


static int init_external_addin(Plugin *plugin) {
	SECURITY_ATTRIBUTES saAttr = {
		.nLength = sizeof(SECURITY_ATTRIBUTES),
		.bInheritHandle = TRUE,
		.lpSecurityDescriptor = NULL
	};
	STARTUPINFO siStartInfo = {
		.cb = sizeof(STARTUPINFO),
		.hStdError = GetStdHandle(STD_ERROR_HANDLE),
		.dwFlags = STARTF_USESTDHANDLES
	};
	PROCESS_INFORMATION procInfo = {};

	// Create pipes
	if (!CreatePipe(&plugin->hStdOutputRead, &siStartInfo.hStdOutput, &saAttr, 0)) {
		perror("stdout CreatePipe");
		return -1;
	}
	if (!SetHandleInformation(plugin->hStdOutputRead, HANDLE_FLAG_INHERIT, 0)) {
		perror("stdout SetHandleInformation");
		return -1;
	}
	if (!CreatePipe(&siStartInfo.hStdInput, &plugin->hStdInputWrite, &saAttr, 0)) {
		perror("stdin CreatePipe");
		return -1;
	}
	if (!SetHandleInformation(plugin->hStdInputWrite, HANDLE_FLAG_INHERIT, 0)) {
		perror("stdin SetHandleInformation");
		return -1;
	}

	// Execute plugin
	if (!CreateProcess(NULL,
			plugin->channelEntryPoints.pExtendedData,			// command line
			NULL,			// process security attributes
			NULL,			// primary thread security attributes
			TRUE,			// handles are inherited
			0,				// creation flags
			NULL,			// use parent's environment
			NULL,			// use parent's current directory
			&siStartInfo,	// STARTUPINFO pointer
			&procInfo		// receives PROCESS_INFORMATION
	)) {
		perror("fork for addin");
		return -1;
	}

	plugin->hProcess = procInfo.hProcess;
	CloseHandle(procInfo.hThread);
	CloseHandle(siStartInfo.hStdOutput);
	CloseHandle(siStartInfo.hStdInput);
	return 0;
}

static void dumpData(char *data, unsigned length) {
	unsigned const limit = 98;
	unsigned l = length>limit ? limit/2 : length;
	for (unsigned i=0; i<l; ++i) {
		printf("%02hhx", data[i]);
	}
	if (length>limit) {
		printf("...");
		for (unsigned i=length-l; i<length; ++i)
			printf("%02hhx", data[i]);
	}
	puts("");
}

static DWORD copyThread(void *data) {
	Plugin *plugin = (Plugin *)data;
	size_t const bufsize = 16*1024;
	while (1) {
		DWORD dwRead;
		char *buffer = malloc(bufsize);
		if (!buffer) {
			fprintf(stderr, "rdp2tcp copyThread: malloc failed\n");
			return -1;
		}
		//if (!ReadFile(plugin->hStdOutputRead, plugin->buffer, sizeof plugin->buffer, &dwRead, NULL))
		if (!ReadFile(plugin->hStdOutputRead, buffer, bufsize, &dwRead, NULL))
			return -1;
		if (debug>1) {
			printf(">%8u ", (unsigned)dwRead);
			dumpData(buffer, dwRead);
		}
		if (plugin->channelEntryPoints.pVirtualChannelWriteEx(plugin->initHandle, plugin->openHandle, buffer, dwRead, NULL) != CHANNEL_RC_OK) {
			fprintf(stderr, "rdp2tcp copyThread failed %i\n", (int)dwRead);
			return -1;
		}
		WaitForSingleObject(plugin->writeComplete, INFINITE);
		ResetEvent(plugin->writeComplete);
	}
	return 0;
}

static void closeChannel(Plugin *plugin) {
	if (debug)
		puts("rdp2tcp closing channel");
	plugin->channelEntryPoints.pVirtualChannelCloseEx(plugin->initHandle, plugin->openHandle);
}

static void dataReceived(Plugin *plugin, void *pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags) {
	DWORD dwWritten;

	if (dataFlags & CHANNEL_FLAG_SUSPEND) {
		if (debug)
			puts("rdp2tcp Channel Suspend");
		return;
	}
	if (dataFlags & CHANNEL_FLAG_RESUME) {
		if (debug)
			puts("rdp2tcp Channel Resume");
		return;
	}
	if (debug>1) {
		printf("<%c%3u/%3u ", dataFlags & CHANNEL_FLAG_FIRST ? ' ': '+', totalLength, dataLength);
		dumpData(pData, dataLength);
	}
	if (dataFlags & CHANNEL_FLAG_FIRST) {
		if (!WriteFile(plugin->hStdInputWrite, &totalLength, sizeof(totalLength), &dwWritten, NULL))
			closeChannel(plugin);
	}
	if (!WriteFile(plugin->hStdInputWrite, pData, dataLength, &dwWritten, NULL))
		closeChannel(plugin);
}

static void VCAPITYPE VirtualChannelOpenEventEx(LPVOID lpUserParam, DWORD openHandle, UINT event, LPVOID pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags) {
	Plugin *plugin = (Plugin*)lpUserParam;
	switch (event) {
		case CHANNEL_EVENT_DATA_RECEIVED:;
			dataReceived(plugin, pData, dataLength, totalLength, dataFlags);
			break;
		case CHANNEL_EVENT_WRITE_COMPLETE:
			SetEvent(plugin->writeComplete);
			break;
	}
}

static VOID VCAPITYPE VirtualChannelInitEventEx(LPVOID lpUserParam, LPVOID pInitHandle, UINT event, LPVOID pData, UINT dataLength) {
	Plugin *plugin = (Plugin*)lpUserParam;
	switch (event) {
		case CHANNEL_EVENT_CONNECTED:
			if (debug)
				puts("rdp2tcp connected");
			plugin->writeComplete = CreateEvent(NULL, TRUE, FALSE, NULL);
			plugin->copyThread = CreateThread(NULL, 0, copyThread, plugin, 0, NULL);
			if (plugin->channelEntryPoints.pVirtualChannelOpenEx(pInitHandle, &plugin->openHandle, RDP2TCP_CHAN_NAME, VirtualChannelOpenEventEx) != CHANNEL_RC_OK)
				return;
			break;
		case CHANNEL_EVENT_DISCONNECTED:
			if (debug)
				puts("rdp2tcp disconnected");
			break;
		case CHANNEL_EVENT_TERMINATED:
			if (debug)
				puts("rdp2tcp terminated");
			if (plugin->copyThread) {
				TerminateThread(plugin->copyThread, 0);
				CloseHandle(plugin->writeComplete);
			}
			CloseHandle(plugin->hStdInputWrite);
			CloseHandle(plugin->hStdOutputRead);
			TerminateProcess(plugin->hProcess, 0);
			CloseHandle(plugin->hProcess);
			free(plugin);
			break;
	}
}

#if 1
#define VirtualChannelEntryEx rdp2tcp_VirtualChannelEntryEx
#else
#define VirtualChannelEntryEx FREERDP_API VirtualChannelEntryEx
#endif
BOOL VCAPITYPE VirtualChannelEntryEx(PCHANNEL_ENTRY_POINTS pEntryPoints, PVOID pInitHandle) {
	Plugin *plugin = (Plugin *)calloc(1, sizeof(Plugin));
	if (!plugin)
		return FALSE;

	CHANNEL_ENTRY_POINTS_FREERDP_EX *pEntryPointsEx = (CHANNEL_ENTRY_POINTS_FREERDP_EX *)pEntryPoints;
	assert(pEntryPointsEx->cbSize >= sizeof(CHANNEL_ENTRY_POINTS_FREERDP_EX) && pEntryPointsEx->MagicNumber == FREERDP_CHANNEL_MAGIC_NUMBER);
	plugin->initHandle = pInitHandle;
	plugin->channelEntryPoints = *pEntryPointsEx;
	
	if (init_external_addin(plugin) < 0)
		return FALSE;

	CHANNEL_DEF channelDef = {
		.name = RDP2TCP_CHAN_NAME,
		.options =
			CHANNEL_OPTION_INITIALIZED |
			CHANNEL_OPTION_ENCRYPT_RDP |
			CHANNEL_OPTION_COMPRESS_RDP
	};
	if (pEntryPointsEx->pVirtualChannelInitEx(plugin, NULL, pInitHandle, &channelDef, 1, VIRTUAL_CHANNEL_VERSION_WIN2000, VirtualChannelInitEventEx) != CHANNEL_RC_OK)
		return FALSE;
	return TRUE;
}

// vim:ts=4
