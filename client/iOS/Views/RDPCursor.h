//
//  RDPCursor.h
//  iFreeRDP
//
//  Created by byungho on 6/19/26.
//

#ifndef RDPCursor_h
#define RDPCursor_h

#import <UIKit/UIKit.h>

@interface RDPCursor : NSObject
{
	UIImage *_image;
	CGPoint _hotspot;
}

@property(nonatomic, retain) UIImage *image;
@property(nonatomic, assign) CGPoint hotspot;

- (id)initWithRGBABytes:(const void *)bytes
                  width:(NSUInteger)width
                 height:(NSUInteger)height
                hotspot:(CGPoint)hotspot;

// default white cursor
+ (RDPCursor *)defaultCursor;

@end

#endif /* RDPCursor_h */
