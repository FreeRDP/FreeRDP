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
#include <freerdp/addin.h>
#include <freerdp/utils/file.h>
#include <freerdp/utils/event.h>
#include <freerdp/utils/debug.h>

#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/file.h>
#include <winpr/synch.h>
#include <winpr/library.h>
#include <winpr/collections.h>

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

extern const STATIC_ADDIN_TABLE CLIENT_STATIC_ADDIN_TABLE[];

FREERDP_ADDIN** freerdp_channels_list_client_static_addins(LPSTR pszName, LPSTR pszSubsystem, LPSTR pszType, DWORD dwFlags)
{
	int i, j;
	DWORD nAddins;
	FREERDP_ADDIN* pAddin;
	FREERDP_ADDIN** ppAddins = NULL;
	STATIC_SUBSYSTEM_ENTRY* subsystems;

	nAddins = 0;
	ppAddins = (FREERDP_ADDIN**) malloc(sizeof(FREERDP_ADDIN*) * 128);
	ppAddins[nAddins] = NULL;

	for (i = 0; CLIENT_STATIC_ADDIN_TABLE[i].name != NULL; i++)
	{
		pAddin = (FREERDP_ADDIN*) malloc(sizeof(FREERDP_ADDIN));
		ZeroMemory(pAddin, sizeof(FREERDP_ADDIN));

		strcpy(pAddin->cName, CLIENT_STATIC_ADDIN_TABLE[i].name);

		pAddin->dwFlags = FREERDP_ADDIN_CLIENT;
		pAddin->dwFlags |= FREERDP_ADDIN_STATIC;
		pAddin->dwFlags |= FREERDP_ADDIN_NAME;

		ppAddins[nAddins++] = pAddin;

		subsystems = (STATIC_SUBSYSTEM_ENTRY*) CLIENT_STATIC_ADDIN_TABLE[i].table;

		for (j = 0; subsystems[j].name != NULL; j++)
		{
			pAddin = (FREERDP_ADDIN*) malloc(sizeof(FREERDP_ADDIN));
			ZeroMemory(pAddin, sizeof(FREERDP_ADDIN));

			strcpy(pAddin->cName, CLIENT_STATIC_ADDIN_TABLE[i].name);
			strcpy(pAddin->cSubsystem, subsystems[j].name);

			pAddin->dwFlags = FREERDP_ADDIN_CLIENT;
			pAddin->dwFlags |= FREERDP_ADDIN_STATIC;
			pAddin->dwFlags |= FREERDP_ADDIN_NAME;
			pAddin->dwFlags |= FREERDP_ADDIN_SUBSYSTEM;

			ppAddins[nAddins++] = pAddin;
		}
	}

	ppAddins[nAddins] = NULL;

	return ppAddins;
}

FREERDP_ADDIN** freerdp_channels_list_dynamic_addins(LPSTR pszName, LPSTR pszSubsystem, LPSTR pszType, DWORD dwFlags)
{
	int index;
	int nDashes;
	HANDLE hFind;
	DWORD nAddins;
	LPSTR pszPattern;
	size_t cchPattern;
	LPCSTR pszAddinPath = FREERDP_ADDIN_PATH;
	LPCSTR pszInstallPrefix = FREERDP_INSTALL_PREFIX;
	LPCSTR pszExtension;
	LPSTR pszSearchPath;
	size_t cchSearchPath;
	size_t cchAddinPath;
	size_t cchInstallPrefix;
	FREERDP_ADDIN** ppAddins;
	WIN32_FIND_DATAA FindData;

	cchAddinPath = strlen(pszAddinPath);
	cchInstallPrefix = strlen(pszInstallPrefix);

	pszExtension = PathGetSharedLibraryExtensionA(0);

	cchPattern = 128 + strlen(pszExtension) + 2;
	pszPattern = (LPSTR) malloc(cchPattern + 1);

	if (pszName && pszSubsystem && pszType)
	{
		sprintf_s(pszPattern, cchPattern, "%s-client-%s-%s.%s", pszName, pszSubsystem, pszType, pszExtension);
	}
	else if (pszName && pszType)
	{
		sprintf_s(pszPattern, cchPattern, "%s-client-?-%s.%s", pszName, pszType, pszExtension);
	}
	else if (pszName)
	{
		sprintf_s(pszPattern, cchPattern, "%s-client*.%s", pszName, pszExtension);
	}
	else
	{
		sprintf_s(pszPattern, cchPattern, "?-client*.%s", pszExtension);
	}

	cchPattern = strlen(pszPattern);

	cchSearchPath = cchInstallPrefix + cchAddinPath + cchPattern + 3;
	pszSearchPath = (LPSTR) malloc(cchSearchPath + 1);

	CopyMemory(pszSearchPath, pszInstallPrefix, cchInstallPrefix);
	pszSearchPath[cchInstallPrefix] = '\0';

	NativePathCchAppendA(pszSearchPath, cchSearchPath + 1, pszAddinPath);
	NativePathCchAppendA(pszSearchPath, cchSearchPath + 1, pszPattern);

	cchSearchPath = strlen(pszSearchPath);

	hFind = FindFirstFileA(pszSearchPath, &FindData);

	nAddins = 0;
	ppAddins = (FREERDP_ADDIN**) malloc(sizeof(FREERDP_ADDIN*) * 128);
	ppAddins[nAddins] = NULL;

	if (hFind == INVALID_HANDLE_VALUE)
		return ppAddins;

	do
	{
		char* p[5];
		FREERDP_ADDIN* pAddin;

		nDashes = 0;
		pAddin = (FREERDP_ADDIN*) malloc(sizeof(FREERDP_ADDIN));
		ZeroMemory(pAddin, sizeof(FREERDP_ADDIN));

		for (index = 0; FindData.cFileName[index]; index++)
			nDashes += (FindData.cFileName[index] == '-') ? 1 : 0;

		if (nDashes == 1)
		{
			/* <name>-client.<extension> */

			p[0] = FindData.cFileName;
			p[1] = strchr(p[0], '-') + 1;

			strncpy(pAddin->cName, p[0], (p[1] - p[0]) - 1);

			pAddin->dwFlags = FREERDP_ADDIN_CLIENT;
			pAddin->dwFlags |= FREERDP_ADDIN_DYNAMIC;
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

			strncpy(pAddin->cName, p[0], (p[1] - p[0]) - 1);
			strncpy(pAddin->cSubsystem, p[2], (p[3] - p[2]) - 1);

			pAddin->dwFlags = FREERDP_ADDIN_CLIENT;
			pAddin->dwFlags |= FREERDP_ADDIN_DYNAMIC;
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

			strncpy(pAddin->cName, p[0], (p[1] - p[0]) - 1);
			strncpy(pAddin->cSubsystem, p[2], (p[3] - p[2]) - 1);
			strncpy(pAddin->cType, p[3], (p[4] - p[3]) - 1);

			pAddin->dwFlags = FREERDP_ADDIN_CLIENT;
			pAddin->dwFlags |= FREERDP_ADDIN_DYNAMIC;
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

FREERDP_ADDIN** freerdp_channels_list_addins(LPSTR pszName, LPSTR pszSubsystem, LPSTR pszType, DWORD dwFlags)
{
	if (dwFlags & FREERDP_ADDIN_STATIC)
		return freerdp_channels_list_client_static_addins(pszName, pszSubsystem, pszType, dwFlags);
	else if (dwFlags & FREERDP_ADDIN_DYNAMIC)
		return freerdp_channels_list_dynamic_addins(pszName, pszSubsystem, pszType, dwFlags);

	return NULL;
}

void freerdp_channels_addin_list_free(FREERDP_ADDIN** ppAddins)
{
	int index;

	for (index = 0; ppAddins[index] != NULL; index++)
		free(ppAddins[index]);

	free(ppAddins);
}

void* freerdp_channels_load_static_addin_entry(LPCSTR pszName, LPSTR pszSubsystem, LPSTR pszType, DWORD dwFlags)
{
	int i, j;
	STATIC_SUBSYSTEM_ENTRY* subsystems;

	for (i = 0; CLIENT_STATIC_ADDIN_TABLE[i].name != NULL; i++)
	{
		if (strcmp(CLIENT_STATIC_ADDIN_TABLE[i].name, pszName) == 0)
		{
			if (pszSubsystem != NULL)
			{
				subsystems = (STATIC_SUBSYSTEM_ENTRY*) CLIENT_STATIC_ADDIN_TABLE[i].table;

				for (j = 0; subsystems[j].name != NULL; j++)
				{
					if (strcmp(subsystems[j].name, pszSubsystem) == 0)
					{
						if (pszType)
						{
							if (strcmp(subsystems[j].type, pszType) == 0)
								return (void*) subsystems[j].entry;
						}
						else
						{
							return (void*) subsystems[j].entry;
						}
					}
				}
			}
			else
			{
				return (void*) CLIENT_STATIC_ADDIN_TABLE[i].entry;
			}
		}
	}

	return NULL;
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

struct _CHANNEL_OPEN_EVENT
{
	void* Data;
	UINT32 DataLength;
	void* UserData;
	int Index;
};
typedef struct _CHANNEL_OPEN_EVENT CHANNEL_OPEN_EVENT;

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

	wMessagePipe* MsgPipe;
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

	lcount = settings->ChannelCount;

	for (lindex = 0; lindex < lcount; lindex++)
	{
		lrdp_channel = settings->ChannelDefArray + lindex;

		if (lrdp_channel->ChannelId == channel_id)
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

	lcount = settings->ChannelCount;

	for (lindex = 0; lindex < lcount; lindex++)
	{
		lrdp_channel = settings->ChannelDefArray + lindex;

		if (strcmp(channel_name, lrdp_channel->Name) == 0)
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

	if (!ppInitHandle)
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

	if (!pChannel)
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

		if (channels->settings->ChannelCount < 16)
		{
			lrdp_channel = channels->settings->ChannelDefArray + channels->settings->ChannelCount;
			strncpy(lrdp_channel->Name, lchannel_def->name, 7);
			lrdp_channel->options = lchannel_def->options;
			channels->settings->ChannelCount++;
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

	if (!pOpenHandle)
	{
		DEBUG_CHANNELS("error bad channel handle");
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;
	}

	if (!pChannelOpenEventProc)
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

	if (!lchannel_data)
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
	CHANNEL_OPEN_EVENT* item;
	rdpChannels* channels;
	struct channel_data* lchannel_data;

	channels = freerdp_channels_find_by_open_handle(openHandle, &index);

	if ((!channels) || (index < 0) || (index >= CHANNEL_MAX_COUNT))
	{
		DEBUG_CHANNELS("error bad channel handle");
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;
	}

	if (!channels->is_connected)
	{
		DEBUG_CHANNELS("error not connected");
		return CHANNEL_RC_NOT_CONNECTED;
	}

	if (!pData)
	{
		DEBUG_CHANNELS("error bad pData");
		return CHANNEL_RC_NULL_DATA;
	}

	if (!dataLength)
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

	item = (CHANNEL_OPEN_EVENT*) malloc(sizeof(CHANNEL_OPEN_EVENT));
	item->Data = pData;
	item->DataLength = dataLength;
	item->UserData = pUserData;
	item->Index = index;

	MessageQueue_Post(channels->MsgPipe->Out, (void*) channels, 0, (void*) item, NULL);

	return CHANNEL_RC_OK;
}

static UINT32 FREERDP_CC MyVirtualChannelEventPush(UINT32 openHandle, RDP_EVENT* event)
{
	int index;
	rdpChannels* channels;
	struct channel_data* lchannel_data;

	channels = freerdp_channels_find_by_open_handle(openHandle, &index);

	if ((!channels) || (index < 0) || (index >= CHANNEL_MAX_COUNT))
	{
		DEBUG_CHANNELS("error bad channels handle");
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;
	}

	if (!channels->is_connected)
	{
		DEBUG_CHANNELS("error not connected");
		return CHANNEL_RC_NOT_CONNECTED;
	}

	if (!event)
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

	if (!channels->is_connected)
	{
		DEBUG_CHANNELS("error not connected");
		return CHANNEL_RC_NOT_CONNECTED;
	}

	/**
	 * We really intend to use the In queue for events, but we're pushing on both
	 * to wake up threads waiting on the out queue. Doing this cleanly would require
	 * breaking freerdp_pop_event() a bit too early in this refactoring.
	 */

	MessageQueue_Post(channels->MsgPipe->In, (void*) channels, 1, (void*) event, NULL);
	MessageQueue_Post(channels->MsgPipe->Out, (void*) channels, 1, (void*) event, NULL);

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

	channels = (rdpChannels*) malloc(sizeof(rdpChannels));
	ZeroMemory(channels, sizeof(rdpChannels));

	channels->MsgPipe = MessagePipe_New();

	/* Add it to the global list */
	channels_list = (rdpChannelsList*) malloc(sizeof(rdpChannelsList));
	ZeroMemory(channels_list, sizeof(rdpChannelsList));
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

	MessagePipe_Free(channels->MsgPipe);

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
		printf("error: too many channels\n");
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
		printf("error: channel export function call failed\n");
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

	entry = (PVIRTUALCHANNELENTRY) freerdp_load_channel_addin_entry(name, NULL, NULL, FREERDP_ADDIN_CHANNEL_STATIC);

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
	hostname = instance->settings->ServerHostname;
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

	if (!channels)
	{
		DEBUG_CHANNELS("could not find channel manager");
		return 1;
	}

	lrdp_channel = freerdp_channels_find_channel_by_id(channels, instance->settings, channel_id, &index);

	if (!lrdp_channel)
	{
		DEBUG_CHANNELS("could not find channel id");
		return 1;
	}

	lchannel_data = freerdp_channels_find_channel_data_by_name(channels, lrdp_channel->Name, &index);

	if (!lchannel_data)
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

	if (!name)
	{
		DEBUG_CHANNELS("unknown event_class %d", event->event_class);
		freerdp_event_free(event);
		return 1;
	}

	lchannel_data = freerdp_channels_find_channel_data_by_name(channels, name, &index);

	if (!lchannel_data)
	{
		DEBUG_CHANNELS("could not find channel name %s", name);
		freerdp_event_free(event);
		return 1;
	}

	if (lchannel_data->open_event_proc)
	{
		lchannel_data->open_event_proc(lchannel_data->open_handle, CHANNEL_EVENT_USER,
			event, sizeof(RDP_EVENT), sizeof(RDP_EVENT), 0);
	}

	return 0;
}

/**
 * called only from main thread
 */
static void freerdp_channels_process_sync(rdpChannels* channels, freerdp* instance)
{
	wMessage message;
	RDP_EVENT* event;
	CHANNEL_OPEN_EVENT* item;
	rdpChannel* lrdp_channel;
	struct channel_data* lchannel_data;

	while (MessageQueue_Peek(channels->MsgPipe->Out, &message, TRUE))
	{
		if (message.id == WMQ_QUIT)
			break;

		if (message.id == 0)
		{
			item = (CHANNEL_OPEN_EVENT*) message.wParam;

			if (!item)
				break;

			lchannel_data = channels->channels_data + item->Index;

			lrdp_channel = freerdp_channels_find_channel_by_name(channels, instance->settings,
				lchannel_data->name, &item->Index);

			if (lrdp_channel)
				instance->SendChannelData(instance, lrdp_channel->ChannelId, item->Data, item->DataLength);

			if (lchannel_data->open_event_proc)
			{
				lchannel_data->open_event_proc(lchannel_data->open_handle,
					CHANNEL_EVENT_WRITE_COMPLETE, item->UserData, item->DataLength, item->DataLength, 0);
			}

			free(item);
		}
		else if (message.id == 1)
		{
			event = (RDP_EVENT*) message.wParam;

			/**
			 * Ignore for now, the same event is being pushed on the In queue,
			 * and we're pushing it on the Out queue just to wake other threads
			 */
		}
	}
}

/**
 * called only from main thread
 */
BOOL freerdp_channels_get_fds(rdpChannels* channels, freerdp* instance, void** read_fds,
	int* read_count, void** write_fds, int* write_count)
{
	void* pfd;

	pfd = GetEventWaitObject(MessageQueue_Event(channels->MsgPipe->Out));

	if (pfd)
	{
		read_fds[*read_count] = pfd;
		(*read_count)++;
	}

	return TRUE;
}

HANDLE freerdp_channels_get_event_handle(freerdp* instance)
{
	HANDLE event = NULL;
	rdpChannels* channels;

	channels = instance->context->channels;
	event = MessageQueue_Event(channels->MsgPipe->Out);

	return event;
}

int freerdp_channels_process_pending_messages(freerdp* instance)
{
	rdpChannels* channels;

	channels = instance->context->channels;

	if (WaitForSingleObject(MessageQueue_Event(channels->MsgPipe->Out), 0) == WAIT_OBJECT_0)
	{
		freerdp_channels_process_sync(channels, instance);
	}

	return TRUE;
}

/**
 * called only from main thread
 */
BOOL freerdp_channels_check_fds(rdpChannels* channels, freerdp* instance)
{
	if (WaitForSingleObject(MessageQueue_Event(channels->MsgPipe->Out), 0) == WAIT_OBJECT_0)
	{
		freerdp_channels_process_sync(channels, instance);
	}

	return TRUE;
}

RDP_EVENT* freerdp_channels_pop_event(rdpChannels* channels)
{
	wMessage message;
	RDP_EVENT* event = NULL;

	if (MessageQueue_Peek(channels->MsgPipe->In, &message, TRUE))
	{
		if (message.id == 1)
		{
			event = (RDP_EVENT*) message.wParam;
		}
	}

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

/* Local variables: */
/* c-basic-offset: 8 */
/* c-file-style: "linux" */
/* End: */
