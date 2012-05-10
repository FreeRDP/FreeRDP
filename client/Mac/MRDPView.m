//
//  MRDPView.m
//  MacFreeRDP
//
//  Created by Laxmikant Rashinkar on 3/28/12.
//  Copyright (c) 2012 FreeRDP.org All rights reserved.
//

/*
 * TODO
 *      + provide a UI for configuring optional parameters, but keep cmd line args
 *      + audio redirection is delayed considerably
 *      + caps lock key needs to be sent in func flagsChanged()
 *      + libfreerdp-utils.1.0.dylib needs to be installed to /usr/local/lib
 */
 
#import "MRDPView.h"
#import "MRDPCursor.h"

#define __RUN_IN_XCODE

@implementation MRDPView

MRDPView *g_mrdpview;

@synthesize is_connected;

struct kkey g_keys[256] =
{
    { 0x1e, 0 }, // a           0
    { 0x1f, 0 }, // s
    { 0x20, 0 }, // d
    { 0x21, 0 }, // f
    { 0x23, 0 }, // h
    { 0x22, 0 }, // g
    { 0x2c, 0 }, // z
    { 0x2d, 0 }, // x
    { 0x2e, 0 }, // c
    { 0x2f, 0 }, // v
    { 0x00, 0 }, //             10
    { 0x30, 0 }, // b
    { 0x10, 0 }, // q
    { 0x11, 0 }, // w
    { 0x12, 0 }, // e
    { 0x13, 0 }, // r
    { 0x15, 0 }, // y
    { 0x14, 0 }, // t
    { 0x02, 0 }, // 1
    { 0x03, 0 }, // 2
    { 0x04, 0 }, // 3           20
    { 0x05, 0 }, // 4
    { 0x07, 0 }, // 6
    { 0x06, 0 }, // 5
    { 0x0d, 0 }, // = or +
    { 0x0a, 0 }, // 9
    { 0x08, 0 }, // 7
    { 0x0c, 0 }, // - or _
    { 0x09, 0 }, // 8
    { 0x0b, 0 }, // 0
    { 0x1b, 0 }, // ] or }      30
    { 0x18, 0 }, // o
    { 0x16, 0 }, // u
    { 0x1a, 0 }, // [ or {
    { 0x17, 0 }, // i
    { 0x19, 0 }, // p
    { 0x1c, 0 }, // enter
    { 0x26, 0 }, // l
    { 0x24, 0 }, // j
    { 0x28, 0 }, // ' or "
    { 0x25, 0 }, // k           40
    { 0x27, 0 }, // ; or :
    { 0x2b, 0 }, // \ or |
    { 0x33, 0 }, // , or <
    { 0x35, 0 }, // / or ?
    { 0x31, 0 }, // n
    { 0x32, 0 }, // m
    { 0x34, 0 }, // . or >
    { 0x0f, 0 }, // tab
    { 0x39, 0 }, // space
    { 0x29, 0 }, // ` or ~      50
    { 0x0e, 0 }, // backspace
    { 0x00, 0 }, //
    { 0x01, 0 }, // esc
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 }, //             60
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x53, 0 }, // KP.
    { 0x00, 0 },
    { 0x37, 0 }, // KP*
    { 0x00, 0 },
    { 0x4e, 0 }, // KP+
    { 0x00, 0 }, //             70
    { 0x45, 0 }, // num lock
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x35, 1 }, // KP/
    { 0x1c, 1 }, // KPEnter
    { 0x00, 0 },
    { 0x4a, 0 }, // KP-
    { 0x00, 0 },
    { 0x00, 0 }, //             80
    { 0x00, 0 },
    { 0x52, 0 }, // KP0
    { 0x4f, 0 }, // KP1
    { 0x50, 0 }, // KP2
    { 0x51, 0 }, // KP3
    { 0x4b, 0 }, // KP4
    { 0x4c, 0 }, // KP5
    { 0x4d, 0 }, // KP6
    { 0x47, 0 }, // KP7
    { 0x00, 0 }, //             90
    { 0x48, 0 }, // KP8
    { 0x49, 0 }, // KP9
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 }, //             100
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x5d, 1 }, // menu        110
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x52, 1 }, // Insert
    { 0x47, 1 }, // Home
    { 0x49, 1 }, // PgUp
    { 0x53, 1 }, // Delete
    { 0x00, 0 },
    { 0x4f, 1 }, // End
    { 0x00, 0 }, //             120
    { 0x51, 1 }, // PgDown
    { 0x3b, 0 }, // f1
    { 0x4b, 1 }, // left
    { 0x4d, 1 }, // right
    { 0x50, 1 }, // down
    { 0x48, 1 }, // up
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
    { 0x00, 0 },
};

/************************************************************************
                         methods we override
************************************************************************/

/** *********************************************************************
 * create MRDPView with specified rectangle
 ***********************************************************************/

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
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
    width = [self bounds].size.width;
    height = [self bounds].size.height;
    
    [[self window] becomeFirstResponder];
    [[self window] setAcceptsMouseMovedEvents:YES];
    
    cursors = [[NSMutableArray alloc] initWithCapacity:10];
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
    int x;
    int y;
    
    [super mouseMoved:event];
    
    if (!is_connected) {
        return;
    }
    
    // send mouse motion event to RDP server
    if ([self eventIsInClientArea :event :&x :&y]) {
        rdp_instance->input->MouseEvent(rdp_instance->input, PTR_FLAGS_MOVE, x, y);
    }
}

/** *********************************************************************
 * called when left mouse button is pressed down
 ***********************************************************************/

- (void)mouseDown:(NSEvent *) event
{
    int x;
    int y;

    [super mouseDown:event];
    
    if (!is_connected) {
        return;
    }
    
    if ([self eventIsInClientArea :event :&x :&y]) {
        rdp_instance->input->MouseEvent(rdp_instance->input, PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON1, x, y);
    }
}

/** *********************************************************************
 * called when left mouse button is released
 ***********************************************************************/

- (void) mouseUp:(NSEvent *) event
{
    int x;
    int y;

    [super mouseUp:event];
    
    if (!is_connected) {
        return;
    }
    
    if ([self eventIsInClientArea :event :&x :&y]) {
        rdp_instance->input->MouseEvent(rdp_instance->input, PTR_FLAGS_BUTTON1, x, y);
    }
}

/** *********************************************************************
 * called when right mouse button is pressed down
 ***********************************************************************/

- (void) rightMouseDown:(NSEvent *)event
{
    int x;
    int y;
    
    [super rightMouseDown:event];
    
    if (!is_connected) {
        return;
    }
    
    if ([self eventIsInClientArea :event :&x :&y]) {
        rdp_instance->input->MouseEvent(rdp_instance->input, PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON2, x, y);
    }
}

/** *********************************************************************
 * called when right mouse button is released
 ***********************************************************************/

- (void) rightMouseUp:(NSEvent *)event
{
    int x;
    int y;
    
    [super rightMouseUp:event];
    
    if (!is_connected) {
        return;
    }
    
    if ([self eventIsInClientArea :event :&x :&y]) {
        rdp_instance->input->MouseEvent(rdp_instance->input, PTR_FLAGS_BUTTON2, x, y);
    }
}

/** *********************************************************************
 * called when middle mouse button is pressed
 ***********************************************************************/

- (void) otherMouseDown:(NSEvent *)event
{
    int x;
    int y;
    
    [super otherMouseDown:event];
    
    if (!is_connected) {
        return;
    }
    
    if ([self eventIsInClientArea :event :&x :&y]) {
        rdp_instance->input->MouseEvent(rdp_instance->input, PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON3, x, y);
    }
}

/** *********************************************************************
 * called when middle mouse button is released
 ***********************************************************************/

- (void) otherMouseUp:(NSEvent *)event
{
    int x;
    int y;
    
    [super otherMouseUp:event];
    
    if (!is_connected) {
        return;
    }
    
    if ([self eventIsInClientArea :event :&x :&y]) {
        rdp_instance->input->MouseEvent(rdp_instance->input, PTR_FLAGS_BUTTON3, x, y);
    }
}

- (void) scrollWheel:(NSEvent *)event
{
    uint16 flags;
    int    x;
    int    y;
    
    [super scrollWheel:event];
    
    if (!is_connected) {
        return;
    }
    
    if ([self eventIsInClientArea :event :&x :&y]) {
        flags = PTR_FLAGS_WHEEL;
        if ([event deltaY] < 0) {
            flags |= PTR_FLAGS_WHEEL_NEGATIVE | 0x0088;
        }
        else {
            flags |= 0x0078;
        }
        x += (int) [event deltaX];
        y += (int) [event deltaY];
        rdp_instance->input->MouseEvent(rdp_instance->input, flags, x, y);
    }
}

/** *********************************************************************
 * called when mouse is moved with left button pressed
 * note: invocation order is: mouseDown, mouseDragged, mouseUp
 ***********************************************************************/

- (void) mouseDragged:(NSEvent *)event
{
    int x;
    int y;
    
    [super mouseDragged:event];
    
    if (!is_connected) {
        return;
    }
    
    // send mouse motion event to RDP server
    if ([self eventIsInClientArea :event :&x :&y]) {
        rdp_instance->input->MouseEvent(rdp_instance->input, PTR_FLAGS_MOVE, x, y);
    }
}

/** *********************************************************************
 * called when a key is pressed
 ***********************************************************************/

- (void) keyDown:(NSEvent *) event
{
    int key;
    
    if (!is_connected) {
        return;
    }
    
    key = [event keyCode];
    rdp_instance->input->KeyboardEvent(rdp_instance->input, g_keys[key].flags | KBD_FLAGS_DOWN, g_keys[key].key_code);
}

/** *********************************************************************
 * called when a key is released
 ***********************************************************************/

- (void) keyUp:(NSEvent *) event
{
    int key;
 
    if (!is_connected) {
        return;
    }
    
    key = [event keyCode];
    rdp_instance->input->KeyboardEvent(rdp_instance->input, g_keys[key].flags | KBD_FLAGS_RELEASE, g_keys[key].key_code);
}

/** *********************************************************************
 * called when shift, control, alt and meta keys are pressed/released
 ***********************************************************************/

- (void) flagsChanged:(NSEvent *) event
{
    NSUInteger mf = [event modifierFlags];
    
    if (!is_connected) {
        return;
    }
    
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

- (void)drawRect:(NSRect)dirtyRect
{
#if 1
    [bmiRep drawInRect:rect];
#else
    // do not delete - may need this soon
    NSImage *image = [[NSImage alloc] initWithSize:[bmiRep size]];
    [image addRepresentation:bmiRep];
    [image setFlipped:0];
    [image drawInRect:dirtyRect fromRect:rect operation:NSCompositeSourceOver fraction:1.0];
#endif
}

/************************************************************************
                              instance methods
************************************************************************/

/** *********************************************************************
 * called when RDP server wants us to update a rect with new data
 ***********************************************************************/

- (void) my_draw_rect:(void *)context
{
    rdpContext *ctx = (rdpContext *) context;
    
    struct rgba_data {
        char red;
        char green;
        char blue;
        char alpha;
    };
    
    rect.size.width = ctx->gdi->width;
    rect.size.height = ctx->gdi->height;
    rect.origin.x = 0;
    rect.origin.y = 0;
    
    if (!bmiRep) {
        pixel_data = (char *) malloc(rect.size.width * rect.size.height * sizeof(struct rgba_data));
        bmiRep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:(unsigned char **) &pixel_data
                                                         pixelsWide:rect.size.width
                                                         pixelsHigh:rect.size.height
                                                      bitsPerSample:8
                                                    samplesPerPixel:sizeof(struct rgba_data)
                                                           hasAlpha:YES
                                                           isPlanar:NO 
                                                     colorSpaceName:NSDeviceRGBColorSpace 
                                                       bitmapFormat:0 //NSAlphaFirstBitmapFormat
                                                        bytesPerRow:rect.size.width * sizeof(struct rgba_data)
                                                       bitsPerPixel:0];
    }
    
    cvt_argb_to_rgba(pixel_data, (char *) ctx->gdi->primary_buffer, rect.size.width * rect.size.height * sizeof(struct rgba_data));
    
    [self setNeedsDisplayInRect:rect]; // TODO: do only for reqd rect
}

/** *********************************************************************
 * save state info for use by other methods later on
 ***********************************************************************/

- (void) saveStateInfo:(freerdp *) instance:(rdpContext *) context
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
    
    if ((x < 0) || (y < 0)) {
        if (mouseInClientArea) {
            // set default cursor before leaving client area
            mouseInClientArea = NO;
            NSCursor *cur = [NSCursor arrowCursor];
            [cur set];
        }
        return NO;
    }
    if ((x > width) || (y > height)) {
        if (mouseInClientArea) {
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

- (void) rdpConnectEror
{
    
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:@"Error connecting to server"];
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

- (void) onPasteboardTimerFired :(NSTimer *) timer
{
    NSArray *types;
    int     i;
    
    i = [pasteboard_rd changeCount];
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

int rdp_connect()
{
    freerdp *inst;
    int     status;
    
    freerdp_channels_global_init();

    inst = freerdp_new();
    inst->PreConnect = mac_pre_connect; 
    inst->PostConnect = mac_post_connect;
    inst->context_size = sizeof(struct mac_context);
    inst->ContextNew = mac_context_new;
    inst->ContextFree = mac_context_free;
    inst->ReceiveChannelData = receive_channel_data;
    freerdp_context_new(inst);
    
    status = freerdp_connect(inst);
    if(status) {
        freerdp_check_fds(inst);
        [g_mrdpview setIs_connected:1];
        return 0;
    }
    [g_mrdpview setIs_connected:0];
    [g_mrdpview rdpConnectEror];
    return -1;
}

/** *********************************************************************
 * a callback given to freerdp_connect() to process the pre-connect operations.
 *
 * @param inst  - pointer to a rdp_freerdp struct that contains the connection's parameters, and 
 *                will be filled with the appropriate informations.
 *
 * @return true if successful. false otherwise.
 ************************************************************************/

boolean mac_pre_connect(freerdp *inst)
{
    char *cptr;
    int  len;
    int  i;
    
    inst->settings->offscreen_bitmap_cache = false;
    inst->settings->glyph_cache = true;
    inst->settings->glyphSupportLevel = GLYPH_SUPPORT_FULL;
    inst->settings->order_support[NEG_GLYPH_INDEX_INDEX] = true;
    inst->settings->order_support[NEG_FAST_GLYPH_INDEX] = false;
    inst->settings->order_support[NEG_FAST_INDEX_INDEX] = false;
    inst->settings->order_support[NEG_SCRBLT_INDEX] = true;
    inst->settings->order_support[NEG_SAVEBITMAP_INDEX] = false;
    
    inst->settings->bitmap_cache = true;
    inst->settings->order_support[NEG_MEMBLT_INDEX] = true;
    inst->settings->order_support[NEG_MEMBLT_V2_INDEX] = true;
    inst->settings->order_support[NEG_MEM3BLT_INDEX] = false;
    inst->settings->order_support[NEG_MEM3BLT_V2_INDEX] = false;
    inst->settings->bitmapCacheV2NumCells = 3; // 5;
    inst->settings->bitmapCacheV2CellInfo[0].numEntries = 0x78; // 600;
    inst->settings->bitmapCacheV2CellInfo[0].persistent = false;
    inst->settings->bitmapCacheV2CellInfo[1].numEntries = 0x78; // 600;
    inst->settings->bitmapCacheV2CellInfo[1].persistent = false;
    inst->settings->bitmapCacheV2CellInfo[2].numEntries = 0x150; // 2048;
    inst->settings->bitmapCacheV2CellInfo[2].persistent = false;
    inst->settings->bitmapCacheV2CellInfo[3].numEntries = 0; // 4096;
    inst->settings->bitmapCacheV2CellInfo[3].persistent = false;
    inst->settings->bitmapCacheV2CellInfo[4].numEntries = 0; // 2048;
    inst->settings->bitmapCacheV2CellInfo[4].persistent = false;
    
    inst->settings->order_support[NEG_MULTIDSTBLT_INDEX] = false;
    inst->settings->order_support[NEG_MULTIPATBLT_INDEX] = false;
    inst->settings->order_support[NEG_MULTISCRBLT_INDEX] = false;
    inst->settings->order_support[NEG_MULTIOPAQUERECT_INDEX] = false;
    inst->settings->order_support[NEG_POLYLINE_INDEX] = false;
    inst->settings->color_depth = 24;
    inst->settings->sw_gdi = 1;
    
    // setup callbacks
    inst->update->BeginPaint = mac_begin_paint;
    inst->update->EndPaint = mac_end_paint;
    inst->update->SetBounds = mac_set_bounds;
    inst->update->BitmapUpdate = mac_bitmap_update;
    
    NSArray *args = [[NSProcessInfo processInfo] arguments];

#ifdef RUN_IN_XCODE
    // if you add any additional hard coded 
    // cmd line args, update this first
    g_mrdpview->argc = 10;
#else
    g_mrdpview->argc = [args count];
#endif

    g_mrdpview->argv = malloc(sizeof(char *) * g_mrdpview->argc);
    if (g_mrdpview->argv == NULL) {
        return false;
    }
    
#ifdef RUN_IN_XCODE
    // create our own cmd line args
    i = 0;
    
    // argv[0]
    NSString *sptr = [args objectAtIndex:0];
    len = [sptr length] + 1;
    cptr = (char *) malloc(len);
    strcpy(cptr, [sptr UTF8String]);
    g_mrdpview->argv[i++] = cptr;

    // argv[1]
    cptr = (char *)malloc(80);
    strcpy(cptr, "-g");
    g_mrdpview->argv[i++] = cptr;
    
    // argv[2]
    cptr = (char *)malloc(80);
    strcpy(cptr, "800x600");
    g_mrdpview->argv[i++] = cptr;
    
    // argv[3]
    cptr = (char *)malloc(80);
    strcpy(cptr, "--ignore-certificate");
    g_mrdpview->argv[i++] = cptr;
    
    // argv[4]
    cptr = (char *)malloc(80);
    strcpy(cptr, "--no-nla");
    g_mrdpview->argv[i++] = cptr;
    
    // argv[5]
    cptr = (char *)malloc(80);
    strcpy(cptr, "--plugin");
    g_mrdpview->argv[i++] = cptr;
    
    // argv[6]
    cptr = (char *)malloc(80);
    strcpy(cptr, "cliprdr");
    g_mrdpview->argv[i++] = cptr;
    
    // argv[7]
    cptr = (char *)malloc(80);
    strcpy(cptr, "--plugin");
    g_mrdpview->argv[i++] = cptr;
    
    // argv[8]
    cptr = (char *)malloc(80);
    strcpy(cptr, "rdpsnd");
    g_mrdpview->argv[i++] = cptr;
    
    // argv[9]
    cptr = (char *)malloc(80);
    strcpy(cptr, "23.20.8.223");
    g_mrdpview->argv[i++] = cptr;
#else
    // MacFreeRDP was not run in Xcode
    i = 0;
    for (NSString *str in args)
    {
        len = [str length] + 1;
        cptr = (char *) malloc(len);
        strcpy(cptr, [str UTF8String]);
        g_mrdpview->argv[i++] = cptr;
    }
#endif
    
    freerdp_parse_args(inst->settings, g_mrdpview->argc, g_mrdpview->argv, process_plugin_args, inst->context->channels, NULL, NULL);
    if ((strcmp(g_mrdpview->argv[1], "-h") == 0) || (strcmp(g_mrdpview->argv[1], "--help") == 0)) {
        [NSApp terminate:nil];
        return true;
    }

    // set window size based on cmd line arguments
    g_mrdpview->width = inst->settings->width;
    g_mrdpview->height = inst->settings->height;

    NSRect rect;
    rect.origin.x = 0;
    rect.origin.y = 0;
    rect.size.width = g_mrdpview->width;
    rect.size.height = g_mrdpview->height;
    
    [[g_mrdpview window] setMaxSize:rect.size];
    [[g_mrdpview window] setMinSize:rect.size];
    [[g_mrdpview window] setFrame:rect display:YES];
    
    freerdp_channels_pre_connect(inst->context->channels, inst);
    return true;
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

boolean mac_post_connect(freerdp *inst)
{
    uint32     flags;
    rdpPointer rdp_pointer;
    void       *rd_fds[32];
    void       *wr_fds[32];
    int        rd_count = 0;
    int        wr_count = 0;
    int        index;
    int        fds[32];
    
    memset(&rdp_pointer, 0, sizeof(rdpPointer));
    rdp_pointer.size = sizeof(rdpPointer);
    rdp_pointer.New = pointer_new;
    rdp_pointer.Free = pointer_free;
    rdp_pointer.Set = pointer_set;
    rdp_pointer.SetNull = pointer_setNull;
    rdp_pointer.SetDefault = pointer_setDefault;
    
    flags = CLRCONV_ALPHA;
    flags |= CLRBUF_32BPP;
    
    gdi_init(inst, flags, NULL);
    pointer_cache_register_callbacks(inst->update);
    graphics_register_pointer(inst->context->graphics, &rdp_pointer);
    
    // register file descriptors with the RunLoop
    if (!freerdp_get_fds(inst, rd_fds, &rd_count, 0, 0))
    {
        printf("mac_post_connect: freerdp_get_fds() failed!\n");
    }
    
    for (index = 0; index < rd_count; index++)
    {
        fds[index] = (int)(long)rd_fds[index];
    }
    register_fds(fds, rd_count, inst);
    
    // register channel manager file descriptors with the RunLoop
    if (!freerdp_channels_get_fds(inst->context->channels, inst, rd_fds, &rd_count, wr_fds, &wr_count))
    {
        printf("ERROR: freerdp_channels_get_fds() failed\n");
    }
    for (index = 0; index < rd_count; index++)
    {
        fds[index] = (int)(long)rd_fds[index];
    }
    register_channel_fds(fds, rd_count, inst);
    freerdp_channels_post_connect(inst->context->channels, inst);
    
    // setup pasteboard (aka clipboard) for copy operations (write only)
    g_mrdpview->pasteboard_wr = [NSPasteboard generalPasteboard];

    // setup pasteboard for read operations
    g_mrdpview->pasteboard_rd = [NSPasteboard generalPasteboard];
    g_mrdpview->pasteboard_changecount = [g_mrdpview->pasteboard_rd changeCount];
    g_mrdpview->pasteboard_timer = [NSTimer scheduledTimerWithTimeInterval:0.5 target:g_mrdpview selector:@selector(onPasteboardTimerFired:) userInfo:nil repeats:YES];
    return true;
}

/** *********************************************************************
 * create a new mouse cursor
 *
 * @param context our context state
 * @param pointer information about the cursor to create
 *
 ************************************************************************/

void pointer_new(rdpContext* context, rdpPointer* pointer)
{
    MRDPCursor *mrdpCursor = [[MRDPCursor alloc] init];
    uint8      *cursor_data;
    
    NSRect rect;
    rect.size.width = pointer->width;
    rect.size.height = pointer->height;
    rect.origin.x = pointer->xPos;
    rect.origin.y = pointer->yPos;

    cursor_data = (uint8 *) malloc(rect.size.width * rect.size.height * 4);
    mrdpCursor->cursor_data = cursor_data;
    
    freerdp_alpha_cursor_convert(cursor_data, pointer->xorMaskData, pointer->andMaskData,
                                 pointer->width, pointer->height, pointer->xorBpp, context->gdi->clrconv);
    
    // TODO if xorBpp is > 24 need to call freerdp_image_swap_color_order
    //         see file df_graphics.c
    
    // store cursor bitmap image in representation - required by NSImage
    NSBitmapImageRep *bmiRep;
    bmiRep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:(unsigned char **) &cursor_data
                                                     pixelsWide:rect.size.width
                                                     pixelsHigh:rect.size.height
                                                  bitsPerSample:8
                                                samplesPerPixel:sizeof(struct rgba_data)
                                                       hasAlpha:YES
                                                       isPlanar:NO 
                                                 colorSpaceName:NSDeviceRGBColorSpace 
                                                   bitmapFormat:0
                                                    bytesPerRow:rect.size.width * 4
                                                   bitsPerPixel:0];
    mrdpCursor->bmiRep = bmiRep;
    
    // create an image using above representation
    NSImage *image = [[NSImage alloc] initWithSize:[bmiRep size]];
    [image addRepresentation: bmiRep];
    [image setFlipped:NO];
    mrdpCursor->nsImage = image;
    
    // need hotspot to create cursor
    NSPoint hotSpot;
    hotSpot.x = pointer->xPos;
    hotSpot.y = pointer->yPos;
    
    NSCursor *cursor = [[NSCursor alloc] initWithImage: image hotSpot:hotSpot];
    mrdpCursor->nsCursor = cursor;
    mrdpCursor->pointer = pointer;

    // save cursor for later use in pointer_set()
    NSMutableArray *ma = g_mrdpview->cursors;
    [ma addObject:mrdpCursor];
}  

/** *********************************************************************
 * release resources on specified cursor
 ************************************************************************/

void pointer_free(rdpContext* context, rdpPointer* pointer)
{
    NSMutableArray *ma = g_mrdpview->cursors;
    
    for (MRDPCursor *cursor in ma)
    {
        if (cursor->pointer == pointer) {
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

void pointer_set(rdpContext* context, rdpPointer* pointer)
{
    NSMutableArray *ma = g_mrdpview->cursors;
    
    if (!g_mrdpview->mouseInClientArea)
    {
        return; // not in client area
    }
    
    for (MRDPCursor *cursor in ma)
    {
        if (cursor->pointer == pointer) {
            [cursor->nsCursor set];
            return;
        }
    }
}

/** *********************************************************************
 * do not display any mouse cursor
 ***********************************************************************/

void pointer_setNull(rdpContext* context)
{
}

/** *********************************************************************
 * display default mouse cursor
 ***********************************************************************/

void pointer_setDefault(rdpContext* context)
{
}

/** *********************************************************************
 * create a new context - but all we really need to do is save state info
 ***********************************************************************/

void mac_context_new(freerdp *inst, rdpContext *context)
{
    [g_mrdpview saveStateInfo:inst :context];
    context->channels = freerdp_channels_new();
}

/** *********************************************************************
 * we don't do much over here
 ***********************************************************************/

void mac_context_free(freerdp *inst, rdpContext *context)
{
}

/** *********************************************************************
 * clip drawing surface so nothing is drawn outside specified bounds
 ***********************************************************************/

void mac_set_bounds(rdpContext *context, rdpBounds *bounds)
{
}

/** *********************************************************************
 * we don't do much over here
 ***********************************************************************/

void mac_bitmap_update(rdpContext *context, BITMAP_UPDATE *bitmap)
{
}

/** *********************************************************************
 * we don't do much over here
 ***********************************************************************/

void mac_begin_paint(rdpContext *context)
{
}

/** *********************************************************************
 * RDP server wants us to draw new data in the view
 ***********************************************************************/

void mac_end_paint(rdpContext* context)
{
    if ((context == 0) || (context->gdi == 0)) {
        return;
    } 
    [g_mrdpview my_draw_rect:(void *) context];
}

/** *********************************************************************
 * called when data is available on a socket
 ***********************************************************************/

void skt_activity_cb(
                     CFSocketRef s,
                     CFSocketCallBackType callbackType,
                     CFDataRef address,
                     const void *data,
                     void *info
                     )
{
    if (!freerdp_check_fds(info)) {
        // lost connection or did not connect
        [NSApp terminate:nil];
    }
}

/** *********************************************************************
 * called when data is available on a virtual channel
 ***********************************************************************/

void channel_activity_cb(
                     CFSocketRef s,
                     CFSocketCallBackType callbackType,
                     CFDataRef address,
                     const void *data,
                     void *info   
                     )
{
    freerdp *inst = (freerdp *) info;
    
    freerdp_channels_check_fds(inst->context->channels, inst);
    process_cliprdr_event(inst);
}

/** *********************************************************************
 * setup callbacks for data availability on sockets
 ***********************************************************************/

int register_fds(int *fds, int count, void *inst)
{
    int i;
    CFSocketRef skt_ref;
    CFSocketContext skt_context = { 0, inst, NULL, NULL, NULL };
    
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

int register_channel_fds(int *fds, int count, void *inst)
{
    int i;
    CFSocketRef skt_ref;
    CFSocketContext skt_context = { 0, inst, NULL, NULL, NULL };
    
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

int receive_channel_data(freerdp *inst, int chan_id, uint8 *data, int size, int flags, int total_size)
{
    return freerdp_channels_data(inst, chan_id, data, size, flags, total_size);
}

/** *********************************************************************
 * convert an array containing ARGB data to RGBA
 ***********************************************************************/

void cvt_argb_to_rgba(char *dest, char *src, int len)
{
    int i;
    
    for (i = 0; i < len; i += 4)
    {
        dest[i    ] = src[i + 2];
        dest[i + 1] = src[i + 1];
        dest[i + 2] = src[i + 0];
        dest[i + 3] = src[i + 3];
    }
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

void cliprdr_process_cb_data_request_event(freerdp *inst)
{
    RDP_CB_DATA_RESPONSE_EVENT *event;
    NSArray                    *types;
    int                        len;
    
    event = (RDP_CB_DATA_RESPONSE_EVENT*) freerdp_event_new(RDP_EVENT_CLASS_CLIPRDR,
                                                          RDP_EVENT_TYPE_CB_DATA_RESPONSE, NULL, NULL);
    types = [NSArray arrayWithObject:NSStringPboardType];
    NSString *str = [g_mrdpview->pasteboard_rd availableTypeFromArray:types];
    if (str == nil)
    {
        event->data = NULL;
        event->size = 0;
    }
    else 
    {
        NSString *data = [g_mrdpview->pasteboard_rd stringForType:NSStringPboardType];
        len = [data length] * 2 + 2;
        event->data = malloc(len);
        [data getCString:(char *) event->data maxLength:len encoding:NSUnicodeStringEncoding];
        event->size = len; 
    }
    freerdp_channels_send_event(inst->context->channels, (RDP_EVENT*) event);
}

void cliprdr_send_data_request(freerdp *inst, uint32 format)
{
    RDP_CB_DATA_REQUEST_EVENT* event;
    
    event = (RDP_CB_DATA_REQUEST_EVENT *) freerdp_event_new(RDP_EVENT_CLASS_CLIPRDR,
                    RDP_EVENT_TYPE_CB_DATA_REQUEST, NULL, NULL);
    event->format = format;
    freerdp_channels_send_event(inst->context->channels, (RDP_EVENT*) event);
}

/**
 * at the moment, only the following formats are supported
 *    CB_FORMAT_TEXT
 *    CB_FORMAT_UNICODETEXT
 */

void cliprdr_process_cb_data_response_event(freerdp *inst, RDP_CB_DATA_RESPONSE_EVENT *event)
{
    NSString *str;
    NSArray  *types;
    
    if (event->size == 0) {
         return;
    }
    
    if (g_mrdpview->pasteboard_format == CB_FORMAT_TEXT || g_mrdpview->pasteboard_format == CB_FORMAT_UNICODETEXT) {
        str = [[NSString alloc] initWithCharacters:(unichar *) event->data length:event->size / 2];
        types = [[NSArray alloc] initWithObjects:NSStringPboardType, nil];
        [g_mrdpview->pasteboard_wr declareTypes:types owner:g_mrdpview];
        [g_mrdpview->pasteboard_wr setString:str forType:NSStringPboardType];        
    }
}

void cliprdr_process_cb_monitor_ready_event(freerdp* inst)
{
    RDP_EVENT* event;
    RDP_CB_FORMAT_LIST_EVENT* format_list_event;
    
    event = freerdp_event_new(RDP_EVENT_CLASS_CLIPRDR, RDP_EVENT_TYPE_CB_FORMAT_LIST, NULL, NULL);
    
    format_list_event = (RDP_CB_FORMAT_LIST_EVENT*)event;
    format_list_event->num_formats = 0;
    
    freerdp_channels_send_event(inst->context->channels, event);
}

/**
 * list of supported clipboard formats; currently only the following are supported
 *    CB_FORMAT_TEXT
 *    CB_FORMAT_UNICODETEXT
 */

void cliprdr_process_cb_format_list_event(freerdp *inst, RDP_CB_FORMAT_LIST_EVENT* event)
{
    int i;
    
    if (event->num_formats == 0) {
        return;
    }
    
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
                cliprdr_send_data_request(inst, CB_FORMAT_UNICODETEXT);
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

void process_cliprdr_event(freerdp *inst)
{
    RDP_EVENT* event;
    
    event = freerdp_channels_pop_event(inst->context->channels);
    
    if (event) {
        switch (event->event_type) 
        {
            // Monitor Ready PDU is sent by server to indicate that it has been
            // inited and is ready. This PDU is transmitted by the server after it has sent
            // Clipboard Capabilities PDU
            case RDP_EVENT_TYPE_CB_MONITOR_READY:
                cliprdr_process_cb_monitor_ready_event(inst);
                break;
                
            // The Format List PDU is sent either by the client or the server when its
            // local system clipboard is updated with new clipboard data. This PDU
            // contains the Clipboard Format ID and name pairs of the new Clipboard
            // Formats on the clipboard
            case RDP_EVENT_TYPE_CB_FORMAT_LIST:
                cliprdr_process_cb_format_list_event(inst, (RDP_CB_FORMAT_LIST_EVENT*) event);
                break;
                
            // The Format Data Request PDU is sent by the receipient of the Format List PDU.
            // It is used to request the data for one of the formats that was listed in the
            // Format List PDU
            case RDP_EVENT_TYPE_CB_DATA_REQUEST:
                cliprdr_process_cb_data_request_event(inst);
                break;
                
            // The Format Data Response PDU is sent as a reply to the Format Data Request PDU.
            // It is used to indicate whether processing of the Format Data Request PDU
            // was successful. If the processing was successful, the Format Data Response PDU
            // includes the contents of the requested clipboard data
            case RDP_EVENT_TYPE_CB_DATA_RESPONSE:
                cliprdr_process_cb_data_response_event(inst, (RDP_CB_DATA_RESPONSE_EVENT*) event);
                break;
                
            default:
                printf("process_cliprdr_event: unknown event type %d\n", event->event_type);
                break;
        }
        freerdp_event_free(event);
    }
}

void cliprdr_send_supported_format_list(freerdp *inst)
{
    RDP_CB_FORMAT_LIST_EVENT* event;
    
    event = (RDP_CB_FORMAT_LIST_EVENT*) freerdp_event_new(RDP_EVENT_CLASS_CLIPRDR,
                                                          RDP_EVENT_TYPE_CB_FORMAT_LIST, NULL, NULL);
    
    event->formats = (uint32 *) malloc(sizeof(uint32) * 1);
    event->num_formats = 1;
    event->formats[0] = CB_FORMAT_UNICODETEXT;
    freerdp_channels_send_event(inst->context->channels, (RDP_EVENT*) event);
}


@end
