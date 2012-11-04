/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Client Channels
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2010-2011 Vic Lee
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include "tables.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <freerdp/freerdp.h>
#include <freerdp/constants.h>
#include <freerdp/client/channels.h>
#include <freerdp/channels/channels.h>
#include <freerdp/svc.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/wait_obj.h>
#include <freerdp/utils/load_plugin.h>
#include <freerdp/utils/file.h>
#include <freerdp/utils/event.h>
#include <freerdp/utils/debug.h>

#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/file.h>
#include <winpr/synch.h>
#include <winpr/interlocked.h>

#ifdef WITH_DEBUG_CHANNELS
#define DEBUG_CHANNELS(fmt, ...) DEBUG_CLASS(CHANNELS, fmt, ## __VA_ARGS__)
#else
#define DEBUG_CHANNELS(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#define CHANNEL_MAX_COUNT 30

/**
 * MS compatible plugin interface
 * reference:
 * http://msdn.microsoft.com/en-us/library/aa383580.aspx
 *
 * Notes on threads:
 * Many virtual channel plugins are built using threads.
 * Non main threads may call MyVirtualChannelOpen,
 * MyVirtualChannelClose, or MyVirtualChannelWrite.
 * Since the plugin's VirtualChannelEntry function is called
 * from the main thread, MyVirtualChannelInit has to be called
 * from the main thread.
 */

extern const STATIC_ENTRY_TABLE CLIENT_STATIC_ENTRY_TABLES[];

void* freerdp_channels_find_static_entry_in_table(const STATIC_ENTRY_TABLE* table, const char* identifier)
{
	int index = 0;
	STATIC_ENTRY* pEntry;

	pEntry = (STATIC_ENTRY*) &table->table[index++];

	while (pEntry->entry != NULL)
	{
		if (strcmp(pEntry->name, identifier) == 0)
		{
			return (void*) pEntry->entry;
		}

		pEntry = (STATIC_ENTRY*) &table->table[index++];
	}

	return NULL;
}

void* freerdp_channels_client_find_static_entry(const char* name, const char* identifier)
{
	int index = 0;
	STATIC_ENTRY_TABLE* pEntry;

	pEntry = (STATIC_ENTRY_TABLE*) &CLIENT_STATIC_ENTRY_TABLES[index++];

	while (pEntry->table != NULL)
	{
		if (strcmp(pEntry->name, name) == 0)
		{
			return freerdp_channels_find_static_entry_in_table(pEntry, identifier);
		}

		pEntry = (STATIC_ENTRY_TABLE*) &CLIENT_STATIC_ENTRY_TABLES[index++];
	}

	return NULL;
}

void* freerdp_channels_client_find_dynamic_entry(const char* name, const char* identifier)
{
	char* path;
	void* entry;
	char* module;

	module = freerdp_append_shared_library_suffix((char*) identifier);
	path = freerdp_construct_path(FREERDP_PLUGIN_PATH, module);

	entry = freerdp_load_library_symbol(path, module);

	free(module);
	free(path);

	return entry;
}

void* freerdp_channels_client_find_entry(const char* name, const char* identifier)
{
	void* pChannelEntry = NULL;

	pChannelEntry = freerdp_channels_client_find_static_entry(name, identifier);

	if (!pChannelEntry)
	{
		pChannelEntry = freerdp_channels_client_find_dynamic_entry(name, identifier);
	}

	return pChannelEntry;
}

LPCSTR gAddinPath = FREERDP_ADDIN_PATH;
LPCSTR gInstallPrefix = FREERDP_INSTALL_PREFIX;

FREERDP_ADDIN** freerdp_channels_list_client_addins(LPSTR lpName, LPSTR lpSubsystem, LPSTR lpType, DWORD dwFlags)
{
	int index;
	int nDashes;
	HANDLE hFind;
	DWORD nAddins;
	LPSTR lpPattern;
	size_t cchPattern;
	LPCSTR lpExtension;
	LPSTR lpSearchPath;
	size_t cchSearchPath;
	size_t cchAddinPath;
	size_t cchInstallPrefix;
	FREERDP_ADDIN** ppAddins;
	WIN32_FIND_DATAA FindData;

	cchAddinPath = strlen(gAddinPath);
	cchInstallPrefix = strlen(gInstallPrefix);

	lpExtension = PathGetSharedLibraryExtensionA(0);

	cchPattern = 128 + strlen(lpExtension) + 2;
	lpPattern = (LPSTR) malloc(cchPattern + 1);

	if (lpName && lpSubsystem && lpType)
	{
		sprintf_s(lpPattern, cchPattern, "%s-client-%s-%s.%s", lpName, lpSubsystem, lpType, lpExtension);
	}
	else if (lpName && lpType)
	{
		sprintf_s(lpPattern, cchPattern, "%s-client-?-%s.%s", lpName, lpType, lpExtension);
	}
	else if (lpName)
	{
		sprintf_s(lpPattern, cchPattern, "%s-client*.%s", lpName, lpExtension);
	}
	else
	{
		sprintf_s(lpPattern, cchPattern, "?-client*.%s", lpExtension);
	}

	cchPattern = strlen(lpPattern);

	cchSearchPath = cchInstallPrefix + cchAddinPath + cchPattern + 3;
	lpSearchPath = (LPSTR) malloc(cchSearchPath + 1);

	CopyMemory(lpSearchPath, gInstallPrefix, cchInstallPrefix);
	lpSearchPath[cchInstallPrefix] = '\0';

	NativePathCchAppendA(lpSearchPath, cchSearchPath + 1, gAddinPath);
	NativePathCchAppendA(lpSearchPath, cchSearchPath + 1, lpPattern);

	cchSearchPath = strlen(lpSearchPath);

	hFind = FindFirstFileA(lpSearchPath, &FindData);

	nAddins = 0;
	ppAddins = (FREERDP_ADDIN**) malloc(sizeof(FREERDP_ADDIN*) * 128);
	ppAddins[nAddins] = NULL;

	if (hFind == INVALID_HANDLE_VALUE)
		return ppAddins;

	do
	{
		char* p[5];
		nDashes = 0;
		FREERDP_ADDIN* pAddin;

		pAddin = (FREERDP_ADDIN*) malloc(sizeof(FREERDP_ADDIN));
		ZeroMemory(pAddin, sizeof(FREERDP_ADDIN));

		for (index = 0; FindData.cFileName[index]; index++)
			nDashes += (FindData.cFileName[index] == '-') ? 1 : 0;

		if (nDashes == 1)
		{
			/* <name>-client.<extension> */

			p[0] = FindData.cFileName;
			p[1] = strchr(p[0], '-') + 1;

			strncpy(pAddin->cName, p[0], p[1] - p[0] - 1);

			pAddin->dwFlags = FREERDP_ADDIN_DYNAMIC;
			pAddin->dwFlags |= FREERDP_ADDIN_NAME;

			ppAddins[nAddins++] = pAddin;
		}
		else if (nDashes == 2)
		{
			/* <name>-client-<subsystem>.<extension> */

			p[0] = FindData.cFileName;
			p[1] = strchr(p[0], '-') + 1;
			p[2] = strchr(p[1], '-') + 1;
			p[3] = strchr(p[2], '.') + 1;

			strncpy(pAddin->cName, p[0], p[1] - p[0] - 1);
			strncpy(pAddin->cSubsystem, p[2], p[3] - p[2] - 1);

			pAddin->dwFlags = FREERDP_ADDIN_DYNAMIC;
			pAddin->dwFlags |= FREERDP_ADDIN_NAME;
			pAddin->dwFlags |= FREERDP_ADDIN_SUBSYSTEM;

			ppAddins[nAddins++] = pAddin;
		}
		else if (nDashes == 3)
		{
			/* <name>-client-<subsystem>-<type>.<extension> */

			p[0] = FindData.cFileName;
			p[1] = strchr(p[0], '-') + 1;
			p[2] = strchr(p[1], '-') + 1;
			p[3] = strchr(p[2], '-') + 1;
			p[4] = strchr(p[3], '.') + 1;

			strncpy(pAddin->cName, p[0], p[1] - p[0] - 1);
			strncpy(pAddin->cSubsystem, p[2], p[3] - p[2] - 1);
			strncpy(pAddin->cType, p[3], p[4] - p[3] - 1);

			pAddin->dwFlags = FREERDP_ADDIN_DYNAMIC;
			pAddin->dwFlags |= FREERDP_ADDIN_NAME;
			pAddin->dwFlags |= FREERDP_ADDIN_SUBSYSTEM;
			pAddin->dwFlags |= FREERDP_ADDIN_TYPE;

			ppAddins[nAddins++] = pAddin;
		}
		else
		{
			free(pAddin);
		}
	}
	while (FindNextFileA(hFind, &FindData));

	FindClose(hFind);

	ppAddins[nAddins] = NULL;

	return ppAddins;
}

struct lib_data
{
	PVIRTUALCHANNELENTRY entry; /* the one and only exported function */
	PCHANNEL_INIT_EVENT_FN init_event_proc;
	void* init_handle;
};

struct channel_data
{
	char name[CHANNEL_NAME_LEN + 1];
	int open_handle;
	int options;
	int flags; /* 0 nothing 1 init 2 open */
	PCHANNEL_OPEN_EVENT_FN open_event_proc;
};

struct _SYNC_DATA
{
	SLIST_ENTRY ItemEntry;
	void* Data;
	UINT32 DataLength;
	void* UserData;
	int Index;
};
typedef struct _SYNC_DATA SYNC_DATA;

typedef struct rdp_init_handle rdpInitHandle;

struct rdp_init_handle
{
	rdpChannels* channels;
};

struct rdp_channels
{
	/**
	 * Only the main thread alters these arrays, before any
	 * library thread is allowed in(post_connect is called)
	 * so no need to use mutex locking
	 * After post_connect, each library thread can only access it's
	 * own array items
	 * ie, no two threads can access index 0, ...
	 */

	struct lib_data libs_data[CHANNEL_MAX_COUNT];
	int num_libs_data;

	struct channel_data channels_data[CHANNEL_MAX_COUNT];
	int num_channels_data;

	rdpInitHandle init_handles[CHANNEL_MAX_COUNT];
	int num_init_handles;

	/* control for entry into MyVirtualChannelInit */
	int can_call_init;
	rdpSettings* settings;

	/* true once freerdp_chanman_post_connect is called */
	int is_connected;

	/* used for locating the channels for a given instance */
	freerdp* instance;

	/* signal for incoming data or event */
	struct wait_obj* signal;

	/* used for sync write */
	PSLIST_HEADER pSyncDataList;

	/* used for sync event */
	HANDLE event_sem;
	RDP_EVENT* event;
};

/**
 * The current channel manager reference passes from VirtualChannelEntry to
 * VirtualChannelInit for the pInitHandle.
 */
static rdpChannels* g_init_channels;

/* The list of all channel managers. */
typedef struct rdp_channels_list rdpChannelsList;

struct rdp_channels_list
{
	rdpChannels* channels;
	rdpChannelsList* next;
};

static rdpChannelsList* g_channels_list;

/* To generate unique sequence for all open handles */
static int g_open_handle_sequence;

/* For locking the global resources */
static HANDLE g_mutex_init;
static HANDLE g_mutex_list;

/* returns the channels for the open handle passed in */
static rdpChannels* freerdp_channels_find_by_open_handle(int open_handle, int* pindex)
{
	int lindex;
	rdpChannels* channels;
	rdpChannelsList* channels_list;

	WaitForSingleObject(g_mutex_list, INFINITE);

	for (channels_list = g_channels_list; channels_list; channels_list = channels_list->next)
	{
		channels = channels_list->channels;

		for (lindex = 0; lindex < channels->num_channels_data; lindex++)
		{
			if (channels->channels_data[lindex].open_handle == open_handle)
			{
				ReleaseMutex(g_mutex_list);
				*pindex = lindex;
				return channels;
			}
		}
	}

	ReleaseMutex(g_mutex_list);

	return NULL;
}

/* returns the channels for the rdp instance passed in */
static rdpChannels* freerdp_channels_find_by_instance(freerdp* instance)
{
	rdpChannels* channels;
	rdpChannelsList* channels_list;

	WaitForSingleObject(g_mutex_list, INFINITE);

	for (channels_list = g_channels_list; channels_list; channels_list = channels_list->next)
	{
		channels = channels_list->channels;
		if (channels->instance == instance)
		{
			ReleaseMutex(g_mutex_list);
			return channels;
		}
	}

	ReleaseMutex(g_mutex_list);

	return NULL;
}

/* returns struct channel_data for the channel name passed in */
static struct channel_data* freerdp_channels_find_channel_data_by_name(rdpChannels* channels, const char* channel_name, int* pindex)
{
	int lindex;
	struct channel_data* lchannel_data;

	for (lindex = 0; lindex < channels->num_channels_data; lindex++)
	{
		lchannel_data = channels->channels_data + lindex;

		if (strcmp(channel_name, lchannel_data->name) == 0)
		{
			if (pindex != 0)
				*pindex = lindex;

			return lchannel_data;
		}
	}

	return NULL;
}

/* returns rdpChannel for the channel id passed in */
static rdpChannel* freerdp_channels_find_channel_by_id(rdpChannels* channels, rdpSettings* settings, int channel_id, int* pindex)
{
	int lindex;
	int lcount;
	rdpChannel* lrdp_channel;

	lcount = settings->num_channels;

	for (lindex = 0; lindex < lcount; lindex++)
	{
		lrdp_channel = settings->channels + lindex;

		if (lrdp_channel->channel_id == channel_id)
		{
			if (pindex != 0)
				*pindex = lindex;

			return lrdp_channel;
		}
	}

	return NULL;
}

/* returns rdpChannel for the channel name passed in */
static rdpChannel* freerdp_channels_find_channel_by_name(rdpChannels* channels,
		rdpSettings* settings, const char* channel_name, int* pindex)
{
	int lindex;
	int lcount;
	rdpChannel* lrdp_channel;

	lcount = settings->num_channels;

	for (lindex = 0; lindex < lcount; lindex++)
	{
		lrdp_channel = settings->channels + lindex;

		if (strcmp(channel_name, lrdp_channel->name) == 0)
		{
			if (pindex != 0)
				*pindex = lindex;

			return lrdp_channel;
		}
	}

	return NULL;
}

/**
 * must be called by same thread that calls freerdp_chanman_load_plugin
 * according to MS docs
 * only called from main thread
 */
static UINT32 FREERDP_CC MyVirtualChannelInit(void** ppInitHandle, PCHANNEL_DEF pChannel,
	int channelCount, UINT32 versionRequested, PCHANNEL_INIT_EVENT_FN pChannelInitEventProc)
{
	int index;
	rdpChannels* channels;
	struct lib_data* llib;
	rdpChannel* lrdp_channel;
	PCHANNEL_DEF lchannel_def;
	struct channel_data* lchannel_data;

	if (ppInitHandle == NULL)
	{
		DEBUG_CHANNELS("error bad init handle");
		return CHANNEL_RC_BAD_INIT_HANDLE;
	}

	channels = g_init_channels;
	channels->init_handles[channels->num_init_handles].channels = channels;
	*ppInitHandle = &channels->init_handles[channels->num_init_handles];
	channels->num_init_handles++;

	DEBUG_CHANNELS("enter");

	if (!channels->can_call_init)
	{
		DEBUG_CHANNELS("error not in entry");
		return CHANNEL_RC_NOT_IN_VIRTUALCHANNELENTRY;
	}

	if (channels->num_channels_data + channelCount >= CHANNEL_MAX_COUNT)
	{
		DEBUG_CHANNELS("error too many channels");
		return CHANNEL_RC_TOO_MANY_CHANNELS;
	}

	if (pChannel == 0)
	{
		DEBUG_CHANNELS("error bad channel");
		return CHANNEL_RC_BAD_CHANNEL;
	}

	if (channels->is_connected)
	{
		DEBUG_CHANNELS("error already connected");
		return CHANNEL_RC_ALREADY_CONNECTED;
	}

	if (versionRequested != VIRTUAL_CHANNEL_VERSION_WIN2000)
	{
		DEBUG_CHANNELS("warning version");
	}

	for (index = 0; index < channelCount; index++)
	{
		lchannel_def = pChannel + index;

		if (freerdp_channels_find_channel_data_by_name(channels, lchannel_def->name, 0) != 0)
		{
			DEBUG_CHANNELS("error channel already used");
			return CHANNEL_RC_BAD_CHANNEL;
		}
	}

	llib = channels->libs_data + channels->num_libs_data;
	llib->init_event_proc = pChannelInitEventProc;
	llib->init_handle = *ppInitHandle;
	channels->num_libs_data++;

	for (index = 0; index < channelCount; index++)
	{
		lchannel_def = pChannel + index;
		lchannel_data = channels->channels_data + channels->num_channels_data;

		WaitForSingleObject(g_mutex_list, INFINITE);
		lchannel_data->open_handle = g_open_handle_sequence++;
		ReleaseMutex(g_mutex_list);

		lchannel_data->flags = 1; /* init */
		strncpy(lchannel_data->name, lchannel_def->name, CHANNEL_NAME_LEN);
		lchannel_data->options = lchannel_def->options;

		if (channels->settings->num_channels < 16)
		{
			lrdp_channel = channels->settings->channels + channels->settings->num_channels;
			strncpy(lrdp_channel->name, lchannel_def->name, 7);
			lrdp_channel->options = lchannel_def->options;
			channels->settings->num_channels++;
		}
		else
		{
			DEBUG_CHANNELS("warning more than 16 channels");
		}

		channels->num_channels_data++;
	}

	return CHANNEL_RC_OK;
}

/**
 * can be called from any thread
 * thread safe because no 2 threads can have the same channel name registered
 */
static UINT32 FREERDP_CC MyVirtualChannelOpen(void* pInitHandle, UINT32* pOpenHandle,
	char* pChannelName, PCHANNEL_OPEN_EVENT_FN pChannelOpenEventProc)
{
	int index;
	rdpChannels* channels;
	struct channel_data* lchannel_data;

	DEBUG_CHANNELS("enter");

	channels = ((rdpInitHandle*) pInitHandle)->channels;

	if (pOpenHandle == 0)
	{
		DEBUG_CHANNELS("error bad channel handle");
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;
	}

	if (pChannelOpenEventProc == 0)
	{
		DEBUG_CHANNELS("error bad proc");
		return CHANNEL_RC_BAD_PROC;
	}

	if (!channels->is_connected)
	{
		DEBUG_CHANNELS("error not connected");
		return CHANNEL_RC_NOT_CONNECTED;
	}

	lchannel_data = freerdp_channels_find_channel_data_by_name(channels, pChannelName, &index);

	if (lchannel_data == 0)
	{
		DEBUG_CHANNELS("error channel name");
		return CHANNEL_RC_UNKNOWN_CHANNEL_NAME;
	}

	if (lchannel_data->flags == 2)
	{
		DEBUG_CHANNELS("error channel already open");
		return CHANNEL_RC_ALREADY_OPEN;
	}

	lchannel_data->flags = 2; /* open */
	lchannel_data->open_event_proc = pChannelOpenEventProc;
	*pOpenHandle = lchannel_data->open_handle;

	return CHANNEL_RC_OK;
}

/**
 * can be called from any thread
 * thread safe because no 2 threads can have the same openHandle
 */
static UINT32 FREERDP_CC MyVirtualChannelClose(UINT32 openHandle)
{
	int index;
	rdpChannels* channels;
	struct channel_data* lchannel_data;

	DEBUG_CHANNELS("enter");

	channels = freerdp_channels_find_by_open_handle(openHandle, &index);

	if ((channels == NULL) || (index < 0) || (index >= CHANNEL_MAX_COUNT))
	{
		DEBUG_CHANNELS("error bad channels");
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;
	}

	lchannel_data = channels->channels_data + index;

	if (lchannel_data->flags != 2)
	{
		DEBUG_CHANNELS("error not open");
		return CHANNEL_RC_NOT_OPEN;
	}

	lchannel_data->flags = 0;

	return CHANNEL_RC_OK;
}

/* can be called from any thread */
static UINT32 FREERDP_CC MyVirtualChannelWrite(UINT32 openHandle, void* pData, UINT32 dataLength, void* pUserData)
{
	int index;
	SYNC_DATA* item;
	rdpChannels* channels;
	struct channel_data* lchannel_data;

	channels = freerdp_channels_find_by_open_handle(openHandle, &index);

	if ((channels == NULL) || (index < 0) || (index >= CHANNEL_MAX_COUNT))
	{
		DEBUG_CHANNELS("error bad channel handle");
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;
	}

	if (!channels->is_connected)
	{
		DEBUG_CHANNELS("error not connected");
		return CHANNEL_RC_NOT_CONNECTED;
	}

	if (pData == 0)
	{
		DEBUG_CHANNELS("error bad pData");
		return CHANNEL_RC_NULL_DATA;
	}

	if (dataLength == 0)
	{
		DEBUG_CHANNELS("error bad dataLength");
		return CHANNEL_RC_ZERO_LENGTH;
	}

	lchannel_data = channels->channels_data + index;

	if (lchannel_data->flags != 2)
	{
		DEBUG_CHANNELS("error not open");
		return CHANNEL_RC_NOT_OPEN;
	}

	if (!channels->is_connected)
	{
		DEBUG_CHANNELS("error not connected");
		return CHANNEL_RC_NOT_CONNECTED;
	}

	item = (SYNC_DATA*) _aligned_malloc(sizeof(SYNC_DATA), MEMORY_ALLOCATION_ALIGNMENT);
	item->Data = pData;
	item->DataLength = dataLength;
	item->UserData = pUserData;
	item->Index = index;

	InterlockedPushEntrySList(channels->pSyncDataList, &(item->ItemEntry));

	/* set the event */
	wait_obj_set(channels->signal);

	return CHANNEL_RC_OK;
}

static UINT32 FREERDP_CC MyVirtualChannelEventPush(UINT32 openHandle, RDP_EVENT* event)
{
	int index;
	rdpChannels* channels;
	struct channel_data* lchannel_data;

	channels = freerdp_channels_find_by_open_handle(openHandle, &index);

	if ((channels == NULL) || (index < 0) || (index >= CHANNEL_MAX_COUNT))
	{
		DEBUG_CHANNELS("error bad channels handle");
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;
	}

	if (!channels->is_connected)
	{
		DEBUG_CHANNELS("error not connected");
		return CHANNEL_RC_NOT_CONNECTED;
	}

	if (event == NULL)
	{
		DEBUG_CHANNELS("error bad event");
		return CHANNEL_RC_NULL_DATA;
	}

	lchannel_data = channels->channels_data + index;

	if (lchannel_data->flags != 2)
	{
		DEBUG_CHANNELS("error not open");
		return CHANNEL_RC_NOT_OPEN;
	}

	 /* lock channels->event */
	WaitForSingleObject(channels->event_sem, INFINITE);

	if (!channels->is_connected)
	{
		ReleaseSemaphore(channels->event_sem, 1, NULL);
		DEBUG_CHANNELS("error not connected");
		return CHANNEL_RC_NOT_CONNECTED;
	}

	channels->event = event;
	/* set the event */
	wait_obj_set(channels->signal);

	return CHANNEL_RC_OK;
}

/**
 * this is called shortly after the application starts and
 * before any other function in the file
 * called only from main thread
 */
int freerdp_channels_global_init(void)
{
	g_init_channels = NULL;
	g_channels_list = NULL;
	g_open_handle_sequence = 1;
	g_mutex_init = CreateMutex(NULL, FALSE, NULL);
	g_mutex_list = CreateMutex(NULL, FALSE, NULL);

	return 0;
}

int freerdp_channels_global_uninit(void)
{
	while (g_channels_list)
		freerdp_channels_free(g_channels_list->channels);

	CloseHandle(g_mutex_init);
	CloseHandle(g_mutex_list);

	return 0;
}

rdpChannels* freerdp_channels_new(void)
{
	rdpChannels* channels;
	rdpChannelsList* channels_list;

	channels = xnew(rdpChannels);

	channels->pSyncDataList = (PSLIST_HEADER) _aligned_malloc(sizeof(SLIST_HEADER), MEMORY_ALLOCATION_ALIGNMENT);
	InitializeSListHead(channels->pSyncDataList);

	channels->event_sem = CreateSemaphore(NULL, 1, 16, NULL);
	channels->signal = wait_obj_new();

	/* Add it to the global list */
	channels_list = xnew(rdpChannelsList);
	channels_list->channels = channels;

	WaitForSingleObject(g_mutex_list, INFINITE);
	channels_list->next = g_channels_list;
	g_channels_list = channels_list;
	ReleaseMutex(g_mutex_list);

	return channels;
}

void freerdp_channels_free(rdpChannels* channels)
{
	rdpChannelsList* list;
	rdpChannelsList* prev;

	InterlockedFlushSList(channels->pSyncDataList);
	_aligned_free(channels->pSyncDataList);

	CloseHandle(channels->event_sem);
	wait_obj_free(channels->signal);

	/* Remove from global list */

	WaitForSingleObject(g_mutex_list, INFINITE);

	for (prev = NULL, list = g_channels_list; list; prev = list, list = list->next)
	{
		if (list->channels == channels)
			break;
	}

	if (list)
	{
		if (prev)
			prev->next = list->next;
		else
			g_channels_list = list->next;
		free(list);
	}

	ReleaseMutex(g_mutex_list);

	free(channels);
}

int freerdp_channels_client_load(rdpChannels* channels, rdpSettings* settings, void* entry, void* data)
{
	int status;
	struct lib_data* lib;
	CHANNEL_ENTRY_POINTS_EX ep;

	if (channels->num_libs_data + 1 >= CHANNEL_MAX_COUNT)
	{
		DEBUG_CHANNELS("too many channels");
		return 1;
	}

	lib = channels->libs_data + channels->num_libs_data;
	lib->entry = (PVIRTUALCHANNELENTRY) entry;

	ep.cbSize = sizeof(ep);
	ep.protocolVersion = VIRTUAL_CHANNEL_VERSION_WIN2000;
	ep.pVirtualChannelInit = MyVirtualChannelInit;
	ep.pVirtualChannelOpen = MyVirtualChannelOpen;
	ep.pVirtualChannelClose = MyVirtualChannelClose;
	ep.pVirtualChannelWrite = MyVirtualChannelWrite;
	ep.pExtendedData = data;
	ep.pVirtualChannelEventPush = MyVirtualChannelEventPush;

	/* enable MyVirtualChannelInit */
	channels->can_call_init = 1;
	channels->settings = settings;

	WaitForSingleObject(g_mutex_init, INFINITE);

	g_init_channels = channels;
	status = lib->entry((PCHANNEL_ENTRY_POINTS) &ep);
	g_init_channels = NULL;

	ReleaseMutex(g_mutex_init);

	/* disable MyVirtualChannelInit */
	channels->settings = 0;
	channels->can_call_init = 0;

	if (!status)
	{
		DEBUG_CHANNELS("export function call failed");
		return 1;
	}

	return 0;
}

/**
 * this is called when processing the command line parameters
 * called only from main thread
 */
int freerdp_channels_load_plugin(rdpChannels* channels, rdpSettings* settings, const char* name, void* data)
{
	void* entry;

	DEBUG_CHANNELS("%s", name);

	entry = (PVIRTUALCHANNELENTRY) freerdp_load_plugin(name, CHANNEL_EXPORT_FUNC_NAME);

	if (entry == NULL)
	{
		DEBUG_CHANNELS("failed to find export function");
		return 1;
	}

	return freerdp_channels_client_load(channels, settings, entry, data);
}

/**
 * go through and inform all the libraries that we are initialized
 * called only from main thread
 */
int freerdp_channels_pre_connect(rdpChannels* channels, freerdp* instance)
{
	int index;
	void* dummy;
	struct lib_data* llib;
	CHANNEL_DEF lchannel_def;

	DEBUG_CHANNELS("enter");
	channels->instance = instance;

	/**
         * If rdpsnd is registered but not rdpdr, it's necessary to register a fake
	 * rdpdr channel to make sound work. This is a workaround for Window 7 and
	 * Windows 2008
	 */
	if (freerdp_channels_find_channel_data_by_name(channels, "rdpsnd", 0) != 0 &&
		freerdp_channels_find_channel_data_by_name(channels, "rdpdr", 0) == 0)
	{
		lchannel_def.options = CHANNEL_OPTION_INITIALIZED |
			CHANNEL_OPTION_ENCRYPT_RDP;
		strcpy(lchannel_def.name, "rdpdr");
		channels->can_call_init = 1;
		channels->settings = instance->settings;
		WaitForSingleObject(g_mutex_init, INFINITE);
		g_init_channels = channels;
		MyVirtualChannelInit(&dummy, &lchannel_def, 1,
			VIRTUAL_CHANNEL_VERSION_WIN2000, 0);
		g_init_channels = NULL;
		ReleaseMutex(g_mutex_init);
		channels->can_call_init = 0;
		channels->settings = 0;
		DEBUG_CHANNELS("registered fake rdpdr for rdpsnd.");
	}

	for (index = 0; index < channels->num_libs_data; index++)
	{
		llib = channels->libs_data + index;

		if (llib->init_event_proc != 0)
			llib->init_event_proc(llib->init_handle, CHANNEL_EVENT_INITIALIZED, 0, 0);
	}

	return 0;
}

/**
 * go through and inform all the libraries that we are connected
 * this will tell the libraries that its ok to call MyVirtualChannelOpen
 * called only from main thread
 */
int freerdp_channels_post_connect(rdpChannels* channels, freerdp* instance)
{
	int index;
	char* hostname;
	int hostname_len;
	struct lib_data* llib;

	channels->is_connected = 1;
	hostname = instance->settings->hostname;
	hostname_len = strlen(hostname);

	DEBUG_CHANNELS("hostname [%s] channels->num_libs [%d]", hostname, channels->num_libs_data);

	for (index = 0; index < channels->num_libs_data; index++)
	{
		llib = channels->libs_data + index;

		if (llib->init_event_proc != 0)
			llib->init_event_proc(llib->init_handle, CHANNEL_EVENT_CONNECTED, hostname, hostname_len);
	}

	return 0;
}

/**
 * data comming from the server to the client
 * called only from main thread
 */
int freerdp_channels_data(freerdp* instance, int channel_id, void* data, int data_size, int flags, int total_size)
{
	int index;
	rdpChannels* channels;
	rdpChannel* lrdp_channel;
	struct channel_data* lchannel_data;

	channels = freerdp_channels_find_by_instance(instance);

	if (channels == 0)
	{
		DEBUG_CHANNELS("could not find channel manager");
		return 1;
	}

	lrdp_channel = freerdp_channels_find_channel_by_id(channels, instance->settings,
		channel_id, &index);
	if (lrdp_channel == 0)
	{
		DEBUG_CHANNELS("could not find channel id");
		return 1;
	}

	lchannel_data = freerdp_channels_find_channel_data_by_name(channels, lrdp_channel->name, &index);

	if (lchannel_data == 0)
	{
		DEBUG_CHANNELS("could not find channel name");
		return 1;
	}

	if (lchannel_data->open_event_proc != 0)
	{
		lchannel_data->open_event_proc(lchannel_data->open_handle,
			CHANNEL_EVENT_DATA_RECEIVED, data, data_size, total_size, flags);
	}

	return 0;
}

static const char* event_class_to_name_table[] =
{
	"rdpdbg",   /* RDP_EVENT_CLASS_DEBUG */
	"cliprdr",  /* RDP_EVENT_CLASS_CLIPRDR */
	"tsmf",     /* RDP_EVENT_CLASS_TSMF */
	"rail",     /* RDP_EVENT_CLASS_RAIL */
	NULL
};

/**
 * Send a plugin-defined event to the plugin.
 * called only from main thread
 * @param channels the channel manager instance
 * @param event an event object created by freerdp_event_new()
 */
FREERDP_API int freerdp_channels_send_event(rdpChannels* channels, RDP_EVENT* event)
{
	int index;
	const char* name;
	struct channel_data* lchannel_data;

	name = event_class_to_name_table[event->event_class];

	if (name == NULL)
	{
		DEBUG_CHANNELS("unknown event_class %d", event->event_class);
		freerdp_event_free(event);
		return 1;
	}

	lchannel_data = freerdp_channels_find_channel_data_by_name(channels, name, &index);

	if (lchannel_data == NULL)
	{
		DEBUG_CHANNELS("could not find channel name %s", name);
		freerdp_event_free(event);
		return 1;
	}

	if (lchannel_data->open_event_proc != NULL)
	{
		lchannel_data->open_event_proc(lchannel_data->open_handle,
			CHANNEL_EVENT_USER,
			event, sizeof(RDP_EVENT), sizeof(RDP_EVENT), 0);
	}

	return 0;
}

/**
 * called only from main thread
 */
static void freerdp_channels_process_sync(rdpChannels* channels, freerdp* instance)
{
	SYNC_DATA* item;
	rdpChannel* lrdp_channel;
	struct channel_data* lchannel_data;

	while (QueryDepthSList(channels->pSyncDataList) > 0)
	{
		item = (SYNC_DATA*) InterlockedPopEntrySList(channels->pSyncDataList);

		if (!item)
			break;

		lchannel_data = channels->channels_data + item->Index;

		lrdp_channel = freerdp_channels_find_channel_by_name(channels, instance->settings,
			lchannel_data->name, &item->Index);

		if (lrdp_channel != NULL)
			instance->SendChannelData(instance, lrdp_channel->channel_id, item->Data, item->DataLength);

		if (lchannel_data->open_event_proc != 0)
		{
			lchannel_data->open_event_proc(lchannel_data->open_handle,
				CHANNEL_EVENT_WRITE_COMPLETE, item->UserData, sizeof(void*), sizeof(void*), 0);
		}

		_aligned_free(item);
	}
}

/**
 * called only from main thread
 */
BOOL freerdp_channels_get_fds(rdpChannels* channels, freerdp* instance, void** read_fds,
	int* read_count, void** write_fds, int* write_count)
{
	wait_obj_get_fds(channels->signal, read_fds, read_count);
	return TRUE;
}

/**
 * called only from main thread
 */
BOOL freerdp_channels_check_fds(rdpChannels* channels, freerdp* instance)
{
	if (wait_obj_is_set(channels->signal))
	{
		wait_obj_clear(channels->signal);
		freerdp_channels_process_sync(channels, instance);
	}

	return TRUE;
}

RDP_EVENT* freerdp_channels_pop_event(rdpChannels* channels)
{
	RDP_EVENT* event;

	if (channels->event == NULL)
		return NULL;

	event = channels->event;
	channels->event = NULL;

	/* release channels->event */
	ReleaseSemaphore(channels->event_sem, 1, NULL);

	return event;
}

void freerdp_channels_close(rdpChannels* channels, freerdp* instance)
{
	int index;
	struct lib_data* llib;

	DEBUG_CHANNELS("closing");
	channels->is_connected = 0;
	freerdp_channels_check_fds(channels, instance);

	/* tell all libraries we are shutting down */
	for (index = 0; index < channels->num_libs_data; index++)
	{
		llib = channels->libs_data + index;

		if (llib->init_event_proc != 0)
			llib->init_event_proc(llib->init_handle, CHANNEL_EVENT_TERMINATED, 0, 0);
	}
}

