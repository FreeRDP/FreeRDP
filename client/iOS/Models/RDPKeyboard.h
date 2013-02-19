/*
 RDP Keyboard helper 
 
 Copyright 2013 Thinstuff Technologies GmbH, Author: Martin Fleisz
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#import <Foundation/Foundation.h>
#import "RDPSession.h"

@class RDPKeyboard;


@protocol RDPKeyboardDelegate <NSObject>
@optional
- (void)modifiersChangedForKeyboard:(RDPKeyboard*)keyboard;
@end


@interface RDPKeyboard : NSObject {

    RDPSession* _session;
    
	int _virtual_key_map[256];
    int _unicode_map[256];
    NSDictionary* _special_keys;

    NSObject<RDPKeyboardDelegate>* _delegate;
    
	BOOL _ctrl_pressed;
    BOOL _alt_pressed;
    BOOL _shift_pressed;
    BOOL _win_pressed;
}

@property (assign) id <RDPKeyboardDelegate> delegate;
@property (readonly) BOOL ctrlPressed;
@property (readonly) BOOL altPressed;
@property (readonly) BOOL shiftPressed;
@property (readonly) BOOL winPressed;

// returns a keyboard instance
+ (RDPKeyboard*)getSharedRDPKeyboard;

// init the keyboard and assign the given rdp session and delegate
- (void)initWithSession:(RDPSession*)session delegate:(NSObject<RDPKeyboardDelegate>*)delegate;

// called to reset any pending key states (i.e. pressed modifier keys)
- (void)reset;

// sends the given unicode character to the server
- (void)sendUnicode:(int)character;

// send a key stroke event using the given virtual key code
- (void)sendVirtualKeyCode:(int)keyCode;

// toggle ctrl key, returns true if pressed, otherwise false
- (void)toggleCtrlKey;

// toggle alt key, returns true if pressed, otherwise false
- (void)toggleAltKey;

// toggle shift key, returns true if pressed, otherwise false
- (void)toggleShiftKey;

// toggle windows key, returns true if pressed, otherwise false
- (void)toggleWinKey;

// send key strokes
- (void)sendEnterKeyStroke;
- (void)sendEscapeKeyStroke;
- (void)sendBackspaceKeyStroke;

@end
