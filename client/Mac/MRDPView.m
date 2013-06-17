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

/*
 * TODO
 *  + provide a UI for configuring optional parameters, but keep cmd line args
 *  + audio redirection is delayed considerably
 *  + caps lock key needs to be sent in func flagsChanged()
 *  + libfreerdp-utils.1.0.dylib needs to be installed to /usr/local/lib
 *
 *  - MRDPView implementation is incomplete
 *  - all variables should have consistent nameing scheme - camel case
 *  - all funcs same as above
 *  - PolygonSc seems to create a transparent rect
 *  - ensure mouse cursor changes are working ok after moving to NSTracking area
 *  - RAIL:
 *  -
 *  -
 *  -       tool tips to be correctly positioned
 *  -       dragging is slightly of
 *  -       resize after dragging not working
 *  -       dragging app from macbook to monitor gives exec/access err
 *  -       unable to drag rect out of monitor boundaries
 *  -
 *  -
 *  -
 */

#import "MRDPView.h"
#import "MRDPCursor.h"
#import "PasswordDialog.h"

#include <winpr/crt.h>
#include <winpr/input.h>

#include <freerdp/constants.h>

#import "freerdp/freerdp.h"
#import "freerdp/types.h"
#import "freerdp/channels/channels.h"
#import "freerdp/gdi/gdi.h"
#import "freerdp/graphics.h"
#import "freerdp/utils/event.h"
#import "freerdp/client/cliprdr.h"
#import "freerdp/client/file.h"
#import "freerdp/client/cmdline.h"

/******************************************
 Forward declarations
 ******************************************/


void mf_Pointer_New(rdpContext* context, rdpPointer* pointer);
void mf_Pointer_Free(rdpContext* context, rdpPointer* pointer);
void mf_Pointer_Set(rdpContext* context, rdpPointer* pointer);
void mf_Pointer_SetNull(rdpContext* context);
void mf_Pointer_SetDefault(rdpContext* context);
// int rdp_connect(void);
BOOL mac_pre_connect(freerdp* instance);
BOOL mac_post_connect(freerdp*	instance);
BOOL mac_authenticate(freerdp* instance, char** username, char** password, char** domain);
int mac_context_new(freerdp* instance, rdpContext* context);
void mac_context_free(freerdp* instance, rdpContext* context);
void mac_set_bounds(rdpContext* context, rdpBounds* bounds);
void mac_bitmap_update(rdpContext* context, BITMAP_UPDATE* bitmap);
void mac_begin_paint(rdpContext* context);
void mac_end_paint(rdpContext* context);
void mac_save_state_info(freerdp* instance, rdpContext* context);
void skt_activity_cb(CFSocketRef s, CFSocketCallBackType callbackType, CFDataRef address, const void* data, void* info);
void channel_activity_cb(CFSocketRef s, CFSocketCallBackType callbackType, CFDataRef address, const void* data, void* info);
int register_fds(int* fds, int count, void* instance);
int invoke_draw_rect(rdpContext* context);
int process_plugin_args(rdpSettings* settings, const char* name, RDP_PLUGIN_DATA* plugin_data, void* user_data);
int receive_channel_data(freerdp* instance, int chan_id, BYTE* data, int size, int flags, int total_size);

void process_cliprdr_event(freerdp* instance, wMessage* event);
void cliprdr_process_cb_format_list_event(freerdp* instance, RDP_CB_FORMAT_LIST_EVENT* event);
void cliprdr_send_data_request(freerdp* instance, UINT32 format);
void cliprdr_process_cb_monitor_ready_event(freerdp* inst);
void cliprdr_process_cb_data_response_event(freerdp* instance, RDP_CB_DATA_RESPONSE_EVENT* event);
void cliprdr_process_text(freerdp* instance, BYTE* data, int len);
void cliprdr_send_supported_format_list(freerdp* instance);
int register_channel_fds(int* fds, int count, void* instance);

struct mac_context
{
	// *must* have this - do not delete
	rdpContext _p;
};

struct cursor
{
	rdpPointer* pointer;
	BYTE* cursor_data;
	void* bmiRep; /* NSBitmapImageRep */
	void* nsCursor; /* NSCursor */
	void* nsImage; /* NSImage */
};

struct rgba_data
{
	char red;
	char green;
	char blue;
	char alpha;
};






@implementation MRDPView

MRDPView *g_mrdpview;

@synthesize is_connected;

//int rdp_connect()
- (int) rdpConnect
{
	int status;
	freerdp* instance;
	
	freerdp_channels_global_init();
	
	instance = freerdp_new();
	instance->PreConnect = mac_pre_connect;
	instance->PostConnect = mac_post_connect;
	instance->ContextSize = sizeof(struct mac_context);
	instance->ContextNew = mac_context_new;
	instance->ContextFree = mac_context_free;
	instance->ReceiveChannelData = receive_channel_data;
	instance->Authenticate = mac_authenticate;
	freerdp_context_new(instance);
	
	status = freerdp_connect(instance);
	
	if (status)
	{
		freerdp_check_fds(instance);
		[g_mrdpview setIs_connected:1];
		return 0;
	}
	
	[g_mrdpview setIs_connected:0];
	[g_mrdpview rdpConnectError];
	
	return -1;
}



/************************************************************************
 methods we override
 ************************************************************************/

/** *********************************************************************
 * create MRDPView with specified rectangle
 ***********************************************************************/

- (id)initWithFrame:(NSRect)frame
{
	self = [super initWithFrame:frame];
	
	if (self)
	{
		// Initialization code here.
	}
	
	return self;
}

/** *********************************************************************
 * called when MRDPView has been successfully created from the NIB
 ***********************************************************************/

- (void) awakeFromNib
{
	g_mrdpview = self;
	
	// store our window dimensions
	width = [self frame].size.width;
	height = [self frame].size.height;
	titleBarHeight = 22;
	
	[[self window] becomeFirstResponder];
	[[self window] setAcceptsMouseMovedEvents:YES];
	
	cursors = [[NSMutableArray alloc] initWithCapacity:10];
    
	// setup a mouse tracking area
	NSTrackingArea * trackingArea = [[NSTrackingArea alloc] initWithRect:[self visibleRect] options:NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingCursorUpdate | NSTrackingEnabledDuringMouseDrag | NSTrackingActiveWhenFirstResponder owner:self userInfo:nil];
	
	[self addTrackingArea:trackingArea];
	
	mouseInClientArea = YES;

}

/** *********************************************************************
 * become first responder so we can get keyboard and mouse events
 ***********************************************************************/

- (BOOL)acceptsFirstResponder
{
	return YES;
}

/** *********************************************************************
 * called when a mouse move event occurrs
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
	
	if (!is_connected)
		return;
	
	NSPoint loc = [event locationInWindow];
	int x = (int) loc.x;
	int y = (int) loc.y;
	
	y = height - y;
	
	// send mouse motion event to RDP server
	((freerdp*)rdp_instance)->input->MouseEvent(((freerdp*)rdp_instance)->input, PTR_FLAGS_MOVE, x, y);
}

/** *********************************************************************
 * called when left mouse button is pressed down
 ***********************************************************************/

- (void)mouseDown:(NSEvent *) event
{
	[super mouseDown:event];
	
	if (!is_connected)
		return;
	
	NSPoint loc = [event locationInWindow];
	int x = (int) loc.x;
	int y = (int) loc.y;
	
	y = height - y;
	
	((freerdp*)rdp_instance)->input->MouseEvent(((freerdp*)rdp_instance)->input, PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON1, x, y);
}

/** *********************************************************************
 * called when left mouse button is released
 ***********************************************************************/

- (void) mouseUp:(NSEvent *) event
{
	[super mouseUp:event];
	
	if (!is_connected)
		return;
	
	NSPoint loc = [event locationInWindow];
	int x = (int) loc.x;
	int y = (int) loc.y;
	
	y = height - y;
	
	((freerdp*)rdp_instance)->input->MouseEvent(((freerdp*)rdp_instance)->input, PTR_FLAGS_BUTTON1, x, y);
}

/** *********************************************************************
 * called when right mouse button is pressed down
 ***********************************************************************/

- (void) rightMouseDown:(NSEvent *)event
{
	[super rightMouseDown:event];
	
	if (!is_connected)
		return;
	
	NSPoint loc = [event locationInWindow];
	int x = (int) loc.x;
	int y = (int) loc.y;
	
	y = height - y;
	
	((freerdp*)rdp_instance)->input->MouseEvent(((freerdp*)rdp_instance)->input, PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON2, x, y);
}

/** *********************************************************************
 * called when right mouse button is released
 ***********************************************************************/

- (void) rightMouseUp:(NSEvent *)event
{
	[super rightMouseUp:event];
	
	if (!is_connected)
		return;
	
	NSPoint loc = [event locationInWindow];
	int x = (int) loc.x;
	int y = (int) loc.y;
	
	y = height - y;
	
	((freerdp*)rdp_instance)->input->MouseEvent(((freerdp*)rdp_instance)->input, PTR_FLAGS_BUTTON2, x, y);
}

/** *********************************************************************
 * called when middle mouse button is pressed
 ***********************************************************************/

- (void) otherMouseDown:(NSEvent *)event
{
	[super otherMouseDown:event];
	
	if (!is_connected)
		return;
	
	NSPoint loc = [event locationInWindow];
	int x = (int) loc.x;
	int y = (int) loc.y;
	
	y = height - y;
	
	((freerdp*)rdp_instance)->input->MouseEvent(((freerdp*)rdp_instance)->input, PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON3, x, y);
}

/** *********************************************************************
 * called when middle mouse button is released
 ***********************************************************************/

- (void) otherMouseUp:(NSEvent *)event
{
	[super otherMouseUp:event];
	
	if (!is_connected)
		return;
	
	NSPoint loc = [event locationInWindow];
	int x = (int) loc.x;
	int y = (int) loc.y;
	
	y = height - y;
	
	((freerdp*)rdp_instance)->input->MouseEvent(((freerdp*)rdp_instance)->input, PTR_FLAGS_BUTTON3, x, y);
}

- (void) scrollWheel:(NSEvent *)event
{
	UINT16 flags;
	
	[super scrollWheel:event];
	
	if (!is_connected)
		return;
	
	NSPoint loc = [event locationInWindow];
	int x = (int) loc.x;
	int y = (int) loc.y;
	
	y = height - y;
	
	flags = PTR_FLAGS_WHEEL;
	
	if ([event deltaY] < 0)
		flags |= PTR_FLAGS_WHEEL_NEGATIVE | 0x0088;
	else
		flags |= 0x0078;
	
	x += (int) [event deltaX];
	y += (int) [event deltaY];
	
	((freerdp*)rdp_instance)->input->MouseEvent(((freerdp*)rdp_instance)->input, flags, x, y);
}

/** *********************************************************************
 * called when mouse is moved with left button pressed
 * note: invocation order is: mouseDown, mouseDragged, mouseUp
 ***********************************************************************/

- (void) mouseDragged:(NSEvent *)event
{
	[super mouseDragged:event];
	
	if (!is_connected)
		return;
	
	NSPoint loc = [event locationInWindow];
	int x = (int) loc.x;
	int y = (int) loc.y;
	
	y = height - y;
	
	// send mouse motion event to RDP server
	((freerdp*)rdp_instance)->input->MouseEvent(((freerdp*)rdp_instance)->input, PTR_FLAGS_MOVE, x, y);
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
	
	if (!is_connected)
		return;
	
	key = [event keyCode] + 8;
	
	vkcode = GetVirtualKeyCodeFromKeycode(key, KEYCODE_TYPE_APPLE);
	scancode = GetVirtualScanCodeFromVirtualKeyCode(vkcode, 4);
	extended = (scancode & KBDEXT) ? KBDEXT : 0;
	
	((freerdp*)rdp_instance)->input->KeyboardEvent(((freerdp*)rdp_instance)->input, extended | KBD_FLAGS_DOWN, scancode & 0xFF);
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
	
	if (!is_connected)
		return;
	
	key = [event keyCode] + 8;
	
	vkcode = GetVirtualKeyCodeFromKeycode(key, KEYCODE_TYPE_APPLE);
	scancode = GetVirtualScanCodeFromVirtualKeyCode(vkcode, 4);
	extended = (scancode & KBDEXT) ? KBDEXT : 0;
	
	((freerdp*)rdp_instance)->input->KeyboardEvent(((freerdp*)rdp_instance)->input, extended | KBD_FLAGS_RELEASE, scancode & 0xFF);
}

/** *********************************************************************
 * called when shift, control, alt and meta keys are pressed/released
 ***********************************************************************/

- (void) flagsChanged:(NSEvent*) event
{
	NSUInteger mf = [event modifierFlags];
	
	if (!is_connected)
		return;
	
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
		((freerdp*)rdp_instance)->input->KeyboardEvent(((freerdp*)rdp_instance)->input, KBD_FLAGS_DOWN, 0x2a);
		kdlshift = 1;
	}
	if ((kdlshift != 0) && ((mf & 2) == 0)) {
		// left shift went up
		((freerdp*)rdp_instance)->input->KeyboardEvent(((freerdp*)rdp_instance)->input, KBD_FLAGS_RELEASE, 0x2a);
		kdlshift = 0;
	}
	
	// right shift
	if ((kdrshift == 0) && ((mf & 4) != 0)) {
		// right shift went down
		((freerdp*)rdp_instance)->input->KeyboardEvent(((freerdp*)rdp_instance)->input, KBD_FLAGS_DOWN, 0x36);
		kdrshift = 1;
	}
	if ((kdrshift != 0) && ((mf & 4) == 0)) {
		// right shift went up
		((freerdp*)rdp_instance)->input->KeyboardEvent(((freerdp*)rdp_instance)->input, KBD_FLAGS_RELEASE, 0x36);
		kdrshift = 0;
	}
	
	// left ctrl
	if ((kdlctrl == 0) && ((mf & 1) != 0)) {
		// left ctrl went down
		((freerdp*)rdp_instance)->input->KeyboardEvent(((freerdp*)rdp_instance)->input, KBD_FLAGS_DOWN, 0x1d);
		kdlctrl = 1;
	}
	if ((kdlctrl != 0) && ((mf & 1) == 0)) {
		// left ctrl went up
		((freerdp*)rdp_instance)->input->KeyboardEvent(((freerdp*)rdp_instance)->input, KBD_FLAGS_RELEASE, 0x1d);
		kdlctrl = 0;
	}
	
	// right ctrl
	if ((kdrctrl == 0) && ((mf & 0x2000) != 0)) {
		// right ctrl went down
		((freerdp*)rdp_instance)->input->KeyboardEvent(((freerdp*)rdp_instance)->input, 1 | KBD_FLAGS_DOWN, 0x1d);
		kdrctrl = 1;
	}
	if ((kdrctrl != 0) && ((mf & 0x2000) == 0)) {
		// right ctrl went up
		((freerdp*)rdp_instance)->input->KeyboardEvent(((freerdp*)rdp_instance)->input, 1 | KBD_FLAGS_RELEASE, 0x1d);
		kdrctrl = 0;
	}
	
	// left alt
	if ((kdlalt == 0) && ((mf & 0x20) != 0)) {
		// left alt went down
		((freerdp*)rdp_instance)->input->KeyboardEvent(((freerdp*)rdp_instance)->input, KBD_FLAGS_DOWN, 0x38);
		kdlalt = 1;
	}
	if ((kdlalt != 0) && ((mf & 0x20) == 0)) {
		// left alt went up
		((freerdp*)rdp_instance)->input->KeyboardEvent(((freerdp*)rdp_instance)->input, KBD_FLAGS_RELEASE, 0x38);
		kdlalt = 0;
	}
	
	// right alt
	if ((kdralt == 0) && ((mf & 0x40) != 0)) {
		// right alt went down
		((freerdp*)rdp_instance)->input->KeyboardEvent(((freerdp*)rdp_instance)->input, 1 | KBD_FLAGS_DOWN, 0x38);
		kdralt = 1;
	}
	if ((kdralt != 0) && ((mf & 0x40) == 0)) {
		// right alt went up
		((freerdp*)rdp_instance)->input->KeyboardEvent(((freerdp*)rdp_instance)->input, 1 | KBD_FLAGS_RELEASE, 0x38);
		kdralt = 0;
	}
	
	// left meta
	if ((kdlmeta == 0) && ((mf & 0x08) != 0)) {
		// left meta went down
		((freerdp*)rdp_instance)->input->KeyboardEvent(((freerdp*)rdp_instance)->input, 1 | KBD_FLAGS_DOWN, 0x5b);
		kdlmeta = 1;
	}
	if ((kdlmeta != 0) && ((mf & 0x08) == 0)) {
		// left meta went up
		((freerdp*)rdp_instance)->input->KeyboardEvent(((freerdp*)rdp_instance)->input, 1 | KBD_FLAGS_RELEASE, 0x5b);
		kdlmeta = 0;
	}
	
	// right meta
	if ((kdrmeta == 0) && ((mf & 0x10) != 0)) {
		// right meta went down
		((freerdp*)rdp_instance)->input->KeyboardEvent(((freerdp*)rdp_instance)->input, 1 | KBD_FLAGS_DOWN, 0x5c);
		kdrmeta = 1;
	}
	if ((kdrmeta != 0) && ((mf & 0x10) == 0)) {
		// right meta went up
		((freerdp*)rdp_instance)->input->KeyboardEvent(((freerdp*)rdp_instance)->input, 1 | KBD_FLAGS_RELEASE, 0x5c);
		kdrmeta = 0;
	}
}

- (void) releaseResources
{
	int i;
	
	for (i = 0; i < argc; i++)
	{
		if (argv[i])
			free(argv[i]);
	}
	
	if (!is_connected)
		return;
	
	freerdp_channels_global_uninit();
	
	if (pixel_data)
		free(pixel_data);
	
	if (run_loop_src != 0)
		CFRunLoopRemoveSource(CFRunLoopGetCurrent(), run_loop_src_channels, kCFRunLoopDefaultMode);
	
	if (run_loop_src != 0)
		CFRunLoopRemoveSource(CFRunLoopGetCurrent(), run_loop_src, kCFRunLoopDefaultMode);
}

/** *********************************************************************
 * called when our view needs refreshing
 ***********************************************************************/

- (void) drawRect:(NSRect)rect
{
	if (!rdp_context)
		return;

    if(g_mrdpview->bitmap_context)
    {
        CGContextRef context = [[NSGraphicsContext currentContext] graphicsPort];
        CGImageRef cgImage = CGBitmapContextCreateImage(g_mrdpview->bitmap_context);

        CGContextClipToRect(context, CGRectMake(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height));
        CGContextDrawImage(context, CGRectMake(0, 0, [self bounds].size.width, [self bounds].size.height), cgImage);

        CGImageRelease(cgImage);
    }
    else
    {
        // just clear the screen with black
        [[NSColor redColor] set];
        NSRectFill([self bounds]);
    }
}

/************************************************************************
 instance methods
 ************************************************************************/

/** *********************************************************************
 * save state info for use by other methods later on
 ***********************************************************************/

- (void) saveStateInfo:(void *) instance :(void *) context
{
	rdp_instance = instance;
	rdp_context = context;
}

/** *********************************************************************
 * double check that a mouse event occurred in our client view
 ***********************************************************************/

- (BOOL) eventIsInClientArea :(NSEvent *) event :(int *) xptr :(int *) yptr
{
	NSPoint loc = [event locationInWindow];
	int x = (int) loc.x;
	int y = (int) loc.y;
	
	if ((x < 0) || (y < 0))
	{
		if (mouseInClientArea)
		{
			// set default cursor before leaving client area
			mouseInClientArea = NO;
			NSCursor *cur = [NSCursor arrowCursor];
			[cur set];
		}
		
		return NO;
	}
	
	if ((x > width) || (y > height))
	{
		if (mouseInClientArea)
		{
			// set default cursor before leaving client area
			mouseInClientArea = NO;
			NSCursor *cur = [NSCursor arrowCursor];
			[cur set];
		}
		
		return NO;
	}
	
	// on Mac origin is at lower left, but we want it on upper left
	y = height - y;
	
	*xptr = x;
	*yptr = y;
	mouseInClientArea = YES;
	return YES;
}

/** *********************************************************************
 * called when we fail to connect to a RDP server
 ***********************************************************************/

- (void) rdpConnectError
{
    NSString* message = @"Error connecting to server";
    if (connectErrorCode == AUTHENTICATIONERROR)
    {
        message = [NSString stringWithFormat:@"%@:\n%@", message, @"Authentication failure, check credentials."];
    }

	NSAlert *alert = [[NSAlert alloc] init];
	[alert setMessageText:message];
	[alert beginSheetModalForWindow:[g_mrdpview window]
			  modalDelegate:g_mrdpview
			 didEndSelector:@selector(alertDidEnd:returnCode:contextInfo:)
			    contextInfo:nil];
}

/** *********************************************************************
 * called when we fail to launch remote app on RDP server
 ***********************************************************************/

- (void) rdpRemoteAppError
{
	NSAlert *alert = [[NSAlert alloc] init];
	[alert setMessageText:@"Error starting remote app on specified server"];
	[alert beginSheetModalForWindow:[g_mrdpview window]
			  modalDelegate:g_mrdpview
			 didEndSelector:@selector(alertDidEnd:returnCode:contextInfo:)
			    contextInfo:nil];
}

/** *********************************************************************
 * just a terminate selector for above call
 ***********************************************************************/

- (void) alertDidEnd:(NSAlert *)a returnCode:(NSInteger)rc contextInfo:(void *)ci
{
	[NSApp terminate:nil];
}

- (void) onPasteboardTimerFired :(NSTimer*) timer
{
	int i;
	NSArray* types;
	
	i = (int) [pasteboard_rd changeCount];
	
	if (i != pasteboard_changecount)
	{
		pasteboard_changecount = i;
		types = [NSArray arrayWithObject:NSStringPboardType];
		NSString *str = [pasteboard_rd availableTypeFromArray:types];
		if (str != nil)
		{
			cliprdr_send_supported_format_list(rdp_instance);
		}
	}
}

- (void) setViewSize : (int) w : (int) h
{
	// store current dimensions
	width = w;
	height = h;
	
	// compute difference between window and client area
	NSRect outerRect = [[g_mrdpview window] frame];
	NSRect innerRect = [g_mrdpview frame];
	
	int widthDiff = outerRect.size.width - innerRect.size.width;
	int heightDiff = outerRect.size.height - innerRect.size.height;
	
	// we are not in RemoteApp mode, disable resizing
	outerRect.size.width = w + widthDiff;
	outerRect.size.height = h + heightDiff;
	[[g_mrdpview window] setMaxSize:outerRect.size];
	[[g_mrdpview window] setMinSize:outerRect.size];
	[[g_mrdpview window] setFrame:outerRect display:YES];
	
	// set client area to specified dimensions
	innerRect.size.width = w;
	innerRect.size.height = h;
	[g_mrdpview setFrame:innerRect];
}

/************************************************************************
 *                                                                      *
 *                              C functions                             *
 *                                                                      *
 ***********************************************************************/

/** *********************************************************************
 * connect to RDP server
 *
 * @return 0 on success, -1 on failure
 ***********************************************************************/


/** *********************************************************************
 * a callback given to freerdp_connect() to process the pre-connect operations.
 *
 * @param inst  - pointer to a rdp_freerdp struct that contains the connection's parameters, and
 *                will be filled with the appropriate informations.
 *
 * @return true if successful. false otherwise.
 ************************************************************************/

BOOL mac_pre_connect(freerdp* instance)
{
	int i;
	int len;
	int status;
	char* cptr;
	rdpSettings* settings;
	BOOL bitmap_cache;

	// setup callbacks
	instance->update->BeginPaint = mac_begin_paint;
	instance->update->EndPaint = mac_end_paint;
	instance->update->SetBounds = mac_set_bounds;
	//instance->update->BitmapUpdate = mac_bitmap_update;
	
	NSArray *args = [[NSProcessInfo processInfo] arguments];
	
	g_mrdpview->argc = (int) [args count];
	
	g_mrdpview->argv = malloc(sizeof(char *) * g_mrdpview->argc);
	
	if (g_mrdpview->argv == NULL)
		return FALSE;
	
	i = 0;
	
	for (NSString * str in args)
    {
        len = (int) ([str length] + 1);
        cptr = (char *) malloc(len);
        strcpy(cptr, [str UTF8String]);
        g_mrdpview->argv[i++] = cptr;
    }
	
	instance->context->argc = g_mrdpview->argc;
	instance->context->argv = g_mrdpview->argv;
	
	status = freerdp_client_parse_command_line_arguments(instance->context->argc, instance->context->argv, instance->settings);
	
	if (status < 0)
	{
		[NSApp terminate:nil];
		return TRUE;
	}

	freerdp_client_load_addins(instance->context->channels, instance->settings);

	settings = instance->settings;
	bitmap_cache = settings->BitmapCacheEnabled;
    
	instance->settings->ColorDepth = 32;
	instance->settings->SoftwareGdi = TRUE;
    
	settings->OsMajorType = OSMAJORTYPE_UNIX;
	settings->OsMinorType = OSMINORTYPE_NATIVE_XSERVER;
    
	settings->OrderSupport[NEG_DSTBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_PATBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_SCRBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_OPAQUE_RECT_INDEX] = TRUE;
	settings->OrderSupport[NEG_DRAWNINEGRID_INDEX] = FALSE;
	settings->OrderSupport[NEG_MULTIDSTBLT_INDEX] = FALSE;
	settings->OrderSupport[NEG_MULTIPATBLT_INDEX] = FALSE;
	settings->OrderSupport[NEG_MULTISCRBLT_INDEX] = FALSE;
	settings->OrderSupport[NEG_MULTIOPAQUERECT_INDEX] = TRUE;
	settings->OrderSupport[NEG_MULTI_DRAWNINEGRID_INDEX] = FALSE;
	settings->OrderSupport[NEG_LINETO_INDEX] = TRUE;
	settings->OrderSupport[NEG_POLYLINE_INDEX] = TRUE;
	settings->OrderSupport[NEG_MEMBLT_INDEX] = bitmap_cache;
    
	settings->OrderSupport[NEG_MEM3BLT_INDEX] = (settings->SoftwareGdi) ? TRUE : FALSE;
    
	settings->OrderSupport[NEG_MEMBLT_V2_INDEX] = bitmap_cache;
	settings->OrderSupport[NEG_MEM3BLT_V2_INDEX] = FALSE;
	settings->OrderSupport[NEG_SAVEBITMAP_INDEX] = FALSE;
	settings->OrderSupport[NEG_GLYPH_INDEX_INDEX] = TRUE;
	settings->OrderSupport[NEG_FAST_INDEX_INDEX] = TRUE;
	settings->OrderSupport[NEG_FAST_GLYPH_INDEX] = TRUE;
    
	settings->OrderSupport[NEG_POLYGON_SC_INDEX] = (settings->SoftwareGdi) ? FALSE : TRUE;
	settings->OrderSupport[NEG_POLYGON_CB_INDEX] = (settings->SoftwareGdi) ? FALSE : TRUE;
    
	settings->OrderSupport[NEG_ELLIPSE_SC_INDEX] = FALSE;
	settings->OrderSupport[NEG_ELLIPSE_CB_INDEX] = FALSE;
        
	[g_mrdpview setViewSize:instance->settings->DesktopWidth :instance->settings->DesktopHeight];
	
	freerdp_channels_pre_connect(instance->context->channels, instance);
	
	return TRUE;
}

/** *********************************************************************
 * a callback registered with freerdp_connect() to perform post-connection operations.
 * we get called only if the connection was initialized properly, and will continue
 * the initialization based on the newly created connection.
 *
 * @param   inst  - pointer to a rdp_freerdp struct
 *
 * @return true on success, false on failure
 *
 ************************************************************************/

BOOL mac_post_connect(freerdp* instance)
{
	int index;
	int fds[32];
	UINT32 flags;
	int rd_count = 0;
	int wr_count = 0;
	void* rd_fds[32];
	void* wr_fds[32];
	rdpPointer rdp_pointer;
	
	ZeroMemory(&rdp_pointer, sizeof(rdpPointer));
	rdp_pointer.size = sizeof(rdpPointer);
	rdp_pointer.New = mf_Pointer_New;
	rdp_pointer.Free = mf_Pointer_Free;
	rdp_pointer.Set = mf_Pointer_Set;
	rdp_pointer.SetNull = mf_Pointer_SetNull;
	rdp_pointer.SetDefault = mf_Pointer_SetDefault;

	flags = CLRBUF_32BPP;
	gdi_init(instance, flags, NULL);

	rdpGdi* gdi = instance->context->gdi;
	CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
	g_mrdpview->bitmap_context = CGBitmapContextCreate(gdi->primary_buffer, gdi->width, gdi->height, 8, gdi->width * 4, colorSpace, kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst);

	pointer_cache_register_callbacks(instance->update);
	graphics_register_pointer(instance->context->graphics, &rdp_pointer);
	
	/* register file descriptors with the RunLoop */
	if (!freerdp_get_fds(instance, rd_fds, &rd_count, 0, 0))
	{
		printf("mac_post_connect: freerdp_get_fds() failed!\n");
	}
	
	for (index = 0; index < rd_count; index++)
	{
		fds[index] = (int)(long)rd_fds[index];
	}
	register_fds(fds, rd_count, instance);
	
	/* register channel manager file descriptors with the RunLoop */
	if (!freerdp_channels_get_fds(instance->context->channels, instance, rd_fds, &rd_count, wr_fds, &wr_count))
	{
		printf("ERROR: freerdp_channels_get_fds() failed\n");
	}
	for (index = 0; index < rd_count; index++)
	{
		fds[index] = (int)(long)rd_fds[index];
	}
	
	register_channel_fds(fds, rd_count, instance);
	freerdp_channels_post_connect(instance->context->channels, instance);

	/* setup pasteboard (aka clipboard) for copy operations (write only) */
	g_mrdpview->pasteboard_wr = [NSPasteboard generalPasteboard];
	
	/* setup pasteboard for read operations */
	g_mrdpview->pasteboard_rd = [NSPasteboard generalPasteboard];
	g_mrdpview->pasteboard_changecount = (int) [g_mrdpview->pasteboard_rd changeCount];
	g_mrdpview->pasteboard_timer = [NSTimer scheduledTimerWithTimeInterval:0.5 target:g_mrdpview selector:@selector(onPasteboardTimerFired:) userInfo:nil repeats:YES];
	
	/* we want to be notified when window resizes */
	[[NSNotificationCenter defaultCenter] addObserver:g_mrdpview selector:@selector(windowDidResize:) name:NSWindowDidResizeNotification object:nil];
	
	return TRUE;
}

BOOL mac_authenticate(freerdp* instance, char** username, char** password, char** domain)
{
	PasswordDialog* dialog = [PasswordDialog new];

	dialog.serverHostname = [NSString stringWithCString:instance->settings->ServerHostname encoding:NSUTF8StringEncoding];

	if (*username)
		dialog.username = [NSString stringWithCString:*username encoding:NSUTF8StringEncoding];

	if (*password)
		dialog.password = [NSString stringWithCString:*password encoding:NSUTF8StringEncoding];

	BOOL ok = [dialog runModal];
    
	if (ok)
	{
		const char* submittedUsername = [dialog.username cStringUsingEncoding:NSUTF8StringEncoding];
		*username = malloc((strlen(submittedUsername) + 1) * sizeof(char));
		strcpy(*username, submittedUsername);

		const char* submittedPassword = [dialog.password cStringUsingEncoding:NSUTF8StringEncoding];
		*password = malloc((strlen(submittedPassword) + 1) * sizeof(char));
		strcpy(*password, submittedPassword);
	}

	return ok;
}

/** *********************************************************************
 * create a new mouse cursor
 *
 * @param context our context state
 * @param pointer information about the cursor to create
 *
 ************************************************************************/

void mf_Pointer_New(rdpContext* context, rdpPointer* pointer)
{
	NSRect rect;
	NSImage* image;
	NSPoint hotSpot;
	NSCursor* cursor;
	BYTE* cursor_data;
	NSMutableArray* ma;
	NSBitmapImageRep* bmiRep;
	MRDPCursor* mrdpCursor = [[MRDPCursor alloc] init];
	
	rect.size.width = pointer->width;
	rect.size.height = pointer->height;
	rect.origin.x = pointer->xPos;
	rect.origin.y = pointer->yPos;
	
	cursor_data = (BYTE*) malloc(rect.size.width * rect.size.height * 4);
	mrdpCursor->cursor_data = cursor_data;
	
	freerdp_alpha_cursor_convert(cursor_data, pointer->xorMaskData, pointer->andMaskData,
				     pointer->width, pointer->height, pointer->xorBpp, context->gdi->clrconv);
	
	// TODO if xorBpp is > 24 need to call freerdp_image_swap_color_order
	//         see file df_graphics.c
	
	/* store cursor bitmap image in representation - required by NSImage */
	bmiRep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:(unsigned char **) &cursor_data
							 pixelsWide:rect.size.width
							 pixelsHigh:rect.size.height
						      bitsPerSample:8
						    samplesPerPixel:4
							   hasAlpha:YES
							   isPlanar:NO
						     colorSpaceName:NSDeviceRGBColorSpace
						       bitmapFormat:0
							bytesPerRow:rect.size.width * 4
						       bitsPerPixel:0];
	mrdpCursor->bmiRep = bmiRep;
	
	/* create an image using above representation */
	image = [[NSImage alloc] initWithSize:[bmiRep size]];
	[image addRepresentation: bmiRep];
	[image setFlipped:NO];
	mrdpCursor->nsImage = image;
	
	/* need hotspot to create cursor */
	hotSpot.x = pointer->xPos;
	hotSpot.y = pointer->yPos;
	
	cursor = [[NSCursor alloc] initWithImage: image hotSpot:hotSpot];
	mrdpCursor->nsCursor = cursor;
	mrdpCursor->pointer = pointer;
	
	/* save cursor for later use in mf_Pointer_Set() */
	ma = g_mrdpview->cursors;
	[ma addObject:mrdpCursor];
}

/** *********************************************************************
 * release resources on specified cursor
 ************************************************************************/

void mf_Pointer_Free(rdpContext* context, rdpPointer* pointer)
{
	NSMutableArray* ma = g_mrdpview->cursors;
	
	for (MRDPCursor* cursor in ma)
	{
		if (cursor->pointer == pointer)
		{
			cursor->nsImage = nil;
			cursor->nsCursor = nil;
			cursor->bmiRep = nil;
			free(cursor->cursor_data);
			[ma removeObject:cursor];
			return;
		}
	}
}

/** *********************************************************************
 * set specified cursor as the current cursor
 ************************************************************************/

void mf_Pointer_Set(rdpContext* context, rdpPointer* pointer)
{
	NSMutableArray* ma = g_mrdpview->cursors;

	return; /* disable pointer until it is fixed */
	
	if (!g_mrdpview->mouseInClientArea)
		return;
	
	for (MRDPCursor* cursor in ma)
	{
		if (cursor->pointer == pointer)
		{
			[cursor->nsCursor set];
			return;
		}
	}
}

/** *********************************************************************
 * do not display any mouse cursor
 ***********************************************************************/

void mf_Pointer_SetNull(rdpContext* context)
{
	
}

/** *********************************************************************
 * display default mouse cursor
 ***********************************************************************/

void mf_Pointer_SetDefault(rdpContext* context)
{
	
}

/** *********************************************************************
 * create a new context - but all we really need to do is save state info
 ***********************************************************************/

int mac_context_new(freerdp* instance, rdpContext* context)
{
	[g_mrdpview saveStateInfo:instance :context];
	context->channels = freerdp_channels_new();
    return 0;
}

/** *********************************************************************
 * we don't do much over here
 ***********************************************************************/

void mac_context_free(freerdp* instance, rdpContext* context)
{
	
}

/** *********************************************************************
 * clip drawing surface so nothing is drawn outside specified bounds
 ***********************************************************************/

void mac_set_bounds(rdpContext* context, rdpBounds* bounds)
{
	
}

/** *********************************************************************
 * we don't do much over here
 ***********************************************************************/

void mac_bitmap_update(rdpContext* context, BITMAP_UPDATE* bitmap)
{
	
}

/** *********************************************************************
 * we don't do much over here
 ***********************************************************************/

void mac_begin_paint(rdpContext* context)
{
	rdpGdi* gdi = context->gdi;
	gdi->primary->hdc->hwnd->invalid->null = 1;
}

/** *********************************************************************
 * RDP server wants us to draw new data in the view
 ***********************************************************************/

void mac_end_paint(rdpContext* context)
{
	int i;
	rdpGdi* gdi;
	NSRect drawRect;
	
	if ((context == 0) || (context->gdi == 0))
		return;
	
	if (context->gdi->primary->hdc->hwnd->invalid->null)
		return;
	
	if (context->gdi->drawing != context->gdi->primary)
		return;
	
	gdi = ((rdpContext*)g_mrdpview->rdp_context)->gdi;
    
	for (i = 0; i < gdi->primary->hdc->hwnd->ninvalid; i++)
	{
		drawRect.origin.x = gdi->primary->hdc->hwnd->cinvalid[i].x;
		drawRect.origin.y = gdi->primary->hdc->hwnd->cinvalid[i].y;
		drawRect.size.width = gdi->primary->hdc->hwnd->cinvalid[i].w;
		drawRect.size.height = gdi->primary->hdc->hwnd->cinvalid[i].h;
		windows_to_apple_cords(&drawRect);
		[g_mrdpview setNeedsDisplayInRect:drawRect];
	}
	
	gdi->primary->hdc->hwnd->ninvalid = 0;
}

/** *********************************************************************
 * called when data is available on a socket
 ***********************************************************************/

void skt_activity_cb(CFSocketRef s, CFSocketCallBackType callbackType,
                     CFDataRef address, const void* data, void* info)
{
	if (!freerdp_check_fds(info))
	{
		/* lost connection or did not connect */
		[NSApp terminate:nil];
	}
}

/** *********************************************************************
 * called when data is available on a virtual channel
 ***********************************************************************/

void channel_activity_cb(CFSocketRef s, CFSocketCallBackType callbackType,
			 CFDataRef address, const void* data, void* info)
{
	wMessage* event;
	freerdp* instance = (freerdp*) info;
	
	freerdp_channels_check_fds(instance->context->channels, instance);
	event = freerdp_channels_pop_event(instance->context->channels);
	
	if (event)
	{
		switch (GetMessageClass(event->id))
		{
			case CliprdrChannel_Class:
				process_cliprdr_event(instance, event);
				break;
		}
	}
}

/** *********************************************************************
 * setup callbacks for data availability on sockets
 ***********************************************************************/

int register_fds(int* fds, int count, void* instance)
{
	int i;
	CFSocketRef skt_ref;
	CFSocketContext skt_context = { 0, instance, NULL, NULL, NULL };
	
	for (i = 0; i < count; i++)
	{
		skt_ref = CFSocketCreateWithNative(NULL, fds[i], kCFSocketReadCallBack, skt_activity_cb, &skt_context);
		g_mrdpview->run_loop_src = CFSocketCreateRunLoopSource(NULL, skt_ref, 0);
		CFRunLoopAddSource(CFRunLoopGetCurrent(), g_mrdpview->run_loop_src, kCFRunLoopDefaultMode);
		CFRelease(skt_ref);
	}
	
	return 0;
}

/** *********************************************************************
 * setup callbacks for data availability on channels
 ***********************************************************************/

int register_channel_fds(int* fds, int count, void* instance)
{
	int i;
	CFSocketRef skt_ref;
	CFSocketContext skt_context = { 0, instance, NULL, NULL, NULL };
	
	for (i = 0; i < count; i++)
	{
		skt_ref = CFSocketCreateWithNative(NULL, fds[i], kCFSocketReadCallBack, channel_activity_cb, &skt_context);
		g_mrdpview->run_loop_src_channels = CFSocketCreateRunLoopSource(NULL, skt_ref, 0);
		CFRunLoopAddSource(CFRunLoopGetCurrent(), g_mrdpview->run_loop_src_channels, kCFRunLoopDefaultMode);
		CFRelease(skt_ref);
	}
	
	return 0;
}

/** *********************************************************************
 * called when channel data is available
 ***********************************************************************/

int receive_channel_data(freerdp* instance, int chan_id, BYTE* data, int size, int flags, int total_size)
{
	return freerdp_channels_data(instance, chan_id, data, size, flags, total_size);
}

/**
 * Used to load plugins based on the commandline parameters.
 * This function is provided as a parameter to freerdp_parse_args(), that will call it
 * each time a plugin name is found on the command line.
 * This function just calls freerdp_channels_load_plugin() for the given plugin, and always returns 1.
 *
 * @param settings
 * @param name
 * @param plugin_data
 * @param user_data
 *
 * @return 1
 */

int process_plugin_args(rdpSettings* settings, const char* name, RDP_PLUGIN_DATA* plugin_data, void* user_data)
{
	rdpChannels* channels = (rdpChannels*) user_data;
	
	freerdp_channels_load_plugin(channels, settings, name, plugin_data);
	
	return 1;
}

/*
 * stuff related to clipboard redirection
 */

/**
 * remote system has requested clipboard data from local system
 */

void cliprdr_process_cb_data_request_event(freerdp* instance)
{
	int len;
	NSArray* types;
	RDP_CB_DATA_RESPONSE_EVENT* event;
	
	event = (RDP_CB_DATA_RESPONSE_EVENT*) freerdp_event_new(CliprdrChannel_Class, CliprdrChannel_DataResponse, NULL, NULL);
	
	types = [NSArray arrayWithObject:NSStringPboardType];
	NSString* str = [g_mrdpview->pasteboard_rd availableTypeFromArray:types];
	
	if (str == nil)
	{
		event->data = NULL;
		event->size = 0;
	}
	else
	{
		NSString* data = [g_mrdpview->pasteboard_rd stringForType:NSStringPboardType];
		len = (int) ([data length] * 2 + 2);
		event->data = malloc(len);
		[data getCString:(char *) event->data maxLength:len encoding:NSUnicodeStringEncoding];
		event->size = len;
	}
	
	freerdp_channels_send_event(instance->context->channels, (wMessage*) event);
}

void cliprdr_send_data_request(freerdp* instance, UINT32 format)
{
	RDP_CB_DATA_REQUEST_EVENT* event;
	
	event = (RDP_CB_DATA_REQUEST_EVENT*) freerdp_event_new(CliprdrChannel_Class, CliprdrChannel_DataRequest, NULL, NULL);
	
	event->format = format;
	freerdp_channels_send_event(instance->context->channels, (wMessage*) event);
}

/**
 * at the moment, only the following formats are supported
 *    CB_FORMAT_TEXT
 *    CB_FORMAT_UNICODETEXT
 */

void cliprdr_process_cb_data_response_event(freerdp* instance, RDP_CB_DATA_RESPONSE_EVENT* event)
{
	NSString* str;
	NSArray* types;
	
	if (event->size == 0)
		return;
	
	if (g_mrdpview->pasteboard_format == CB_FORMAT_TEXT || g_mrdpview->pasteboard_format == CB_FORMAT_UNICODETEXT)
	{
		str = [[NSString alloc] initWithCharacters:(unichar *) event->data length:event->size / 2];
		types = [[NSArray alloc] initWithObjects:NSStringPboardType, nil];
		[g_mrdpview->pasteboard_wr declareTypes:types owner:g_mrdpview];
		[g_mrdpview->pasteboard_wr setString:str forType:NSStringPboardType];
	}
}

void cliprdr_process_cb_monitor_ready_event(freerdp* instance)
{
	wMessage* event;
	RDP_CB_FORMAT_LIST_EVENT* format_list_event;
	
	event = freerdp_event_new(CliprdrChannel_Class, CliprdrChannel_FormatList, NULL, NULL);
	
	format_list_event = (RDP_CB_FORMAT_LIST_EVENT*) event;
	format_list_event->num_formats = 0;
	
	freerdp_channels_send_event(instance->context->channels, event);
}

/**
 * list of supported clipboard formats; currently only the following are supported
 *    CB_FORMAT_TEXT
 *    CB_FORMAT_UNICODETEXT
 */

void cliprdr_process_cb_format_list_event(freerdp* instance, RDP_CB_FORMAT_LIST_EVENT* event)
{
	int i;
	
	if (event->num_formats == 0)
		return;
	
	for (i = 0; i < event->num_formats; i++)
	{
		switch (event->formats[i])
		{
			case CB_FORMAT_RAW:
				printf("CB_FORMAT_RAW: not yet supported\n");
				break;
				
			case CB_FORMAT_TEXT:
			case CB_FORMAT_UNICODETEXT:
				g_mrdpview->pasteboard_format = CB_FORMAT_UNICODETEXT;
				cliprdr_send_data_request(instance, CB_FORMAT_UNICODETEXT);
				return;
				break;
				
			case CB_FORMAT_DIB:
				printf("CB_FORMAT_DIB: not yet supported\n");
				break;
				
			case CB_FORMAT_HTML:
				printf("CB_FORMAT_HTML\n");
				break;
				
			case CB_FORMAT_PNG:
				printf("CB_FORMAT_PNG: not yet supported\n");
				break;
				
			case CB_FORMAT_JPEG:
				printf("CB_FORMAT_JPEG: not yet supported\n");
				break;
				
			case CB_FORMAT_GIF:
				printf("CB_FORMAT_GIF: not yet supported\n");
				break;
		}
	}
}

void process_cliprdr_event(freerdp* instance, wMessage* event)
{
	if (event)
	{
		switch (GetMessageType(event->id))
		{
				/*
				 * Monitor Ready PDU is sent by server to indicate that it has been
				 * initialized and is ready. This PDU is transmitted by the server after it has sent
				 * Clipboard Capabilities PDU
				 */
			case CliprdrChannel_MonitorReady:
				cliprdr_process_cb_monitor_ready_event(instance);
				break;
				
				/* 
				 * The Format List PDU is sent either by the client or the server when its
				 * local system clipboard is updated with new clipboard data. This PDU
				 * contains the Clipboard Format ID and name pairs of the new Clipboard
				 * Formats on the clipboard
				 */
			case CliprdrChannel_FormatList:
				cliprdr_process_cb_format_list_event(instance, (RDP_CB_FORMAT_LIST_EVENT*) event);
				break;
				
				/*
				 * The Format Data Request PDU is sent by the receipient of the Format List PDU.
				 * It is used to request the data for one of the formats that was listed in the
				 * Format List PDU
				 */
			case CliprdrChannel_DataRequest:
				cliprdr_process_cb_data_request_event(instance);
				break;
				
				/* 
				 * The Format Data Response PDU is sent as a reply to the Format Data Request PDU.
				 * It is used to indicate whether processing of the Format Data Request PDU
				 * was successful. If the processing was successful, the Format Data Response PDU
				 * includes the contents of the requested clipboard data
				 */
			case CliprdrChannel_DataResponse:
				cliprdr_process_cb_data_response_event(instance, (RDP_CB_DATA_RESPONSE_EVENT*) event);
				break;
				
			default:
				printf("process_cliprdr_event: unknown event type %d\n", GetMessageType(event->id));
				break;
		}
		
		freerdp_event_free(event);
	}
}

void cliprdr_send_supported_format_list(freerdp* instance)
{
	RDP_CB_FORMAT_LIST_EVENT* event;
	
	event = (RDP_CB_FORMAT_LIST_EVENT*) freerdp_event_new(CliprdrChannel_Class, CliprdrChannel_FormatList, NULL, NULL);
	
	event->formats = (UINT32*) malloc(sizeof(UINT32) * 1);
	event->num_formats = 1;
	event->formats[0] = CB_FORMAT_UNICODETEXT;
	
	freerdp_channels_send_event(instance->context->channels, (wMessage*) event);
}


/**
 * given a rect with 0,0 at the bottom left (apple cords)
 * convert it to a rect with 0,0 at the top left (windows cords)
 */

void apple_to_windows_cords(NSRect* r)
{
	r->origin.y = g_mrdpview->height - (r->origin.y + r->size.height);
}

/**
 * given a rect with 0,0 at the top left (windows cords)
 * convert it to a rect with 0,0 at the bottom left (apple cords)
 */

void windows_to_apple_cords(NSRect* r)
{
	r->origin.y = g_mrdpview->height - (r->origin.y + r->size.height);
}

@end
