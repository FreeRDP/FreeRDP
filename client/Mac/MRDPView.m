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

#include <winpr/crt.h>
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

#define TAG CLIENT_TAG("mac")

static BOOL mf_Pointer_New(rdpContext* context, rdpPointer* pointer);
static void mf_Pointer_Free(rdpContext* context, rdpPointer* pointer);
static BOOL mf_Pointer_Set(rdpContext* context, const rdpPointer* pointer);
static BOOL mf_Pointer_SetNull(rdpContext* context);
static BOOL mf_Pointer_SetDefault(rdpContext* context);
static BOOL mf_Pointer_SetPosition(rdpContext* context, UINT32 x, UINT32 y);

static BOOL mac_begin_paint(rdpContext* context);
static BOOL mac_end_paint(rdpContext* context);
static BOOL mac_desktop_resize(rdpContext* context);

static void update_activity_cb(freerdp* instance);
static void input_activity_cb(freerdp* instance);

static DWORD mac_client_thread(void* param);

@implementation MRDPView

@synthesize is_connected;

- (int) rdpStart:(rdpContext*) rdp_context
{
	rdpSettings* settings;
	EmbedWindowEventArgs e;
	[self initializeView];
	context = rdp_context;
	mfc = (mfContext*) rdp_context;
	instance = context->instance;
	settings = context->settings;
	EventArgsInit(&e, "mfreerdp");
	e.embed = TRUE;
	e.handle = (void*) self;
	PubSub_OnEmbedWindow(context->pubSub, context, &e);
	NSScreen* screen = [[NSScreen screens] objectAtIndex:0];
	NSRect screenFrame = [screen frame];

	if (instance->settings->Fullscreen)
	{
		instance->settings->DesktopWidth  = screenFrame.size.width;
		instance->settings->DesktopHeight = screenFrame.size.height;
		[self enterFullScreenMode:[NSScreen mainScreen] withOptions:nil];
	}
	else
	{
		[self exitFullScreenModeWithOptions:nil];
	}

	mfc->client_height = instance->settings->DesktopHeight;
	mfc->client_width = instance->settings->DesktopWidth;

	if (!(mfc->thread = CreateThread(NULL, 0, mac_client_thread, (void*) context, 0,
	                                 &mfc->mainThreadId)))
	{
		WLog_ERR(TAG, "failed to create client thread");
		return -1;
	}

	return 0;
}

static DWORD mac_client_update_thread(void* param)
{
	int status;
	wMessage message;
	wMessageQueue* queue;
	rdpContext* context = (rdpContext*) param;
	status = 1;
	queue = freerdp_get_message_queue(context->instance,
	                                  FREERDP_UPDATE_MESSAGE_QUEUE);

	while (MessageQueue_Wait(queue))
	{
		while (MessageQueue_Peek(queue, &message, TRUE))
		{
			status = freerdp_message_queue_process_message(context->instance,
			         FREERDP_UPDATE_MESSAGE_QUEUE, &message);

			if (!status)
				break;
		}

		if (!status)
			break;
	}

	ExitThread(0);
	return 0;
}

static DWORD WINAPI mac_client_input_thread(LPVOID param)
{
	int status;
	wMessage message;
	wMessageQueue* queue;
	rdpContext* context = (rdpContext*) param;
	status = 1;
	queue = freerdp_get_message_queue(context->instance,
	                                  FREERDP_INPUT_MESSAGE_QUEUE);

	while (MessageQueue_Wait(queue))
	{
		while (MessageQueue_Peek(queue, &message, TRUE))
		{
			status = freerdp_message_queue_process_message(context->instance,
			         FREERDP_INPUT_MESSAGE_QUEUE, &message);

			if (!status)
				break;
		}

		if (!status)
			break;
	}

	ExitThread(0);
	return 0;
}

DWORD mac_client_thread(void* param)
{
	@autoreleasepool
	{
		int status;
		HANDLE events[16];
		HANDLE inputEvent;
		HANDLE inputThread = NULL;
		DWORD nCount;
		DWORD nCountTmp;
		DWORD nCountBase;
		rdpContext* context = (rdpContext*) param;
		mfContext* mfc = (mfContext*) context;
		freerdp* instance = context->instance;
		MRDPView* view = mfc->view;
		rdpSettings* settings = context->settings;
		status = freerdp_connect(context->instance);

		if (!status)
		{
			[view setIs_connected:0];
			return 0;
		}

		[view setIs_connected:1];
		nCount = 0;
		events[nCount++] = mfc->stopEvent;

		if (settings->AsyncInput)
		{
			if (!(inputThread = CreateThread(NULL, 0, mac_client_input_thread, context, 0, NULL)))
			{
				WLog_ERR(TAG,  "failed to create async input thread");
				goto disconnect;
			}
		}
		else
		{
			if (!(inputEvent = freerdp_get_message_queue_event_handle(instance,
			                   FREERDP_INPUT_MESSAGE_QUEUE)))
			{
				WLog_ERR(TAG, "failed to get input event handle");
				goto disconnect;
			}

			events[nCount++] = inputEvent;
		}

		nCountBase = nCount;

		while (!freerdp_shall_disconnect(instance))
		{
			nCount = nCountBase;

			if (!settings->AsyncTransport)
			{
				if (!(nCountTmp = freerdp_get_event_handles(context, &events[nCount],
				                  16 - nCount)))
				{
					WLog_ERR(TAG, "freerdp_get_event_handles failed");
					break;
				}

				nCount += nCountTmp;
			}

			status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

			if (status >= (WAIT_OBJECT_0 + nCount))
			{
				WLog_ERR(TAG, "WaitForMultipleObjects failed (0x%08X)", status);
				break;
			}

			if (status == WAIT_OBJECT_0)
			{
				/* stop event triggered */
				break;
			}

			if (!settings->AsyncInput)
			{
				if (WaitForSingleObject(inputEvent, 0) == WAIT_OBJECT_0)
				{
					input_activity_cb(instance);
				}
			}

			if (!settings->AsyncTransport)
			{
				if (!freerdp_check_event_handles(context))
				{
					WLog_ERR(TAG, "freerdp_check_event_handles failed");
					break;
				}
			}
		}

	disconnect:
		freerdp_disconnect(instance);

		if (settings->AsyncInput && inputThread)
		{
			wMessageQueue* inputQueue = freerdp_get_message_queue(instance,
			                            FREERDP_INPUT_MESSAGE_QUEUE);

			if (inputQueue)
			{
				MessageQueue_PostQuit(inputQueue, 0);
				WaitForSingleObject(inputThread, INFINITE);
			}

			CloseHandle(inputThread);
		}

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

- (void) viewDidLoad
{
	[self initializeView];
}

- (void) initializeView
{
	if (!initialized)
	{
		cursors = [[NSMutableArray alloc] initWithCapacity:10];
		// setup a mouse tracking area
		NSTrackingArea* trackingArea = [[NSTrackingArea alloc] initWithRect:[self
		                                                       visibleRect] options:NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved |
		                                                       NSTrackingCursorUpdate | NSTrackingEnabledDuringMouseDrag |
		                                                       NSTrackingActiveWhenFirstResponder owner:self userInfo:nil];
		[self addTrackingArea:trackingArea];
		// Set the default cursor
		currentCursor = [NSCursor arrowCursor];
		initialized = YES;
	}
}

- (void) setCursor: (NSCursor*) cursor
{
	self->currentCursor = cursor;
	[[self window] invalidateCursorRectsForView:self];
}

- (void) resetCursorRects
{
	[self addCursorRect:[self visibleRect] cursor:currentCursor];
}

- (BOOL)acceptsFirstResponder
{
	return YES;
}

- (void) mouseMoved:(NSEvent*)event
{
	[super mouseMoved:event];

	if (!is_connected)
		return;

	NSPoint loc = [event locationInWindow];
	int x = (int) loc.x;
	int y = (int) loc.y;
	mf_scale_mouse_event(context, instance->input, PTR_FLAGS_MOVE, x, y);
}

- (void)mouseDown:(NSEvent*) event
{
	[super mouseDown:event];

	if (!is_connected)
		return;

	NSPoint loc = [event locationInWindow];
	int x = (int) loc.x;
	int y = (int) loc.y;
	mf_scale_mouse_event(context, instance->input,
	                     PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON1, x, y);
}

- (void) mouseUp:(NSEvent*) event
{
	[super mouseUp:event];

	if (!is_connected)
		return;

	NSPoint loc = [event locationInWindow];
	int x = (int) loc.x;
	int y = (int) loc.y;
	mf_scale_mouse_event(context, instance->input, PTR_FLAGS_BUTTON1, x, y);
}

- (void) rightMouseDown:(NSEvent*)event
{
	[super rightMouseDown:event];

	if (!is_connected)
		return;

	NSPoint loc = [event locationInWindow];
	int x = (int) loc.x;
	int y = (int) loc.y;
	mf_scale_mouse_event(context, instance->input,
	                     PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON2, x, y);
}

- (void) rightMouseUp:(NSEvent*)event
{
	[super rightMouseUp:event];

	if (!is_connected)
		return;

	NSPoint loc = [event locationInWindow];
	int x = (int) loc.x;
	int y = (int) loc.y;
	mf_scale_mouse_event(context, instance->input, PTR_FLAGS_BUTTON2, x, y);
}

- (void) otherMouseDown:(NSEvent*)event
{
	[super otherMouseDown:event];

	if (!is_connected)
		return;

	NSPoint loc = [event locationInWindow];
	int x = (int) loc.x;
	int y = (int) loc.y;
	mf_scale_mouse_event(context, instance->input,
	                     PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON3, x, y);
}

- (void) otherMouseUp:(NSEvent*)event
{
	[super otherMouseUp:event];

	if (!is_connected)
		return;

	NSPoint loc = [event locationInWindow];
	int x = (int) loc.x;
	int y = (int) loc.y;
	mf_scale_mouse_event(context, instance->input, PTR_FLAGS_BUTTON3, x, y);
}

- (void) scrollWheel:(NSEvent*)event
{
	UINT16 flags;
	[super scrollWheel:event];

	if (!is_connected)
		return;

	NSPoint loc = [event locationInWindow];
	int x = (int) loc.x;
	int y = (int) loc.y;
	flags = PTR_FLAGS_WHEEL;
	/* 1 event = 120 units */
	int units = [event deltaY] * 120;

	/* send out all accumulated rotations */
	while (units != 0)
	{
		/* limit to maximum value in WheelRotationMask (9bit signed value) */
		int step = MIN(MAX(-256, units), 255);
		mf_scale_mouse_event(context, instance->input,
		                     flags | ((UINT16)step & WheelRotationMask), x, y);
		units -= step;
	}
}

- (void) mouseDragged:(NSEvent*)event
{
	[super mouseDragged:event];

	if (!is_connected)
		return;

	NSPoint loc = [event locationInWindow];
	int x = (int) loc.x;
	int y = (int) loc.y;
	// send mouse motion event to RDP server
	mf_scale_mouse_event(context, instance->input, PTR_FLAGS_MOVE, x, y);
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

- (void) keyDown:(NSEvent*) event
{
	DWORD keyCode;
	DWORD keyFlags;
	DWORD vkcode;
	DWORD scancode;
	unichar keyChar;
	NSString* characters;

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
	freerdp_input_send_keyboard_event(instance->input, keyFlags, scancode);
}

- (void) keyUp:(NSEvent*) event
{
	DWORD keyCode;
	DWORD keyFlags;
	DWORD vkcode;
	DWORD scancode;
	unichar keyChar;
	NSString* characters;

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
	freerdp_input_send_keyboard_event(instance->input, keyFlags, scancode);
}

- (void) flagsChanged:(NSEvent*) event
{
	int key;
	DWORD keyFlags;
	DWORD vkcode;
	DWORD scancode;
	DWORD modFlags;

	if (!is_connected)
		return;

	keyFlags = 0;
	key = [event keyCode] + 8;
	modFlags = [event modifierFlags] & NSDeviceIndependentModifierFlagsMask;
	vkcode = GetVirtualKeyCodeFromKeycode(key, KEYCODE_TYPE_APPLE);
	scancode = GetVirtualScanCodeFromVirtualKeyCode(vkcode, 4);
	keyFlags |= (scancode & KBDEXT) ? KBDEXT : 0;
	scancode &= 0xFF;
	vkcode &= 0xFF;
#if 0
	WLog_DBG(TAG,
	         "flagsChanged: key: 0x%04X scancode: 0x%04X vkcode: 0x%04X extended: %d name: %s modFlags: 0x%04X",
	         key - 8, scancode, vkcode, keyFlags, GetVirtualKeyName(vkcode), modFlags);

	if (modFlags & NSAlphaShiftKeyMask)
		WLog_DBG(TAG,  "NSAlphaShiftKeyMask");

	if (modFlags & NSShiftKeyMask)
		WLog_DBG(TAG,  "NSShiftKeyMask");

	if (modFlags & NSControlKeyMask)
		WLog_DBG(TAG,  "NSControlKeyMask");

	if (modFlags & NSAlternateKeyMask)
		WLog_DBG(TAG,  "NSAlternateKeyMask");

	if (modFlags & NSCommandKeyMask)
		WLog_DBG(TAG,  "NSCommandKeyMask");

	if (modFlags & NSNumericPadKeyMask)
		WLog_DBG(TAG,  "NSNumericPadKeyMask");

	if (modFlags & NSHelpKeyMask)
		WLog_DBG(TAG,  "NSHelpKeyMask");

#endif

	if ((modFlags & NSAlphaShiftKeyMask) && !(kbdModFlags & NSAlphaShiftKeyMask))
		freerdp_input_send_keyboard_event(instance->input, keyFlags | KBD_FLAGS_DOWN,
		                                  scancode);
	else if (!(modFlags & NSAlphaShiftKeyMask)
	         && (kbdModFlags & NSAlphaShiftKeyMask))
		freerdp_input_send_keyboard_event(instance->input, keyFlags | KBD_FLAGS_RELEASE,
		                                  scancode);

	if ((modFlags & NSShiftKeyMask) && !(kbdModFlags & NSShiftKeyMask))
		freerdp_input_send_keyboard_event(instance->input, keyFlags | KBD_FLAGS_DOWN,
		                                  scancode);
	else if (!(modFlags & NSShiftKeyMask) && (kbdModFlags & NSShiftKeyMask))
		freerdp_input_send_keyboard_event(instance->input, keyFlags | KBD_FLAGS_RELEASE,
		                                  scancode);

	if ((modFlags & NSControlKeyMask) && !(kbdModFlags & NSControlKeyMask))
		freerdp_input_send_keyboard_event(instance->input, keyFlags | KBD_FLAGS_DOWN,
		                                  scancode);
	else if (!(modFlags & NSControlKeyMask) && (kbdModFlags & NSControlKeyMask))
		freerdp_input_send_keyboard_event(instance->input, keyFlags | KBD_FLAGS_RELEASE,
		                                  scancode);

	if ((modFlags & NSAlternateKeyMask) && !(kbdModFlags & NSAlternateKeyMask))
		freerdp_input_send_keyboard_event(instance->input, keyFlags | KBD_FLAGS_DOWN,
		                                  scancode);
	else if (!(modFlags & NSAlternateKeyMask) && (kbdModFlags & NSAlternateKeyMask))
		freerdp_input_send_keyboard_event(instance->input, keyFlags | KBD_FLAGS_RELEASE,
		                                  scancode);

	if ((modFlags & NSCommandKeyMask) && !(kbdModFlags & NSCommandKeyMask))
		freerdp_input_send_keyboard_event(instance->input, keyFlags | KBD_FLAGS_DOWN,
		                                  scancode);
	else if (!(modFlags & NSCommandKeyMask) && (kbdModFlags & NSCommandKeyMask))
		freerdp_input_send_keyboard_event(instance->input, keyFlags | KBD_FLAGS_RELEASE,
		                                  scancode);

	if ((modFlags & NSNumericPadKeyMask) && !(kbdModFlags & NSNumericPadKeyMask))
		freerdp_input_send_keyboard_event(instance->input, keyFlags | KBD_FLAGS_DOWN,
		                                  scancode);
	else if (!(modFlags & NSNumericPadKeyMask)
	         && (kbdModFlags & NSNumericPadKeyMask))
		freerdp_input_send_keyboard_event(instance->input, keyFlags | KBD_FLAGS_RELEASE,
		                                  scancode);

	if ((modFlags & NSHelpKeyMask) && !(kbdModFlags & NSHelpKeyMask))
		freerdp_input_send_keyboard_event(instance->input, keyFlags | KBD_FLAGS_DOWN,
		                                  scancode);
	else if (!(modFlags & NSHelpKeyMask) && (kbdModFlags & NSHelpKeyMask))
		freerdp_input_send_keyboard_event(instance->input, keyFlags | KBD_FLAGS_RELEASE,
		                                  scancode);

	kbdModFlags = modFlags;
}

- (void) releaseResources
{
	int i;

	for (i = 0; i < argc; i++)
		free(argv[i]);

	if (!is_connected)
		return;

	gdi_free(context->instance);
	free(pixel_data);
}

- (void) drawRect:(NSRect)rect
{
	if (!context)
		return;

	if (self->bitmap_context)
	{
		CGContextRef cgContext = [[NSGraphicsContext currentContext] graphicsPort];
		CGImageRef cgImage = CGBitmapContextCreateImage(self->bitmap_context);
		CGContextSaveGState(cgContext);
		CGContextClipToRect(cgContext, CGRectMake(rect.origin.x, rect.origin.y,
		                    rect.size.width, rect.size.height));
		CGContextDrawImage(cgContext, CGRectMake(0,
		                   0, [self bounds].size.width, [self bounds].size.height), cgImage);
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

- (void) onPasteboardTimerFired :(NSTimer*) timer
{
	const BYTE* data;
	UINT32 size;
	UINT32 formatId;
	BOOL formatMatch;
	int changeCount;
	NSData* formatData;
	const char* formatType;
	NSPasteboardItem* item;
	changeCount = (int) [pasteboard_rd changeCount];

	if (changeCount == pasteboard_changecount)
		return;

	pasteboard_changecount = changeCount;
	NSArray* items = [pasteboard_rd pasteboardItems];

	if ([items count] < 1)
		return;

	item = [items objectAtIndex:0];
	/**
	 * System-Declared Uniform Type Identifiers:
	 * https://developer.apple.com/library/ios/documentation/Miscellaneous/Reference/UTIRef/Articles/System-DeclaredUniformTypeIdentifiers.html
	 */
	formatMatch = FALSE;

	for (NSString * type in [item types])
	{
		formatType = [type UTF8String];

		if (strcmp(formatType, "public.utf8-plain-text") == 0)
		{
			formatData = [item dataForType:type];
			formatId = ClipboardRegisterFormat(mfc->clipboard, "UTF8_STRING");
			size = (UINT32) [formatData length];
			data = [formatData bytes];
			ClipboardSetData(mfc->clipboard, formatId, data, size);
			formatMatch = TRUE;
			break;
		}
	}

	if (!formatMatch)
		ClipboardEmpty(mfc->clipboard);

	if (mfc->clipboardSync)
		mac_cliprdr_send_client_format_list(mfc->cliprdr);
}

- (void) pause
{
	dispatch_async(dispatch_get_main_queue(), ^
	{
		[self->pasteboard_timer invalidate];
	});
	NSArray* trackingAreas = self.trackingAreas;

	for (NSTrackingArea * ta in trackingAreas)
	{
		[self removeTrackingArea:ta];
	}
}

- (void)resume
{
	dispatch_async(dispatch_get_main_queue(), ^
	{
		self->pasteboard_timer = [NSTimer scheduledTimerWithTimeInterval:0.5 target:self selector:@selector(onPasteboardTimerFired:) userInfo:nil repeats:YES];
	});
	NSTrackingArea* trackingArea = [[NSTrackingArea alloc] initWithRect:[self
	                                                       visibleRect] options:NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved |
	                                                       NSTrackingCursorUpdate | NSTrackingEnabledDuringMouseDrag |
	                                                       NSTrackingActiveWhenFirstResponder owner:self userInfo:nil];
	[self addTrackingArea:trackingArea];
	[trackingArea release];
}

- (void) setScrollOffset:(int)xOffset y:(int)yOffset w:(int)width h:(int)height
{
	mfc->yCurrentScroll = yOffset;
	mfc->xCurrentScroll = xOffset;
	mfc->client_height = height;
	mfc->client_width = width;
}

void mac_OnChannelConnectedEventHandler(void* context,
                                        ChannelConnectedEventArgs* e)
{
	mfContext* mfc = (mfContext*) context;
	rdpSettings* settings = mfc->context.settings;

	if (strcmp(e->name, RDPEI_DVC_CHANNEL_NAME) == 0)
	{
	}
	else if (strcmp(e->name, RDPGFX_DVC_CHANNEL_NAME) == 0)
	{
		if (settings->SoftwareGdi)
			gdi_graphics_pipeline_init(mfc->context.gdi, (RdpgfxClientContext*) e->pInterface);
	}
	else if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
		mac_cliprdr_init(mfc, (CliprdrClientContext*) e->pInterface);
	}
	else if (strcmp(e->name, ENCOMSP_SVC_CHANNEL_NAME) == 0)
	{
	}
}

void mac_OnChannelDisconnectedEventHandler(void* context,
        ChannelDisconnectedEventArgs* e)
{
	mfContext* mfc = (mfContext*) context;
	rdpSettings* settings = mfc->context.settings;

	if (strcmp(e->name, RDPEI_DVC_CHANNEL_NAME) == 0)
	{
	}
	else if (strcmp(e->name, RDPGFX_DVC_CHANNEL_NAME) == 0)
	{
		if (settings->SoftwareGdi)
			gdi_graphics_pipeline_uninit(mfc->context.gdi,
			                             (RdpgfxClientContext*) e->pInterface);
	}
	else if (strcmp(e->name, CLIPRDR_SVC_CHANNEL_NAME) == 0)
	{
		mac_cliprdr_uninit(mfc, (CliprdrClientContext*) e->pInterface);
	}
	else if (strcmp(e->name, ENCOMSP_SVC_CHANNEL_NAME) == 0)
	{
	}
}

BOOL mac_pre_connect(freerdp* instance)
{
	rdpSettings* settings;
	instance->update->BeginPaint = mac_begin_paint;
	instance->update->EndPaint = mac_end_paint;
	instance->update->DesktopResize = mac_desktop_resize;
	settings = instance->settings;

	if (!settings->ServerHostname)
	{
		WLog_ERR(TAG,
		         "error: server hostname was not specified with /v:<server>[:port]");
		[NSApp terminate:nil];
		return -1;
	}

	settings->SoftwareGdi = TRUE;
	settings->OsMajorType = OSMAJORTYPE_MACINTOSH;
	settings->OsMinorType = OSMINORTYPE_MACINTOSH;
	ZeroMemory(settings->OrderSupport, 32);
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
	settings->OrderSupport[NEG_MEMBLT_INDEX] = settings->BitmapCacheEnabled;
	settings->OrderSupport[NEG_MEM3BLT_INDEX] = (settings->SoftwareGdi) ? TRUE :
	        FALSE;
	settings->OrderSupport[NEG_MEMBLT_V2_INDEX] = settings->BitmapCacheEnabled;
	settings->OrderSupport[NEG_MEM3BLT_V2_INDEX] = FALSE;
	settings->OrderSupport[NEG_SAVEBITMAP_INDEX] = FALSE;
	settings->OrderSupport[NEG_GLYPH_INDEX_INDEX] = TRUE;
	settings->OrderSupport[NEG_FAST_INDEX_INDEX] = TRUE;
	settings->OrderSupport[NEG_FAST_GLYPH_INDEX] = TRUE;
	settings->OrderSupport[NEG_POLYGON_SC_INDEX] = FALSE;
	settings->OrderSupport[NEG_POLYGON_CB_INDEX] = FALSE;
	settings->OrderSupport[NEG_ELLIPSE_SC_INDEX] = FALSE;
	settings->OrderSupport[NEG_ELLIPSE_CB_INDEX] = FALSE;
	PubSub_SubscribeChannelConnected(instance->context->pubSub,
	                                 mac_OnChannelConnectedEventHandler);
	PubSub_SubscribeChannelDisconnected(instance->context->pubSub,
	                                    mac_OnChannelDisconnectedEventHandler);

	if (!freerdp_client_load_addins(instance->context->channels,
	                                instance->settings))
		return FALSE;

	return TRUE;
}

BOOL mac_post_connect(freerdp* instance)
{
	rdpGdi* gdi;
	rdpSettings* settings;
	rdpPointer rdp_pointer;
	mfContext* mfc = (mfContext*) instance->context;
	MRDPView* view = (MRDPView*) mfc->view;
	ZeroMemory(&rdp_pointer, sizeof(rdpPointer));
	rdp_pointer.size = sizeof(rdpPointer);
	rdp_pointer.New = mf_Pointer_New;
	rdp_pointer.Free = mf_Pointer_Free;
	rdp_pointer.Set = mf_Pointer_Set;
	rdp_pointer.SetNull = mf_Pointer_SetNull;
	rdp_pointer.SetDefault = mf_Pointer_SetDefault;
	rdp_pointer.SetPosition = mf_Pointer_SetPosition;
	settings = instance->settings;

	if (!gdi_init(instance, PIXEL_FORMAT_BGRX32))
		return FALSE;

	gdi = instance->context->gdi;
	view->bitmap_context = mac_create_bitmap_context(instance->context);
	pointer_cache_register_callbacks(instance->update);
	graphics_register_pointer(instance->context->graphics, &rdp_pointer);
	/* setup pasteboard (aka clipboard) for copy operations (write only) */
	view->pasteboard_wr = [NSPasteboard generalPasteboard];
	/* setup pasteboard for read operations */
	dispatch_async(dispatch_get_main_queue(), ^
	{
		view->pasteboard_rd = [NSPasteboard generalPasteboard];
		view->pasteboard_changecount = -1;
	});
	[view resume];
	mfc->appleKeyboardType = mac_detect_keyboard_type();
	return TRUE;
}

BOOL mac_authenticate(freerdp* instance, char** username, char** password,
                      char** domain)
{
	mfContext* mfc = (mfContext*) instance->context;
	MRDPView* view = (MRDPView*) mfc->view;
	PasswordDialog* dialog = [PasswordDialog new];
	dialog.serverHostname = [NSString stringWithFormat:@"%@:%u",
	                                  [NSString stringWithCString:instance->settings->ServerHostname encoding:
	                                   NSUTF8StringEncoding],
	                                  instance->settings->ServerPort];

	if (*username)
		dialog.username = [NSString stringWithCString:*username encoding:
		                            NSUTF8StringEncoding];

	if (*password)
		dialog.password = [NSString stringWithCString:*password encoding:
		                            NSUTF8StringEncoding];

	if (*domain)
		dialog.domain = [NSString stringWithCString:*domain encoding:
		                          NSUTF8StringEncoding];

	[dialog performSelectorOnMainThread:@selector(runModal:) withObject:[view
	        window] waitUntilDone:TRUE];
	BOOL ok = dialog.modalCode;

	if (ok)
	{
		const char* submittedUsername = [dialog.username cStringUsingEncoding:
		                                                 NSUTF8StringEncoding];
		*username = malloc((strlen(submittedUsername) + 1) * sizeof(char));
		strcpy(*username, submittedUsername);
		const char* submittedPassword = [dialog.password cStringUsingEncoding:
		                                                 NSUTF8StringEncoding];
		*password = malloc((strlen(submittedPassword) + 1) * sizeof(char));
		strcpy(*password, submittedPassword);
		const char* submittedDomain = [dialog.domain cStringUsingEncoding:
		                                             NSUTF8StringEncoding];
		*domain = malloc((strlen(submittedDomain) + 1) * sizeof(char));
		strcpy(*domain, submittedDomain);
	}

	return ok;
}

BOOL mf_Pointer_New(rdpContext* context, rdpPointer* pointer)
{
	rdpGdi* gdi;
	NSRect rect;
	NSImage* image;
	NSPoint hotSpot;
	NSCursor* cursor;
	BYTE* cursor_data;
	NSMutableArray* ma;
	NSBitmapImageRep* bmiRep;
	MRDPCursor* mrdpCursor = [[MRDPCursor alloc] init];
	mfContext* mfc = (mfContext*) context;
	MRDPView* view;
	UINT32 format;

	if (!mfc || !context || !pointer)
		return FALSE;

	view = (MRDPView*) mfc->view;
	gdi = context->gdi;

	if (!gdi || !view)
		return FALSE;

	rect.size.width = pointer->width;
	rect.size.height = pointer->height;
	rect.origin.x = pointer->xPos;
	rect.origin.y = pointer->yPos;
	cursor_data = (BYTE*) malloc(rect.size.width * rect.size.height * 4);

	if (!cursor_data)
		return FALSE;

	mrdpCursor->cursor_data = cursor_data;
	format = PIXEL_FORMAT_RGBA32;

	if (!freerdp_image_copy_from_pointer_data(
	        cursor_data, format,
	        0, 0, 0, pointer->width, pointer->height,
	        pointer->xorMaskData, pointer->lengthXorMask,
	        pointer->andMaskData, pointer->lengthAndMask,
	        pointer->xorBpp, NULL))
	{
		free(cursor_data);
		mrdpCursor->cursor_data = NULL;
		return FALSE;
	}

	/* store cursor bitmap image in representation - required by NSImage */
	bmiRep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:
	                                   (unsigned char**) &cursor_data
	                                   pixelsWide:rect.size.width
	                                   pixelsHigh:rect.size.height
	                                   bitsPerSample:8
	                                   samplesPerPixel:4
	                                   hasAlpha:YES
	                                   isPlanar:NO
	                                   colorSpaceName:NSDeviceRGBColorSpace
	                                   bitmapFormat:0
	                                   bytesPerRow:rect.size.width * GetBytesPerPixel(format)
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
	ma = view->cursors;
	[ma addObject:mrdpCursor];
	return TRUE;
}

void mf_Pointer_Free(rdpContext* context, rdpPointer* pointer)
{
	mfContext* mfc = (mfContext*) context;
	MRDPView* view = (MRDPView*) mfc->view;
	NSMutableArray* ma = view->cursors;

	for (MRDPCursor * cursor in ma)
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

BOOL mf_Pointer_Set(rdpContext* context, const rdpPointer* pointer)
{
	mfContext* mfc = (mfContext*) context;
	MRDPView* view = (MRDPView*) mfc->view;
	NSMutableArray* ma = view->cursors;

	for (MRDPCursor * cursor in ma)
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

BOOL mf_Pointer_SetNull(rdpContext* context)
{
	return TRUE;
}

BOOL mf_Pointer_SetDefault(rdpContext* context)
{
	mfContext* mfc = (mfContext*) context;
	MRDPView* view = (MRDPView*) mfc->view;
	[view setCursor:[NSCursor arrowCursor]];
	return TRUE;
}

static BOOL mf_Pointer_SetPosition(rdpContext* context, UINT32 x, UINT32 y)
{
	mfContext* mfc = (mfContext*) context;

	if (!mfc)
		return FALSE;

	/* TODO: Set pointer position */
	return TRUE;
}

CGContextRef mac_create_bitmap_context(rdpContext* context)
{
	CGContextRef bitmap_context;
	rdpGdi* gdi = context->gdi;
	UINT32 bpp = GetBytesPerPixel(gdi->dstFormat);
	CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();

	if (bpp == 2)
	{
		bitmap_context = CGBitmapContextCreate(gdi->primary_buffer,
		                                       gdi->width, gdi->height, 5, gdi->stride,
		                                       colorSpace, kCGBitmapByteOrder16Little | kCGImageAlphaNoneSkipFirst);
	}
	else
	{
		bitmap_context = CGBitmapContextCreate(gdi->primary_buffer,
		                                       gdi->width, gdi->height, 8, gdi->stride,
		                                       colorSpace, kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst);
	}

	CGColorSpaceRelease(colorSpace);
	return bitmap_context;
}

BOOL mac_begin_paint(rdpContext* context)
{
	rdpGdi* gdi = context->gdi;

	if (!gdi)
		return FALSE;

	gdi->primary->hdc->hwnd->invalid->null = TRUE;
	return TRUE;
}

BOOL mac_end_paint(rdpContext* context)
{
	rdpGdi* gdi;
	HGDI_RGN invalid;
	NSRect newDrawRect;
	int ww, wh, dw, dh;
	mfContext* mfc = (mfContext*) context;
	MRDPView* view = (MRDPView*) mfc->view;
	gdi = context->gdi;

	if (!gdi)
		return FALSE;

	ww = mfc->client_width;
	wh = mfc->client_height;
	dw = mfc->context.settings->DesktopWidth;
	dh = mfc->context.settings->DesktopHeight;

	if ((!context) || (!context->gdi))
		return FALSE;

	if (context->gdi->primary->hdc->hwnd->invalid->null)
		return TRUE;

	invalid = gdi->primary->hdc->hwnd->invalid;
	newDrawRect.origin.x = invalid->x;
	newDrawRect.origin.y = invalid->y;
	newDrawRect.size.width = invalid->w;
	newDrawRect.size.height = invalid->h;

	if (mfc->context.settings->SmartSizing && (ww != dw || wh != dh))
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
	[view setNeedsDisplayInRect:newDrawRect];
	gdi->primary->hdc->hwnd->ninvalid = 0;
	return TRUE;
}

BOOL mac_desktop_resize(rdpContext* context)
{
	ResizeWindowEventArgs e;
	mfContext* mfc = (mfContext*) context;
	MRDPView* view = (MRDPView*) mfc->view;
	rdpSettings* settings = context->settings;

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

void update_activity_cb(freerdp* instance)
{
	int status;
	wMessage message;
	wMessageQueue* queue;
	status = 1;
	queue = freerdp_get_message_queue(instance, FREERDP_UPDATE_MESSAGE_QUEUE);

	if (queue)
	{
		while (MessageQueue_Peek(queue, &message, TRUE))
		{
			status = freerdp_message_queue_process_message(instance,
			         FREERDP_UPDATE_MESSAGE_QUEUE, &message);

			if (!status)
				break;
		}
	}
	else
	{
		WLog_ERR(TAG,  "update_activity_cb: No queue!");
	}
}

void input_activity_cb(freerdp* instance)
{
	int status;
	wMessage message;
	wMessageQueue* queue;
	status = 1;
	queue = freerdp_get_message_queue(instance, FREERDP_INPUT_MESSAGE_QUEUE);

	if (queue)
	{
		while (MessageQueue_Peek(queue, &message, TRUE))
		{
			status = freerdp_message_queue_process_message(instance,
			         FREERDP_INPUT_MESSAGE_QUEUE, &message);

			if (!status)
				break;
		}
	}
	else
	{
		WLog_ERR(TAG,  "input_activity_cb: No queue!");
	}
}

/**
 * given a rect with 0,0 at the top left (windows cords)
 * convert it to a rect with 0,0 at the bottom left (apple cords)
 *
 * Note: the formula works for conversions in both directions.
 *
 */

void windows_to_apple_cords(MRDPView* view, NSRect* r)
{
	r->origin.y = [view frame].size.height - (r->origin.y + r->size.height);
}

void sync_keyboard_state(freerdp* instance)
{
	mfContext* context = (mfContext*)instance->context;
	UINT32 flags = 0;
	CGEventFlags currentFlags = CGEventSourceFlagsState(kCGEventSourceStateHIDSystemState);

	if (context->kbdFlags != currentFlags)
	{
		if (currentFlags & kCGEventFlagMaskAlphaShift)
			flags |= KBD_SYNC_CAPS_LOCK;

		if (currentFlags & kCGEventFlagMaskNumericPad)
			flags |= KBD_SYNC_NUM_LOCK;

		freerdp_input_send_synchronize_event(instance->input, flags);
		context->kbdFlags = currentFlags;
	}
}

@end
