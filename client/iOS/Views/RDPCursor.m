//
//  RDPCursor.m
//  iFreeRDP
//
//  Created by byungho on 6/19/26.
//

#import "RDPCursor.h"

#include <winpr/wtypes.h>

@implementation RDPCursor

@synthesize image = _image;
@synthesize hotspot = _hotspot;

- (id)initWithRGBABytes:(const void *)bytes
                  width:(NSUInteger)width
                 height:(NSUInteger)height
                hotspot:(CGPoint)hotspot
{
	self = [super init];
	if (!self)
		return nil;

	if (!bytes || (width == 0) || (height == 0))
	{
		[self release];
		return nil;
	}

	NSData *pixelData = [NSData dataWithBytes:bytes length:width * height * 4];
	CGDataProviderRef provider = CGDataProviderCreateWithCFData((CFDataRef)pixelData);
	CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
	CGImageRef imageRef = CGImageCreate(width, height, 8, 32, width * 4, colorSpace,
	                                    kCGBitmapByteOrderDefault | kCGImageAlphaLast, provider,
	                                    nullptr, NO, kCGRenderingIntentDefault);

	CGColorSpaceRelease(colorSpace);
	CGDataProviderRelease(provider);

	if (!imageRef)
	{
		[self release];
		return nil;
	}

	_image = [[UIImage imageWithCGImage:imageRef scale:1.0
	                        orientation:UIImageOrientationUp] retain];
	_hotspot = hotspot;
	CGImageRelease(imageRef);
	return self;
}

// default white cursor
+ (RDPCursor *)defaultCursor
{
	const CGSize size = CGSizeMake(24.0, 32.0);
	UIGraphicsBeginImageContextWithOptions(size, NO, 1.0);

	UIBezierPath *path = [UIBezierPath bezierPath];
	[path moveToPoint:CGPointMake(2.0, 1.0)];
	[path addLineToPoint:CGPointMake(2.0, 25.0)];
	[path addLineToPoint:CGPointMake(8.5, 19.0)];
	[path addLineToPoint:CGPointMake(13.0, 30.0)];
	[path addLineToPoint:CGPointMake(18.0, 28.0)];
	[path addLineToPoint:CGPointMake(13.5, 17.0)];
	[path addLineToPoint:CGPointMake(22.0, 17.0)];
	[path closePath];
	[[UIColor whiteColor] setFill];
	[[UIColor blackColor] setStroke];
	[path setLineWidth:2.0];
	[path fill];
	[path stroke];

	UIImage *image = UIGraphicsGetImageFromCurrentImageContext();
	UIGraphicsEndImageContext();

	RDPCursor *cursor = [[[RDPCursor alloc] init] autorelease];
	[cursor setImage:image];
	[cursor setHotspot:CGPointMake(2.0, 1.0)];
	return cursor;
}

- (void)dealloc
{
	[_image release];
	[super dealloc];
}

@end
