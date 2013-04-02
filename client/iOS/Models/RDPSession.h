/*
 RDP Session object 
 
 Copyright 2013 Thinstuff Technologies GmbH, Authors: Martin Fleisz, Dorian Johnson
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include <freerdp/freerdp.h>

// forward declaration
@class RDPSession;
@class ComputerBookmark;
@class ConnectionParams;

// notification handler for session disconnect
extern NSString* TSXSessionDidDisconnectNotification;
extern NSString* TSXSessionDidFailToConnectNotification;

// protocol for session notifications
@protocol RDPSessionDelegate <NSObject>
@optional
- (void)session:(RDPSession*)session didFailToConnect:(int)reason;
- (void)sessionWillConnect:(RDPSession*)session;
- (void)sessionDidConnect:(RDPSession*)session;
- (void)sessionWillDisconnect:(RDPSession*)session;
- (void)sessionDidDisconnect:(RDPSession*)session;
- (void)sessionBitmapContextWillChange:(RDPSession*)session;
- (void)sessionBitmapContextDidChange:(RDPSession*)session;
- (void)session:(RDPSession*)session needsRedrawInRect:(CGRect)rect;
- (CGSize)sizeForFitScreenForSession:(RDPSession*)session;
- (void)showGoProScreen:(RDPSession*)session;

- (void)session:(RDPSession*)session requestsAuthenticationWithParams:(NSMutableDictionary*)params;
- (void)session:(RDPSession*)session verifyCertificateWithParams:(NSMutableDictionary*)params;

@end

// rdp session
@interface RDPSession : NSObject 
{
@private
	freerdp* _freerdp;

    ComputerBookmark* _bookmark;
    
	ConnectionParams* _params;
	
	NSObject<RDPSessionDelegate>* _delegate;
    
    NSCondition* _ui_request_completed;
    
    NSString* _name;
    
    // flag if the session is suspended
    BOOL _suspended;
    
	// flag that specifies whether the RDP toolbar is visible
	BOOL _toolbar_visible;  
}

@property (readonly) ConnectionParams* params;
@property (readonly) ComputerBookmark* bookmark;
@property (assign) id <RDPSessionDelegate> delegate;
@property (assign) BOOL toolbarVisible;
@property (readonly) CGContextRef bitmapContext;
@property (readonly) NSCondition* uiRequestCompleted;


// initialize a new session with the given bookmark
- (id)initWithBookmark:(ComputerBookmark*)bookmark;

#pragma mark - session control functions

// connect the session
-(void)connect;

// disconnect session
-(void)disconnect;

// suspends the session
-(void)suspend;

// resumes a previously suspended session
-(void)resume;

// returns YES if the session is started
-(BOOL)isSuspended;

// send input event to the server
-(void)sendInputEvent:(NSDictionary*)event;

// session needs a refresh of its view
- (void)setNeedsDisplayInRectAsValue:(NSValue*)rect_value;

// get a small session screenshot
- (UIImage*)getScreenshotWithSize:(CGSize)size;

// returns the session's current paramters
- (rdpSettings*)getSessionParams;

// returns the session's name (usually the label of the bookmark the session was created with)
- (NSString*)sessionName;

@end
