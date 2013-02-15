/*
 Utility functions
 
 Copyright 2013 Thinstuff Technologies GmbH, Authors: Martin Fleisz, Dorian Johnson
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#import <Foundation/Foundation.h>
#import "TSXTypes.h"

// helper macro to encode a table path into a tag value (used to identify controls in their delegate handlers)
#define GET_TAG(section, row) ((((int)section) << 16) | ((int)(row)))
#define GET_TAG_FROM_PATH(path) ((((int)path.section) << 16) | ((int)(path.row)))


BOOL ScanHostNameAndPort(NSString* address, NSString** host, unsigned short* port);

#pragma mark -
#pragma mark Screen Resolutions

NSString* ScreenResolutionDescription(TSXScreenOptions type, int width, int height);
BOOL ScanScreenResolution(NSString* description, int* width, int* height, TSXScreenOptions* type);

NSDictionary* SelectionForColorSetting();
NSArray* ResolutionModes();

#pragma mark Security Protocol

NSString* ProtocolSecurityDescription(TSXProtocolSecurityOptions type);
BOOL ScanProtocolSecurity(NSString* description, TSXProtocolSecurityOptions* type);
NSDictionary* SelectionForSecuritySetting();

#pragma mark Bookmarks
@class BookmarkBase;
NSMutableArray* FilterBookmarks(NSArray* bookmarks, NSArray* filter_words);
NSMutableArray* FilterHistory(NSArray* history, NSString* filterStr);

#pragma mark iPad/iPhone detection

BOOL IsPad();
BOOL IsPhone();

#pragma mark Version Info
NSString* TSXAppFullVersion();

#pragma mark Touch/Mouse handling

// set mouse buttons swapped flag
void SetSwapMouseButtonsFlag(BOOL swapped);

// set invert scrolling flag
void SetInvertScrollingFlag(BOOL invert);

// return event value for left mouse button 
int GetLeftMouseButtonClickEvent(BOOL down);

// return event value for right mouse button 
int GetRightMouseButtonClickEvent(BOOL down);

// return event value for mouse move event
int GetMouseMoveEvent();

// return mouse wheel event
int GetMouseWheelEvent(BOOL down);

// scrolling gesture detection delta
CGFloat GetScrollGestureDelta();

#pragma mark Connectivity tools
// activates the iphone's WWAN interface in case it is offline
void WakeUpWWAN();

#pragma mark System Info functions
NSString* TSXGetPlatform();
BOOL TSXDeviceHasJailBreak();
NSString* TSXGetPrimaryMACAddress(NSString *sep);

