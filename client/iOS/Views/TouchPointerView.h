/*
 RDP Touch Pointer View 
 
 Copyright 2013 Thinstuff Technologies GmbH, Author: Martin Fleisz
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#import <UIKit/UIKit.h>

// protocol for touch pointer callbacks
@protocol TouchPointerDelegate
// callback if touch pointer should be closed
-(void)touchPointerClose;
// callback for a left click action
-(void)touchPointerLeftClick:(CGPoint)pos down:(BOOL)down;
// callback for a right click action
-(void)touchPointerRightClick:(CGPoint)pos down:(BOOL)down;
// callback for pointer move action
-(void)touchPointerMove:(CGPoint)pos;
// callback if scrolling is performed
-(void)touchPointerScrollDown:(BOOL)down;
// callback for toggling the standard keyboard
-(void)touchPointerToggleKeyboard;
// callback for toggling the extended keyboard
-(void)touchPointerToggleExtendedKeyboard;
// callback for reset session view
-(void)touchPointerResetSessionView;
@end


@interface TouchPointerView : UIView
{
    // transformation and image currently drawn
    CGAffineTransform _pointer_transformation;
    UIImage* _cur_pointer_img;

    // action images
    UIImage* _default_pointer_img;
    UIImage* _active_pointer_img;
    UIImage* _lclick_pointer_img;
    UIImage* _rclick_pointer_img;
    UIImage* _scroll_pointer_img;
    UIImage* _extkeyboard_pointer_img;
    UIImage* _keyboard_pointer_img;
    UIImage* _reset_pointer_img;

    // predefined areas for all actions
    CGRect _pointer_areas[9]; 
    
    // scroll/drag n drop handling
    CGPoint _prev_touch_location;
    BOOL _pointer_moving;
    BOOL _pointer_scrolling;
    
    NSObject<TouchPointerDelegate>* _delegate;
}

@property (assign) IBOutlet NSObject<TouchPointerDelegate>* delegate;

// positions the pointer on screen if it got offscreen after an orentation change or after displaying the keyboard
-(void)ensurePointerIsVisible;

// returns the extent required for the scrollview to use the touch pointer near the edges of the session view
-(UIEdgeInsets)getEdgeInsets;

// return pointer dimension and position information
- (CGPoint)getPointerPosition;
- (int)getPointerWidth;
- (int)getPointerHeight;

@end
