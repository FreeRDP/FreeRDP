/*
 RDP Session View Controller

 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import <UIKit/UIKit.h>
#import "RDPSession.h"
#import "RDPKeyboard.h"
#import "RDPSessionView.h"
#import "RDPCursor.h"
#import "AdvancedKeyboardView.h"

@interface RDPSessionViewController
    : UIViewController <RDPSessionDelegate, AdvancedKeyboardDelegate, RDPKeyboardDelegate,
                        UIScrollViewDelegate, UITextFieldDelegate, UIGestureRecognizerDelegate,
                        UIPointerInteractionDelegate>
{
	// scrollview that hosts the rdp session view
	IBOutlet UIScrollView *_session_scrollview;

	// rdp session view
	IBOutlet RDPSessionView *_session_view;

	// rdp session toolbar
	IBOutlet UIToolbar *_session_toolbar;
	BOOL _session_toolbar_visible;

	// dummy text field used to display the keyboard
	IBOutlet UITextField *_dummy_textfield;

	// connecting view and the controls within that view
	IBOutlet UIView *_connecting_view;
	IBOutlet UILabel *_lbl_connecting;
	IBOutlet UIActivityIndicatorView *_connecting_indicator_view;
	IBOutlet UIButton *_cancel_connect_button;

	// extended keyboard toolbar
	UIToolbar *_keyboard_toolbar;

	// rdp session
	RDPSession *_session;
	BOOL _session_initilized;

	// flag that indicates whether the keyboard is visible or not
	BOOL _keyboard_visible;

	// keyboard extension view
	AdvancedKeyboardView *_advanced_keyboard_view;
	BOOL _advanced_keyboard_visible;
	BOOL _requesting_advanced_keyboard;
	CGSize _last_session_viewport_size;

	CGPoint _prev_long_press_position;
	CGPoint _cursor_view_position;
	CGPoint _last_mouse_pan_location;
	BOOL _has_cursor_view_position;
	BOOL _has_user_moved_cursor;
	BOOL _mouse_pan_active;
	BOOL _long_press_active;
	BOOL _mouse_drag_active;
	BOOL _pointer_is_indirect;
}

- (id)initWithNibName:(NSString *)nibNameOrNil
               bundle:(NSBundle *)nibBundleOrNil
              session:(RDPSession *)session;

@end
