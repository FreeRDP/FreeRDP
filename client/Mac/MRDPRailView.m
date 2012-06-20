#include "MRDPRailView.h"

#define USE_RAIL_CVT

@implementation MRDPRailView

MRDPRailView * g_mrdpRailView;

struct kkey
{
    int key_code;
    int flags;
};

extern struct kkey g_keys[];

- (void) updateDisplay
{
    boolean   moveWindow = NO;
    int       i;
    NSRect    drawRect;
    NSRect    srcRectOuter;
    NSRect    destRectOuter;
    
    rdpGdi *  gdi;
    
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
    if (destRectOuter.size.width > screenSize.size.width) {
        destRectOuter.size.width = screenSize.size.width;
        moveWindow = YES;
    }
    
    // RAIL_TODO do  not hardcode to 22
    if (destRectOuter.size.height > screenSize.size.height) {
        destRectOuter.size.height = screenSize.size.height;
        moveWindow = YES;        
    }
    
    // cannot have negative cords
    if (destRectOuter.origin.x < 0) {
        destRectOuter.origin.x = 0;
        moveWindow = YES;    
    }
    
    if (destRectOuter.origin.y < 0) {
        destRectOuter.origin.y = 0;
        moveWindow = YES;
    }
    
    [self setupBmiRep:destRectOuter.size.width :destRectOuter.size.height];

    if (moveWindow) {
        moveWindow = NO;
        RAIL_WINDOW_MOVE_ORDER newWndLoc;
        apple_to_windowMove(&destRectOuter, &newWndLoc);
        newWndLoc.windowId = savedWindowId;
        //skipMoveWindowOnce = TRUE;
        //mac_send_rail_client_event(g_mrdpRailView->rdp_instance->context->channels, RDP_EVENT_TYPE_RAIL_CLIENT_WINDOW_MOVE, &newWndLoc);
    }
        
    //printf("MRDPRailView : updateDisplay : drawing %d rectangles\n", gdi->primary->hdc->hwnd->ninvalid);
    
    // if src and dest rect are not the same size, copy the entire
    // rectangle in one go instead of in many small rectangles
    
    //if (destRectOuter.size.width != self->width) {
    if (1) {
        destRectOuter.origin.y = height - destRectOuter.origin.y - destRectOuter.size.height;
        rail_convert_color_space1(pixelData, (char *) gdi->primary_buffer, 
                            &destRectOuter, self->width, self->height);
        
        if (moveWindow) 
            [self setNeedsDisplayInRect:destRectOuter];
        else
            [self setNeedsDisplayInRect:[self frame]];

        gdi->primary->hdc->hwnd->ninvalid = 0;
        
        return;
    }
    
    for (i = 0; i < gdi->primary->hdc->hwnd->ninvalid; i++)
    {
        drawRect.origin.x = gdi->primary->hdc->hwnd->cinvalid[i].x;
        drawRect.origin.y = gdi->primary->hdc->hwnd->cinvalid[i].y;
        drawRect.size.width = gdi->primary->hdc->hwnd->cinvalid[i].w;
        drawRect.size.height = gdi->primary->hdc->hwnd->cinvalid[i].h;
        
        rail_convert_color_space(pixelData, (char *) gdi->primary_buffer, 
                                 &drawRect, &destRectOuter, 
                                 &drawRect, &srcRectOuter);
        
        [self setNeedsDisplayInRect:drawRect];
    }
    gdi->primary->hdc->hwnd->ninvalid = 0;
}

/** *********************************************************************
 * called when our view needs to be redrawn
 ***********************************************************************/

- (void) drawRect:(NSRect)dirtyRect
{
    [bmiRep drawInRect:dirtyRect fromRect:dirtyRect operation:NSCompositeCopy fraction:1.0 respectFlipped:NO hints:nil];
    if (pixelData) {
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
    
    NSRect winFrame = [[self window] frame];
    NSPoint loc = [event locationInWindow];
    int x = (int) (winFrame.origin.x + loc.x);
    int y = (int) (winFrame.origin.y + loc.y);
    
    y = height - y;
    
    // send mouse motion event to RDP server
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
    y = height - y;
    
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

    NSRect winFrame = [[self window] frame];
    NSPoint loc = [event locationInWindow];
    int x = (int) (winFrame.origin.x + loc.x);
    int y = (int) (winFrame.origin.y + loc.y);
    y = height - y;

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

/** *********************************************************************
 * called when mouse is moved with left button pressed
 * note: invocation order is: mouseDown, mouseDragged, mouseUp
 ***********************************************************************/

- (void) mouseDragged:(NSEvent *)event
{
    [super mouseDragged:event];

    NSPoint loc = [event locationInWindow];
    int x = (int) loc.x;
    int y = (int) loc.y;
    
    if (isMoveSizeInProgress) {
        if (saveInitialDragLoc) {
            saveInitialDragLoc = NO;
            savedDragLocation.x = x;
            savedDragLocation.y = y;
            return;
        }
        
        int newX = x - savedDragLocation.x;
        int newY = y - savedDragLocation.y;
        
        NSRect r = [[self window] frame];
        r.origin.x += newX;
        r.origin.y += newY;
        [[self window] setFrame:r display:YES];
        
        return;
    }
    
    NSRect winFrame = [[self window] frame];
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
    
    key = [event keyCode];
    rdp_instance->input->KeyboardEvent(rdp_instance->input, g_keys[key].flags | KBD_FLAGS_DOWN, g_keys[key].key_code);
}

/** *********************************************************************
 * called when a key is released
 ***********************************************************************/

- (void) keyUp:(NSEvent *) event
{
    int key;

    key = [event keyCode];
    rdp_instance->input->KeyboardEvent(rdp_instance->input, g_keys[key].flags | KBD_FLAGS_RELEASE, g_keys[key].key_code);
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

void rail_cvt_from_rect(char *dest, char *src, NSRect destRect, int destWidth, int destHeight, NSRect srcRect)
{
}

/** *********************************************************************
 * color space conversion used specifically in RAIL
 ***********************************************************************/

int rail_convert_color_space(char * destBuf, char * srcBuf, 
                             NSRect * destRect, NSRect * destRectOuter, 
                             NSRect * srcRect, NSRect * srcRectOuter)
{
    int   i;
    int   j;
    int   numRows;
    int   srcX;
    int   srcY;
    int   destX;
    int   destY;
    int   pixelsPerRow;
    int   pixel;
    int   pixel1;
    int   pixel2;
    int * src32;
    int * dest32;
    
    int destWidth  = destRectOuter->size.width;
    int destHeight = destRectOuter->size.height;
    int srcWidth   = srcRectOuter->size.width;
    int srcHeight  = srcRectOuter->size.height;
    
    if ((!destBuf) || (!srcBuf)) {
        return 1;
    }
    
    if ((destRect->size.width != srcRect->size.width) || (destRect->size.height != srcRect->size.height)) {
        printf("##### RAIL_TODO: rail_convert_color_space : destRect & srcRect dimensions don't match\n");
        return 1;
    }
    
    numRows = srcRect->size.height;
    srcX  = srcRect->origin.x;
    srcY  = srcRect->origin.y;
    destX = destRect->origin.x;
    destY = destRect->origin.y;
    pixelsPerRow = destRect->size.width;

    for (i = 0; i < numRows; i++)
    {
        src32  = (int *) (srcBuf  + ((srcY  + i) * srcWidth  + srcX)  * 4);
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

    return 0;
} 

// RAIL_TODO rename this func
void rail_convert_color_space1(char *destBuf, char * srcBuf, 
                               NSRect * destRect, int width, int height)
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
    
    if ((!destBuf) || (!srcBuf)) {
        return;
    }
    
    numRows = destHeight;
    srcX  = destRect->origin.x;
    srcY  = destRect->origin.y;
    destX = 0;
    destY = 0;
    pixelsPerRow = destWidth;
    
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
    if (g_mrdpRailView->isMoveSizeInProgress) {
        return;
    }
    
    if (g_mrdpRailView->skipMoveWindowOnce) {
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

@end

