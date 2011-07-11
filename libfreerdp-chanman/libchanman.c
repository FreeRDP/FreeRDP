/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Virtual Channel Manager
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2010-2011 Vic Lee
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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/freerdp.h>
#include <freerdp/constants.h>
#include <freerdp/chanman.h>
#include <freerdp/svc.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/list.h>
#include <freerdp/utils/semaphore.h>
#include <freerdp/utils/mutex.h>
#include <freerdp/utils/wait_obj.h>
#include <freerdp/utils/load_plugin.h>

#include "libchanman.h"

#define CHANNEL_MAX_COUNT 30

struct lib_data
{
	PVIRTUALCHANNELENTRY entry; /* the one and only exported function */
	PCHANNEL_INIT_EVENT_FN init_event_proc;
	void* init_handle;
};

struct chan_data
{
	char name[CHANNEL_NAME_LEN + 1];
	int open_handle;
	int options;
	int flags; /* 0 nothing 1 init 2 open */
	PCHANNEL_OPEN_EVENT_FN open_event_proc;
};

typedef struct rdp_init_handle rdpInitHandle;
struct rdp_init_handle
{
	rdpChanMan* chan_man;
};

struct rdp_chan_man
{
	/**
	 * Only the main thread alters these arrays, before any
	 * library thread is allowed in(post_connect is called)
	 * so no need to use mutex locking
	 * After post_connect, each library thread can only access it's
	 * own array items
	 * ie, no two threads can access index 0, ...
	 */
	struct lib_data libs[CHANNEL_MAX_COUNT];
	int num_libs;
	struct chan_data chans[CHANNEL_MAX_COUNT];
	int num_chans;
	rdpInitHandle init_handles[CHANNEL_MAX_COUNT];
	int num_init_handles;

	/* control for entry into MyVirtualChannelInit */
	int can_call_init;
	rdpSettings* settings;

	/* true once freerdp_chanman_post_connect is called */
	int is_connected;

	/* used for locating the chan_man for a given instance */
	rdpInst* inst;

	/* signal for incoming data or event */
	struct wait_obj* signal;

	/* used for sync write */
	freerdp_sem sync_data_sem;
	void* sync_data;
	uint32 sync_data_length;
	void* sync_user_data;
	int sync_index;

	/* used for sync event */
	freerdp_sem event_sem;
	FRDP_EVENT* event;
};

/**
 * The current channel manager reference passes from VirtualChannelEntry to
 * VirtualChannelInit for the pInitHandle.
 */
static rdpChanMan* g_init_chan_man;

/* The list of all channel managers. */
typedef struct rdp_chan_man_list rdpChanManList;
struct rdp_chan_man_list
{
	rdpChanMan* chan_man;
	rdpChanManList* next;
};

static rdpChanManList* g_chan_man_list;

/* To generate unique sequence for all open handles */
static int g_open_handle_sequence;

/* For locking the global resources */
static freerdp_mutex g_mutex_init;
static freerdp_mutex g_mutex_list;

/* returns the chan_man for the open handle passed in */
static rdpChanMan* freerdp_chanman_find_by_open_handle(int open_handle, int* pindex)
{
	rdpChanManList* list;
	rdpChanMan* chan_man;
	int lindex;

	freerdp_mutex_lock(g_mutex_list);
	for (list = g_chan_man_list; list; list = list->next)
	{
		chan_man = list->chan_man;
		for (lindex = 0; lindex < chan_man->num_chans; lindex++)
		{
			if (chan_man->chans[lindex].open_handle == open_handle)
			{
				freerdp_mutex_unlock(g_mutex_list);
				*pindex = lindex;
				return chan_man;
			}
		}
	}
	freerdp_mutex_unlock(g_mutex_list);
	return NULL;
}

/* returns the chan_man for the rdp instance passed in */
static rdpChanMan* freerdp_chanman_find_by_rdp_inst(rdpInst* inst)
{
	rdpChanManList* list;
	rdpChanMan* chan_man;

	freerdp_mutex_lock(g_mutex_list);
	for (list = g_chan_man_list; list; list = list->next)
	{
		chan_man = list->chan_man;
		if (chan_man->inst == inst)
		{
			freerdp_mutex_unlock(g_mutex_list);
			return chan_man;
		}
	}
	freerdp_mutex_unlock(g_mutex_list);
	return NULL;
}

/* returns struct chan_data for the channel name passed in */
static struct chan_data* freerdp_chanman_find_chan_data_by_name(rdpChanMan* chan_man,
	const char* chan_name, int* pindex)
{
	int lindex;
	struct chan_data* lchan_data;

	for (lindex = 0; lindex < chan_man->num_chans; lindex++)
	{
		lchan_data = chan_man->chans + lindex;
		if (strcmp(chan_name, lchan_data->name) == 0)
		{
			if (pindex != 0)
			{
				*pindex = lindex;
			}
			return lchan_data;
		}
	}
	return NULL;
}

/* returns struct rdp_chan for the channel id passed in */
static struct rdp_chan* freerdp_chanman_find_rdp_chan_by_id(rdpChanMan* chan_man,
	rdpSettings* settings, int chan_id, int* pindex)
{
	int lindex;
	int lcount;
	struct rdp_chan* lrdp_chan;

	lcount = settings->num_channels;
	for (lindex = 0; lindex < lcount; lindex++)
	{
		lrdp_chan = settings->channels + lindex;
		if (lrdp_chan->chan_id == chan_id)
		{
			if (pindex != 0)
			{
				*pindex = lindex;
			}
			return lrdp_chan;
		}
	}
	return NULL;
}

/* returns struct rdp_chan for the channel name passed in */
static struct rdp_chan* freerdp_chanman_find_rdp_chan_by_name(rdpChanMan* chan_man,
	rdpSettings* settings, const char* chan_name, int* pindex)
{
	int lindex;
	int lcount;
	struct rdp_chan* lrdp_chan;

	lcount = settings->num_channels;
	for (lindex = 0; lindex < lcount; lindex++)
	{
		lrdp_chan = settings->channels + lindex;
		if (strcmp(chan_name, lrdp_chan->name) == 0)
		{
			if (pindex != 0)
			{
				*pindex = lindex;
			}
			return lrdp_chan;
		}
	}
	return NULL;
}

/**
 * must be called by same thread that calls freerdp_chanman_load_plugin
 * according to MS docs
 * only called from main thread
 */
static uint32 FREERDP_CC MyVirtualChannelInit(void** ppInitHandle, PCHANNEL_DEF pChannel,
	int channelCount, uint32 versionRequested,
	PCHANNEL_INIT_EVENT_FN pChannelInitEventProc)
{
	rdpChanMan* chan_man;
	int index;
	struct lib_data* llib;
	struct chan_data* lchan;
	struct rdp_chan* lrdp_chan;
	PCHANNEL_DEF lchan_def;

	chan_man = g_init_chan_man;
	chan_man->init_handles[chan_man->num_init_handles].chan_man = chan_man;
	*ppInitHandle = &chan_man->init_handles[chan_man->num_init_handles];
	chan_man->num_init_handles++;

	DEBUG_CHANMAN("enter");
	if (!chan_man->can_call_init)
	{
		DEBUG_CHANMAN("error not in entry");
		return CHANNEL_RC_NOT_IN_VIRTUALCHANNELENTRY;
	}
	if (ppInitHandle == 0)
	{
		DEBUG_CHANMAN("error bad pphan");
		return CHANNEL_RC_BAD_INIT_HANDLE;
	}
	if (chan_man->num_chans + channelCount >= CHANNEL_MAX_COUNT)
	{
		DEBUG_CHANMAN("error too many channels");
		return CHANNEL_RC_TOO_MANY_CHANNELS;
	}
	if (pChannel == 0)
	{
		DEBUG_CHANMAN("error bad pchan");
		return CHANNEL_RC_BAD_CHANNEL;
	}
	if (chan_man->is_connected)
	{
		DEBUG_CHANMAN("error already connected");
		return CHANNEL_RC_ALREADY_CONNECTED;
	}
	if (versionRequested != VIRTUAL_CHANNEL_VERSION_WIN2000)
	{
		DEBUG_CHANMAN("warning version");
	}
	for (index = 0; index < channelCount; index++)
	{
		lchan_def = pChannel + index;
		if (freerdp_chanman_find_chan_data_by_name(chan_man, lchan_def->name, 0) != 0)
		{
			DEBUG_CHANMAN("error channel already used");
			return CHANNEL_RC_BAD_CHANNEL;
		}
	}
	llib = chan_man->libs + chan_man->num_libs;
	llib->init_event_proc = pChannelInitEventProc;
	llib->init_handle = *ppInitHandle;
	chan_man->num_libs++;
	for (index = 0; index < channelCount; index++)
	{
		lchan_def = pChannel + index;
		lchan = chan_man->chans + chan_man->num_chans;

		freerdp_mutex_lock(g_mutex_list);
		lchan->open_handle = g_open_handle_sequence++;
		freerdp_mutex_unlock(g_mutex_list);

		lchan->flags = 1; /* init */
		strncpy(lchan->name, lchan_def->name, CHANNEL_NAME_LEN);
		lchan->options = lchan_def->options;
		if (chan_man->settings->num_channels < 16)
		{
			lrdp_chan = chan_man->settings->channels + chan_man->settings->num_channels;
			strncpy(lrdp_chan->name, lchan_def->name, 7);
			lrdp_chan->options = lchan_def->options;
			chan_man->settings->num_channels++;
		}
		else
		{
			DEBUG_CHANMAN("warning more than 16 channels");
		}
		chan_man->num_chans++;
	}
	return CHANNEL_RC_OK;
}

/**
 * can be called from any thread
 * thread safe because no 2 threads can have the same channel name registered
 */
static uint32 FREERDP_CC MyVirtualChannelOpen(void* pInitHandle, uint32* pOpenHandle,
	char* pChannelName, PCHANNEL_OPEN_EVENT_FN pChannelOpenEventProc)
{
	rdpChanMan* chan_man;
	int index;
	struct chan_data* lchan;

	DEBUG_CHANMAN("enter");
	chan_man = ((rdpInitHandle*)pInitHandle)->chan_man;
	if (pOpenHandle == 0)
	{
		DEBUG_CHANMAN("error bad chanhan");
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;
	}
	if (pChannelOpenEventProc == 0)
	{
		DEBUG_CHANMAN("error bad proc");
		return CHANNEL_RC_BAD_PROC;
	}
	if (!chan_man->is_connected)
	{
		DEBUG_CHANMAN("error not connected");
		return CHANNEL_RC_NOT_CONNECTED;
	}
	lchan = freerdp_chanman_find_chan_data_by_name(chan_man, pChannelName, &index);
	if (lchan == 0)
	{
		DEBUG_CHANMAN("error chan name");
		return CHANNEL_RC_UNKNOWN_CHANNEL_NAME;
	}
	if (lchan->flags == 2)
	{
		DEBUG_CHANMAN("error chan already open");
		return CHANNEL_RC_ALREADY_OPEN;
	}

	lchan->flags = 2; /* open */
	lchan->open_event_proc = pChannelOpenEventProc;
	*pOpenHandle = lchan->open_handle;
	return CHANNEL_RC_OK;
}

/**
 * can be called from any thread
 * thread safe because no 2 threads can have the same openHandle
 */
static uint32 FREERDP_CC MyVirtualChannelClose(uint32 openHandle)
{
	rdpChanMan* chan_man;
	struct chan_data* lchan;
	int index;

	DEBUG_CHANMAN("enter");
	chan_man = freerdp_chanman_find_by_open_handle(openHandle, &index);
	if ((chan_man == NULL) || (index < 0) || (index >= CHANNEL_MAX_COUNT))
	{
		DEBUG_CHANMAN("error bad chanhan");
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;
	}
	lchan = chan_man->chans + index;
	if (lchan->flags != 2)
	{
		DEBUG_CHANMAN("error not open");
		return CHANNEL_RC_NOT_OPEN;
	}
	lchan->flags = 0;
	return CHANNEL_RC_OK;
}

/* can be called from any thread */
static uint32 FREERDP_CC MyVirtualChannelWrite(uint32 openHandle, void* pData, uint32 dataLength,
	void* pUserData)
{
	rdpChanMan* chan_man;
	struct chan_data* lchan;
	int index;

	chan_man = freerdp_chanman_find_by_open_handle(openHandle, &index);
	if ((chan_man == NULL) || (index < 0) || (index >= CHANNEL_MAX_COUNT))
	{
		DEBUG_CHANMAN("error bad chanhan");
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;
	}
	if (!chan_man->is_connected)
	{
		DEBUG_CHANMAN("error not connected");
		return CHANNEL_RC_NOT_CONNECTED;
	}
	if (pData == 0)
	{
		DEBUG_CHANMAN("error bad pData");
		return CHANNEL_RC_NULL_DATA;
	}
	if (dataLength == 0)
	{
		DEBUG_CHANMAN("error bad dataLength");
		return CHANNEL_RC_ZERO_LENGTH;
	}
	lchan = chan_man->chans + index;
	if (lchan->flags != 2)
	{
		DEBUG_CHANMAN("error not open");
		return CHANNEL_RC_NOT_OPEN;
	}
	freerdp_sem_wait(chan_man->sync_data_sem); /* lock chan_man->sync* vars */
	if (!chan_man->is_connected)
	{
		freerdp_sem_signal(chan_man->sync_data_sem);
		DEBUG_CHANMAN("error not connected");
		return CHANNEL_RC_NOT_CONNECTED;
	}
	chan_man->sync_data = pData;
	chan_man->sync_data_length = dataLength;
	chan_man->sync_user_data = pUserData;
	chan_man->sync_index = index;
	/* set the event */
	wait_obj_set(chan_man->signal);
	return CHANNEL_RC_OK;
}

static uint32 FREERDP_CC MyVirtualChannelEventPush(uint32 openHandle, FRDP_EVENT* event)
{
	rdpChanMan* chan_man;
	struct chan_data* lchan;
	int index;

	chan_man = freerdp_chanman_find_by_open_handle(openHandle, &index);
	if ((chan_man == NULL) || (index < 0) || (index >= CHANNEL_MAX_COUNT))
	{
		DEBUG_CHANMAN("error bad chanhan");
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;
	}
	if (!chan_man->is_connected)
	{
		DEBUG_CHANMAN("error not connected");
		return CHANNEL_RC_NOT_CONNECTED;
	}
	if (event == NULL)
	{
		DEBUG_CHANMAN("error bad event");
		return CHANNEL_RC_NULL_DATA;
	}
	lchan = chan_man->chans + index;
	if (lchan->flags != 2)
	{
		DEBUG_CHANMAN("error not open");
		return CHANNEL_RC_NOT_OPEN;
	}
	freerdp_sem_wait(chan_man->event_sem); /* lock chan_man->event */
	if (!chan_man->is_connected)
	{
		freerdp_sem_signal(chan_man->event_sem);
		DEBUG_CHANMAN("error not connected");
		return CHANNEL_RC_NOT_CONNECTED;
	}
	chan_man->event = event;
	/* set the event */
	wait_obj_set(chan_man->signal);
	return CHANNEL_RC_OK;
}

/**
 * this is called shortly after the application starts and
 * before any other function in the file
 * called only from main thread
 */
int freerdp_chanman_global_init(void)
{
	g_init_chan_man = NULL;
	g_chan_man_list = NULL;
	g_open_handle_sequence = 1;
	g_mutex_init = freerdp_mutex_new();
	g_mutex_list = freerdp_mutex_new();

	return 0;
}

int freerdp_chanman_global_uninit(void)
{
	while (g_chan_man_list)
		freerdp_chanman_free(g_chan_man_list->chan_man);

	freerdp_mutex_free(g_mutex_init);
	freerdp_mutex_free(g_mutex_list);

	return 0;
}

rdpChanMan* freerdp_chanman_new(void)
{
	rdpChanMan* chan_man;
	rdpChanManList* list;

	chan_man = (rdpChanMan*)xmalloc(sizeof(rdpChanMan));
	memset(chan_man, 0, sizeof(rdpChanMan));

	chan_man->sync_data_sem = freerdp_sem_new(1);
	chan_man->event_sem = freerdp_sem_new(1);
	chan_man->signal = wait_obj_new();

	/* Add it to the global list */
	list = (rdpChanManList*)xmalloc(sizeof(rdpChanManList));
	list->chan_man = chan_man;

	freerdp_mutex_lock(g_mutex_list);
	list->next = g_chan_man_list;
	g_chan_man_list = list;
	freerdp_mutex_unlock(g_mutex_list);

	return chan_man;
}

void freerdp_chanman_free(rdpChanMan * chan_man)
{
	rdpChanManList * list;
	rdpChanManList * prev;

	freerdp_sem_free(chan_man->sync_data_sem);
	freerdp_sem_free(chan_man->event_sem);
	wait_obj_free(chan_man->signal);

	/* Remove from global list */
	freerdp_mutex_lock(g_mutex_list);
	for (prev = NULL, list = g_chan_man_list; list; prev = list, list = list->next)
	{
		if (list->chan_man == chan_man)
			break;
	}
	if (list)
	{
		if (prev)
			prev->next = list->next;
		else
			g_chan_man_list = list->next;
		xfree(list);
	}
	freerdp_mutex_unlock(g_mutex_list);

	xfree(chan_man);
}

/**
 * this is called when processing the command line parameters
 * called only from main thread
 */
int freerdp_chanman_load_plugin(rdpChanMan* chan_man, rdpSettings* settings,
	const char* name, void* data)
{
	struct lib_data* lib;
	CHANNEL_ENTRY_POINTS_EX ep;
	int ok;

	DEBUG_CHANMAN("%s", name);
	if (chan_man->num_libs + 1 >= CHANNEL_MAX_COUNT)
	{
		DEBUG_CHANMAN("too many channels");
		return 1;
	}
	lib = chan_man->libs + chan_man->num_libs;
	lib->entry = (PVIRTUALCHANNELENTRY)freerdp_load_plugin(name, CHANNEL_EXPORT_FUNC_NAME);
	if (lib->entry == NULL)
	{
		DEBUG_CHANMAN("failed to find export function");
		return 1;
	}
	ep.cbSize = sizeof(ep);
	ep.protocolVersion = VIRTUAL_CHANNEL_VERSION_WIN2000;
	ep.pVirtualChannelInit = MyVirtualChannelInit;
	ep.pVirtualChannelOpen = MyVirtualChannelOpen;
	ep.pVirtualChannelClose = MyVirtualChannelClose;
	ep.pVirtualChannelWrite = MyVirtualChannelWrite;
	ep.pExtendedData = data;
	ep.pVirtualChannelEventPush = MyVirtualChannelEventPush;

	/* enable MyVirtualChannelInit */
	chan_man->can_call_init = 1;
	chan_man->settings = settings;

	freerdp_mutex_lock(g_mutex_init);
	g_init_chan_man = chan_man;
	ok = lib->entry((PCHANNEL_ENTRY_POINTS)&ep);
	g_init_chan_man = NULL;
	freerdp_mutex_unlock(g_mutex_init);

	/* disable MyVirtualChannelInit */
	chan_man->settings = 0;
	chan_man->can_call_init = 0;
	if (!ok)
	{
		DEBUG_CHANMAN("export function call failed");
		return 1;
	}
	return 0;
}

/**
 * go through and inform all the libraries that we are initialized
 * called only from main thread
 */
int freerdp_chanman_pre_connect(rdpChanMan* chan_man, rdpInst* inst)
{
	int index;
	struct lib_data* llib;
	CHANNEL_DEF lchannel_def;
	void* dummy;

	DEBUG_CHANMAN("enter");
	chan_man->inst = inst;

	/**
         * If rdpsnd is registered but not rdpdr, it's necessary to register a fake
	 * rdpdr channel to make sound work. This is a workaround for Window 7 and
	 * Windows 2008
	 */
	if (freerdp_chanman_find_chan_data_by_name(chan_man, "rdpsnd", 0) != 0 &&
		freerdp_chanman_find_chan_data_by_name(chan_man, "rdpdr", 0) == 0)
	{
		lchannel_def.options = CHANNEL_OPTION_INITIALIZED |
			CHANNEL_OPTION_ENCRYPT_RDP;
		strcpy(lchannel_def.name, "rdpdr");
		chan_man->can_call_init = 1;
		chan_man->settings = inst->settings;
		freerdp_mutex_lock(g_mutex_init);
		g_init_chan_man = chan_man;
		MyVirtualChannelInit(&dummy, &lchannel_def, 1,
			VIRTUAL_CHANNEL_VERSION_WIN2000, 0);
		g_init_chan_man = NULL;
		freerdp_mutex_unlock(g_mutex_init);
		chan_man->can_call_init = 0;
		chan_man->settings = 0;
		DEBUG_CHANMAN("registered fake rdpdr for rdpsnd.");
	}

	for (index = 0; index < chan_man->num_libs; index++)
	{
		llib = chan_man->libs + index;
		if (llib->init_event_proc != 0)
		{
			llib->init_event_proc(llib->init_handle, CHANNEL_EVENT_INITIALIZED,
				0, 0);
		}
	}
	return 0;
}

/**
 * go through and inform all the libraries that we are connected
 * this will tell the libraries that its ok to call MyVirtualChannelOpen
 * called only from main thread
 */
int freerdp_chanman_post_connect(rdpChanMan* chan_man, rdpInst* inst)
{
	int index;
	struct lib_data* llib;
	char* hostname;
	int hostname_len;

	chan_man->is_connected = 1;
	hostname = inst->settings->hostname;
	hostname_len = strlen(hostname);
	DEBUG_CHANMAN("hostname [%s] chan_man->num_libs [%d]",
		hostname, chan_man->num_libs);
	for (index = 0; index < chan_man->num_libs; index++)
	{
		llib = chan_man->libs + index;
		if (llib->init_event_proc != 0)
		{
			llib->init_event_proc(llib->init_handle, CHANNEL_EVENT_CONNECTED,
				hostname, hostname_len);
		}
	}
	return 0;
}

/**
 * data comming from the server to the client
 * called only from main thread
 */
int freerdp_chanman_data(rdpInst* inst, int chan_id, char* data, int data_size,
	int flags, int total_size)
{
	rdpChanMan* chan_man;
	struct rdp_chan* lrdp_chan;
	struct chan_data* lchan_data;
	int index;

	chan_man = freerdp_chanman_find_by_rdp_inst(inst);
	if (chan_man == 0)
	{
		DEBUG_CHANMAN("could not find channel manager");
		return 1;
	}

	lrdp_chan = freerdp_chanman_find_rdp_chan_by_id(chan_man, inst->settings,
		chan_id, &index);
	if (lrdp_chan == 0)
	{
		DEBUG_CHANMAN("could not find channel id");
		return 1;
	}
	lchan_data = freerdp_chanman_find_chan_data_by_name(chan_man, lrdp_chan->name,
		&index);
	if (lchan_data == 0)
	{
		DEBUG_CHANMAN("could not find channel name");
		return 1;
	}
	if (lchan_data->open_event_proc != 0)
	{
		lchan_data->open_event_proc(lchan_data->open_handle,
			CHANNEL_EVENT_DATA_RECEIVED,
			data, data_size, total_size, flags);
	}
	return 0;
}

/**
 * Send a plugin-defined event to the plugin.
 * called only from main thread
 * @param chan_man the channel manager instance
 * @param name the static virtual channel name, such as 'cliprdr'
 * @param event an event object created by freerdp_event_new()
 */
FREERDP_API int freerdp_chanman_send_event(rdpChanMan* chan_man, const char* name, FRDP_EVENT* event)
{
	struct chan_data* lchan_data;
	int index;

	lchan_data = freerdp_chanman_find_chan_data_by_name(chan_man, name, &index);
	if (lchan_data == NULL)
	{
		DEBUG_CHANMAN("could not find channel name %s", name);
		return 1;
	}
	if (lchan_data->open_event_proc != NULL)
	{
		lchan_data->open_event_proc(lchan_data->open_handle,
			CHANNEL_EVENT_USER,
			event, sizeof(FRDP_EVENT), sizeof(FRDP_EVENT), 0);
	}
	return 0;
}

/**
 * called only from main thread
 */
static void freerdp_chanman_process_sync(rdpChanMan* chan_man, rdpInst* inst)
{
	void* ldata;
	uint32 ldata_len;
	void* luser_data;
	int lindex;
	struct chan_data* lchan_data;
	struct rdp_chan* lrdp_chan;

	if (chan_man->sync_data == NULL)
		return;

	ldata = chan_man->sync_data;
	ldata_len = chan_man->sync_data_length;
	luser_data = chan_man->sync_user_data;
	lindex = chan_man->sync_index;
	chan_man->sync_data = NULL;
	chan_man->sync_data_length = 0;
	chan_man->sync_user_data = NULL;
	chan_man->sync_index = 0;
	freerdp_sem_signal(chan_man->sync_data_sem); /* release chan_man->sync* vars */
	lchan_data = chan_man->chans + lindex;
	lrdp_chan = freerdp_chanman_find_rdp_chan_by_name(chan_man, inst->settings,
		lchan_data->name, &lindex);
	if (lrdp_chan != 0)
	{
		inst->rdp_channel_data(inst, lrdp_chan->chan_id, ldata, ldata_len);
	}
	if (lchan_data->open_event_proc != 0)
	{
		lchan_data->open_event_proc(lchan_data->open_handle,
			CHANNEL_EVENT_WRITE_COMPLETE,
			luser_data, sizeof(void *), sizeof(void *), 0);
	}
}

/**
 * called only from main thread
 */
int freerdp_chanman_get_fds(rdpChanMan* chan_man, rdpInst* inst, void** read_fds,
	int* read_count, void** write_fds, int* write_count)
{
	wait_obj_get_fds(chan_man->signal, read_fds, read_count);
	return 0;
}

/**
 * called only from main thread
 */
int freerdp_chanman_check_fds(rdpChanMan * chan_man, rdpInst * inst)
{
	if (wait_obj_is_set(chan_man->signal))
	{
		wait_obj_clear(chan_man->signal);
		freerdp_chanman_process_sync(chan_man, inst);
	}
	return 0;
}

FRDP_EVENT* freerdp_chanman_pop_event(rdpChanMan* chan_man)
{
	FRDP_EVENT* event;

	if (chan_man->event == NULL)
		return NULL;
	event = chan_man->event;
	chan_man->event = NULL;
	freerdp_sem_signal(chan_man->event_sem); /* release chan_man->event */
	return event;
}

void freerdp_chanman_close(rdpChanMan* chan_man, rdpInst* inst)
{
	int index;
	struct lib_data* llib;

	DEBUG_CHANMAN("closing");
	chan_man->is_connected = 0;
	freerdp_chanman_check_fds(chan_man, inst);
	/* tell all libraries we are shutting down */
	for (index = 0; index < chan_man->num_libs; index++)
	{
		llib = chan_man->libs + index;
		if (llib->init_event_proc != 0)
		{
			llib->init_event_proc(llib->init_handle, CHANNEL_EVENT_TERMINATED,
				0, 0);
		}
	}
}
