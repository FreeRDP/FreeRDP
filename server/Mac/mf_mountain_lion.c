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
#include "CoreVideo/CoreVideo.h"

#include "mf_mountain_lion.h"

dispatch_semaphore_t region_sem;
dispatch_queue_t screen_update_q;
CGDisplayStreamRef stream;

CGDisplayStreamUpdateRef lastUpdate = NULL;

CVPixelBufferRef pxbuffer = NULL;
void *baseAddress = NULL;

CGContextRef bitmapcontext = NULL;

CGImageRef image = NULL;

void (^streamHandler)(CGDisplayStreamFrameStatus, uint64_t, IOSurfaceRef, CGDisplayStreamUpdateRef) =  ^(CGDisplayStreamFrameStatus status, uint64_t displayTime, IOSurfaceRef frameSurface, CGDisplayStreamUpdateRef updateRef)
{
    
    dispatch_semaphore_wait(region_sem, DISPATCH_TIME_FOREVER);
    
    if (lastUpdate == NULL)
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

int mf_mlion_screen_updates_init()
{
    printf("mf_mlion_screen_updates_init()\n");
    CGDirectDisplayID display_id;
    
    display_id = CGMainDisplayID();
    
    screen_update_q = dispatch_queue_create("mfreerdp.server.screenUpdate", NULL);
    
    region_sem = dispatch_semaphore_create(1);
 
    CGDisplayModeRef mode = CGDisplayCopyDisplayMode(display_id);
    
    size_t pixelWidth = CGDisplayModeGetPixelWidth(mode);
    size_t pixelHeight = CGDisplayModeGetPixelHeight(mode);
    
    CGDisplayModeRelease(mode);

    stream = CGDisplayStreamCreateWithDispatchQueue(display_id,
                                                    pixelWidth,
                                                    pixelHeight,
                                                    'BGRA',
                                                    NULL,
                                                    screen_update_q,
                                                    streamHandler);
    
    
    CFDictionaryRef opts;
    
    long ImageCompatibility;
    long BitmapContextCompatibility;
    
    void * keys[3];
    keys[0] = (void *) kCVPixelBufferCGImageCompatibilityKey;
    keys[1] = (void *) kCVPixelBufferCGBitmapContextCompatibilityKey;
    keys[2] = NULL;
    
    void * values[3];
    values[0] = (void *) &ImageCompatibility;
    values[1] = (void *) &BitmapContextCompatibility;
    values[2] = NULL;
    
    opts = CFDictionaryCreate(kCFAllocatorDefault, (const void **) keys, (const void **) values, 2, NULL, NULL);
    
    if (opts == NULL)
    {
        printf("failed to create dictionary\n");
        //return 1;
    }
    
    CVReturn status = CVPixelBufferCreate(kCFAllocatorDefault, pixelWidth,
                                          pixelHeight,  kCVPixelFormatType_32ARGB, opts,
                                          &pxbuffer);
    
    if (status != kCVReturnSuccess)
    {
        printf("Failed to create pixel buffer! \n");
        //return 1;
    }
    
    CFRelease(opts);
    
    CVPixelBufferLockBaseAddress(pxbuffer, 0);
    baseAddress = CVPixelBufferGetBaseAddress(pxbuffer);
    
    CGColorSpaceRef rgbColorSpace = CGColorSpaceCreateDeviceRGB();

    bitmapcontext = CGBitmapContextCreate(baseAddress,
                                                 pixelWidth,
                                                 pixelHeight, 8, 4*pixelWidth, rgbColorSpace,
                                                 kCGImageAlphaNoneSkipLast);
    
    if (bitmapcontext == NULL) {
        printf("context = null!!!\n\n\n");
    }
    CGColorSpaceRelease(rgbColorSpace);
    
    
    return 0;
    
}

int mf_mlion_start_getting_screen_updates()
{
    CGDisplayStreamStart(stream);
    
    return 0;

}
int mf_mlion_stop_getting_screen_updates()
{
    CGDisplayStreamStop(stream);
    
    return 0;
}

int mf_mlion_get_dirty_region(RFX_RECT* invalid)
{
    size_t num_rects;
    CGRect dirtyRegion;
    
    //it may be faster to copy the cgrect and then convert....
    
    dispatch_semaphore_wait(region_sem, DISPATCH_TIME_FOREVER);

    const CGRect * rects = CGDisplayStreamUpdateGetRects(lastUpdate, kCGDisplayStreamUpdateDirtyRects, &num_rects);
    
    printf("\trectangles: %zd\n", num_rects);
    
    if (num_rects == 0) {
        dispatch_semaphore_signal(region_sem);
        return 0;
    }
    
    dirtyRegion = *rects;
    for (size_t i = 0; i < num_rects; i++)
    {        
        dirtyRegion = CGRectUnion(dirtyRegion, *(rects+i));
    }
    
    invalid->x = dirtyRegion.origin.x;
    invalid->y = dirtyRegion.origin.y;
    invalid->height = dirtyRegion.size.height;
    invalid->width = dirtyRegion.size.width;
    
    CFRelease(lastUpdate);
    
    lastUpdate = NULL;

    dispatch_semaphore_signal(region_sem);
    
    return 0;
}

int mf_mlion_clear_dirty_region()
{
   /* dispatch_semaphore_wait(region_sem, DISPATCH_TIME_FOREVER);

    clean = TRUE;
    dirtyRegion.size.width = 0;
    dirtyRegion.size.height = 0;
    
    dispatch_semaphore_signal(region_sem);
    */
    return 0;
}

int mf_mlion_get_pixelData(long x, long y, long width, long height, BYTE** pxData)
{
    if (image != NULL) {
        CGImageRelease(image);
    }
    image = CGDisplayCreateImageForRect(
                                                   kCGDirectMainDisplay,
                                                   CGRectMake(x, y, width, height) );
    
    CGContextDrawImage(
                       bitmapcontext,
                       CGRectMake(0, 1800 - height, width, height),
                       image);
    
    *pxData = baseAddress;
    
    return 0;
}

