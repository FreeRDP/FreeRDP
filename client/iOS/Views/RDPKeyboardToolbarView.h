/*
 Toolbar of modifier and special keys shown above the ios keyboard

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import <UIKit/UIKit.h>

@class RDPKeyboard;

@interface RDPKeyboardToolbarView : UIView <UIScrollViewDelegate, UIInputViewAudioFeedback>
{
  @private
	UIView *_bar;
	UIVisualEffectView *_background;
	UIView *_keys_container;
	UIScrollView *_keys_scrollview;
	CAGradientLayer *_fade_mask;
	NSMutableArray *_key_buttons;
	UIView *_divider;
	UIButton *_toggle_button;
}

- (id)initWithTarget:(id)target;

// latch the modifier keys that are currently pressed on the keyboard
- (void)updateModifiersWithKeyboard:(RDPKeyboard *)keyboard;

// highlight the keyboard toggle while the extended keyboard is shown
- (void)setExtendedKeyboardActive:(BOOL)active;

@end
