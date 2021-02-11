#ifndef FREERDP_CLIENT_MAC_MRDPVIEW_H
#define FREERDP_CLIENT_MAC_MRDPVIEW_H

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

#import "mfreerdp.h"
#import "mf_client.h"
#import "Keyboard.h"

#import <CoreGraphics/CoreGraphics.h>

@interface MRDPView : NSView
{
	mfContext *mfc;
	NSBitmapImageRep *bmiRep;
	NSMutableArray *cursors;
	NSMutableArray *windows;
	NSTimer *pasteboard_timer;
	NSCursor *currentCursor;
	NSRect prevWinPosition;
	freerdp *instance;
	rdpContext *context;
	CGContextRef bitmap_context;
	char *pixel_data;
	int argc;
	char **argv;
	DWORD kbdModFlags;
	BOOL initialized;
	NSPoint savedDragLocation;
	BOOL firstCreateWindow;
	BOOL isMoveSizeInProgress;
	BOOL skipResizeOnce;
	BOOL saveInitialDragLoc;
	BOOL skipMoveWindowOnce;
  @public
	NSPasteboard *pasteboard_rd;
	NSPasteboard *pasteboard_wr;
	int pasteboard_changecount;
	int pasteboard_format;
	int is_connected;
}

- (int)rdpStart:(rdpContext *)rdp_context;
- (void)setCursor:(NSCursor *)cursor;
- (void)setScrollOffset:(int)xOffset y:(int)yOffset w:(int)width h:(int)height;

- (void)onPasteboardTimerFired:(NSTimer *)timer;
- (void)pause;
- (void)resume;
- (void)releaseResources;

@property(assign) int is_connected;

@end

BOOL mac_pre_connect(freerdp *instance);
BOOL mac_post_connect(freerdp *instance);
void mac_post_disconnect(freerdp *instance);
BOOL mac_authenticate(freerdp *instance, char **username, char **password, char **domain);
BOOL mac_gw_authenticate(freerdp *instance, char **username, char **password, char **domain);

DWORD mac_verify_certificate_ex(freerdp *instance, const char *host, UINT16 port,
                                const char *common_name, const char *subject, const char *issuer,
                                const char *fingerprint, DWORD flags);
DWORD mac_verify_changed_certificate_ex(freerdp *instance, const char *host, UINT16 port,
                                        const char *common_name, const char *subject,
                                        const char *issuer, const char *fingerprint,
                                        const char *old_subject, const char *old_issuer,
                                        const char *old_fingerprint, DWORD flags);

int mac_logon_error_info(freerdp *instance, UINT32 data, UINT32 type);
#endif /* FREERDP_CLIENT_MAC_MRDPVIEW_H */
