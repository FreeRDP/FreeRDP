//
//  MRDPView.h
//  MacFreeRDP
//
//  Created by Laxmikant Rashinkar on 3/28/12.
//  Copyright (c) 2012 FreeRDP.org All rights reserved.
//

#import <Cocoa/Cocoa.h>

typedef int boolean;

#include "freerdp/freerdp.h"
#include "freerdp/types.h"
#include "freerdp/channels/channels.h"
#include "freerdp/gdi/gdi.h"
#include "freerdp/graphics.h"
#include "freerdp/utils/event.h"
#include "freerdp/plugins/cliprdr.h"
#include "freerdp/utils/args.h"

@interface MRDPView : NSView
{
    CFRunLoopSourceRef run_loop_src;
    CFRunLoopSourceRef run_loop_src_channels;
    NSBitmapImageRep   *bmiRep;
    NSMutableArray     *cursors;
    NSTimer            *pasteboard_timer;
    NSRect             rect;
    freerdp            *rdp_instance;
    rdpContext         *rdp_context;
    boolean            mouseInClientArea;
    char               *pixel_data;
    int                width;
    int                height;
    int                argc;
    char               **argv;
    
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

- (void) rdpConnectEror;
- (void) saveStateInfo :(freerdp *) instance :(rdpContext *) context;
- (BOOL) eventIsInClientArea :(NSEvent *) event :(int *) xptr :(int *) yptr;
- (void) onPasteboardTimerFired :(NSTimer *) timer;
- (void) my_draw_rect :(void *) context;
- (void) releaseResources;

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
int rdp_connect();
boolean mac_pre_connect(freerdp *inst);
boolean mac_post_connect(freerdp *inst);
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
int receive_channel_data(freerdp *inst, int chan_id, uint8 *data, int size, int flags, int total_size);
void process_cliprdr_event(freerdp *inst);
void cliprdr_process_cb_format_list_event(freerdp *inst, RDP_CB_FORMAT_LIST_EVENT* event);
void cliprdr_send_data_request(freerdp *inst, uint32 format);
void cliprdr_process_cb_monitor_ready_event(freerdp* inst);
void cliprdr_process_cb_data_response_event(freerdp *inst, RDP_CB_DATA_RESPONSE_EVENT *event);
void cliprdr_process_text(freerdp *inst, uint8 *data, int len);
void cliprdr_send_supported_format_list(freerdp *inst);
int register_channel_fds(int *fds, int count, void *inst);

/* LK_TODO
int freerdp_parse_args(rdpSettings* settings, int argc, char** argv,
                       ProcessPluginArgs plugin_callback, void* plugin_user_data,
ProcessUIArgs ui_callback, void* ui_user_data);
*/

struct mac_context
{
    // *must* have this - do not delete
    rdpContext _p;
};

struct cursor
{
    rdpPointer  *pointer;
    uint8       *cursor_data;   // bitmapped pixel data
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

