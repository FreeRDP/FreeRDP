#import <Cocoa/Cocoa.h>

#define boolean int

#import "freerdp/gdi/gdi.h"
#import "freerdp/rail/rail.h"

@interface MRDPRailView : NSView
{
    freerdp *           rdp_instance;
    rdpContext *        context;
    NSBitmapImageRep *  bmiRep;
    NSPoint             savedDragLocation;
    char *              pixelData;
    boolean             mouseInClientArea;
    boolean             titleBarClicked;
    int                 width;
    int                 height;
    int                 savedWindowId;
    
    // store state info for some keys
    int                 kdlshift;
    int                 kdrshift;
    int                 kdlctrl;
    int                 kdrctrl;
    int                 kdlalt;
    int                 kdralt;
    int                 kdlmeta;
    int                 kdrmeta;
    int                 kdcapslock;
    
    @public
    boolean             isMoveSizeInProgress;    
    boolean             saveInitialDragLoc;
    boolean             skipMoveWindowOnce;
    int                 localMoveType;
}

- (void) updateDisplay;
- (void) setRdpInstance:(freerdp *) instance width:(int) w andHeight:(int) h windowID:(int) windowID;
- (BOOL) eventIsInClientArea :(NSEvent *) event :(int *) xptr :(int *) yptr;
- (void) setupBmiRep:(int) width :(int) height;

void mac_rail_MoveWindow(rdpRail *rail, rdpWindow *window);
void apple_to_windowMove(NSRect * r, RAIL_WINDOW_MOVE_ORDER * windowMove);
void mac_send_rail_client_event(rdpChannels *channels, uint16 event_type, void *param);

@end

