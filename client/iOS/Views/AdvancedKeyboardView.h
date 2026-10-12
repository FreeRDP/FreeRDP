/*
 Advanced keyboard view interface

 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import <UIKit/UIKit.h>

// forward declaration
@protocol AdvancedKeyboardDelegate <NSObject>
@optional
// called when a function key was pressed and a virtual keycode is provided
//  @key: virtual key code
- (void)advancedKeyPressedVKey:(NSInteger)key;
@end

@interface AdvancedKeyboardView : UIInputView <UIInputViewAudioFeedback>
{
  @private
	NSMutableArray *_key_buttons;
	NSMutableArray *_tab_buttons;
	NSMutableArray *_separators;

	// selected page and the layout the views were built for
	NSInteger _page;
	BOOL _wide_layout;
	BOOL _has_layout;

	// auto repeat of a held key
	NSTimer *_repeat_timer;
	NSInteger _repeat_key;

	// delegate
	NSObject<AdvancedKeyboardDelegate> *_delegate;
}

@property(assign) NSObject<AdvancedKeyboardDelegate> *delegate;

// init keyboard view with frame and delegate
- (id)initWithFrame:(CGRect)frame delegate:(NSObject<AdvancedKeyboardDelegate> *)delegate;

@end
