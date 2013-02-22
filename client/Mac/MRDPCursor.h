//
//  MRDPCursor.h
//  MacFreeRDP
//
//  Created by Laxmikant Rashinkar
//  Copyright (c) 2012 FreeRDP.org All rights reserved.
//

#import <Cocoa/Cocoa.h>

#include "freerdp/graphics.h"

@interface MRDPCursor : NSObject
{
@public
	rdpPointer       *pointer;
	BYTE            *cursor_data;   // bitmapped pixel data
	NSBitmapImageRep *bmiRep;
	NSCursor         *nsCursor;
	NSImage          *nsImage;
}

@end
