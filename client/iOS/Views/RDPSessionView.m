/*
 RDP Session View
 
 Copyright 2013 Thinstuff Technologies GmbH, Author: Martin Fleisz
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#import "RDPSessionView.h"

@implementation RDPSessionView

- (void)setSession:(RDPSession*)session
{
    _session = session;
}

- (void)awakeFromNib
{
    [super awakeFromNib];
    _session = nil;
}

- (void)drawRect:(CGRect)rect 
{
	if(_session != nil && [_session bitmapContext])
	{
		CGContextRef context = UIGraphicsGetCurrentContext();
		CGImageRef cgImage = CGBitmapContextCreateImage([_session bitmapContext]);

        CGContextTranslateCTM(context, 0, [self bounds].size.height);
        CGContextScaleCTM(context, 1.0, -1.0);      
        CGContextClipToRect(context, CGRectMake(rect.origin.x, [self bounds].size.height - rect.origin.y - rect.size.height, rect.size.width, rect.size.height));
        CGContextDrawImage(context, CGRectMake(0, 0, [self bounds].size.width, [self bounds].size.height), cgImage);        
		
        CGImageRelease(cgImage);
	}
    else
    {
        // just clear the screen with black
        [[UIColor blackColor] set];
        UIRectFill([self bounds]);        
    }
}

- (void)dealloc {
    [super dealloc];
}

@end
