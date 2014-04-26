/*
 RDP Session View Controller
 
 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#import <UIKit/UIKit.h>
#import "RDPSession.h"
#import "RDPKeyboard.h"
#import "RDPSessionView.h"
#import "TouchPointerView.h"
#import "AdvancedKeyboardView.h"

@interface RDPSessionViewController : UIViewController <RDPSessionDelegate, TouchPointerDelegate, AdvancedKeyboardDelegate, RDPKeyboardDelegate, UIScrollViewDelegate, UITextFieldDelegate>
{
	// scrollview that hosts the rdp session view
	IBOutlet UIScrollView* _session_scrollview;
	
	// rdp session view
	IBOutlet RDPSessionView* _session_view;

    // touch pointer view
    IBOutlet TouchPointerView* _touchpointer_view;
    BOOL _autoscroll_with_touchpointer;
    BOOL _is_autoscrolling;

	// rdp session toolbar
	IBOutlet UIToolbar* _session_toolbar;
    BOOL _session_toolbar_visible;
	
	// dummy text field used to display the keyboard
	IBOutlet UITextField* _dummy_textfield;
	
    // connecting view and the controls within that view
    IBOutlet UIView* _connecting_view;
    IBOutlet UILabel* _lbl_connecting;
    IBOutlet UIActivityIndicatorView* _connecting_indicator_view;
    IBOutlet UIButton* _cancel_connect_button;
    
    // extended keyboard toolbar
    UIToolbar* _keyboard_toolbar;
    
    // rdp session
    RDPSession* _session;
    BOOL _session_initilized;
    
	// flag that indicates whether the keyboard is visible or not
	BOOL _keyboard_visible;
    
    // flag to switch between left/right mouse button mode
    BOOL _toggle_mouse_button;
    
    // keyboard extension view
    AdvancedKeyboardView* _advanced_keyboard_view;
    BOOL _advanced_keyboard_visible;
    BOOL _requesting_advanced_keyboard;
    CGFloat _keyboard_last_height;
    
    // delayed mouse move event sending
    NSTimer* _mouse_move_event_timer;
    int _mouse_move_events_skipped;
    CGPoint _prev_long_press_position;
}

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil session:(RDPSession*)session;

@end
