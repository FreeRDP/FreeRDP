/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * OS X Server Event Handling
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

#include <dispatch/dispatch.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreVideo/CoreVideo.h>
#include <IOKit/IOKitLib.h>
#include <IOSurface/IOSurface.h>

#include "mf_mountain_lion.h"

dispatch_semaphore_t region_sem;
dispatch_semaphore_t data_sem;
dispatch_queue_t screen_update_q;
CGDisplayStreamRef stream;

CGDisplayStreamUpdateRef lastUpdate = NULL;

BYTE* localBuf = NULL;

BOOL ready = FALSE;

void (^streamHandler)(CGDisplayStreamFrameStatus, uint64_t, IOSurfaceRef, CGDisplayStreamUpdateRef) =  ^(CGDisplayStreamFrameStatus status, uint64_t displayTime, IOSurfaceRef frameSurface, CGDisplayStreamUpdateRef updateRef)
{
	
	dispatch_semaphore_wait(region_sem, DISPATCH_TIME_FOREVER);
	
	//may need to move this down
	if(ready == TRUE)
	{
		
		RFX_RECT rect;
		unsigned long offset_beg;
		unsigned long stride;
		int i;
		
		rect.x = 0;
		rect.y = 0;
		rect.width = 0;
		rect.height = 0;
		mf_mlion_peek_dirty_region(&rect);
		
		
		//lock surface
		IOSurfaceLock(frameSurface, kIOSurfaceLockReadOnly, NULL);
		//get pointer
		void* baseAddress = IOSurfaceGetBaseAddress(frameSurface);
		//copy region
		
		stride = IOSurfaceGetBytesPerRow(frameSurface);
		//memcpy(localBuf, baseAddress + offset_beg, surflen);
		for(i = 0; i < rect.height; i++)
		{
			offset_beg = (stride * (rect.y + i) + (rect.x * 4));
			memcpy(localBuf + offset_beg,
			       baseAddress + offset_beg,
			       rect.width * 4);
		}
		
		//unlock surface
		IOSurfaceUnlock(frameSurface, kIOSurfaceLockReadOnly, NULL);
		
		ready = FALSE;
		dispatch_semaphore_signal(data_sem);
	}
	
	if (status != kCGDisplayStreamFrameStatusFrameComplete)
	{
		switch(status)
		{
			case kCGDisplayStreamFrameStatusFrameIdle:
				break;
				
			case kCGDisplayStreamFrameStatusStopped:
				break;
				
			case kCGDisplayStreamFrameStatusFrameBlank:
				break;
				
			default:
				break;
				
		}
	}
	else if (lastUpdate == NULL)
	{
		CFRetain(updateRef);
		lastUpdate = updateRef;
	}
	else
	{
		CGDisplayStreamUpdateRef tmpRef;
		tmpRef = lastUpdate;
		lastUpdate = CGDisplayStreamUpdateCreateMergedUpdate(tmpRef, updateRef);
		CFRelease(tmpRef);
	}
	
	dispatch_semaphore_signal(region_sem);
};

int mf_mlion_display_info(UINT32* disp_width, UINT32* disp_height, UINT32* scale)
{
	CGDirectDisplayID display_id;
	
	display_id = CGMainDisplayID();
	
	CGDisplayModeRef mode = CGDisplayCopyDisplayMode(display_id);
	
	size_t pixelWidth = CGDisplayModeGetPixelWidth(mode);
	//size_t pixelHeight = CGDisplayModeGetPixelHeight(mode);
	
	size_t wide = CGDisplayPixelsWide(display_id);
	size_t high = CGDisplayPixelsHigh(display_id);
	
	CGDisplayModeRelease(mode);
	
	*disp_width = wide;//pixelWidth;
	*disp_height = high;//pixelHeight;
	*scale = pixelWidth / wide;
	
	return 0;
}

int mf_mlion_screen_updates_init()
{
	CGDirectDisplayID display_id;
	
	display_id = CGMainDisplayID();
	
	screen_update_q = dispatch_queue_create("mfreerdp.server.screenUpdate", NULL);
	
	region_sem = dispatch_semaphore_create(1);
	data_sem = dispatch_semaphore_create(1);
	
	UINT32 pixelWidth;
	UINT32 pixelHeight;
	UINT32 scale;
	
	mf_mlion_display_info(&pixelWidth, &pixelHeight, &scale);
	
	localBuf = malloc(pixelWidth * pixelHeight * 4);
	if (!localBuf)
		return -1;
	
	CFDictionaryRef opts;
	
	void * keys[2];
	void * values[2];
	
	keys[0] = (void *) kCGDisplayStreamShowCursor;
	values[0] = (void *) kCFBooleanFalse;
	
	opts = CFDictionaryCreate(kCFAllocatorDefault, (const void **) keys, (const void **) values, 1, NULL, NULL);
	
	
	stream = CGDisplayStreamCreateWithDispatchQueue(display_id,
							pixelWidth,
							pixelHeight,
							'BGRA',
							opts,
							screen_update_q,
							streamHandler);
	
	CFRelease(opts);
	
	return 0;
	
}

int mf_mlion_start_getting_screen_updates()
{
	CGError err;
	
	err = CGDisplayStreamStart(stream);
	
	if (err != kCGErrorSuccess)
	{
		return 1;
	}
	
	return 0;
	
}
int mf_mlion_stop_getting_screen_updates()
{
	CGError err;
	
	err = CGDisplayStreamStop(stream);
	
	if (err != kCGErrorSuccess)
	{
		return 1;
	}
	
	return 0;
}

int mf_mlion_get_dirty_region(RFX_RECT* invalid)
{
	dispatch_semaphore_wait(region_sem, DISPATCH_TIME_FOREVER);
	
	if (lastUpdate != NULL)
	{
		mf_mlion_peek_dirty_region(invalid);
	}
	
	dispatch_semaphore_signal(region_sem);
	
	return 0;
}

int mf_mlion_peek_dirty_region(RFX_RECT* invalid)
{
	size_t num_rects, i;
	CGRect dirtyRegion;
	
	const CGRect * rects = CGDisplayStreamUpdateGetRects(lastUpdate, kCGDisplayStreamUpdateDirtyRects, &num_rects);
		
	if (num_rects == 0) {
		return 0;
	}
	
	dirtyRegion = *rects;
	for (i = 0; i < num_rects; i++)
	{
		dirtyRegion = CGRectUnion(dirtyRegion, *(rects+i));
	}
	
	invalid->x = dirtyRegion.origin.x;
	invalid->y = dirtyRegion.origin.y;
	invalid->height = dirtyRegion.size.height;
	invalid->width = dirtyRegion.size.width;
	
	return 0;
}

int mf_mlion_clear_dirty_region()
{
	dispatch_semaphore_wait(region_sem, DISPATCH_TIME_FOREVER);
	
	CFRelease(lastUpdate);
	lastUpdate = NULL;
	
	dispatch_semaphore_signal(region_sem);
	
	
	return 0;
}

int mf_mlion_get_pixelData(long x, long y, long width, long height, BYTE** pxData)
{
	dispatch_semaphore_wait(region_sem, DISPATCH_TIME_FOREVER);
	ready = TRUE;
	dispatch_semaphore_wait(data_sem, DISPATCH_TIME_FOREVER);
	dispatch_semaphore_signal(region_sem);
	
	//this second wait allows us to block until data is copied... more on this later
	dispatch_semaphore_wait(data_sem, DISPATCH_TIME_FOREVER);
	*pxData = localBuf;
	dispatch_semaphore_signal(data_sem);
	
	return 0;
}
