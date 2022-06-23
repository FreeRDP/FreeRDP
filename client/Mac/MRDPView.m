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

#include <winpr/windows.h>

#include "mf_client.h"
#import "mfreerdp.h"
#import "MRDPView.h"
#import "MRDPCursor.h"
#import "Clipboard.h"
#import "PasswordDialog.h"
#import "CertificateDialog.h"

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/input.h>
#include <winpr/synch.h>
#include <winpr/sysinfo.h>

#include <freerdp/constants.h>

#import "freerdp/freerdp.h"
#import "freerdp/types.h"
#import "freerdp/channels/channels.h"
#import "freerdp/gdi/gdi.h"
#import "freerdp/gdi/dc.h"
#import "freerdp/gdi/region.h"
#import "freerdp/graphics.h"
#import "freerdp/client/file.h"
#import "freerdp/client/cmdline.h"
#import "freerdp/log.h"

#import <CoreGraphics/CoreGraphics.h>

#define TAG CLIENT_TAG("mac")

static BOOL mf_Pointer_New(rdpContext *context, rdpPointer *pointer);
static void mf_Pointer_Free(rdpContext *context, rdpPointer *pointer);
static BOOL mf_Pointer_Set(rdpContext *context, rdpPointer *pointer);
static BOOL mf_Pointer_SetNull(rdpContext *context);
static BOOL mf_Pointer_SetDefault(rdpContext *context);
static BOOL mf_Pointer_SetPosition(rdpContext *context, UINT32 x, UINT32 y);

static BOOL mac_begin_paint(rdpContext *context);
static BOOL mac_end_paint(rdpContext *context);
static BOOL mac_desktop_resize(rdpContext *context);

static void input_activity_cb(freerdp *instance);

static DWORD WINAPI mac_client_thread(void *param);

@implementation MRDPView

@synthesize is_connected;

- (int)rdpStart:(rdpContext *)rdp_context
{
	rdpSettings *settings;
	EmbedWindowEventArgs e;
	[self initializeView];

	WINPR_ASSERT(rdp_context);
	context = rdp_context;
	mfc = (mfContext *)rdp_context;

	instance = context->instance;
	WINPR_ASSERT(instance);

	settings = context->settings;
	WINPR_ASSERT(settings);

	EventArgsInit(&e, "mfreerdp");
	e.embed = TRUE;
	e.handle = (void *)self;
	PubSub_OnEmbedWindow(context->pubSub, context, &e);
	NSScreen *screen = [[NSScreen screens] objectAtIndex:0];
	NSRect screenFrame = [screen frame];

	if (settings->Fullscreen)
	{
		settings->DesktopWidth = screenFrame.size.width;
		settings->DesktopHeight = screenFrame.size.height;
		[self enterFullScreenMode:[NSScreen mainScreen] withOptions:nil];
	}
	else
	{
		[self exitFullScreenModeWithOptions:nil];
	}

	mfc->client_height = settings->DesktopHeight;
	mfc->client_width = settings->DesktopWidth;

	if (!(mfc->common.thread =
	          CreateThread(NULL, 0, mac_client_thread, (void *)context, 0, &mfc->mainThreadId)))
	{
		WLog_ERR(TAG, "failed to create client thread");
		return -1;
	}

	return 0;
}

DWORD WINAPI mac_client_thread(void *param)
{
	@autoreleasepool
	{
		int status;
		DWORD rc;
		HANDLE events[16] = { 0 };
		HANDLE inputEvent;
		DWORD nCount;
		DWORD nCountTmp;
		DWORD nCountBase;
		rdpContext *context = (rdpContext *)param;
		mfContext *mfc = (mfContext *)context;
		freerdp *instance = context->instance;
		MRDPView *view = mfc->view;
		rdpSettings *settings = context->settings;
		status = freerdp_connect(context->instance);

		if (!status)
		{
			[view setIs_connected:0];
			return 0;
		}

		[view setIs_connected:1];
		nCount = 0;
		events[nCount++] = mfc->stopEvent;

		if (!(inputEvent =
		          freerdp_get_message_queue_event_handle(instance, FREERDP_INPUT_MESSAGE_QUEUE)))
		{
			WLog_ERR(TAG, "failed to get input event handle");
			goto disconnect;
		}

		events[nCount++] = inputEvent;

		nCountBase = nCount;

		while (!freerdp_shall_disconnect_context(instance->context))
		{
			nCount = nCountBase;
			{
				if (!(nCountTmp = freerdp_get_event_handles(context, &events[nCount], 16 - nCount)))
				{
					WLog_ERR(TAG, "freerdp_get_event_handles failed");
					break;
				}

				nCount += nCountTmp;
			}
			rc = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

			if (rc >= (WAIT_OBJECT_0 + nCount))
			{
				WLog_ERR(TAG, "WaitForMultipleObjects failed (0x%08X)", rc);
				break;
			}

			if (rc == WAIT_OBJECT_0)
			{
				/* stop event triggered */
				break;
			}

			if (WaitForSingleObject(inputEvent, 0) == WAIT_OBJECT_0)
			{
				input_activity_cb(instance);
			}

			{
				if (!freerdp_check_event_handles(context))
				{
					WLog_ERR(TAG, "freerdp_check_event_handles failed");
					break;
				}
			}
		}

	disconnect:
		[view setIs_connected:0];
		freerdp_disconnect(instance);

		ExitThread(0);
		return 0;
	}
}

- (id)initWithFrame:(NSRect)frame
{
	self = [super initWithFrame:frame];

	if (self)
	{
		// Initialization code here.
	}

	return self;
}

- (void)viewDidLoad
{
	[self initializeView];
}

- (void)initializeView
{
	if (!initialized)
	{
		cursors = [[NSMutableArray alloc] initWithCapacity:10];
		// setup a mouse tracking area
		NSTrackingArea *trackingArea = [[NSTrackingArea alloc]
		    initWithRect:[self visibleRect]
		         options:NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved |
		                 NSTrackingCursorUpdate | NSTrackingEnabledDuringMouseDrag |
		                 NSTrackingActiveWhenFirstResponder
		           owner:self
		        userInfo:nil];
		[self addTrackingArea:trackingArea];
		// Set the default cursor
		currentCursor = [NSCursor arrowCursor];
		initialized = YES;
	}
}

- (void)setCursor:(NSCursor *)cursor
{
	self->currentCursor = cursor;
	dispatch_async(dispatch_get_main_queue(), ^{
		[[self window] invalidateCursorRectsForView:self];
	});
}

- (void)resetCursorRects
{
	[self addCursorRect:[self visibleRect] cursor:currentCursor];
}

- (BOOL)acceptsFirstResponder
{
	return YES;
}

- (void)mouseMoved:(NSEvent *)event
{
	[super mouseMoved:event];

	if (!self.is_connected)
		return;

	NSPoint loc = [event locationInWindow];
	int x = (int)loc.x;
	int y = (int)loc.y;
	mf_scale_mouse_event(context, PTR_FLAGS_MOVE, x, y);
}

- (void)mouseDown:(NSEvent *)event
{
	[super mouseDown:event];

	if (!self.is_connected)
		return;

	NSPoint loc = [event locationInWindow];
	int x = (int)loc.x;
	int y = (int)loc.y;
	mf_press_mouse_button(context, 0, x, y, TRUE);
}

- (void)mouseUp:(NSEvent *)event
{
	[super mouseUp:event];

	if (!self.is_connected)
		return;

	NSPoint loc = [event locationInWindow];
	int x = (int)loc.x;
	int y = (int)loc.y;
	mf_press_mouse_button(context, 0, x, y, FALSE);
}

- (void)rightMouseDown:(NSEvent *)event
{
	[super rightMouseDown:event];

	if (!self.is_connected)
		return;

	NSPoint loc = [event locationInWindow];
	int x = (int)loc.x;
	int y = (int)loc.y;
	mf_press_mouse_button(context, 1, x, y, TRUE);
}

- (void)rightMouseUp:(NSEvent *)event
{
	[super rightMouseUp:event];

	if (!self.is_connected)
		return;

	NSPoint loc = [event locationInWindow];
	int x = (int)loc.x;
	int y = (int)loc.y;
	mf_press_mouse_button(context, 1, x, y, FALSE);
}

- (void)otherMouseDown:(NSEvent *)event
{
	[super otherMouseDown:event];

	if (!self.is_connected)
		return;

	NSPoint loc = [event locationInWindow];
	int x = (int)loc.x;
	int y = (int)loc.y;
	int pressed = [event buttonNumber];
	mf_press_mouse_button(context, pressed, x, y, TRUE);
}

- (void)otherMouseUp:(NSEvent *)event
{
	[super otherMouseUp:event];

	if (!self.is_connected)
		return;

	NSPoint loc = [event locationInWindow];
	int x = (int)loc.x;
	int y = (int)loc.y;
	int pressed = [event buttonNumber];
	mf_press_mouse_button(context, pressed, x, y, FALSE);
}

- (void)scrollWheel:(NSEvent *)event
{
	UINT16 flags;
	[super scrollWheel:event];

	if (!self.is_connected)
		return;

	float dx = [event deltaX];
	float dy = [event deltaY];
	/* 1 event = 120 units */
	UINT16 units = 0;

	if (fabsf(dy) > FLT_EPSILON)
	{
		flags = PTR_FLAGS_WHEEL;
		units = fabsf(dy) * 120;

		if (dy < 0)
			flags |= PTR_FLAGS_WHEEL_NEGATIVE;
	}
	else if (fabsf(dx) > FLT_EPSILON)
	{
		flags = PTR_FLAGS_HWHEEL;
		units = fabsf(dx) * 120;

		if (dx > 0)
			flags |= PTR_FLAGS_WHEEL_NEGATIVE;
	}
	else
		return;

	/* Wheel rotation steps:
	 *
	 * positive: 0 ... 0xFF  -> slow ... fast
	 * negative: 0 ... 0xFF  -> fast ... slow
	 */
	UINT16 step = units;
	if (step > 0xFF)
		step = 0xFF;

	/* Negative rotation, so count down steps from top
	 * 9bit twos complement */
	if (flags & PTR_FLAGS_WHEEL_NEGATIVE)
		step = 0x100 - step;

	mf_scale_mouse_event(context, flags | step, 0, 0);
}

- (void)mouseDragged:(NSEvent *)event
{
	[super mouseDragged:event];

	if (!self.is_connected)
		return;

	NSPoint loc = [event locationInWindow];
	int x = (int)loc.x;
	int y = (int)loc.y;
	// send mouse motion event to RDP server
	mf_scale_mouse_event(context, PTR_FLAGS_MOVE, x, y);
}

DWORD fixKeyCode(DWORD keyCode, unichar keyChar, enum APPLE_KEYBOARD_TYPE type)
{
	/**
	 * In 99% of cases, the given key code is truly keyboard independent.
	 * This function handles the remaining 1% of edge cases.
	 *
	 * Hungarian Keyboard: This is 'QWERTZ' and not 'QWERTY'.
	 * The '0' key is on the left of the '1' key, where '~' is on a US keyboard.
	 * A special 'i' letter key with acute is found on the right of the left shift key.
	 * On the hungarian keyboard, the 'i' key is at the left of the 'Y' key
	 * Some international keyboards have a corresponding key which would be at
	 * the left of the 'Z' key when using a QWERTY layout.
	 *
	 * The Apple Hungarian keyboard sends inverted key codes for the '0' and 'i' keys.
	 * When using the US keyboard layout, key codes are left as-is (inverted).
	 * When using the Hungarian keyboard layout, key codes are swapped (non-inverted).
	 * This means that when using the Hungarian keyboard layout with a US keyboard,
	 * the keys corresponding to '0' and 'i' will effectively be inverted.
	 *
	 * To fix the '0' and 'i' key inversion, we use the corresponding output character
	 * provided by OS X and check for a character to key code mismatch: for instance,
	 * when the output character is '0' for the key code corresponding to the 'i' key.
	 */
#if 0
	switch (keyChar)
	{
		case '0':
		case 0x00A7: /* section sign */
			if (keyCode == APPLE_VK_ISO_Section)
				keyCode = APPLE_VK_ANSI_Grave;

			break;

		case 0x00ED: /* latin small letter i with acute */
		case 0x00CD: /* latin capital letter i with acute */
			if (keyCode == APPLE_VK_ANSI_Grave)
				keyCode = APPLE_VK_ISO_Section;

			break;
	}

#endif

	/* Perform keycode correction for all ISO keyboards */

	if (type == APPLE_KEYBOARD_TYPE_ISO)
	{
		if (keyCode == APPLE_VK_ANSI_Grave)
			keyCode = APPLE_VK_ISO_Section;
		else if (keyCode == APPLE_VK_ISO_Section)
			keyCode = APPLE_VK_ANSI_Grave;
	}

	return keyCode;
}

- (void)keyDown:(NSEvent *)event
{
	DWORD keyCode;
	DWORD keyFlags;
	DWORD vkcode;
	DWORD scancode;
	unichar keyChar;
	NSString *characters;

	if (!is_connected)
		return;

	keyFlags = KBD_FLAGS_DOWN;
	keyCode = [event keyCode];
	characters = [event charactersIgnoringModifiers];

	if ([characters length] > 0)
	{
		keyChar = [characters characterAtIndex:0];
		keyCode = fixKeyCode(keyCode, keyChar, mfc->appleKeyboardType);
	}

	vkcode = GetVirtualKeyCodeFromKeycode(keyCode + 8, KEYCODE_TYPE_APPLE);
	scancode = GetVirtualScanCodeFromVirtualKeyCode(vkcode, 4);
	keyFlags |= (scancode & KBDEXT) ? KBDEXT : 0;
	scancode &= 0xFF;
	vkcode &= 0xFF;
#if 0
	WLog_ERR(TAG,
	         "keyDown: keyCode: 0x%04X scancode: 0x%04X vkcode: 0x%04X keyFlags: %d name: %s",
	         keyCode, scancode, vkcode, keyFlags, GetVirtualKeyName(vkcode));
#endif
	sync_keyboard_state(instance);
	WINPR_ASSERT(instance->context);
	freerdp_input_send_keyboard_event(instance->context->input, keyFlags, scancode);
}

- (void)keyUp:(NSEvent *)event
{
	DWORD keyCode;
	DWORD keyFlags;
	DWORD vkcode;
	DWORD scancode;
	unichar keyChar;
	NSString *characters;

	if (!is_connected)
		return;

	keyFlags = KBD_FLAGS_RELEASE;
	keyCode = [event keyCode];
	characters = [event charactersIgnoringModifiers];

	if ([characters length] > 0)
	{
		keyChar = [characters characterAtIndex:0];
		keyCode = fixKeyCode(keyCode, keyChar, mfc->appleKeyboardType);
	}

	vkcode = GetVirtualKeyCodeFromKeycode(keyCode + 8, KEYCODE_TYPE_APPLE);
	scancode = GetVirtualScanCodeFromVirtualKeyCode(vkcode, 4);
	keyFlags |= (scancode & KBDEXT) ? KBDEXT : 0;
	scancode &= 0xFF;
	vkcode &= 0xFF;
#if 0
	WLog_DBG(TAG,
	         "keyUp: key: 0x%04X scancode: 0x%04X vkcode: 0x%04X keyFlags: %d name: %s",
	         keyCode, scancode, vkcode, keyFlags, GetVirtualKeyName(vkcode));
#endif
	WINPR_ASSERT(instance->context);
	freerdp_input_send_keyboard_event(instance->context->input, keyFlags, scancode);
}

- (void)flagsChanged:(NSEvent *)event
{
	int key;
	DWORD keyFlags;
	DWORD vkcode;
	DWORD scancode;
	DWORD modFlags;
	rdpInput *input;

	if (!is_connected)
		return;

	keyFlags = 0;
	key = [event keyCode] + 8;
	modFlags = [event modifierFlags] & NSEventModifierFlagDeviceIndependentFlagsMask;
	vkcode = GetVirtualKeyCodeFromKeycode(key, KEYCODE_TYPE_APPLE);
	scancode = GetVirtualScanCodeFromVirtualKeyCode(vkcode, 4);
	keyFlags |= (scancode & KBDEXT) ? KBDEXT : 0;
	scancode &= 0xFF;
	vkcode &= 0xFF;
#if 0
	WLog_DBG(TAG,
	         "flagsChanged: key: 0x%04X scancode: 0x%04X vkcode: 0x%04X extended: %d name: %s modFlags: 0x%04X",
	         key - 8, scancode, vkcode, keyFlags, GetVirtualKeyName(vkcode), modFlags);

	if (modFlags & NSEventModifierFlagCapsLock)
		WLog_DBG(TAG,  "NSEventModifierFlagCapsLock");

	if (modFlags & NSEventModifierFlagShift)
		WLog_DBG(TAG,  "NSEventModifierFlagShift");

	if (modFlags & NSEventModifierFlagControl)
		WLog_DBG(TAG,  "NSEventModifierFlagControl");

	if (modFlags & NSEventModifierFlagOption)
		WLog_DBG(TAG,  "NSEventModifierFlagOption");

	if (modFlags & NSEventModifierFlagCommand)
		WLog_DBG(TAG,  "NSEventModifierFlagCommand");

	if (modFlags & NSEventModifierFlagNumericPad)
		WLog_DBG(TAG,  "NSEventModifierFlagNumericPad");

	if (modFlags & NSEventModifierFlagHelp)
		WLog_DBG(TAG,  "NSEventModifierFlagHelp");

#endif

	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);

	input = instance->context->input;

	if ((modFlags & NSEventModifierFlagCapsLock) && !(kbdModFlags & NSEventModifierFlagCapsLock))
		freerdp_input_send_keyboard_event(input, keyFlags | KBD_FLAGS_DOWN, scancode);
	else if (!(modFlags & NSEventModifierFlagCapsLock) &&
	         (kbdModFlags & NSEventModifierFlagCapsLock))
		freerdp_input_send_keyboard_event(input, keyFlags | KBD_FLAGS_RELEASE, scancode);

	if ((modFlags & NSEventModifierFlagShift) && !(kbdModFlags & NSEventModifierFlagShift))
		freerdp_input_send_keyboard_event(input, keyFlags | KBD_FLAGS_DOWN, scancode);
	else if (!(modFlags & NSEventModifierFlagShift) && (kbdModFlags & NSEventModifierFlagShift))
		freerdp_input_send_keyboard_event(input, keyFlags | KBD_FLAGS_RELEASE, scancode);

	if ((modFlags & NSEventModifierFlagControl) && !(kbdModFlags & NSEventModifierFlagControl))
		freerdp_input_send_keyboard_event(input, keyFlags | KBD_FLAGS_DOWN, scancode);
	else if (!(modFlags & NSEventModifierFlagControl) && (kbdModFlags & NSEventModifierFlagControl))
		freerdp_input_send_keyboard_event(input, keyFlags | KBD_FLAGS_RELEASE, scancode);

	if ((modFlags & NSEventModifierFlagOption) && !(kbdModFlags & NSEventModifierFlagOption))
		freerdp_input_send_keyboard_event(input, keyFlags | KBD_FLAGS_DOWN, scancode);
	else if (!(modFlags & NSEventModifierFlagOption) && (kbdModFlags & NSEventModifierFlagOption))
		freerdp_input_send_keyboard_event(input, keyFlags | KBD_FLAGS_RELEASE, scancode);

	if ((modFlags & NSEventModifierFlagCommand) && !(kbdModFlags & NSEventModifierFlagCommand))
		freerdp_input_send_keyboard_event(input, keyFlags | KBD_FLAGS_DOWN, scancode);
	else if (!(modFlags & NSEventModifierFlagCommand) && (kbdModFlags & NSEventModifierFlagCommand))
		freerdp_input_send_keyboard_event(input, keyFlags | KBD_FLAGS_RELEASE, scancode);

	if ((modFlags & NSEventModifierFlagNumericPad) &&
	    !(kbdModFlags & NSEventModifierFlagNumericPad))
		freerdp_input_send_keyboard_event(input, keyFlags | KBD_FLAGS_DOWN, scancode);
	else if (!(modFlags & NSEventModifierFlagNumericPad) &&
	         (kbdModFlags & NSEventModifierFlagNumericPad))
		freerdp_input_send_keyboard_event(input, keyFlags | KBD_FLAGS_RELEASE, scancode);

	if ((modFlags & NSEventModifierFlagHelp) && !(kbdModFlags & NSEventModifierFlagHelp))
		freerdp_input_send_keyboard_event(input, keyFlags | KBD_FLAGS_DOWN, scancode);
	else if (!(modFlags & NSEventModifierFlagHelp) && (kbdModFlags & NSEventModifierFlagHelp))
		freerdp_input_send_keyboard_event(input, keyFlags | KBD_FLAGS_RELEASE, scancode);

	kbdModFlags = modFlags;
}

- (void)releaseResources
{
	int i;

	for (i = 0; i < argc; i++)
		free(argv[i]);

	if (!is_connected)
		return;

	free(pixel_data);
}

- (void)drawRect:(NSRect)rect
{
	if (!context)
		return;

	if (self->bitmap_context)
	{
		CGContextRef cgContext = [[NSGraphicsContext currentContext] CGContext];
		CGImageRef cgImage = CGBitmapContextCreateImage(self->bitmap_context);
		CGContextSaveGState(cgContext);
		CGContextClipToRect(
		    cgContext, CGRectMake(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height));
		CGContextDrawImage(cgContext,
		                   CGRectMake(0, 0, [self bounds].size.width, [self bounds].size.height),
		                   cgImage);
		CGContextRestoreGState(cgContext);
		CGImageRelease(cgImage);
	}
	else
	{
		/* Fill the screen with black */
		[[NSColor blackColor] set];
		NSRectFill([self bounds]);
	}
}

- (void)onPasteboardTimerFired:(NSTimer *)timer
{
	const void *data;
	UINT32 size;
	UINT32 formatId;
	BOOL formatMatch;
	int changeCount;
	NSData *formatData;
	NSString *formatString;
	const char *formatType;
	NSPasteboardItem *item;
	changeCount = (int)[pasteboard_rd changeCount];

	if (changeCount == pasteboard_changecount)
		return;

	pasteboard_changecount = changeCount;
	NSArray *items = [pasteboard_rd pasteboardItems];

	if ([items count] < 1)
		return;

	item = [items objectAtIndex:0];
	/**
	 * System-Declared Uniform Type Identifiers:
	 * https://developer.apple.com/library/ios/documentation/Miscellaneous/Reference/UTIRef/Articles/System-DeclaredUniformTypeIdentifiers.html
	 */
	formatMatch = FALSE;

	for (NSString *type in [item types])
	{
		formatType = [type UTF8String];

		if (strcmp(formatType, "public.utf8-plain-text") == 0)
		{
			formatData = [item dataForType:type];

			if (formatData == nil)
			{
				break;
			}

			formatString = [[NSString alloc] initWithData:formatData encoding:NSUTF8StringEncoding];

			size = strlen([formatString UTF8String]) + 1;
			data = [formatString UTF8String];
			formatId = ClipboardRegisterFormat(mfc->clipboard, "UTF8_STRING");
			ClipboardSetData(mfc->clipboard, formatId, data, size);
			[formatString release];

			formatMatch = TRUE;

			break;
		}
	}

	if (!formatMatch)
		ClipboardEmpty(mfc->clipboard);

	if (mfc->clipboardSync)
		mac_cliprdr_send_client_format_list(mfc->cliprdr);
}

- (void)pause
{
	dispatch_async(dispatch_get_main_queue(), ^{
		[self->pasteboard_timer invalidate];
	});
	NSArray *trackingAreas = self.trackingAreas;

	for (NSTrackingArea *ta in trackingAreas)
	{
		[self removeTrackingArea:ta];
	}
}

- (void)resume
{
	if (!self.is_connected)
		return;

	dispatch_async(dispatch_get_main_queue(), ^{
		self->pasteboard_timer =
		    [NSTimer scheduledTimerWithTimeInterval:0.5
		                                     target:self
		                                   selector:@selector(onPasteboardTimerFired:)
		                                   userInfo:nil
		                                    repeats:YES];

		NSTrackingArea *trackingArea = [[NSTrackingArea alloc]
		    initWithRect:[self visibleRect]
		         options:NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved |
		                 NSTrackingCursorUpdate | NSTrackingEnabledDuringMouseDrag |
		                 NSTrackingActiveWhenFirstResponder
		           owner:self
		        userInfo:nil];
		[self addTrackingArea:trackingArea];
		[trackingArea release];
	});
}

- (void)setScrollOffset:(int)xOffset y:(int)yOffset w:(int)width h:(int)height
{
	WINPR_ASSERT(mfc);

	mfc->yCurrentScroll = yOffset;
	mfc->xCurrentScroll = xOffset;
	mfc->client_height = height;
	mfc->client_width = width;
}

static void mac_OnChannelConnectedEventHandler(void *context, const ChannelConnectedEventArgs *e)
{
	rdpSettings *settings;
	mfContext *mfc = (mfContext *)context;

	WINPR_ASSERT(mfc);
	WINPR_ASSERT(e);

	settings = mfc->common.context.settings;
	WINPR_ASSERT(settings);

	if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
		mac_cliprdr_init(mfc, (CliprdrClientContext *)e->pInterface);
	}
	else if (strcmp(e->name, ENCOMSP_SVC_CHANNEL_NAME) == 0)
	{
	}
	else
		freerdp_client_OnChannelConnectedEventHandler(context, e);
}

static void mac_OnChannelDisconnectedEventHandler(void *context,
                                                  const ChannelDisconnectedEventArgs *e)
{
	rdpSettings *settings;
	mfContext *mfc = (mfContext *)context;

	WINPR_ASSERT(mfc);
	WINPR_ASSERT(e);

	settings = mfc->common.context.settings;
	WINPR_ASSERT(settings);

	if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
		mac_cliprdr_uninit(mfc, (CliprdrClientContext *)e->pInterface);
	}
	else if (strcmp(e->name, ENCOMSP_SVC_CHANNEL_NAME) == 0)
	{
	}
	else
		freerdp_client_OnChannelDisconnectedEventHandler(context, e);
}

BOOL mac_pre_connect(freerdp *instance)
{
	rdpSettings *settings;
	rdpUpdate *update;

	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);

	update = instance->context->update;
	WINPR_ASSERT(update);

	update->BeginPaint = mac_begin_paint;
	update->EndPaint = mac_end_paint;
	update->DesktopResize = mac_desktop_resize;

	settings = instance->context->settings;
	WINPR_ASSERT(settings);

	if (!settings->ServerHostname)
	{
		WLog_ERR(TAG, "error: server hostname was not specified with /v:<server>[:port]");
		return FALSE;
	}

	settings->OsMajorType = OSMAJORTYPE_MACINTOSH;
	settings->OsMinorType = OSMINORTYPE_MACINTOSH;
	PubSub_SubscribeChannelConnected(instance->context->pubSub, mac_OnChannelConnectedEventHandler);
	PubSub_SubscribeChannelDisconnected(instance->context->pubSub,
	                                    mac_OnChannelDisconnectedEventHandler);

	return TRUE;
}

BOOL mac_post_connect(freerdp *instance)
{
	rdpGdi *gdi;
	rdpPointer rdp_pointer = { 0 };
	mfContext *mfc;
	MRDPView *view;

	WINPR_ASSERT(instance);

	mfc = (mfContext *)instance->context;
	WINPR_ASSERT(mfc);

	view = (MRDPView *)mfc->view;
	WINPR_ASSERT(view);

	rdp_pointer.size = sizeof(rdpPointer);
	rdp_pointer.New = mf_Pointer_New;
	rdp_pointer.Free = mf_Pointer_Free;
	rdp_pointer.Set = mf_Pointer_Set;
	rdp_pointer.SetNull = mf_Pointer_SetNull;
	rdp_pointer.SetDefault = mf_Pointer_SetDefault;
	rdp_pointer.SetPosition = mf_Pointer_SetPosition;

	if (!gdi_init(instance, PIXEL_FORMAT_BGRX32))
		return FALSE;

	gdi = instance->context->gdi;
	view->bitmap_context = mac_create_bitmap_context(instance->context);
	graphics_register_pointer(instance->context->graphics, &rdp_pointer);
	/* setup pasteboard (aka clipboard) for copy operations (write only) */
	view->pasteboard_wr = [NSPasteboard generalPasteboard];
	/* setup pasteboard for read operations */
	dispatch_async(dispatch_get_main_queue(), ^{
		view->pasteboard_rd = [NSPasteboard generalPasteboard];
		view->pasteboard_changecount = -1;
	});
	[view resume];
	mfc->appleKeyboardType = mac_detect_keyboard_type();
	return TRUE;
}

void mac_post_disconnect(freerdp *instance)
{
	mfContext *mfc;
	MRDPView *view;
	if (!instance || !instance->context)
		return;

	mfc = (mfContext *)instance->context;
	view = (MRDPView *)mfc->view;

	[view pause];

	PubSub_UnsubscribeChannelConnected(instance->context->pubSub,
	                                   mac_OnChannelConnectedEventHandler);
	PubSub_UnsubscribeChannelDisconnected(instance->context->pubSub,
	                                      mac_OnChannelDisconnectedEventHandler);
	gdi_free(instance);
}

static BOOL mac_authenticate_int(NSString *title, freerdp *instance, char **username,
                                 char **password, char **domain)
{
	mfContext *mfc = (mfContext *)instance->context;
	MRDPView *view = (MRDPView *)mfc->view;
	PasswordDialog *dialog = [PasswordDialog new];
	dialog.serverHostname = title;

	if (*username)
		dialog.username = [NSString stringWithCString:*username encoding:NSUTF8StringEncoding];

	if (*password)
		dialog.password = [NSString stringWithCString:*password encoding:NSUTF8StringEncoding];

	if (*domain)
		dialog.domain = [NSString stringWithCString:*domain encoding:NSUTF8StringEncoding];

	dispatch_sync(dispatch_get_main_queue(), ^{
		[dialog performSelectorOnMainThread:@selector(runModal:)
		                         withObject:[view window]
		                      waitUntilDone:TRUE];
	});
	BOOL ok = dialog.modalCode;

	if (ok)
	{
		size_t ulen, plen, dlen;
		const char *submittedUsername = [dialog.username cStringUsingEncoding:NSUTF8StringEncoding];
		ulen = (strlen(submittedUsername) + 1) * sizeof(char);
		*username = malloc(ulen);

		if (!(*username))
			return FALSE;

		sprintf_s(*username, ulen, "%s", submittedUsername);
		const char *submittedPassword = [dialog.password cStringUsingEncoding:NSUTF8StringEncoding];
		plen = (strlen(submittedPassword) + 1) * sizeof(char);
		*password = malloc(plen);

		if (!(*password))
			return FALSE;

		sprintf_s(*password, plen, "%s", submittedPassword);
		const char *submittedDomain = [dialog.domain cStringUsingEncoding:NSUTF8StringEncoding];
		dlen = (strlen(submittedDomain) + 1) * sizeof(char);
		*domain = malloc(dlen);

		if (!(*domain))
			return FALSE;

		sprintf_s(*domain, dlen, "%s", submittedDomain);
	}

	return ok;
}

BOOL mac_authenticate(freerdp *instance, char **username, char **password, char **domain)
{
	rdpSettings *settings;

	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);

	settings = instance->context->settings;
	WINPR_ASSERT(settings);

	NSString *title =
	    [NSString stringWithFormat:@"%@:%u",
	                               [NSString stringWithCString:settings->ServerHostname
	                                                  encoding:NSUTF8StringEncoding],
	                               settings -> ServerPort];
	return mac_authenticate_int(title, instance, username, password, domain);
}

BOOL mac_gw_authenticate(freerdp *instance, char **username, char **password, char **domain)
{
	rdpSettings *settings;

	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);

	settings = instance->context->settings;
	WINPR_ASSERT(settings);

	NSString *title =
	    [NSString stringWithFormat:@"%@:%u",
	                               [NSString stringWithCString:settings->GatewayHostname
	                                                  encoding:NSUTF8StringEncoding],
	                               settings -> GatewayPort];
	return mac_authenticate_int(title, instance, username, password, domain);
}

DWORD mac_verify_certificate_ex(freerdp *instance, const char *host, UINT16 port,
                                const char *common_name, const char *subject, const char *issuer,
                                const char *fingerprint, DWORD flags)
{
	mfContext *mfc = (mfContext *)instance->context;
	MRDPView *view = (MRDPView *)mfc->view;
	CertificateDialog *dialog = [CertificateDialog new];
	const char *type = "RDP-Server";
	char hostname[8192];

	if (flags & VERIFY_CERT_FLAG_GATEWAY)
		type = "RDP-Gateway";

	if (flags & VERIFY_CERT_FLAG_REDIRECT)
		type = "RDP-Redirect";

	sprintf_s(hostname, sizeof(hostname), "%s %s:%" PRIu16, type, host, port);
	dialog.serverHostname = [NSString stringWithCString:hostname encoding:NSUTF8StringEncoding];
	dialog.commonName = [NSString stringWithCString:common_name encoding:NSUTF8StringEncoding];
	dialog.subject = [NSString stringWithCString:subject encoding:NSUTF8StringEncoding];
	dialog.issuer = [NSString stringWithCString:issuer encoding:NSUTF8StringEncoding];
	dialog.fingerprint = [NSString stringWithCString:fingerprint encoding:NSUTF8StringEncoding];

	if (flags & VERIFY_CERT_FLAG_MISMATCH)
		dialog.hostMismatch = TRUE;

	if (flags & VERIFY_CERT_FLAG_CHANGED)
		dialog.changed = TRUE;

	[dialog performSelectorOnMainThread:@selector(runModal:)
	                         withObject:[view window]
	                      waitUntilDone:TRUE];
	return dialog.result;
}

DWORD mac_verify_changed_certificate_ex(freerdp *instance, const char *host, UINT16 port,
                                        const char *common_name, const char *subject,
                                        const char *issuer, const char *fingerprint,
                                        const char *old_subject, const char *old_issuer,
                                        const char *old_fingerprint, DWORD flags)
{
	mfContext *mfc = (mfContext *)instance->context;
	MRDPView *view = (MRDPView *)mfc->view;
	CertificateDialog *dialog = [CertificateDialog new];
	const char *type = "RDP-Server";
	char hostname[8192];

	if (flags & VERIFY_CERT_FLAG_GATEWAY)
		type = "RDP-Gateway";

	if (flags & VERIFY_CERT_FLAG_REDIRECT)
		type = "RDP-Redirect";

	sprintf_s(hostname, sizeof(hostname), "%s %s:%" PRIu16, type, host, port);
	dialog.serverHostname = [NSString stringWithCString:hostname encoding:NSUTF8StringEncoding];
	dialog.commonName = [NSString stringWithCString:common_name encoding:NSUTF8StringEncoding];
	dialog.subject = [NSString stringWithCString:subject encoding:NSUTF8StringEncoding];
	dialog.issuer = [NSString stringWithCString:issuer encoding:NSUTF8StringEncoding];
	dialog.fingerprint = [NSString stringWithCString:fingerprint encoding:NSUTF8StringEncoding];

	if (flags & VERIFY_CERT_FLAG_MISMATCH)
		dialog.hostMismatch = TRUE;

	if (flags & VERIFY_CERT_FLAG_CHANGED)
		dialog.changed = TRUE;

	[dialog performSelectorOnMainThread:@selector(runModal:)
	                         withObject:[view window]
	                      waitUntilDone:TRUE];
	return dialog.result;
}

int mac_logon_error_info(freerdp *instance, UINT32 data, UINT32 type)
{
	const char *str_data = freerdp_get_logon_error_info_data(data);
	const char *str_type = freerdp_get_logon_error_info_type(type);
	// TODO: Error message dialog
	WLog_INFO(TAG, "Logon Error Info %s [%s]", str_data, str_type);
	return 1;
}

BOOL mf_Pointer_New(rdpContext *context, rdpPointer *pointer)
{
	rdpGdi *gdi;
	NSRect rect;
	NSImage *image;
	NSPoint hotSpot;
	NSCursor *cursor;
	BYTE *cursor_data;
	NSMutableArray *ma;
	NSBitmapImageRep *bmiRep;
	MRDPCursor *mrdpCursor = [[MRDPCursor alloc] init];
	mfContext *mfc = (mfContext *)context;
	MRDPView *view;
	UINT32 format;

	if (!mfc || !context || !pointer)
		return FALSE;

	view = (MRDPView *)mfc->view;
	gdi = context->gdi;

	if (!gdi || !view)
		return FALSE;

	rect.size.width = pointer->width;
	rect.size.height = pointer->height;
	rect.origin.x = pointer->xPos;
	rect.origin.y = pointer->yPos;
	cursor_data = (BYTE *)malloc(rect.size.width * rect.size.height * 4);

	if (!cursor_data)
		return FALSE;

	mrdpCursor->cursor_data = cursor_data;
	format = PIXEL_FORMAT_RGBA32;

	if (!freerdp_image_copy_from_pointer_data(cursor_data, format, 0, 0, 0, pointer->width,
	                                          pointer->height, pointer->xorMaskData,
	                                          pointer->lengthXorMask, pointer->andMaskData,
	                                          pointer->lengthAndMask, pointer->xorBpp, NULL))
	{
		free(cursor_data);
		mrdpCursor->cursor_data = NULL;
		return FALSE;
	}

	/* store cursor bitmap image in representation - required by NSImage */
	bmiRep = [[NSBitmapImageRep alloc]
	    initWithBitmapDataPlanes:(unsigned char **)&cursor_data
	                  pixelsWide:rect.size.width
	                  pixelsHigh:rect.size.height
	               bitsPerSample:8
	             samplesPerPixel:4
	                    hasAlpha:YES
	                    isPlanar:NO
	              colorSpaceName:NSDeviceRGBColorSpace
	                bitmapFormat:0
	                 bytesPerRow:rect.size.width * FreeRDPGetBytesPerPixel(format)
	                bitsPerPixel:0];
	mrdpCursor->bmiRep = bmiRep;
	/* create an image using above representation */
	image = [[NSImage alloc] initWithSize:[bmiRep size]];
	[image addRepresentation:bmiRep];
	mrdpCursor->nsImage = image;
	/* need hotspot to create cursor */
	hotSpot.x = pointer->xPos;
	hotSpot.y = pointer->yPos;
	cursor = [[NSCursor alloc] initWithImage:image hotSpot:hotSpot];
	mrdpCursor->nsCursor = cursor;
	mrdpCursor->pointer = pointer;
	/* save cursor for later use in mf_Pointer_Set() */
	ma = view->cursors;
	[ma addObject:mrdpCursor];
	return TRUE;
}

void mf_Pointer_Free(rdpContext *context, rdpPointer *pointer)
{
	mfContext *mfc = (mfContext *)context;
	MRDPView *view = (MRDPView *)mfc->view;
	NSMutableArray *ma = view->cursors;

	for (MRDPCursor *cursor in ma)
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

BOOL mf_Pointer_Set(rdpContext *context, rdpPointer *pointer)
{
	mfContext *mfc = (mfContext *)context;
	MRDPView *view = (MRDPView *)mfc->view;
	NSMutableArray *ma = view->cursors;

	for (MRDPCursor *cursor in ma)
	{
		if (cursor->pointer == pointer)
		{
			[view setCursor:cursor->nsCursor];
			return TRUE;
		}
	}

	NSLog(@"Cursor not found");
	return TRUE;
}

BOOL mf_Pointer_SetNull(rdpContext *context)
{
	return TRUE;
}

BOOL mf_Pointer_SetDefault(rdpContext *context)
{
	mfContext *mfc = (mfContext *)context;
	MRDPView *view = (MRDPView *)mfc->view;
	[view setCursor:[NSCursor arrowCursor]];
	return TRUE;
}

static BOOL mf_Pointer_SetPosition(rdpContext *context, UINT32 x, UINT32 y)
{
	mfContext *mfc = (mfContext *)context;

	if (!mfc)
		return FALSE;

	/* TODO: Set pointer position */
	return TRUE;
}

CGContextRef mac_create_bitmap_context(rdpContext *context)
{
	CGContextRef bitmap_context;
	rdpGdi *gdi = context->gdi;
	UINT32 bpp = FreeRDPGetBytesPerPixel(gdi->dstFormat);
	CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();

	if (bpp == 2)
	{
		bitmap_context = CGBitmapContextCreate(
		    gdi->primary_buffer, gdi->width, gdi->height, 5, gdi->stride, colorSpace,
		    kCGBitmapByteOrder16Little | kCGImageAlphaNoneSkipFirst);
	}
	else
	{
		bitmap_context = CGBitmapContextCreate(
		    gdi->primary_buffer, gdi->width, gdi->height, 8, gdi->stride, colorSpace,
		    kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst);
	}

	CGColorSpaceRelease(colorSpace);
	return bitmap_context;
}

BOOL mac_begin_paint(rdpContext *context)
{
	rdpGdi *gdi = context->gdi;

	if (!gdi)
		return FALSE;

	gdi->primary->hdc->hwnd->invalid->null = TRUE;
	return TRUE;
}

BOOL mac_end_paint(rdpContext *context)
{
	rdpGdi *gdi;
	HGDI_RGN invalid;
	NSRect newDrawRect;
	int ww, wh, dw, dh;
	mfContext *mfc = (mfContext *)context;
	MRDPView *view = (MRDPView *)mfc->view;
	gdi = context->gdi;

	if (!gdi)
		return FALSE;

	ww = mfc->client_width;
	wh = mfc->client_height;
	dw = mfc->common.context.settings->DesktopWidth;
	dh = mfc->common.context.settings->DesktopHeight;

	if ((!context) || (!context->gdi))
		return FALSE;

	if (context->gdi->primary->hdc->hwnd->invalid->null)
		return TRUE;

	invalid = gdi->primary->hdc->hwnd->invalid;
	newDrawRect.origin.x = invalid->x;
	newDrawRect.origin.y = invalid->y;
	newDrawRect.size.width = invalid->w;
	newDrawRect.size.height = invalid->h;

	if (mfc->common.context.settings->SmartSizing && (ww != dw || wh != dh))
	{
		newDrawRect.origin.y = newDrawRect.origin.y * wh / dh - 1;
		newDrawRect.size.height = newDrawRect.size.height * wh / dh + 1;
		newDrawRect.origin.x = newDrawRect.origin.x * ww / dw - 1;
		newDrawRect.size.width = newDrawRect.size.width * ww / dw + 1;
	}
	else
	{
		newDrawRect.origin.y = newDrawRect.origin.y - 1;
		newDrawRect.size.height = newDrawRect.size.height + 1;
		newDrawRect.origin.x = newDrawRect.origin.x - 1;
		newDrawRect.size.width = newDrawRect.size.width + 1;
	}

	windows_to_apple_cords(mfc->view, &newDrawRect);
	dispatch_sync(dispatch_get_main_queue(), ^{
		[view setNeedsDisplayInRect:newDrawRect];
	});
	gdi->primary->hdc->hwnd->ninvalid = 0;
	return TRUE;
}

BOOL mac_desktop_resize(rdpContext *context)
{
	ResizeWindowEventArgs e;
	mfContext *mfc = (mfContext *)context;
	MRDPView *view = (MRDPView *)mfc->view;
	rdpSettings *settings = context->settings;

	if (!context->gdi)
		return TRUE;

	/**
	 * TODO: Fix resizing race condition. We should probably implement a message to be
	 * put on the update message queue to be able to properly flush pending updates,
	 * resize, and then continue with post-resizing graphical updates.
	 */
	CGContextRef old_context = view->bitmap_context;
	view->bitmap_context = NULL;
	CGContextRelease(old_context);
	mfc->width = settings->DesktopWidth;
	mfc->height = settings->DesktopHeight;

	if (!gdi_resize(context->gdi, mfc->width, mfc->height))
		return FALSE;

	view->bitmap_context = mac_create_bitmap_context(context);

	if (!view->bitmap_context)
		return FALSE;

	mfc->client_width = mfc->width;
	mfc->client_height = mfc->height;
	[view setFrameSize:NSMakeSize(mfc->width, mfc->height)];
	EventArgsInit(&e, "mfreerdp");
	e.width = settings->DesktopWidth;
	e.height = settings->DesktopHeight;
	PubSub_OnResizeWindow(context->pubSub, context, &e);
	return TRUE;
}

void input_activity_cb(freerdp *instance)
{
	int status;
	wMessage message;
	wMessageQueue *queue;
	status = 1;
	queue = freerdp_get_message_queue(instance, FREERDP_INPUT_MESSAGE_QUEUE);

	if (queue)
	{
		while (MessageQueue_Peek(queue, &message, TRUE))
		{
			status = freerdp_message_queue_process_message(instance, FREERDP_INPUT_MESSAGE_QUEUE,
			                                               &message);

			if (!status)
				break;
		}
	}
	else
	{
		WLog_ERR(TAG, "input_activity_cb: No queue!");
	}
}

/**
 * given a rect with 0,0 at the top left (windows cords)
 * convert it to a rect with 0,0 at the bottom left (apple cords)
 *
 * Note: the formula works for conversions in both directions.
 *
 */

void windows_to_apple_cords(MRDPView *view, NSRect *r)
{
	dispatch_sync(dispatch_get_main_queue(), ^{
		r->origin.y = [view frame].size.height - (r->origin.y + r->size.height);
	});
}

void sync_keyboard_state(freerdp *instance)
{
	UINT32 flags = 0;
	CGEventFlags currentFlags = CGEventSourceFlagsState(kCGEventSourceStateHIDSystemState);
	mfContext *context;

	WINPR_ASSERT(instance);
	context = (mfContext *)instance->context;
	WINPR_ASSERT(context);

	if (context->kbdFlags != currentFlags)
	{
		if (currentFlags & kCGEventFlagMaskAlphaShift)
			flags |= KBD_SYNC_CAPS_LOCK;

		if (currentFlags & kCGEventFlagMaskNumericPad)
			flags |= KBD_SYNC_NUM_LOCK;

		freerdp_input_send_synchronize_event(instance->context->input, flags);
		context->kbdFlags = currentFlags;
	}
}

@end
