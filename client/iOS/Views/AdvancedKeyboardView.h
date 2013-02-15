/*
 Advanced keyboard view interface
 
 Copyright 2013 Thinstuff Technologies GmbH, Author: Martin Fleisz
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */


#import <UIKit/UIKit.h>

// forward declaration
@protocol AdvancedKeyboardDelegate <NSObject>
@optional
// called when a function key was pressed and a virtual keycode is provided
//  @key: virtual key code
-(void)advancedKeyPressedVKey:(int)key;
// called when a function key was pressed and the keys unicode is provided
//  @key: unicode character
-(void)advancedKeyPressedUnicode:(int)key;
@end


@interface AdvancedKeyboardView : UIView 
{
@private
    // view containing function keys (F-keys) and function block (ins, del, home, end, ...)
    UIView* _function_keys_view;

    // view containing numpad keys (0-9, +-/*)
    UIView* _numpad_keys_view;
    
    // view containing cursor keys (up, down, left, right)
    UIView* _cursor_keys_view;
    
    // currently visible view
    UIView* _cur_view;

    // delegate
	NSObject<AdvancedKeyboardDelegate>* _delegate;
}

@property (assign) NSObject<AdvancedKeyboardDelegate>* delegate;

// init keyboard view with frame and delegate
- (id)initWithFrame:(CGRect)frame delegate:(NSObject<AdvancedKeyboardDelegate>*)delegate;

@end

