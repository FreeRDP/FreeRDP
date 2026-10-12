/*
 Toolbar of modifier and special keys shown above the ios keyboard

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import "RDPKeyboardToolbarView.h"
#import "RDPOverlayStyle.h"
#import "RDPKeyboard.h"
#import "Utils.h"

// index of the keys in _key_buttons
enum
{
	ACCESSORY_KEY_WIN,
	ACCESSORY_KEY_ESC,
	ACCESSORY_KEY_SHIFT,
	ACCESSORY_KEY_CTRL,
	ACCESSORY_KEY_ALT,
	ACCESSORY_KEY_ALTGR,
	ACCESSORY_KEY_DEL,
	ACCESSORY_KEY_TAB,
	ACCESSORY_KEY_COUNT
};

static const CGFloat kViewHeight = 64.0;
static const CGFloat kBarHeight = 52.0;
static const CGFloat kBarSideMargin = 16.0; // between the screen (safe area) and the bar
static const CGFloat kKeyInset = 16.0;      // between the bar's ends and the first/last key
static const CGFloat kKeyHeight = 36.0;
static const CGFloat kKeySpacing = 10.0;
static const CGFloat kDividerGap = 15.0; // on either side of the divider
static const CGFloat kDividerHeight = 25.0;
static const CGFloat kFadeWidth = 24.0;

// metrics that differ between iPhone and iPad
typedef struct
{
	CGFloat keyWidth;
	CGFloat logoSize;   // Windows logo
	CGFloat symbolSize; // advanced keyboard toggle icon
} RDPKeyboardToolbarMetrics;

static const RDPKeyboardToolbarMetrics kPhoneMetrics = { 56.0, 18.0, 17.0 };
static const RDPKeyboardToolbarMetrics kPadMetrics = { 64.0, 20.0, 19.0 };

static const RDPKeyboardToolbarMetrics *RDPGetKeyboardToolbarMetrics(void)
{
	return IsPad() ? &kPadMetrics : &kPhoneMetrics;
}

@interface RDPKeyScrollView : UIScrollView
@end

@implementation RDPKeyScrollView

- (BOOL)touchesShouldCancelInContentView:(UIView *)view
{
	(void)view;
	return YES;
}

@end

@interface RDPKeyboardToolbarView (Private)
- (void)addKeyWithTitle:(NSString *)title
                  image:(UIImage *)image
     accessibilityLabel:(NSString *)label
                 target:(id)target
                 action:(SEL)action;
- (void)updateFadeMask;
@end

@implementation RDPKeyboardToolbarView

- (id)initWithTarget:(id)target
{
	self = [super initWithFrame:CGRectMake(0, 0, 320, kViewHeight)];
	if (self)
	{
		[self setAutoresizingMask:UIViewAutoresizingFlexibleWidth];

		_bar = [[UIView alloc] initWithFrame:CGRectZero];
		[self addSubview:_bar];

		_background = [RDPCreateGlassCapsuleView() retain];
		[_bar addSubview:_background];

		// the scroll view sits in a container so the fade mask stays put while scrolling
		_keys_container = [[UIView alloc] initWithFrame:CGRectZero];
		[_bar addSubview:_keys_container];

		_keys_scrollview = [[RDPKeyScrollView alloc] initWithFrame:CGRectZero];
		[_keys_scrollview setShowsHorizontalScrollIndicator:NO];
		[_keys_scrollview setDelaysContentTouches:NO];
		[_keys_scrollview setDelegate:self];
		[_keys_container addSubview:_keys_scrollview];

		_fade_mask = [[CAGradientLayer layer] retain];
		[_fade_mask setStartPoint:CGPointMake(0.0, 0.5)];
		[_fade_mask setEndPoint:CGPointMake(1.0, 0.5)];

		const RDPKeyboardToolbarMetrics *metrics = RDPGetKeyboardToolbarMetrics();
		_key_buttons = [[NSMutableArray alloc] initWithCapacity:ACCESSORY_KEY_COUNT];

		// buttons
		[self addKeyWithTitle:nil
		                 image:RDPWindowsLogoImage(metrics->logoSize)
		    accessibilityLabel:NSLocalizedString(@"Windows", @"Windows key")
		                target:target
		                action:@selector(toggleWinKey:)];
		[self addKeyWithTitle:@"esc"
		                 image:nil
		    accessibilityLabel:NSLocalizedString(@"Escape", @"Escape key")
		                target:target
		                action:@selector(pressEscKey:)];
		[self addKeyWithTitle:@"shift"
		                 image:nil
		    accessibilityLabel:NSLocalizedString(@"Shift", @"Shift key")
		                target:target
		                action:@selector(toggleShiftKey:)];
		[self addKeyWithTitle:@"ctrl"
		                 image:nil
		    accessibilityLabel:NSLocalizedString(@"Control", @"Control key")
		                target:target
		                action:@selector(toggleCtrlKey:)];
		[self addKeyWithTitle:@"alt"
		                 image:nil
		    accessibilityLabel:NSLocalizedString(@"Alt", @"Alt key")
		                target:target
		                action:@selector(toggleAltKey:)];
		[self addKeyWithTitle:@"alt gr"
		                 image:nil
		    accessibilityLabel:NSLocalizedString(@"Alt Gr", @"Alt Gr key")
		                target:target
		                action:@selector(toggleAltGrKey:)];
		[self addKeyWithTitle:@"del"
		                 image:nil
		    accessibilityLabel:NSLocalizedString(@"Delete", @"Delete key")
		                target:target
		                action:@selector(pressDeleteKey:)];
		[self addKeyWithTitle:@"tab"
		                 image:nil
		    accessibilityLabel:NSLocalizedString(@"Tab", @"Tab key")
		                target:target
		                action:@selector(pressTabKey:)];

		_divider = [[UIView alloc] initWithFrame:CGRectZero];

		[_divider setBackgroundColor:[UIColor colorWithWhite:0.5 alpha:0.6]];
		[_bar addSubview:_divider];

		UIImageSymbolConfiguration *config =
		    [UIImageSymbolConfiguration configurationWithPointSize:metrics->symbolSize
		                                                    weight:UIImageSymbolWeightMedium];
		_toggle_button =
		    [RDPCreateKeyButton(nil, [UIImage systemImageNamed:@"arrow.right.arrow.left"
		                                     withConfiguration:config]) retain];
		[_toggle_button setAccessibilityLabel:NSLocalizedString(@"Extended keyboard",
		                                                        @"Extended keyboard toggle")];
		[_toggle_button addTarget:target
		                   action:@selector(toggleKeyboardWhenOtherVisible:)
		         forControlEvents:UIControlEventTouchUpInside];
		[_toggle_button addTarget:self
		                   action:@selector(playInputClick:)
		         forControlEvents:UIControlEventTouchUpInside];
		[_bar addSubview:_toggle_button];
	}
	return self;
}

- (void)dealloc
{
	[_bar release];
	[_background release];
	[_keys_container release];
	[_keys_scrollview setDelegate:nil];
	[_keys_scrollview release];
	[_fade_mask release];
	[_key_buttons release];
	[_divider release];
	[_toggle_button release];
	[super dealloc];
}

- (CGSize)intrinsicContentSize
{
	return CGSizeMake(UIViewNoIntrinsicMetric, kViewHeight);
}

- (void)safeAreaInsetsDidChange
{
	[super safeAreaInsetsDidChange];
	[self setNeedsLayout];
}

- (void)layoutSubviews
{
	[super layoutSubviews];

	UIEdgeInsets safe = [self safeAreaInsets];
	CGFloat barWidth = [self bounds].size.width - safe.left - safe.right - 2 * kBarSideMargin;
	[_bar setFrame:CGRectMake(safe.left + kBarSideMargin, (kViewHeight - kBarHeight) * 0.5,
	                          barWidth, kBarHeight)];
	[_background setFrame:[_bar bounds]];
	RDPUpdateGlassCapsuleCorners(_background);

	const CGFloat keyWidth = RDPGetKeyboardToolbarMetrics()->keyWidth;
	const CGFloat keyY = (kBarHeight - kKeyHeight) * 0.5;
	const CGFloat toggleX = barWidth - kKeyInset - keyWidth;
	const CGFloat dividerX = toggleX - kDividerGap;

	[_toggle_button setFrame:CGRectMake(toggleX, keyY, keyWidth, kKeyHeight)];
	[_divider setFrame:CGRectMake(dividerX - 0.5, (kBarHeight - kDividerHeight) * 0.5, 1.0,
	                              kDividerHeight)];

	// remaining keys share the space in front of the divider
	const CGFloat keysWidth = MAX(dividerX - kDividerGap - kKeyInset, 0.0);
	[_keys_container setFrame:CGRectMake(kKeyInset, 0.0, keysWidth, kBarHeight)];
	[_keys_scrollview setFrame:[_keys_container bounds]];

	const NSUInteger count = [_key_buttons count];
	const CGFloat spacingTotal = kKeySpacing * (count - 1);
	CGFloat width = keyWidth;
	if (keysWidth > count * keyWidth + spacingTotal)
		width = MIN((keysWidth - spacingTotal) / count, keyWidth * 1.35);

	CGFloat x = 0.0;
	for (UIButton *btn in _key_buttons)
	{
		[btn setFrame:CGRectMake(x, keyY, width, kKeyHeight)];
		x += width + kKeySpacing;
	}
	[_keys_scrollview setContentSize:CGSizeMake(MAX(x - kKeySpacing, 0.0), kBarHeight)];

	[self updateFadeMask];
}

- (void)updateModifiersWithKeyboard:(RDPKeyboard *)keyboard
{
	[[_key_buttons objectAtIndex:ACCESSORY_KEY_WIN] setSelected:[keyboard winPressed]];
	[[_key_buttons objectAtIndex:ACCESSORY_KEY_SHIFT] setSelected:[keyboard shiftPressed]];
	[[_key_buttons objectAtIndex:ACCESSORY_KEY_CTRL] setSelected:[keyboard ctrlPressed]];
	[[_key_buttons objectAtIndex:ACCESSORY_KEY_ALT] setSelected:[keyboard altPressed]];
	[[_key_buttons objectAtIndex:ACCESSORY_KEY_ALTGR] setSelected:[keyboard altGrPressed]];
}

- (void)setExtendedKeyboardActive:(BOOL)active
{
	[_toggle_button setSelected:active];
}

- (BOOL)enableInputClicksWhenVisible
{
	return YES;
}

- (void)playInputClick:(id)sender
{
	(void)sender;
	[[UIDevice currentDevice] playInputClick];
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView *)scrollView
{
	(void)scrollView;
	[self updateFadeMask];
}

@end

@implementation RDPKeyboardToolbarView (Private)

- (void)addKeyWithTitle:(NSString *)title
                  image:(UIImage *)image
     accessibilityLabel:(NSString *)label
                 target:(id)target
                 action:(SEL)action
{
	UIButton *btn = RDPCreateKeyButton(title, image);
	[btn setAccessibilityLabel:label];
	[btn addTarget:target action:action forControlEvents:UIControlEventTouchUpInside];
	[btn addTarget:self
	              action:@selector(playInputClick:)
	    forControlEvents:UIControlEventTouchUpInside];
	[_keys_scrollview addSubview:btn];
	[_key_buttons addObject:btn];
}

// if the button overflows, display a fade out.
- (void)updateFadeMask
{
	CGSize size = [_keys_scrollview bounds].size;
	CGFloat offset = [_keys_scrollview contentOffset].x;
	CGFloat overflow = [_keys_scrollview contentSize].width - size.width;

	// if the button does not overflow or does not overflow sufficiently
	// off the fade out
	if (size.width <= 0.0 || overflow <= 0.5)
	{
		[[_keys_container layer] setMask:nil];
		return;
	}

	const CGFloat fade = MIN(kFadeWidth / size.width, 0.5);
	BOOL fadeLeading = offset > 0.5;
	BOOL fadeTrailing = offset < overflow - 0.5;
	id opaque = (id)[[UIColor blackColor] CGColor];
	id clear = (id)[[UIColor clearColor] CGColor];

	// turn off animation
	[CATransaction begin];
	[CATransaction setDisableActions:YES];
	[_fade_mask setFrame:[_keys_container bounds]];
	[_fade_mask
	    setColors:@[fadeLeading ? clear : opaque, opaque, opaque, fadeTrailing ? clear : opaque]];
	[_fade_mask setLocations:@[@0.0, @(fade), @(1.0 - fade), @1.0]];
	[[_keys_container layer] setMask:_fade_mask];
	[CATransaction commit];
}

@end
