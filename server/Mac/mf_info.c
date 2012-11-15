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
//#include "mf_update.h"

static mfInfo* mfInfoInstance = NULL;
//static int _IDcount = 0;

int mf_info_lock(mfInfo* mfi)
{
    
    int status = pthread_mutex_lock(&mfi->mutex);
    
    switch (status) {
        case 0:
            return TRUE;
            break;
            
        default:
            printf("mf_info_lock failed with %#X\n", status);
            return -1;
            break;
    }
    
}

int mf_info_try_lock(mfInfo* mfi, UINT32 ms)
{
    
    int status = pthread_mutex_trylock(&mfi->mutex);
    
    switch (status) {
        case 0:
            return TRUE;
            break;
            
        case EBUSY:
            return FALSE;
            break;
            
        default:
            printf("mf_info_try_lock failed with %#X\n", status);
            return -1;
            break;
    }
    
}

int mf_info_unlock(mfInfo* mfi)
{
    int status = pthread_mutex_unlock(&mfi->mutex);
    
    switch (status) {
        case 0:
            return TRUE;
            break;
            
        default:
            printf("mf_info_unlock failed with %#X\n", status);
            return -1;
            break;
    }
    
}

mfInfo* mf_info_init()
{
	mfInfo* mfi;

	mfi = (mfInfo*) malloc(sizeof(mfInfo));
    memset(mfi, 0, sizeof(mfInfo));
	

	if (mfi != NULL)
	{
/*		HKEY hKey;
		LONG status;
		DWORD dwType;
		DWORD dwSize;
		DWORD dwValue;
*/
        
        int mutexInitStatus = pthread_mutex_init(&mfi->mutex, NULL);
        
		if (mutexInitStatus != 0)
		{
			printf(_T("CreateMutex error: %#X\n"), mutexInitStatus);
		}

		mfi->peers = (freerdp_peer**) malloc(sizeof(freerdp_peer*) * MF_INFO_MAXPEERS);
		memset(mfi->peers, 0, sizeof(freerdp_peer*) * MF_INFO_MAXPEERS);

		//Set FPS
		mfi->framesPerSecond = MF_INFO_DEFAULT_FPS;

		/*status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\FreeRDP\\Server"), 0, KEY_READ | KEY_WOW64_64KEY, &hKey);
		if (status == ERROR_SUCCESS)
		{
			if (RegQueryValueEx(hKey, _T("FramesPerSecond"), NULL, &dwType, (BYTE*) &dwValue, &dwSize) == ERROR_SUCCESS)
				mfi->framesPerSecond = dwValue;		
		}
		RegCloseKey(hKey);*/

		//Set input toggle
		mfi->input_disabled = FALSE;

		/*status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\FreeRDP\\Server"), 0, KEY_READ | KEY_WOW64_64KEY, &hKey);
		if (status == ERROR_SUCCESS)
		{
			if (RegQueryValueEx(hKey, _T("DisableInput"), NULL, &dwType, (BYTE*) &dwValue, &dwSize) == ERROR_SUCCESS)
			{
				if (dwValue != 0)
					mfi->input_disabled = TRUE;
			}
		}
		RegCloseKey(hKey);*/

		
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
            printf("TODO: socketClose on OS X\n");
			//context->socketClose = TRUE;
			mf_info_unlock(mfi);
			return;
		}

		context->info = mfi;
		
		//get the offset of the top left corner of selected screen
		//EnumDisplayMonitors(NULL, NULL, mf_info_monEnumCB, 0);
		//_IDcount = 0;

        //initialize screen capture
		if (mfi->peerCount == 0)
			//mf_dxgi_init(mfi);

		//look trhough the array of peers until an empty slot
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
		printf("Registering Peer: id=%d #=%d\n", peerId, mfi->peerCount);

		mf_info_unlock(mfi);

		//mfreerdp_server_peer_callback_event(peerId, MF_SRV_CALLBACK_EVENT_CONNECT);
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

		printf("Unregistering Peer: id=%d, #=%d\n", peerId, mfi->peerCount);

        //screen capture cleanup
		if (mfi->peerCount == 0)
			//mf_dxgi_cleanup(mfi);

		mf_info_unlock(mfi);

		//mfreerdp_server_peer_callback_event(peerId, MF_SRV_CALLBACK_EVENT_DISCONNECT);
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
/*#ifdef WITH_WIN8
	mf_dxgi_nextFrame(mfi, mfi->framesPerSecond * 1000);
#else
	GETCHANGESBUF* buf;

	buf = (GETCHANGESBUF*) mfi->changeBuffer;
	mfi->nextUpdate = buf->buffer->counter;
#endif*/
}

void mf_info_find_invalid_region(mfInfo* mfi)
{
    //fixme
}

void mf_info_clear_invalid_region(mfInfo* mfi)
{
	//mfi->lastUpdate = mfi->nextUpdate;
	//SetRectEmpty(&mfi->invalid);
}

void mf_info_invalidate_full_screen(mfInfo* mfi)
{
	//SetRect(&mfi->invalid, 0, 0, mfi->servscreen_width, mfi->servscreen_height);
}

BOOL mf_info_have_invalid_region(mfInfo* mfi)
{
	//return IsRectEmpty(&mfi->invalid);
}

void mf_info_getScreenData(mfInfo* mfi, long* width, long* height, BYTE** pBits, int* pitch)
{
	/*
    *width = (mfi->invalid.right - mfi->invalid.left);
	*height = (mfi->invalid.bottom - mfi->invalid.top);

#ifdef WITH_WIN8
	mf_dxgi_getPixelData(mfi, pBits, pitch, &mfi->invalid);
#else
	{
		long offset;
		GETCHANGESBUF* changes;
		changes = (GETCHANGESBUF*) mfi->changeBuffer;

		*width += 1;
		*height += 1;

		offset = (4 * mfi->invalid.left) + (mfi->invalid.top * mfi->virtscreen_width * 4);
		*pBits = ((BYTE*) (changes->Userbuffer)) + offset;
		*pitch = mfi->virtscreen_width * 4;
	}
#endif
     */
}

/*
BOOL CALLBACK mf_info_monEnumCB(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
	mfInfo * mfi;

	mfi = mf_info_get_instance();

	if(_IDcount == mfi->screenID)
	{
		mfi->servscreen_xoffset = lprcMonitor->left;
		mfi->servscreen_yoffset = lprcMonitor->top;
	}
	
	_IDcount++;	

	return TRUE;
}
*/
