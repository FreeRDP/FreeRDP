/*
 Advanced keyboard view interface

 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import "AdvancedKeyboardView.h"
#include <freerdp/locale/keyboard.h>

// helper struct to define button layouts/settings
struct ButtonItem
{
	NSString *title;
	int tag;
};

@interface AdvancedKeyboardView (Private)
- (UIView *)keyboardViewForItems:(struct ButtonItem *)items columns:(int)columns rows:(int)rows;
@end

@implementation AdvancedKeyboardView

@synthesize delegate = _delegate;

// defines for the different views
#define KEY_SHOW_FUNCVIEW 0x1000
#define KEY_SHOW_CURSORVIEW 0x1001
#define KEY_SHOW_NUMPADVIEW 0x1002
#define KEY_SKIP 0x8000
#define KEY_MERGE_COLUMN 0x8001

#define KEYCODE_UNICODE 0x80000000

struct ButtonItem functionKeysItems[24] = { { @"F1", VK_F1 },
	                                        { @"F2", VK_F2 },
	                                        { @"F3", VK_F3 },
	                                        { @"F4", VK_F4 },
	                                        { @"F5", VK_F5 },
	                                        { @"F6", VK_F6 },
	                                        { @"F7", VK_F7 },
	                                        { @"F8", VK_F8 },
	                                        { @"F9", VK_F9 },
	                                        { @"F10", VK_F10 },
	                                        { @"F11", VK_F11 },
	                                        { @"F12", VK_F12 },

	                                        { @"img:icon_key_arrows", KEY_SHOW_CURSORVIEW },
	                                        { @"Tab", VK_TAB },
	                                        { @"Ins", VK_INSERT | KBDEXT },
	                                        { @"Home", VK_HOME | KBDEXT },
	                                        { @"PgUp", VK_PRIOR | KBDEXT },
	                                        { @"img:icon_key_win", VK_LWIN | KBDEXT },

	                                        { @"123", KEY_SHOW_NUMPADVIEW },
	                                        { @"Print", VK_PRINT },
	                                        { @"Del", VK_DELETE | KBDEXT },
	                                        { @"End", VK_END | KBDEXT },
	                                        { @"PgDn", VK_NEXT | KBDEXT },
	                                        { @"img:icon_key_menu", VK_APPS | KBDEXT } };

struct ButtonItem numPadKeysItems[24] = { { @"(", KEYCODE_UNICODE | 40 },
	                                      { @")", KEYCODE_UNICODE | 41 },
	                                      { @"7", VK_NUMPAD7 },
	                                      { @"8", VK_NUMPAD8 },
	                                      { @"9", VK_NUMPAD9 },
	                                      { @"-", VK_SUBTRACT },

	                                      { @"/", VK_DIVIDE | KBDEXT },
	                                      { @"*", VK_MULTIPLY },
	                                      { @"4", VK_NUMPAD4 },
	                                      { @"5", VK_NUMPAD5 },
	                                      { @"6", VK_NUMPAD6 },
	                                      { @"+", VK_ADD },

	                                      { @"Fn", KEY_SHOW_FUNCVIEW },
	                                      { @"Num", VK_NUMLOCK },
	                                      { @"1", VK_NUMPAD1 },
	                                      { @"2", VK_NUMPAD2 },
	                                      { @"3", VK_NUMPAD3 },
	                                      { @"img:icon_key_backspace", VK_BACK },

	                                      { @"img:icon_key_arrows", KEY_SHOW_CURSORVIEW },
	                                      { @"=", KEYCODE_UNICODE | 61 },
	                                      { @"", KEY_MERGE_COLUMN },
	                                      { @"0", VK_NUMPAD0 },
	                                      { @".", VK_DECIMAL },
	                                      { @"img:icon_key_return", VK_RETURN | KBDEXT } };

struct ButtonItem cursorKeysItems[24] = { { @"", KEY_SKIP },
	                                      { @"", KEY_SKIP },
	                                      { @"", KEY_SKIP },
	                                      { @"", KEY_SKIP },
	                                      { @"", KEY_SKIP },
	                                      { @"", KEY_SKIP },

	                                      { @"", KEY_SKIP },
	                                      { @"", KEY_SKIP },
	                                      { @"", KEY_SKIP },
	                                      { @"img:icon_key_arrow_up", VK_UP | KBDEXT },
	                                      { @"", KEY_SKIP },
	                                      { @"", KEY_SKIP },

	                                      { @"Fn", KEY_SHOW_FUNCVIEW },
	                                      { @"", KEY_SKIP },
	                                      { @"img:icon_key_arrow_left", VK_LEFT | KBDEXT },
	                                      { @"", KEY_SKIP },
	                                      { @"img:icon_key_arrow_right", VK_RIGHT | KBDEXT },
	                                      { @"img:icon_key_backspace", VK_BACK },

	                                      { @"123", KEY_SHOW_NUMPADVIEW },
	                                      { @"", KEY_SKIP },
	                                      { @"", KEY_SKIP },
	                                      { @"img:icon_key_arrow_down", VK_DOWN | KBDEXT },
	                                      { @"", KEY_SKIP },
	                                      { @"img:icon_key_return", VK_RETURN | KBDEXT } };

- (void)initFunctionKeysView
{
	_function_keys_view = [[self keyboardViewForItems:functionKeysItems columns:6 rows:4] retain];
	[self addSubview:_function_keys_view];
}

- (void)initNumPadKeysView
{
	_numpad_keys_view = [[self keyboardViewForItems:numPadKeysItems columns:6 rows:4] retain];
	[self addSubview:_numpad_keys_view];
}

- (void)initCursorKeysView
{
	_cursor_keys_view = [[self keyboardViewForItems:cursorKeysItems columns:6 rows:4] retain];
	[self addSubview:_cursor_keys_view];
}

- (id)initWithFrame:(CGRect)frame delegate:(NSObject<AdvancedKeyboardDelegate> *)delegate
{
	self = [super initWithFrame:frame];
	if (self)
	{
		_delegate = delegate;

		self.autoresizingMask = UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleWidth;
		self.backgroundColor = [UIColor blackColor];
		// Initialization code

		[self initCursorKeysView];
		[self initNumPadKeysView];
		[self initFunctionKeysView];

		// set function keys view to the initial view and hide others
		_cur_view = _function_keys_view;
		[_numpad_keys_view setHidden:YES];
		[_cursor_keys_view setHidden:YES];
	}
	return self;
}

/*
// Only override drawRect: if you perform custom drawing.
// An empty implementation adversely affects performance during animation.
- (void)drawRect:(CGRect)rect
{
    // Drawing code
}
*/

- (void)drawRect:(CGRect)rect
{
	// draw a nice background gradient
	CGContextRef currentContext = UIGraphicsGetCurrentContext();

	CGGradientRef glossGradient;
	CGColorSpaceRef rgbColorspace;
	size_t num_locations = 2;
	CGFloat locations[2] = { 0.0, 1.0 };
	CGFloat components[8] = { 1.0, 1.0, 1.0, 0.35,   // Start color
		                      1.0, 1.0, 1.0, 0.06 }; // End color

	rgbColorspace = CGColorSpaceCreateDeviceRGB();
	glossGradient =
	    CGGradientCreateWithColorComponents(rgbColorspace, components, locations, num_locations);

	CGRect currentBounds = self.bounds;
	CGPoint topCenter = CGPointMake(CGRectGetMidX(currentBounds), 0.0f);
	CGPoint midCenter = CGPointMake(CGRectGetMidX(currentBounds), currentBounds.size.height);
	CGContextDrawLinearGradient(currentContext, glossGradient, topCenter, midCenter, 0);

	CGGradientRelease(glossGradient);
	CGColorSpaceRelease(rgbColorspace);
}

- (void)dealloc
{
	[_function_keys_view autorelease];
	[_numpad_keys_view autorelease];
	[_cursor_keys_view autorelease];
	[super dealloc];
}

#pragma mark -
#pragma mark button events

- (IBAction)keyPressed:(id)sender
{
	UIButton *btn = (UIButton *)sender;
	switch ([btn tag])
	{
		case KEY_SHOW_CURSORVIEW:
			// switch to cursor view
			[_cur_view setHidden:YES];
			[_cursor_keys_view setHidden:NO];
			_cur_view = _cursor_keys_view;
			break;

		case KEY_SHOW_NUMPADVIEW:
			// switch to numpad view
			[_cur_view setHidden:YES];
			[_numpad_keys_view setHidden:NO];
			_cur_view = _numpad_keys_view;
			break;

		case KEY_SHOW_FUNCVIEW:
			// switch to function keys view
			[_cur_view setHidden:YES];
			[_function_keys_view setHidden:NO];
			_cur_view = _function_keys_view;
			break;

		default:
			if ([btn tag] & KEYCODE_UNICODE)
			{
				if ([[self delegate] respondsToSelector:@selector(advancedKeyPressedUnicode:)])
					[[self delegate] advancedKeyPressedUnicode:([btn tag] & ~KEYCODE_UNICODE)];
			}
			else
			{
				if ([[self delegate] respondsToSelector:@selector(advancedKeyPressedVKey:)])
					[[self delegate] advancedKeyPressedVKey:[btn tag]];
			}
			break;
	}
}

@end

#pragma mark -
@implementation AdvancedKeyboardView (Private)

- (UIView *)keyboardViewForItems:(struct ButtonItem *)items columns:(int)columns rows:(int)rows
{
	UIView *result_view = [[[UIView alloc] initWithFrame:self.bounds] autorelease];
	result_view.autoresizingMask =
	    UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleWidth;

	// calculate maximum button size
	int max_btn_width = result_view.bounds.size.width / ((columns * 2) + 1);
	int max_btn_height = result_view.bounds.size.height / ((rows * 2) + 1);

	// ensure minimum button size
	CGSize btn_size = CGSizeMake(45, 30);
	if (btn_size.width < max_btn_width)
		btn_size.width = max_btn_width;
	if (btn_size.height < max_btn_height)
		btn_size.height = max_btn_height;

	// calc distance width and height between buttons
	int dist_width = (result_view.bounds.size.width - (columns * btn_size.width)) / (columns + 1);
	int dist_height = (result_view.bounds.size.height - (rows * btn_size.height)) / (rows + 1);

	UIImage *btn_background_img = [UIImage
	    imageWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"keyboard_button_background"
	                                                            ofType:@"png"]];
	for (int j = 0; j < rows; j++)
	{
		for (int i = 0; i < columns; i++)
		{
			struct ButtonItem *curItem = &items[j * columns + i];

			// skip this spot?
			if (curItem->tag == KEY_SKIP)
				continue;

			// create button, set autoresizing mask and add action handler
			UIButton *btn = [UIButton buttonWithType:UIButtonTypeRoundedRect];
			[btn setAutoresizingMask:(UIViewAutoresizingFlexibleLeftMargin |
			                          UIViewAutoresizingFlexibleRightMargin |
			                          UIViewAutoresizingFlexibleTopMargin |
			                          UIViewAutoresizingFlexibleBottomMargin |
			                          UIViewAutoresizingFlexibleWidth |
			                          UIViewAutoresizingFlexibleHeight)];
			[btn addTarget:self
			              action:@selector(keyPressed:)
			    forControlEvents:UIControlEventTouchUpInside];

			// if merge is specified we merge this button's position with the next one
			if (curItem->tag == KEY_MERGE_COLUMN)
			{
				// calc merged frame
				[btn setFrame:CGRectMake(dist_width + (i * dist_width) + (i * btn_size.width),
				                         dist_height + (j * dist_height) + (j * btn_size.height),
				                         btn_size.width * 2 + dist_width, btn_size.height)];

				// proceed to the next column item
				i++;
				curItem = &items[j * columns + i];
			}
			else
			{
				[btn setFrame:CGRectMake(dist_width + (i * dist_width) + (i * btn_size.width),
				                         dist_height + (j * dist_height) + (j * btn_size.height),
				                         btn_size.width, btn_size.height)];
			}

			// set button text or image parameters
			if ([curItem->title hasPrefix:@"img:"] == YES)
			{
				UIImage *btn_image =
				    [UIImage imageWithContentsOfFile:[[NSBundle mainBundle]
				                                         pathForResource:[curItem->title
				                                                             substringFromIndex:4]
				                                                  ofType:@"png"]];
				[btn setImage:btn_image forState:UIControlStateNormal];
			}
			else
			{
				[btn setTitle:curItem->title forState:UIControlStateNormal];
				[btn setTitleColor:[UIColor blackColor] forState:UIControlStateNormal];
			}

			[btn setBackgroundImage:btn_background_img forState:UIControlStateNormal];
			[btn setTag:curItem->tag];

			// add button to view
			[result_view addSubview:btn];
		}
	}

	return result_view;
}

@end
