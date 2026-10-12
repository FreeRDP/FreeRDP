/*
 Advanced keyboard view interface

 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import "AdvancedKeyboardView.h"
#import "RDPOverlayStyle.h"
#import "Utils.h"
#include <freerdp/locale/keyboard.h>

// a key of the extended keyboard
typedef struct
{
	NSString *title;  // label, or nil for an icon key
	NSString *symbol; // SF Symbol for icon key
	NSString *label;  // accessibility label of an icon key
	NSInteger vkey;
	BOOL repeats; // auto repeat while held
} AdvancedKey;

// empty cell in a block
#define KEY_GAP { nil, nil, nil, 0, NO }

// a block of keys laid out in a grid, row by row from the top left
typedef struct
{
	const AdvancedKey *keys;
	int count;
	int columns;
} AdvancedKeyBlock;

// a block placed on a page, with its width in key columns
typedef struct
{
	const AdvancedKeyBlock *block;
	int width;
} AdvancedPageBlock;

typedef struct
{
	const AdvancedPageBlock *blocks;
	int count;
} AdvancedPage;

#define COUNT_OF(a) ((int)(sizeof(a) / sizeof((a)[0])))

static const AdvancedKey navKeys[] = {
	{ @"prt scn", nil, nil, VK_SNAPSHOT | KBDEXT, NO },
	{ @"scr lk", nil, nil, VK_SCROLL, NO },
	{ @"break", nil, nil, VK_PAUSE | KBDEXT, NO },

	{ @"home", nil, nil, VK_HOME | KBDEXT, NO },
	{ @"insert", nil, nil, VK_INSERT | KBDEXT, NO },
	{ @"pg up", nil, nil, VK_PRIOR | KBDEXT, YES },

	{ @"end", nil, nil, VK_END | KBDEXT, NO },
	{ nil, @"arrow.up", @"Up", VK_UP | KBDEXT, YES },
	{ @"pg dn", nil, nil, VK_NEXT | KBDEXT, YES },

	{ nil, @"arrow.left", @"Left", VK_LEFT | KBDEXT, YES },
	{ nil, @"arrow.down", @"Down", VK_DOWN | KBDEXT, YES },
	{ nil, @"arrow.right", @"Right", VK_RIGHT | KBDEXT, YES },
};

static const AdvancedKey numKeys[] = {
	{ @"/", nil, nil, VK_DIVIDE | KBDEXT, NO },
	{ @"7", nil, nil, VK_KEY_7, NO },
	{ @"8", nil, nil, VK_KEY_8, NO },
	{ @"9", nil, nil, VK_KEY_9, NO },

	{ @"*", nil, nil, VK_MULTIPLY, NO },
	{ @"4", nil, nil, VK_KEY_4, NO },
	{ @"5", nil, nil, VK_KEY_5, NO },
	{ @"6", nil, nil, VK_KEY_6, NO },

	{ @"-", nil, nil, VK_SUBTRACT, NO },
	{ @"1", nil, nil, VK_KEY_1, NO },
	{ @"2", nil, nil, VK_KEY_2, NO },
	{ @"3", nil, nil, VK_KEY_3, NO },

	{ @"+", nil, nil, VK_ADD, NO },
	{ @"0", nil, nil, VK_KEY_0, NO },
	KEY_GAP,
	{ @".", nil, nil, VK_OEM_PERIOD, NO },
};

static const AdvancedKey fnKeys[] = {
	{ @"F1", nil, nil, VK_F1, NO },   { @"F2", nil, nil, VK_F2, NO },
	{ @"F3", nil, nil, VK_F3, NO },   { @"F4", nil, nil, VK_F4, NO },

	{ @"F5", nil, nil, VK_F5, NO },   { @"F6", nil, nil, VK_F6, NO },
	{ @"F7", nil, nil, VK_F7, NO },   { @"F8", nil, nil, VK_F8, NO },

	{ @"F9", nil, nil, VK_F9, NO },   { @"F10", nil, nil, VK_F10, NO },
	{ @"F11", nil, nil, VK_F11, NO }, { @"F12", nil, nil, VK_F12, NO },
};

static const AdvancedKey actionKeys[] = {
	{ nil, @"delete.left", @"Backspace", VK_BACK, YES },
	{ nil, @"return", @"Enter", VK_RETURN | KBDEXT, NO },
};

static const AdvancedKeyBlock navBlock = { navKeys, COUNT_OF(navKeys), 3 };
static const AdvancedKeyBlock numBlock = { numKeys, COUNT_OF(numKeys), 4 };
static const AdvancedKeyBlock fnBlock = { fnKeys, COUNT_OF(fnKeys), 4 };
static const AdvancedKeyBlock actionBlock = { actionKeys, COUNT_OF(actionKeys), 1 };

// pages, indexed by the page enum below
enum
{
	PAGE_NAV,
	PAGE_NUM,
	PAGE_FN,
	PAGE_COUNT
};

static NSString *const pageTitles[PAGE_COUNT] = { @"nav", @"123", @"Fn" };

static const AdvancedPageBlock narrowNavBlocks[] = { { &navBlock, 3 }, { &actionBlock, 1 } };
static const AdvancedPageBlock narrowNumBlocks[] = { { &numBlock, 4 }, { &actionBlock, 1 } };
static const AdvancedPageBlock narrowFnBlocks[] = { { &fnBlock, 4 }, { &actionBlock, 1 } };
static const AdvancedPage narrowPages[PAGE_COUNT] = {
	{ narrowNavBlocks, COUNT_OF(narrowNavBlocks) },
	{ narrowNumBlocks, COUNT_OF(narrowNumBlocks) },
	{ narrowFnBlocks, COUNT_OF(narrowFnBlocks) },
};

static const AdvancedPageBlock wideNumBlocks[] = { { &navBlock, 3 },
	                                               { &numBlock, 4 },
	                                               { &actionBlock, 1 } };
static const AdvancedPageBlock wideFnBlocks[] = { { &fnBlock, 7 }, { &actionBlock, 1 } };
static const AdvancedPage widePages[PAGE_COUNT] = {
	{ NULL, 0 },
	{ wideNumBlocks, COUNT_OF(wideNumBlocks) },
	{ wideFnBlocks, COUNT_OF(wideFnBlocks) },
};

static const CGFloat kWideLayoutWidth = 600.0; // narrower keyboards use the paged layout
static const CGFloat kPadding = 8.0;
static const CGFloat kTopPadding = 14.0; // keeps the keys clear of the toolbar above
static const CGFloat kKeySpacing = 8.0;
static const CGFloat kBlockGap = 20.0; // between blocks, with a separator in the middle
static const CGFloat kTabRowHeight = 36.0;
static const CGFloat kTabWidth = 64.0;
static const CGFloat kTabHeight = 30.0;

// key auto repeat
static const NSTimeInterval kRepeatDelay = 0.4;
static const NSTimeInterval kRepeatInterval = 0.08;

@interface AdvancedKeyboardView (Private)
- (NSInteger)displayedPage;
- (const AdvancedPage *)pageForDisplay;
- (void)rebuildPage;
- (void)sendKey:(NSInteger)vkey;
- (void)stopRepeat;
@end

@implementation AdvancedKeyboardView

@synthesize delegate = _delegate;

- (id)initWithFrame:(CGRect)frame delegate:(NSObject<AdvancedKeyboardDelegate> *)delegate
{
	self = [super initWithFrame:frame inputViewStyle:UIInputViewStyleKeyboard];
	if (self)
	{
		_delegate = delegate;
		[self setAutoresizingMask:UIViewAutoresizingFlexibleWidth];

		_key_buttons = [[NSMutableArray alloc] init];
		_tab_buttons = [[NSMutableArray alloc] init];
		_separators = [[NSMutableArray alloc] init];
		_page = PAGE_NAV;
	}
	return self;
}

- (void)dealloc
{
	[self stopRepeat];
	[_key_buttons release];
	[_tab_buttons release];
	[_separators release];
	[super dealloc];
}

- (void)willMoveToWindow:(UIWindow *)newWindow
{
	if (newWindow == nil)
		[self stopRepeat];
	[super willMoveToWindow:newWindow];
}

- (void)layoutSubviews
{
	[super layoutSubviews];

	UIEdgeInsets safe = [self safeAreaInsets];
	CGRect area = UIEdgeInsetsInsetRect(
	    [self bounds], UIEdgeInsetsMake(kTopPadding, safe.left + kPadding,
	                                    MAX(safe.bottom, kPadding), safe.right + kPadding));

	// check wide mode
	BOOL wide = (area.size.width >= kWideLayoutWidth);
	if (!_has_layout || wide != _wide_layout)
	{
		_wide_layout = wide;
		_has_layout = YES;
		[self rebuildPage];
	}

	CGFloat tabSlot = area.size.width / MAX([_tab_buttons count], 1);
	CGFloat tabY = CGRectGetMaxY(area) - (kTabRowHeight + kTabHeight) * 0.5;
	for (NSUInteger i = 0; i < [_tab_buttons count]; i++)
		[[_tab_buttons objectAtIndex:i]
		    setFrame:CGRectMake(CGRectGetMinX(area) + tabSlot * i + (tabSlot - kTabWidth) * 0.5,
		                        tabY, kTabWidth, kTabHeight)];

	const AdvancedPage *page = [self pageForDisplay];
	CGRect keys = area;
	keys.size.height -= kTabRowHeight + kKeySpacing;

	int totalWidth = 0;
	for (int i = 0; i < page->count; i++)
		totalWidth += page->blocks[i].width;
	const CGFloat available = keys.size.width - kBlockGap * (page->count - 1);

	CGFloat x = CGRectGetMinX(keys);
	NSUInteger keyIndex = 0;
	for (int i = 0; i < page->count; i++)
	{
		const AdvancedKeyBlock *block = page->blocks[i].block;
		const CGFloat blockWidth = available * page->blocks[i].width / totalWidth;
		const int rows = block->count / block->columns;
		const CGFloat cellWidth =
		    (blockWidth - kKeySpacing * (block->columns - 1)) / block->columns;
		const CGFloat cellHeight = (keys.size.height - kKeySpacing * (rows - 1)) / rows;

		for (int k = 0; k < block->count; k++)
		{
			if (block->keys[k].vkey == 0)
				continue;

			const int col = k % block->columns;
			const int row = k / block->columns;
			[[_key_buttons objectAtIndex:keyIndex++]
			    setFrame:CGRectMake(x + col * (cellWidth + kKeySpacing),
			                        CGRectGetMinY(keys) + row * (cellHeight + kKeySpacing),
			                        cellWidth, cellHeight)];
		}

		x += blockWidth;
		if (i < page->count - 1)
		{
			[[_separators objectAtIndex:i]
			    setFrame:CGRectMake(x + kBlockGap * 0.5 - 0.5, CGRectGetMinY(keys), 1.0,
			                        keys.size.height)];
			x += kBlockGap;
		}
	}
}

- (BOOL)enableInputClicksWhenVisible
{
	return YES;
}

#pragma mark -
#pragma mark button events

- (void)keyPressed:(UIButton *)sender
{
	[self sendKey:[sender tag]];
}

// repeating keys send on touch down and then repeat until they are released
- (void)keyDown:(UIButton *)sender
{
	[self stopRepeat];
	[self sendKey:[sender tag]];

	_repeat_key = [sender tag];
	_repeat_timer = [[NSTimer timerWithTimeInterval:kRepeatInterval
	                                         target:self
	                                       selector:@selector(repeatTimerFired:)
	                                       userInfo:nil
	                                        repeats:YES] retain];
	[_repeat_timer setFireDate:[NSDate dateWithTimeIntervalSinceNow:kRepeatDelay]];
	[[NSRunLoop currentRunLoop] addTimer:_repeat_timer forMode:NSRunLoopCommonModes];
}

- (void)keyUp:(UIButton *)sender
{
	(void)sender;
	[self stopRepeat];
}

- (void)repeatTimerFired:(NSTimer *)timer
{
	(void)timer;
	[self sendKey:_repeat_key];
}

- (void)tabSelected:(UIButton *)sender
{
	if ([sender tag] == [self displayedPage])
		return;

	[[UIDevice currentDevice] playInputClick];
	_page = [sender tag];
	[self rebuildPage];
	[self setNeedsLayout];
}

@end

#pragma mark -
@implementation AdvancedKeyboardView (Private)

// wide screen devices show the navigation keys on the number page
- (NSInteger)displayedPage
{
	return (_wide_layout && _page == PAGE_NAV) ? PAGE_NUM : _page;
}

- (const AdvancedPage *)pageForDisplay
{
	return _wide_layout ? &widePages[[self displayedPage]] : &narrowPages[[self displayedPage]];
}

- (void)rebuildPage
{
	[self stopRepeat];
	for (UIView *view in _key_buttons)
		[view removeFromSuperview];
	for (UIView *view in _tab_buttons)
		[view removeFromSuperview];
	for (UIView *view in _separators)
		[view removeFromSuperview];
	[_key_buttons removeAllObjects];
	[_tab_buttons removeAllObjects];
	[_separators removeAllObjects];

	UIImageSymbolConfiguration *symbolConfig =
	    [UIImageSymbolConfiguration configurationWithPointSize:IsPad() ? 20.0 : 18.0
	                                                    weight:UIImageSymbolWeightRegular];

	const AdvancedPage *page = [self pageForDisplay];
	for (int i = 0; i < page->count; i++)
	{
		const AdvancedKeyBlock *block = page->blocks[i].block;
		for (int k = 0; k < block->count; k++)
		{
			const AdvancedKey *key = &block->keys[k];
			if (key->vkey == 0)
				continue;

			UIImage *image = key->symbol ? [UIImage systemImageNamed:key->symbol
			                                       withConfiguration:symbolConfig]
			                             : nil;
			UIButton *btn = RDPCreateKeyButton(key->title, image);
			[btn setTag:key->vkey];
			if (key->label)
				[btn setAccessibilityLabel:NSLocalizedString(key->label, @"Extended keyboard key")];

			if (key->repeats)
			{
				[btn addTarget:self
				              action:@selector(keyDown:)
				    forControlEvents:UIControlEventTouchDown];
				[btn addTarget:self
				              action:@selector(keyUp:)
				    forControlEvents:UIControlEventTouchUpInside | UIControlEventTouchUpOutside |
				                     UIControlEventTouchCancel];
			}
			else
			{
				[btn addTarget:self
				              action:@selector(keyPressed:)
				    forControlEvents:UIControlEventTouchUpInside];
			}

			[self addSubview:btn];
			[_key_buttons addObject:btn];
		}

		if (i < page->count - 1)
		{
			UIView *separator = [[[UIView alloc] initWithFrame:CGRectZero] autorelease];
			[separator setBackgroundColor:[UIColor separatorColor]];
			[self addSubview:separator];
			[_separators addObject:separator];
		}
	}

	for (NSInteger p = _wide_layout ? PAGE_NUM : PAGE_NAV; p < PAGE_COUNT; p++)
	{
		UIButton *tab = RDPCreateTabButton(pageTitles[p]);
		[tab setTag:p];
		[tab setSelected:(p == [self displayedPage])];
		[tab addTarget:self
		              action:@selector(tabSelected:)
		    forControlEvents:UIControlEventTouchUpInside];
		[self addSubview:tab];
		[_tab_buttons addObject:tab];
	}
}

- (void)sendKey:(NSInteger)vkey
{
	[[UIDevice currentDevice] playInputClick];
	if ([[self delegate] respondsToSelector:@selector(advancedKeyPressedVKey:)])
		[[self delegate] advancedKeyPressedVKey:vkey];
}

- (void)stopRepeat
{
	[_repeat_timer invalidate];
	[_repeat_timer release];
	_repeat_timer = nil;
}

@end
