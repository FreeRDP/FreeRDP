//
//  MRDPView.h
//  MacFreeRDP
//
//  Created by Laxmikant Rashinkar
//  Copyright (c) 2012 FreeRDP.org All rights reserved.
//

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
    NSBitmapImageRep   *bmiRep;
    NSMutableArray     *cursors;
    NSMutableArray     *windows;
    NSTimer            *pasteboard_timer;
    NSRect             rect;
    NSRect             prevWinPosition;
    freerdp            *rdp_instance;
    rdpContext         *rdp_context;
    char               *pixel_data;
    int                width;
    int                height;
    int                argc;
    int                titleBarHeight;
    char               **argv;
    
    // RAIL stuff
    MRDPWindow         *currentWindow;
    NSPoint            savedDragLocation;
    BOOL            mouseInClientArea;
    BOOL            isRemoteApp;
    BOOL            firstCreateWindow;
    BOOL            isMoveSizeInProgress;
    BOOL            skipResizeOnce;
    BOOL            saveInitialDragLoc;
    BOOL            skipMoveWindowOnce;
    
    // store state info for some keys
    int                kdlshift;
    int                kdrshift;
    int                kdlctrl;
    int                kdrctrl;
    int                kdlalt;
    int                kdralt;
    int                kdlmeta;
    int                kdrmeta;
    int                kdcapslock;

@public
    NSWindow           *ourMainWindow;
    NSPasteboard       *pasteboard_rd; // for reading from clipboard
    NSPasteboard       *pasteboard_wr; // for writing to clipboard
    int                pasteboard_changecount;
    int                pasteboard_format;
    int                is_connected;   // true when connected to RDP server
}

- (void) rdpConnectError;
- (void) rdpRemoteAppError;
- (void) saveStateInfo :(freerdp *) instance :(rdpContext *) context;
- (void) onPasteboardTimerFired :(NSTimer *) timer;
- (void) my_draw_rect :(void *) context;
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

void pointer_new(rdpContext* context, rdpPointer* pointer);
void pointer_free(rdpContext* context, rdpPointer* pointer);
void pointer_set(rdpContext* context, rdpPointer* pointer);
void pointer_setNull(rdpContext* context);
void pointer_setDefault(rdpContext* context);
int rdp_connect(void);
BOOL mac_pre_connect(freerdp *inst);
BOOL mac_post_connect(freerdp *inst);
void mac_context_new(freerdp *inst, rdpContext *context);
void mac_context_free(freerdp *inst, rdpContext *context);
void mac_set_bounds(rdpContext *context, rdpBounds *bounds);
void mac_bitmap_update(rdpContext *context, BITMAP_UPDATE *bitmap);
void mac_begin_paint(rdpContext *context);
void mac_end_paint(rdpContext* context);
void mac_save_state_info(freerdp *inst, rdpContext *context);
void skt_activity_cb(CFSocketRef s, CFSocketCallBackType callbackType, CFDataRef address, const void *data, void *info);
void channel_activity_cb(CFSocketRef s, CFSocketCallBackType callbackType, CFDataRef address, const void *data, void *info);
int register_fds(int *fds, int count, void *inst);
int invoke_draw_rect(rdpContext *context);
int process_plugin_args(rdpSettings* settings, const char* name, RDP_PLUGIN_DATA* plugin_data, void* user_data);
int receive_channel_data(freerdp *inst, int chan_id, BYTE *data, int size, int flags, int total_size);
void process_cliprdr_event(freerdp *inst, RDP_EVENT *event);
void cliprdr_process_cb_format_list_event(freerdp *inst, RDP_CB_FORMAT_LIST_EVENT* event);
void cliprdr_send_data_request(freerdp *inst, UINT32 format);
void cliprdr_process_cb_monitor_ready_event(freerdp* inst);
void cliprdr_process_cb_data_response_event(freerdp *inst, RDP_CB_DATA_RESPONSE_EVENT *event);
void cliprdr_process_text(freerdp *inst, BYTE *data, int len);
void cliprdr_send_supported_format_list(freerdp *inst);
int register_channel_fds(int *fds, int count, void *inst);

void mac_process_rail_event(freerdp *inst, RDP_EVENT *event);
void mac_rail_register_callbacks(freerdp *inst, rdpRail *rail);
void mac_rail_CreateWindow(rdpRail *rail, rdpWindow *window);
void mac_rail_MoveWindow(rdpRail *rail, rdpWindow *window);
void mac_rail_ShowWindow(rdpRail *rail, rdpWindow *window, BYTE state);
void mac_rail_SetWindowText(rdpRail *rail, rdpWindow *window);
void mac_rail_SetWindowIcon(rdpRail *rail, rdpWindow *window, rdpIcon *icon);
void mac_rail_SetWindowRects(rdpRail *rail, rdpWindow *window);
void mac_rail_SetWindowVisibilityRects(rdpRail *rail, rdpWindow *window);
void mac_rail_DestroyWindow(rdpRail *rail, rdpWindow *window);
void mac_process_rail_get_sysparams_event(rdpChannels *channels, RDP_EVENT *event);
void mac_send_rail_client_event(rdpChannels *channels, UINT16 event_type, void *param);
void mac_on_free_rail_client_event(RDP_EVENT* event);
void mac_process_rail_server_sysparam_event(rdpChannels* channels, RDP_EVENT* event);
void mac_process_rail_exec_result_event(rdpChannels* channels, RDP_EVENT* event);
void mac_rail_enable_remoteapp_mode(void);
void mac_process_rail_server_minmaxinfo_event(rdpChannels* channels, RDP_EVENT* event);
void mac_process_rail_server_localmovesize_event(freerdp *inst, RDP_EVENT *event);
void apple_center_window(NSRect * r);
void apple_to_windowMove(NSRect * r, RAIL_WINDOW_MOVE_ORDER * windowMove);

struct mac_context
{
    // *must* have this - do not delete
    rdpContext _p;
};

struct cursor
{
    rdpPointer  *pointer;
    BYTE       *cursor_data;   // bitmapped pixel data
    void        *bmiRep;        // NSBitmapImageRep
    void        *nsCursor;      // NSCursor
    void        *nsImage;       // NSImage
};

struct rgba_data
{
    char red;
    char green;
    char blue;
    char alpha;
};

struct kkey
{
    int key_code;
    int flags;
};

