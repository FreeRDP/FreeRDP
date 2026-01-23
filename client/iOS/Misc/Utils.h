/*
 Utility functions

 Copyright 2013 Thincast Technologies GmbH, Authors: Martin Fleisz, Dorian Johnson

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import "TSXTypes.h"

// helper macro to encode a table path into a tag value (used to identify controls in their delegate
// handlers)
#define GET_TAG(section, row) ((((int)section) << 16) | ((int)(row)))
#define GET_TAG_FROM_PATH(path) ((((int)path.section) << 16) | ((int)(path.row)))

WINPR_ATTR_NODISCARD BOOL ScanHostNameAndPort(NSString *address, NSString **host,
                                              unsigned short *port);

#pragma mark -
#pragma mark Screen Resolutions

WINPR_ATTR_NODISCARD NSString *ScreenResolutionDescription(TSXScreenOptions type, int width,
                                                           int height);
WINPR_ATTR_NODISCARD BOOL ScanScreenResolution(NSString *description, int *width, int *height,
                                               TSXScreenOptions *type);

WINPR_ATTR_NODISCARD NSDictionary *SelectionForColorSetting(void);
WINPR_ATTR_NODISCARD NSArray *ResolutionModes(void);

#pragma mark Security Protocol

WINPR_ATTR_NODISCARD NSString *ProtocolSecurityDescription(TSXProtocolSecurityOptions type);
WINPR_ATTR_NODISCARD BOOL ScanProtocolSecurity(NSString *description,
                                               TSXProtocolSecurityOptions *type);
WINPR_ATTR_NODISCARD NSDictionary *SelectionForSecuritySetting(void);

#pragma mark Bookmarks
@class BookmarkBase;
WINPR_ATTR_NODISCARD NSMutableArray *FilterBookmarks(NSArray *bookmarks, NSArray *filter_words);
WINPR_ATTR_NODISCARD NSMutableArray *FilterHistory(NSArray *history, NSString *filterStr);

#pragma mark iPad/iPhone detection

WINPR_ATTR_NODISCARD BOOL IsPad(void);
WINPR_ATTR_NODISCARD BOOL IsPhone(void);

#pragma mark Version Info
WINPR_ATTR_NODISCARD NSString *TSXAppFullVersion(void);

#pragma mark Touch/Mouse handling

// set mouse buttons swapped flag
void SetSwapMouseButtonsFlag(BOOL swapped);

// set invert scrolling flag
void SetInvertScrollingFlag(BOOL invert);

// return event value for left mouse button
WINPR_ATTR_NODISCARD int GetLeftMouseButtonClickEvent(BOOL down);

// return event value for right mouse button
WINPR_ATTR_NODISCARD int GetRightMouseButtonClickEvent(BOOL down);

// return event value for mouse move event
WINPR_ATTR_NODISCARD int GetMouseMoveEvent(void);

// return mouse wheel event
WINPR_ATTR_NODISCARD int GetMouseWheelEvent(BOOL down);

// scrolling gesture detection delta
WINPR_ATTR_NODISCARD CGFloat GetScrollGestureDelta(void);

#pragma mark Connectivity tools
// activates the iphone's WWAN interface in case it is offline
void WakeUpWWAN(void);

#pragma mark System Info functions
WINPR_ATTR_NODISCARD NSString *TSXGetPlatform(void);
WINPR_ATTR_NODISCARD BOOL TSXDeviceHasJailBreak(void);
WINPR_ATTR_NODISCARD NSString *TSXGetPrimaryMACAddress(NSString *sep);
