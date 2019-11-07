//
//  BlockBackground.m
//  arrived
//
//  Created by Gustavo Ambrozio on 29/11/11.
//  Copyright (c) 2011 N/A. All rights reserved.
//

#import "BlockBackground.h"

@implementation BlockBackground

@synthesize backgroundImage = _backgroundImage;
@synthesize vignetteBackground = _vignetteBackground;

static BlockBackground *_sharedInstance = nil;

+ (BlockBackground *)sharedInstance
{
	if (_sharedInstance != nil)
	{
		return _sharedInstance;
	}

	@synchronized(self)
	{
		if (_sharedInstance == nil)
		{
			[[[self alloc] init] autorelease];
		}
	}

	return _sharedInstance;
}

+ (id)allocWithZone:(NSZone *)zone
{
	@synchronized(self)
	{
		if (_sharedInstance == nil)
		{
			_sharedInstance = [super allocWithZone:zone];
			return _sharedInstance;
		}
	}
	NSAssert(NO, @ "[BlockBackground alloc] explicitly called on singleton class.");
	return nil;
}

- (id)copyWithZone:(NSZone *)zone
{
	return self;
}

- (id)retain
{
	return self;
}

- (unsigned)retainCount
{
	return UINT_MAX;
}

- (oneway void)release
{
}

- (id)autorelease
{
	return self;
}

- (void)setRotation:(NSNotification *)notification
{
	UIInterfaceOrientation orientation = [[UIApplication sharedApplication] statusBarOrientation];

	CGRect orientationFrame = [UIScreen mainScreen].bounds;

	if ((UIInterfaceOrientationIsLandscape(orientation) &&
	     orientationFrame.size.height > orientationFrame.size.width) ||
	    (UIInterfaceOrientationIsPortrait(orientation) &&
	     orientationFrame.size.width > orientationFrame.size.height))
	{
		float temp = orientationFrame.size.width;
		orientationFrame.size.width = orientationFrame.size.height;
		orientationFrame.size.height = temp;
	}

	self.transform = CGAffineTransformIdentity;
	self.frame = orientationFrame;

	CGFloat posY = orientationFrame.size.height / 2;
	CGFloat posX = orientationFrame.size.width / 2;

	CGPoint newCenter;
	CGFloat rotateAngle;

	switch (orientation)
	{
		case UIInterfaceOrientationPortraitUpsideDown:
			rotateAngle = M_PI;
			newCenter = CGPointMake(posX, orientationFrame.size.height - posY);
			break;
		case UIInterfaceOrientationLandscapeLeft:
			rotateAngle = -M_PI / 2.0f;
			newCenter = CGPointMake(posY, posX);
			break;
		case UIInterfaceOrientationLandscapeRight:
			rotateAngle = M_PI / 2.0f;
			newCenter = CGPointMake(orientationFrame.size.height - posY, posX);
			break;
		default: // UIInterfaceOrientationPortrait
			rotateAngle = 0.0;
			newCenter = CGPointMake(posX, posY);
			break;
	}

	self.transform = CGAffineTransformMakeRotation(rotateAngle);
	self.center = newCenter;

	[self setNeedsLayout];
	[self layoutSubviews];
}

- (id)init
{
	self = [super initWithFrame:[[UIScreen mainScreen] bounds]];
	if (self)
	{
		self.windowLevel = UIWindowLevelStatusBar;
		self.hidden = YES;
		self.userInteractionEnabled = NO;
		self.backgroundColor = [UIColor colorWithWhite:0.4 alpha:0.5f];
		self.vignetteBackground = NO;

		[[NSNotificationCenter defaultCenter]
		    addObserver:self
		       selector:@selector(setRotation:)
		           name:UIApplicationDidChangeStatusBarOrientationNotification
		         object:nil];
		[self setRotation:nil];
	}
	return self;
}

- (void)addToMainWindow:(UIView *)view
{
	[self setRotation:nil];

	if ([self.subviews containsObject:view])
		return;

	if (self.hidden)
	{
		_previousKeyWindow = [[[UIApplication sharedApplication] keyWindow] retain];
		self.alpha = 0.0f;
		self.hidden = NO;
		self.userInteractionEnabled = YES;
		[self makeKeyWindow];
	}

	if (self.subviews.count > 0)
	{
		((UIView *)[self.subviews lastObject]).userInteractionEnabled = NO;
	}

	if (_backgroundImage)
	{
		UIImageView *backgroundView = [[UIImageView alloc] initWithImage:_backgroundImage];
		backgroundView.frame = self.bounds;
		backgroundView.contentMode = UIViewContentModeScaleToFill;
		[self addSubview:backgroundView];
		[backgroundView release];
		[_backgroundImage release];
		_backgroundImage = nil;
	}

	[self addSubview:view];
}

- (void)reduceAlphaIfEmpty
{
	if (self.subviews.count == 1 ||
	    (self.subviews.count == 2 &&
	     [[self.subviews objectAtIndex:0] isKindOfClass:[UIImageView class]]))
	{
		self.alpha = 0.0f;
		self.userInteractionEnabled = NO;
	}
}

- (void)removeView:(UIView *)view
{
	[view removeFromSuperview];

	UIView *topView = [self.subviews lastObject];
	if ([topView isKindOfClass:[UIImageView class]])
	{
		// It's a background. Remove it too
		[topView removeFromSuperview];
	}

	if (self.subviews.count == 0)
	{
		self.hidden = YES;
		[_previousKeyWindow makeKeyWindow];
		[_previousKeyWindow release];
		_previousKeyWindow = nil;
	}
	else
	{
		((UIView *)[self.subviews lastObject]).userInteractionEnabled = YES;
	}
}

- (void)drawRect:(CGRect)rect
{
	if (_backgroundImage || !_vignetteBackground)
		return;
	CGContextRef context = UIGraphicsGetCurrentContext();

	size_t locationsCount = 2;
	CGFloat locations[2] = { 0.0f, 1.0f };
	CGFloat colors[8] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.75f };
	CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
	CGGradientRef gradient =
	    CGGradientCreateWithColorComponents(colorSpace, colors, locations, locationsCount);
	CGColorSpaceRelease(colorSpace);

	CGPoint center = CGPointMake(self.bounds.size.width / 2, self.bounds.size.height / 2);
	float radius = MIN(self.bounds.size.width, self.bounds.size.height);
	CGContextDrawRadialGradient(context, gradient, center, 0, center, radius,
	                            kCGGradientDrawsAfterEndLocation);
	CGGradientRelease(gradient);
}

@end
