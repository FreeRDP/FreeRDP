/**
 * FreeRDP: A Remote Desktop Protocol Client
 * FreeRDP Mac OS X Server
 *
 * Copyright 2012 Corey Clayton <can.of.tuna@gmail.com>
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

#include <stdlib.h>
#include <errno.h>

#include "mf_info.h"
#include "mf_mountain_lion.h"

#define MF_INFO_DEFAULT_FPS 30
#define MF_INFO_MAXPEERS    32

static mfInfo* mfInfoInstance = NULL;

int mf_info_lock(mfInfo* mfi)
{
	int status = pthread_mutex_lock(&mfi->mutex);
	
	switch (status)
	{
		case 0:
			return TRUE;
			break;
			
		default:
			return -1;
			break;
	}
	
	return 1;
}

int mf_info_try_lock(mfInfo* mfi, UINT32 ms)
{
	int status = pthread_mutex_trylock(&mfi->mutex);
	
	switch (status)
	{
		case 0:
			return TRUE;
			break;
			
		case EBUSY:
			return FALSE;
			break;
			
		default:
			return -1;
			break;
	}
	
	return 1;
}

int mf_info_unlock(mfInfo* mfi)
{
	int status = pthread_mutex_unlock(&mfi->mutex);
	
	switch (status)
	{
		case 0:
			return TRUE;
			break;
			
		default:
			return -1;
			break;
	}
	
	return 1;
}

mfInfo* mf_info_init()
{
	mfInfo* mfi;
	
	mfi = (mfInfo*) calloc(1, sizeof(mfInfo));
	
	if (mfi != NULL)
	{
		pthread_mutex_init(&mfi->mutex, NULL);
		
		mfi->peers = (freerdp_peer**) calloc(MF_INFO_MAXPEERS, sizeof(freerdp_peer*));
		if (!mfi->peers)
		{
			free(mfi);
			return NULL;
		}
		
		mfi->framesPerSecond = MF_INFO_DEFAULT_FPS;
		mfi->input_disabled = FALSE;
	}
	
	return mfi;
}

mfInfo* mf_info_get_instance()
{
	if (mfInfoInstance == NULL)
		mfInfoInstance = mf_info_init();
	
	return mfInfoInstance;
}

void mf_info_peer_register(mfInfo* mfi, mfPeerContext* context)
{
	if (mf_info_lock(mfi) > 0)
	{
		int i;
		int peerId;
		
		if (mfi->peerCount == MF_INFO_MAXPEERS)
		{
			mf_info_unlock(mfi);
			return;
		}
		
		context->info = mfi;
		
		if (mfi->peerCount == 0)
		{
			mf_mlion_display_info(&mfi->servscreen_width, &mfi->servscreen_height, &mfi->scale);
			mf_mlion_screen_updates_init();
			mf_mlion_start_getting_screen_updates();
		}
		
		peerId = 0;

		for(i=0; i<MF_INFO_MAXPEERS; ++i)
		{
			//empty index will be our peer id
			if (mfi->peers[i] == NULL)
			{
				peerId = i;
				break;
			}
		}
		
		mfi->peers[peerId] = ((rdpContext*) context)->peer;
		mfi->peers[peerId]->pId = peerId;
		mfi->peerCount++;
		
		mf_info_unlock(mfi);
	}
}

void mf_info_peer_unregister(mfInfo* mfi, mfPeerContext* context)
{
	if (mf_info_lock(mfi) > 0)
	{
		int peerId;
		
		peerId = ((rdpContext*) context)->peer->pId;
		mfi->peers[peerId] = NULL;
		mfi->peerCount--;
		
		if (mfi->peerCount == 0)
			mf_mlion_stop_getting_screen_updates();

		mf_info_unlock(mfi);
	}
}

BOOL mf_info_have_updates(mfInfo* mfi)
{
	if(mfi->framesWaiting == 0)
		return FALSE;
	
	return TRUE;
}

void mf_info_update_changes(mfInfo* mfi)
{

}

void mf_info_find_invalid_region(mfInfo* mfi)
{
	mf_mlion_get_dirty_region(&mfi->invalid);
}

void mf_info_clear_invalid_region(mfInfo* mfi)
{
	mf_mlion_clear_dirty_region();
	mfi->invalid.height = 0;
	mfi->invalid.width = 0;
}

void mf_info_invalidate_full_screen(mfInfo* mfi)
{
	mfi->invalid.x = 0;
	mfi->invalid.y = 0;
	mfi->invalid.height = mfi->servscreen_height;
	mfi->invalid.height = mfi->servscreen_width;
}

BOOL mf_info_have_invalid_region(mfInfo* mfi)
{
	if (mfi->invalid.width * mfi->invalid.height == 0)
		return FALSE;

	return TRUE;
}

void mf_info_getScreenData(mfInfo* mfi, long* width, long* height, BYTE** pBits, int* pitch)
{
	*width = mfi->invalid.width / mfi->scale;
	*height = mfi->invalid.height / mfi->scale;
	*pitch = mfi->servscreen_width * mfi->scale * 4;
	
	mf_mlion_get_pixelData(mfi->invalid.x / mfi->scale, mfi->invalid.y / mfi->scale, *width, *height, pBits);
	
	*pBits = *pBits + (mfi->invalid.x * 4) + (*pitch * mfi->invalid.y);
	
}
