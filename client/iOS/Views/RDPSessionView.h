/*
 RDP Session View

 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import <UIKit/UIKit.h>
#import "RDPSession.h"

@class RDPCursor;

@interface RDPSessionView : UIView
{
	RDPSession *_session;
}

@property(nonatomic, assign) BOOL hardwareKeyboardActive;

- (void)setSession:(RDPSession *)session;
- (void)prepareForBitmapContextChange;
- (void)setNeedsDisplayInRemoteRect:(CGRect)rect;
- (void)setRemoteCursor:(RDPCursor *)cursor;
- (void)setRemoteCursorPosition:(CGPoint)position;
- (void)showRemoteCursor;
- (void)hideRemoteCursor;
- (void)setDefaultRemoteCursor;

@end
