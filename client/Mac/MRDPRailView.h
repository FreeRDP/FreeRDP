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

#import "freerdp/gdi/gdi.h"
#import "freerdp/rail/rail.h"

#import "MRDPRailWindow.h"

@interface MRDPRailView : NSView
{
	freerdp* rdp_instance;
	rdpContext* context;
	NSBitmapImageRep* bmiRep;
	NSPoint savedDragLocation;
	char* pixelData;
	BOOL mouseInClientArea;
	BOOL titleBarClicked;
	BOOL gestureEventInProgress;
	int width;
	int height;
	int savedWindowId;
	int scrollWheelCount;
	
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
	BOOL isMoveSizeInProgress;
	BOOL saveInitialDragLoc;
	BOOL skipMoveWindowOnce;
	int localMoveType;
}

@property (assign) MRDPRailWindow* mrdpRailWindow;
@property (assign) int windowIndex;
@property (assign) BOOL activateWindow;

- (void) windowDidMove:(NSNotification*) notification;
- (void) updateDisplay;
- (void) setRdpInstance:(freerdp*) instance width:(int) w andHeight:(int) h windowID:(int) windowID;
- (void) setupBmiRep:(int) width :(int) height;
- (void) releaseResources;

void mac_rail_MoveWindow(rdpRail* rail, rdpWindow* window);
void apple_to_windowMove(NSRect* r, RAIL_WINDOW_MOVE_ORDER* windowMove);
void mac_send_rail_client_event(rdpChannels* channels, UINT16 event_type, void* param);
void windows_to_apple_cords(NSRect* r);
void rail_MoveWindow(rdpRail* rail, rdpWindow* window);
void mac_rail_send_activate(int window_id);

@end
