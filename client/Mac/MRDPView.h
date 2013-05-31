/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * MacFreeRDP
 *
 * Copyright 2012 Thomas Goddard
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

#import <Cocoa/Cocoa.h>

#ifdef HAVE_RAIL
#import "MRDPWindow.h"
#endif

/*
 #import "freerdp/freerdp.h"
#import "freerdp/types.h"
#import "freerdp/channels/channels.h"
#import "freerdp/gdi/gdi.h"
#import "freerdp/graphics.h"
#import "freerdp/utils/event.h"
#import "freerdp/client/cliprdr.h"
#import "freerdp/client/file.h"
#import "freerdp/client/cmdline.h"
#import "freerdp/rail/rail.h"
#import "freerdp/rail.h"
#import "freerdp/utils/rail.h"
*/



@interface MRDPView : NSView
{
	CFRunLoopSourceRef run_loop_src;
	CFRunLoopSourceRef run_loop_src_channels;
	NSBitmapImageRep* bmiRep;
	NSMutableArray* cursors;
	NSMutableArray* windows;
	NSTimer* pasteboard_timer;
	NSRect prevWinPosition;
	int titleBarHeight;
	void* rdp_instance;
	void* rdp_context;
	CGContextRef bitmap_context;
	char* pixel_data;
	int width;
	int height;
	int argc;
	char** argv;

#ifdef HAVE_RAIL
	// RemoteApp
	MRDPWindow* currentWindow;
#endif
    
	NSPoint savedDragLocation;
	BOOL mouseInClientArea;
	BOOL isRemoteApp;
	BOOL firstCreateWindow;
	BOOL isMoveSizeInProgress;
	BOOL skipResizeOnce;
	BOOL saveInitialDragLoc;
	BOOL skipMoveWindowOnce;

	/* store state info for some keys */
	int kdlshift;
	int kdrshift;
	int kdlctrl;
	int kdrctrl;
	int kdlalt;
	int kdralt;
	int kdlmeta;
	int kdrmeta;
	int kdcapslock;
	
@public
	NSWindow* ourMainWindow;
	NSPasteboard* pasteboard_rd; /* for reading from clipboard */
	NSPasteboard* pasteboard_wr; /* for writing to clipboard */
	int pasteboard_changecount;
	int pasteboard_format;
	int is_connected;
}

- (int) rdpConnect;
- (void) rdpConnectError;
- (void) rdpRemoteAppError;
- (void) saveStateInfo :(void *) instance :(void *) context;
- (void) onPasteboardTimerFired :(NSTimer *) timer;
- (void) releaseResources;
- (void) setViewSize : (int) width : (int) height;

@property (assign) int is_connected;

@end

/* Pointer Flags */
#define PTR_FLAGS_WHEEL                 0x0200
#define PTR_FLAGS_WHEEL_NEGATIVE        0x0100
#define PTR_FLAGS_MOVE                  0x0800
#define PTR_FLAGS_DOWN                  0x8000
#define PTR_FLAGS_BUTTON1               0x1000
#define PTR_FLAGS_BUTTON2               0x2000
#define PTR_FLAGS_BUTTON3               0x4000
#define WheelRotationMask               0x01FF
