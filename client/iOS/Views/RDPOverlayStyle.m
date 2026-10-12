/*
 Shared look of the session overlays (key bar, extended keyboard, toolbar)

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import "RDPOverlayStyle.h"
#import "Utils.h"

void RDPApplyKeyCapColors(UIButtonConfiguration *config, BOOL latched, BOOL pressed);

BOOL RDPSupportLiquidGlass(void);

UIVisualEffectView *RDPCreateGlassCapsuleView(void)
{
	UIVisualEffect *effect;
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= 260000
	if (@available(iOS 26.0, *))
	{
		effect = [UIGlassEffect effectWithStyle:UIGlassEffectStyleRegular];
	}
	else
#endif
	{
		effect = [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemChromeMaterial];
	}

	UIVisualEffectView *view = [[[UIVisualEffectView alloc] initWithEffect:effect] autorelease];
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= 260000
	if (@available(iOS 26.0, *))
	{
		[view setCornerConfiguration:[UICornerConfiguration capsuleConfiguration]];
	}
	else
#endif
	{
		[view setClipsToBounds:YES];
		[[view layer] setCornerCurve:kCACornerCurveContinuous];
	}
	return view;
}

void RDPUpdateGlassCapsuleCorners(UIVisualEffectView *view)
{
	if (RDPSupportLiquidGlass() == YES)
	{
		return;
	}

	CGSize size = [view bounds].size;
	[[view layer] setCornerRadius:MIN(size.width, size.height) * 0.5];
}

UIImage *RDPWindowsLogoImage(CGFloat size)
{
	UIGraphicsImageRenderer *renderer =
	    [[[UIGraphicsImageRenderer alloc] initWithSize:CGSizeMake(size, size)] autorelease];

	// draw 4 square like windows logo
	UIImage *image = [renderer imageWithActions:^(UIGraphicsImageRendererContext *context) {
		const CGFloat gap = MAX(1.0, round(size * 0.08));
		const CGFloat tile = (size - gap) * 0.5;
		[[UIColor blackColor] setFill];
		UIRectFill(CGRectMake(0, 0, tile, tile));
		UIRectFill(CGRectMake(tile + gap, 0, tile, tile));
		UIRectFill(CGRectMake(0, tile + gap, tile, tile));
		UIRectFill(CGRectMake(tile + gap, tile + gap, tile, tile));
	}];
	return [image imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
}

UIButton *RDPCreateKeyButton(NSString *title, UIImage *image)
{
	UIButtonConfiguration *config = [UIButtonConfiguration grayButtonConfiguration];
	[config setCornerStyle:UIButtonConfigurationCornerStyleMedium];
	[config setContentInsets:NSDirectionalEdgeInsetsMake(4.0, 4.0, 4.0, 4.0)];

	if (title != nil)
	{
		[config setTitle:title];
		[config setTitleLineBreakMode:NSLineBreakByClipping];
	}
	else if (image != nil)
	{
		[config setImage:image];
	}
	else
	{
		return nil;
	}

	RDPApplyKeyCapColors(config, NO, NO);

	UIButton *btn = [UIButton buttonWithConfiguration:config primaryAction:nil];
	[btn setExclusiveTouch:YES];
	[btn setConfigurationUpdateHandler:^(UIButton *button) {
		UIButtonConfiguration *updated = [[[button configuration] copy] autorelease];
		RDPApplyKeyCapColors(updated, [button isSelected], [button isHighlighted]);
		[button setConfiguration:updated];
	}];
	return btn;
}

UIButton *RDPCreateTabButton(NSString *title)
{
	UIButton *btn = RDPCreateKeyButton(title, nil);
	if (btn == nil)
		return nil;

	// set style
	UIButtonConfiguration *config = [[[btn configuration] copy] autorelease];
	[config setCornerStyle:UIButtonConfigurationCornerStyleCapsule];

	[btn setConfiguration:config];
	return btn;
}

void RDPApplyKeyCapColors(UIButtonConfiguration *config, BOOL latched, BOOL pressed)
{
	UIColor *background;
	if (latched)
		background = pressed ? [UIColor secondaryLabelColor] : [UIColor labelColor];
	else
		background = [UIColor colorWithDynamicProvider:^UIColor *(UITraitCollection *traits) {
			if ([traits userInterfaceStyle] == UIUserInterfaceStyleDark)
				return pressed ? [UIColor systemGray2Color] : [UIColor systemGray3Color];
			return pressed ? [UIColor systemGray4Color] : [UIColor whiteColor];
		}];

	[[config background] setBackgroundColor:background];
	[config
	    setBaseForegroundColor:latched ? [UIColor systemBackgroundColor] : [UIColor labelColor]];
}

BOOL RDPSupportLiquidGlass(void)
{
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= 260000
	if (@available(iOS 26.0, *))
		return YES;
#endif
	return NO;
}
