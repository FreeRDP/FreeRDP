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

#include "MRDPRailView.h"

#define USE_RAIL_CVT

@implementation MRDPRailView

@synthesize mrdpRailWindow, windowIndex, activateWindow;

MRDPRailView* g_mrdpRailView;

- (void) updateDisplay
{
	BOOL moveWindow = NO;
	NSRect srcRectOuter;
	NSRect destRectOuter;
	
	rdpGdi* gdi;
	
	if ((context == 0) || (context->gdi == 0))
		return;
	
	if (context->gdi->primary->hdc->hwnd->invalid->null)
		return;
	
	if (context->gdi->drawing != context->gdi->primary)
		return;
	
	gdi = context->gdi;
	
	srcRectOuter = NSMakeRect(0, 0, self->width, self->height);
	destRectOuter = [[self window] frame];
	
	// cannot be bigger than our current screen size
	NSRect screenSize = [[NSScreen mainScreen] frame];
	
	if (destRectOuter.size.width > screenSize.size.width)
	{
		destRectOuter.size.width = screenSize.size.width;
		moveWindow = YES;
	}
	
	if (destRectOuter.size.height > screenSize.size.height)
	{
		destRectOuter.size.height = screenSize.size.height;
		moveWindow = YES;
	}
	
	if (destRectOuter.origin.x + destRectOuter.size.width > width)
		destRectOuter.size.width = width - destRectOuter.origin.x;
	
	[self setupBmiRep:destRectOuter.size.width :destRectOuter.size.height];
	
	if (moveWindow)
	{
		moveWindow = NO;
		RAIL_WINDOW_MOVE_ORDER newWndLoc;
		apple_to_windowMove(&destRectOuter, &newWndLoc);
		newWndLoc.windowId = savedWindowId;
		//skipMoveWindowOnce = TRUE;
		//mac_send_rail_client_event(g_mrdpRailView->rdp_instance->context->channels, RDP_EVENT_TYPE_RAIL_CLIENT_WINDOW_MOVE, &newWndLoc);
	}
	
	destRectOuter.origin.y = height - destRectOuter.origin.y - destRectOuter.size.height;
	rail_convert_color_space(pixelData, (char *) gdi->primary_buffer,
				 &destRectOuter, self->width, self->height);
	
	if (moveWindow)
		[self setNeedsDisplayInRect:destRectOuter];
	else
		[self setNeedsDisplayInRect:[self frame]];
	
	gdi->primary->hdc->hwnd->ninvalid = 0;
}

/** *********************************************************************
 * called when our view needs to be redrawn
 ***********************************************************************/

- (void) drawRect:(NSRect)dirtyRect
{
	[bmiRep drawInRect:dirtyRect fromRect:dirtyRect operation:NSCompositeCopy fraction:1.0 respectFlipped:NO hints:nil];
	
	if (pixelData)
	{
		free(pixelData);
		pixelData = NULL;
	}
	
	bmiRep = nil;
}

/** *********************************************************************
 * become first responder so we can get keyboard and mouse events
 ***********************************************************************/

- (BOOL)acceptsFirstResponder
{
	return YES;
}

- (BOOL)acceptsFirstMouse:(NSEvent *)theEvent
{
	return NO;
}

/** *********************************************************************
 * called when a mouse move event occurs
 *
 * ideally we want to be called when the mouse moves over NSView client area,
 * but in reality we get called any time the mouse moves anywhere on the screen;
 * we could use NSTrackingArea class to handle this but this class is available
 * on Mac OS X v10.5 and higher; since we want to be compatible with older
 * versions, we do this manually.
 *
 * TODO: here is how it can be done using legacy methods
 * http://developer.apple.com/library/mac/#documentation/Cocoa/Conceptual/EventOverview/MouseTrackingEvents/MouseTrackingEvents.html#//apple_ref/doc/uid/10000060i-CH11-SW1
 ***********************************************************************/

- (void) mouseMoved:(NSEvent *)event
{
	[super mouseMoved:event];
	
	NSRect winFrame = [[self window] frame];
	NSPoint loc = [event locationInWindow];
	
	int x = (int) (winFrame.origin.x + loc.x);
	int y = (int) (winFrame.origin.y + loc.y);
	
	y = height - y;
	
	/* send mouse motion event to RDP server */
	rdp_instance->input->MouseEvent(rdp_instance->input, PTR_FLAGS_MOVE, x, y);
}

/** *********************************************************************
 * called when left mouse button is pressed down
 ***********************************************************************/

- (void)mouseDown:(NSEvent *) event
{
	[super mouseDown:event];
	
	NSRect winFrame = [[self window] frame];
	NSPoint loc = [event locationInWindow];
	int x = (int) (winFrame.origin.x + loc.x);
	int y = (int) (winFrame.origin.y + loc.y);
	int yPos = (int) (winFrame.size.height - loc.y);
	
	y = height - y;
	
	if ((yPos >= 4) && (yPos <= 20))
		titleBarClicked = YES;
	else
		titleBarClicked = NO;
	
	savedDragLocation.x = loc.x;
	savedDragLocation.y = loc.y;
	
	rdp_instance->input->MouseEvent(rdp_instance->input, PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON1, x, y);
}

/** *********************************************************************
 * called when left mouse button is released
 ***********************************************************************/

- (void) mouseUp:(NSEvent *) event
{
	[super mouseUp:event];
	
	NSRect winFrame = [[self window] frame];
	NSPoint loc = [event locationInWindow];
	int x = (int) (winFrame.origin.x + loc.x);
	int y = (int) (winFrame.origin.y + loc.y);
	y = height - y;
	
	rdp_instance->input->MouseEvent(rdp_instance->input, PTR_FLAGS_BUTTON1, x, y);
	titleBarClicked = NO;
}

/** *********************************************************************
 * called when right mouse button is pressed down
 ***********************************************************************/

- (void) rightMouseDown:(NSEvent *)event
{
	[super rightMouseDown:event];
	
	NSRect winFrame = [[self window] frame];
	NSPoint loc = [event locationInWindow];
	int x = (int) (winFrame.origin.x + loc.x);
	int y = (int) (winFrame.origin.y + loc.y);
	y = height - y;
	
	rdp_instance->input->MouseEvent(rdp_instance->input, PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON2, x, y);
}

/** *********************************************************************
 * called when right mouse button is released
 ***********************************************************************/

- (void) rightMouseUp:(NSEvent *)event
{
	[super rightMouseUp:event];
	
	NSRect winFrame = [[self window] frame];
	NSPoint loc = [event locationInWindow];
	int x = (int) (winFrame.origin.x + loc.x);
	int y = (int) (winFrame.origin.y + loc.y);
	y = height - y;
	
	rdp_instance->input->MouseEvent(rdp_instance->input, PTR_FLAGS_BUTTON2, x, y);
}

/** *********************************************************************
 * called when middle mouse button is pressed
 ***********************************************************************/

- (void) otherMouseDown:(NSEvent *)event
{
	[super otherMouseDown:event];
	
	NSRect winFrame = [[self window] frame];
	NSPoint loc = [event locationInWindow];
	int x = (int) (winFrame.origin.x + loc.x);
	int y = (int) (winFrame.origin.y + loc.y);
	y = height - y;
	
	rdp_instance->input->MouseEvent(rdp_instance->input, PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON3, x, y);
}

/** *********************************************************************
 * called when middle mouse button is released
 ***********************************************************************/

- (void) otherMouseUp:(NSEvent *)event
{
	[super otherMouseUp:event];
	
	NSRect winFrame = [[self window] frame];
	NSPoint loc = [event locationInWindow];
	int x = (int) (winFrame.origin.x + loc.x);
	int y = (int) (winFrame.origin.y + loc.y);
	y = height - y;
	
	rdp_instance->input->MouseEvent(rdp_instance->input, PTR_FLAGS_BUTTON3, x, y);
}

- (void) scrollWheel:(NSEvent *)event
{
	uint16 flags;
	
	[super scrollWheel:event];
	
	// we get more two finger trackpad scroll events
	// than scrollWheel events, so we drop some
	
	if (gestureEventInProgress)
	{
		scrollWheelCount++;
	
		if (scrollWheelCount % 8 != 0)
			return;
	}
	
	if ([event scrollingDeltaY] < 0)
	{
		flags = PTR_FLAGS_WHEEL | PTR_FLAGS_WHEEL_NEGATIVE | 0x0088;
	}
	else
	{
		flags = PTR_FLAGS_WHEEL | 0x78;
	}
	
	rdp_instance->input->MouseEvent(rdp_instance->input, flags, 0, 0);
}

/** *********************************************************************
 * called when mouse is moved with left button pressed
 * note: invocation order is: mouseDown, mouseDragged, mouseUp
 ***********************************************************************/
- (void) mouseDragged:(NSEvent *)event
{
	[super mouseDragged:event];
	
	NSRect winFrame = [[self window] frame];
	NSPoint loc = [event locationInWindow];
	int x = (int) loc.x;
	int y = (int) loc.y;
	
	if (titleBarClicked)
	{
		// window is being dragged to a new location
		int newX = x - savedDragLocation.x;
		int newY = y - savedDragLocation.y;
		
		if ((newX == 0) && (newY == 0))
			return;
		
		winFrame.origin.x += newX;
		winFrame.origin.y += newY;
		
		[[self window] setFrame:winFrame display:YES];
		return;
	}
	
	if (localMoveType == RAIL_WMSZ_LEFT)
	{
		// left border resize taking place
		int diff = (int) (loc.x - savedDragLocation.x);
		
		if (diff == 0)
			return;
		
		if (diff < 0)
		{
			diff = abs(diff);
			winFrame.origin.x -= diff;
			winFrame.size.width += diff;
		}
		else
		{
			winFrame.origin.x += diff;
			winFrame.size.width -= diff;
		}
		
		[[self window] setFrame:winFrame display:YES];
		return;
	}
	
	if (localMoveType == RAIL_WMSZ_RIGHT)
	{
		// right border resize taking place
		int diff = (int) (loc.x - savedDragLocation.x);
		
		if (diff == 0)
			return;
		
		savedDragLocation.x = loc.x;
		savedDragLocation.y = loc.y;
		
		winFrame.size.width += diff;
		[[self window] setFrame:winFrame display:YES];
		
		return;
	}
	
	if (localMoveType == RAIL_WMSZ_TOP)
	{
		// top border resize taking place
		int diff = (int) (loc.y - savedDragLocation.y);
		
		if (diff == 0)
			return;
		
		savedDragLocation.x = loc.x;
		savedDragLocation.y = loc.y;
		
		winFrame.size.height += diff;
		[[self window] setFrame:winFrame display:YES];
		
		return;
	}
	
	if (localMoveType == RAIL_WMSZ_BOTTOM)
	{
		// bottom border resize taking place
		int diff = (int) (loc.y - savedDragLocation.y);
		
		if (diff == 0)
			return;
		
		if (diff < 0)
		{
			diff = abs(diff);
			winFrame.origin.y -= diff;
			winFrame.size.height += diff;
		}
		else
		{
			winFrame.origin.y += diff;
			winFrame.size.height -= diff;
		}
		
		[[self window] setFrame:winFrame display:YES];
		return;
	}
	
	if (localMoveType == RAIL_WMSZ_TOPLEFT)
	{
		// top left border resize taking place
		int diff = (int) (loc.x - savedDragLocation.x);
		
		if (diff != 0)
		{
			if (diff < 0)
			{
				diff = abs(diff);
				winFrame.origin.x -= diff;
				winFrame.size.width += diff;
			}
			else
			{
				winFrame.origin.x += diff;
				winFrame.size.width -= diff;
			}
		}
		
		diff = (int) (loc.y - savedDragLocation.y);
		
		if (diff != 0)
		{
			savedDragLocation.y = loc.y;
			winFrame.size.height += diff;
		}
		
		[[self window] setFrame:winFrame display:YES];
		return;
	}
	
	if (localMoveType == RAIL_WMSZ_TOPRIGHT)
	{
		// top right border resize taking place
		int diff = (int) (loc.x - savedDragLocation.x);
		
		if (diff != 0)
			winFrame.size.width += diff;
		
		diff = (int) (loc.y - savedDragLocation.y);
		
		if (diff != 0)
			winFrame.size.height += diff;
		
		savedDragLocation.x = loc.x;
		savedDragLocation.y = loc.y;
		
		[[self window] setFrame:winFrame display:YES];
		return;
	}
	
	if (localMoveType == RAIL_WMSZ_BOTTOMLEFT)
	{
		// bottom left border resize taking place
		int diff = (int) (loc.x - savedDragLocation.x);
		
		if (diff != 0)
		{
			if (diff < 0)
			{
				diff = abs(diff);
				winFrame.origin.x -= diff;
				winFrame.size.width += diff;
			}
			else
			{
				winFrame.origin.x += diff;
				winFrame.size.width -= diff;
			}
		}
		
		diff = (int) (loc.y - savedDragLocation.y);
		
		if (diff != 0)
		{
			if (diff < 0)
			{
				diff = abs(diff);
				winFrame.origin.y -= diff;
				winFrame.size.height += diff;
			}
			else
			{
				winFrame.origin.y += diff;
				winFrame.size.height -= diff;
			}
		}
		
		[[self window] setFrame:winFrame display:YES];
		return;
	}
	
	if (localMoveType == RAIL_WMSZ_BOTTOMRIGHT)
	{
		// bottom right border resize taking place
		int diff = (int) (loc.x - savedDragLocation.x);
	
		if (diff != 0)
		{
			savedDragLocation.x = loc.x;
			//savedDragLocation.y = loc.y;
			winFrame.size.width += diff;
		}
		
		diff = (int) (loc.y - savedDragLocation.y);
		
		if (diff != 0)
		{
			if (diff < 0)
			{
				diff = abs(diff);
				winFrame.origin.y -= diff;
				winFrame.size.height += diff;
			}
			else
			{
				winFrame.origin.y += diff;
				winFrame.size.height -= diff;
			}
		}
		
		[[self window] setFrame:winFrame display:YES];
		return;
	}
	
	x = (int) (winFrame.origin.x + loc.x);
	y = (int) (winFrame.origin.y + loc.y);
	y = height - y;
	
	// send mouse motion event to RDP server
	rdp_instance->input->MouseEvent(rdp_instance->input, PTR_FLAGS_MOVE, x, y);
}

/** *********************************************************************
 * called when a key is pressed
 ***********************************************************************/

- (void) keyDown:(NSEvent *) event
{
	int key;
	USHORT extended;
	DWORD vkcode;
	DWORD scancode;
	
	key = [event keyCode] + 8;
	
	vkcode = GetVirtualKeyCodeFromKeycode(key, KEYCODE_TYPE_APPLE);
	scancode = GetVirtualScanCodeFromVirtualKeyCode(vkcode, 4);
	extended = (scancode & KBDEXT) ? KBDEXT : 0;
	
	rdp_instance->input->KeyboardEvent(rdp_instance->input, extended | KBD_FLAGS_DOWN, scancode & 0xFF);
}

/** *********************************************************************
 * called when a key is released
 ***********************************************************************/

- (void) keyUp:(NSEvent *) event
{
	int key;
	USHORT extended;
	DWORD vkcode;
	DWORD scancode;
	
	key = [event keyCode] + 8;
	
	vkcode = GetVirtualKeyCodeFromKeycode(key, KEYCODE_TYPE_APPLE);
	scancode = GetVirtualScanCodeFromVirtualKeyCode(vkcode, 4);
	extended = (scancode & KBDEXT) ? TRUE : FALSE;
	
	rdp_instance->input->KeyboardEvent(rdp_instance->input, extended | KBD_FLAGS_RELEASE, scancode & 0xFF);
}

/** *********************************************************************
 * called when shift, control, alt and meta keys are pressed/released
 ***********************************************************************/

- (void) flagsChanged:(NSEvent *) event
{
	NSUInteger mf = [event modifierFlags];
	
	// caps lock
	if (mf == 0x10100) {
		printf("TODO: caps lock is on\n");
		kdcapslock = 1;
	}
	if (kdcapslock && (mf == 0x100)) {
		kdcapslock = 0;
		printf("TODO: caps lock is off\n");
	}
	// left shift
	if ((kdlshift == 0) && ((mf & 2) != 0)) {
		// left shift went down
		rdp_instance->input->KeyboardEvent(rdp_instance->input, KBD_FLAGS_DOWN, 0x2a);
		kdlshift = 1;
	}
	if ((kdlshift != 0) && ((mf & 2) == 0)) {
		// left shift went up
		rdp_instance->input->KeyboardEvent(rdp_instance->input, KBD_FLAGS_RELEASE, 0x2a);
		kdlshift = 0;
	}
	
	// right shift
	if ((kdrshift == 0) && ((mf & 4) != 0)) {
		// right shift went down
		rdp_instance->input->KeyboardEvent(rdp_instance->input, KBD_FLAGS_DOWN, 0x36);
		kdrshift = 1;
	}
	if ((kdrshift != 0) && ((mf & 4) == 0)) {
		// right shift went up
		rdp_instance->input->KeyboardEvent(rdp_instance->input, KBD_FLAGS_RELEASE, 0x36);
		kdrshift = 0;
	}
	
	// left ctrl
	if ((kdlctrl == 0) && ((mf & 1) != 0)) {
		// left ctrl went down
		rdp_instance->input->KeyboardEvent(rdp_instance->input, KBD_FLAGS_DOWN, 0x1d);
		kdlctrl = 1;
	}
	if ((kdlctrl != 0) && ((mf & 1) == 0)) {
		// left ctrl went up
		rdp_instance->input->KeyboardEvent(rdp_instance->input, KBD_FLAGS_RELEASE, 0x1d);
		kdlctrl = 0;
	}
	
	// right ctrl
	if ((kdrctrl == 0) && ((mf & 0x2000) != 0)) {
		// right ctrl went down
		rdp_instance->input->KeyboardEvent(rdp_instance->input, 1 | KBD_FLAGS_DOWN, 0x1d);
		kdrctrl = 1;
	}
	if ((kdrctrl != 0) && ((mf & 0x2000) == 0)) {
		// right ctrl went up
		rdp_instance->input->KeyboardEvent(rdp_instance->input, 1 | KBD_FLAGS_RELEASE, 0x1d);
		kdrctrl = 0;
	}
	
	// left alt
	if ((kdlalt == 0) && ((mf & 0x20) != 0)) {
		// left alt went down
		rdp_instance->input->KeyboardEvent(rdp_instance->input, KBD_FLAGS_DOWN, 0x38);
		kdlalt = 1;
	}
	if ((kdlalt != 0) && ((mf & 0x20) == 0)) {
		// left alt went up
		rdp_instance->input->KeyboardEvent(rdp_instance->input, KBD_FLAGS_RELEASE, 0x38);
		kdlalt = 0;
	}
	
	// right alt
	if ((kdralt == 0) && ((mf & 0x40) != 0)) {
		// right alt went down
		rdp_instance->input->KeyboardEvent(rdp_instance->input, 1 | KBD_FLAGS_DOWN, 0x38);
		kdralt = 1;
	}
	if ((kdralt != 0) && ((mf & 0x40) == 0)) {
		// right alt went up
		rdp_instance->input->KeyboardEvent(rdp_instance->input, 1 | KBD_FLAGS_RELEASE, 0x38);
		kdralt = 0;
	}
	
	// left meta
	if ((kdlmeta == 0) && ((mf & 0x08) != 0)) {
		// left meta went down
		rdp_instance->input->KeyboardEvent(rdp_instance->input, 1 | KBD_FLAGS_DOWN, 0x5b);
		kdlmeta = 1;
	}
	if ((kdlmeta != 0) && ((mf & 0x08) == 0)) {
		// left meta went up
		rdp_instance->input->KeyboardEvent(rdp_instance->input, 1 | KBD_FLAGS_RELEASE, 0x5b);
		kdlmeta = 0;
	}
	
	// right meta
	if ((kdrmeta == 0) && ((mf & 0x10) != 0)) {
		// right meta went down
		rdp_instance->input->KeyboardEvent(rdp_instance->input, 1 | KBD_FLAGS_DOWN, 0x5c);
		kdrmeta = 1;
	}
	if ((kdrmeta != 0) && ((mf & 0x10) == 0)) {
		// right meta went up
		rdp_instance->input->KeyboardEvent(rdp_instance->input, 1 | KBD_FLAGS_RELEASE, 0x5c);
		kdrmeta = 0;
	}
}

- (void) setRdpInstance:(freerdp *) instance width:(int) w andHeight:(int) h windowID:(int) windowID
{
	rdp_instance = instance;
	context = instance->context;
	width = w;
	height = h;
	savedWindowId = windowID;
	
	NSRect tr = NSMakeRect(0, 0,
			       [[NSScreen mainScreen] frame].size.width,
			       [[NSScreen mainScreen] frame].size.height);
	
	NSTrackingArea * trackingArea = [[NSTrackingArea alloc] initWithRect:tr options:NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingCursorUpdate | NSTrackingEnabledDuringMouseDrag | NSTrackingActiveAlways owner:self userInfo:nil];
	
	[self addTrackingArea:trackingArea];
	
	g_mrdpRailView = self;
	
	[self becomeFirstResponder];
	[self setAcceptsTouchEvents:YES];
	
	// we want to be notified when window resizes....
	[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(windowDidResize:) name:NSWindowDidResizeNotification object:nil];
	
	// ...moves
	[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(windowDidMove:) name:NSWindowDidMoveNotification object:nil];
	
	// ...and becomes the key window
	[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(windowDidBecomeKey:) name:NSWindowDidBecomeKeyNotification object:nil];
}

- (void) setupBmiRep:(int) frameWidth :(int) frameHeight
{
	struct rgba_data
	{
		char red;
		char green;
		char blue;
		char alpha;
	};
	
	if (pixelData)
		free(pixelData);
	
	pixelData = (char *) malloc(frameWidth * frameHeight * sizeof(struct rgba_data));
	bmiRep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:(unsigned char **) &pixelData
							 pixelsWide:frameWidth
							 pixelsHigh:frameHeight
						      bitsPerSample:8
						    samplesPerPixel:sizeof(struct rgba_data)
							   hasAlpha:YES
							   isPlanar:NO
						     colorSpaceName:NSDeviceRGBColorSpace
						       bitmapFormat:0
							bytesPerRow:frameWidth * sizeof(struct rgba_data)
						       bitsPerPixel:0];
}
- (void) beginGestureWithEvent:(NSEvent *)event
{
	gestureEventInProgress = YES;
}

- (void) endGestureWithEvent:(NSEvent *)event
{
	gestureEventInProgress = NO;
}

/**
 * called when a bordered window changes size
 */

- (void) windowDidResize:(NSNotification *) notification
{
	// if we are not the source of this notification, just return
	if ([notification object] != [self mrdpRailWindow])
		return;
	
	// let RDP server know that window has moved
	RAIL_WINDOW_MOVE_ORDER windowMove;
	NSRect r = [[self window] frame];
	
	int diffInHeight = [[self window] frame].size.height - [self frame].size.height;
	r.size.height -= diffInHeight;
	
	apple_to_windowMove(&r, &windowMove);
	windowMove.windowId = self->savedWindowId;
	mac_send_rail_client_event(self->context->channels, RailChannel_ClientWindowMove, &windowMove);
}

/**
 * called when user moves a bordered window
 */

- (void) windowDidMove:(NSNotification *) notification
{
	// if we are not the source of this notification, just return
	if ([notification object] != [self mrdpRailWindow])
		return;
	
	// let RDP server know that window has moved
	RAIL_WINDOW_MOVE_ORDER windowMove;
	NSRect r = [[self window] frame];
	
	int diffInHeight = [[self window] frame].size.height - [self frame].size.height;
	r.size.height -= diffInHeight;
	
	apple_to_windowMove(&r, &windowMove);
	windowMove.windowId = self->savedWindowId;
	mac_send_rail_client_event(self->context->channels, RailChannel_ClientWindowMove, &windowMove);
}

/**
 * called when a NSWindow becomes the key window
 */

- (void) windowDidBecomeKey:(NSNotification *) notification
{
	// if we are not the source of this notification, just return
	if ([notification object] != [self mrdpRailWindow])
		return;
	
	if (![self activateWindow])
		return;
	
	[[self window] setAcceptsMouseMovedEvents: YES];
	
	//if ([self activateWindow])
        mac_rail_send_activate(savedWindowId);
	
	// set_current_window(windowIndex); // ? code mis-merge?
}

- (void) releaseResources
{
	[[NSNotificationCenter defaultCenter] removeObserver:self];
}

void rail_cvt_from_rect(char* dest, char* src, NSRect destRect, int destWidth, int destHeight, NSRect srcRect)
{
}

/** *********************************************************************
 * color space conversion used specifically in RAIL
 ***********************************************************************/
void rail_convert_color_space(char* destBuf, char* srcBuf, NSRect* destRect, int width, int height)
{
	int     i;
	int     j;
	int     numRows;
	int     srcX;
	int     srcY;
	int     destX;
	int     destY;
	int     pixelsPerRow;
	int     pixel;
	int     pixel1;
	int     pixel2;
	int *   src32;
	int *   dest32;
	
	int destWidth  = destRect->size.width;
	int destHeight = destRect->size.height;
	
	if ((!destBuf) || (!srcBuf))
		return;
	
	numRows = (destRect->origin.y + destHeight > height) ? height - destRect->origin.y : destHeight;
	pixelsPerRow = destWidth;
	
	srcX  = destRect->origin.x;
	srcY  = destRect->origin.y;
	destX = 0;
	destY = 0;
	
	for (i = 0; i < numRows; i++)
	{
		src32  = (int *) (srcBuf  + ((srcY  + i) * width  + srcX)  * 4);
		dest32 = (int *) (destBuf + ((destY + i) * destWidth + destX) * 4);
		
		for (j = 0; j < pixelsPerRow; j++)
		{
			pixel = *src32;
			pixel1 = (pixel & 0x00ff0000) >> 16;
			pixel2 = (pixel & 0x000000ff) << 16;
			pixel = (pixel & 0xff00ff00) | pixel1 | pixel2;
			
			*dest32 = pixel;
			
			src32++;
			dest32++;
		}
	}
	
	destRect->origin.y = destHeight - destRect->origin.y - destRect->size.height;
	return;
}

/**
 *  let RDP server know that window has moved
 */

void rail_MoveWindow(rdpRail * rail, rdpWindow * window)
{
	if (g_mrdpRailView->isMoveSizeInProgress)
		return;
	
	if (g_mrdpRailView->skipMoveWindowOnce)
	{
		g_mrdpRailView->skipMoveWindowOnce = NO;
		return;
	}
	
	// this rect is based on Windows co-ordinates...
	NSRect r;
	r.origin.x = window->windowOffsetX;
	r.origin.y = window->windowOffsetY;
	r.size.width = window->windowWidth;
	r.size.height = window->windowHeight;
	
	windows_to_apple_cords(&r);
	[[g_mrdpRailView window] setFrame:r display:YES];
}


void mac_rail_send_activate(int window_id)
{
	RAIL_ACTIVATE_ORDER activate;
	
	activate.windowId = window_id;
	activate.enabled = 1;
	
	mac_send_rail_client_event(g_mrdpRailView->context->channels, RailChannel_ClientActivate, &activate);
}

@end

