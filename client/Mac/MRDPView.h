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

#import "MRDPWindow.h"
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
	freerdp* rdp_instance;
	rdpContext* rdp_context;
	CGContextRef bitmap_context;
	char* pixel_data;
	int width;
	int height;
	int argc;
	char** argv;
	
	/* RemoteApp */
	MRDPWindow* currentWindow;
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

- (void) rdpConnectError;
- (void) rdpRemoteAppError;
- (void) saveStateInfo :(freerdp *) instance :(rdpContext *) context;
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

void mf_Pointer_New(rdpContext* context, rdpPointer* pointer);
void mf_Pointer_Free(rdpContext* context, rdpPointer* pointer);
void mf_Pointer_Set(rdpContext* context, rdpPointer* pointer);
void mf_Pointer_SetNull(rdpContext* context);
void mf_Pointer_SetDefault(rdpContext* context);
int rdp_connect(void);
BOOL mac_pre_connect(freerdp* instance);
BOOL mac_post_connect(freerdp*	instance);
BOOL mac_authenticate(freerdp* instance, char** username, char** password, char** domain);
void mac_context_new(freerdp* instance, rdpContext* context);
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

void mac_process_rail_event(freerdp* instance, wMessage* event);
void mac_rail_register_callbacks(freerdp* instance, rdpRail* rail);
void mac_rail_CreateWindow(rdpRail* rail, rdpWindow* window);
void mac_rail_MoveWindow(rdpRail* rail, rdpWindow* window);
void mac_rail_ShowWindow(rdpRail* rail, rdpWindow* window, BYTE state);
void mac_rail_SetWindowText(rdpRail* rail, rdpWindow* window);
void mac_rail_SetWindowIcon(rdpRail* rail, rdpWindow* window, rdpIcon* icon);
void mac_rail_SetWindowRects(rdpRail* rail, rdpWindow* window);
void mac_rail_SetWindowVisibilityRects(rdpRail* rail, rdpWindow* window);
void mac_rail_DestroyWindow(rdpRail* rail, rdpWindow* window);
void mac_process_rail_get_sysparams_event(rdpChannels* channels, wMessage* event);
void mac_send_rail_client_event(rdpChannels* channels, UINT16 event_type, void* param);
void mac_on_free_rail_client_event(wMessage* event);
void mac_process_rail_server_sysparam_event(rdpChannels* channels, wMessage* event);
void mac_process_rail_exec_result_event(rdpChannels* channels, wMessage* event);
void mac_rail_enable_remoteapp_mode(void);
void mac_process_rail_server_minmaxinfo_event(rdpChannels* channels, wMessage* event);
void mac_process_rail_server_localmovesize_event(freerdp* instance, wMessage* event);
void apple_center_window(NSRect* r);
void apple_to_windowMove(NSRect* r, RAIL_WINDOW_MOVE_ORDER * windowMove);

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

