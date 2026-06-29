//
//  RDPSessionToolbar.m
//  iFreeRDP
//
//  Created by byungho on 6/21/26.
//

#import "RDPSessionToolbar.h"

@implementation RDPSessionToolbar

@synthesize passthroughView = _passthroughView;

- (BOOL)isPointerEvent:(UIEvent *)event
{
	// allTouches is not yet populated when hit-testing a press, so rely on signals
	// that are valid at that point: hover events and a pressed pointer button. Finger
	// touches report an empty button mask.
	return ([event type] == UIEventTypeHover) || ([event buttonMask] != 0);
}

- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event
{
	// only direct screen touches operate the toolbar; pointer input (hover and
	// mouse/trackpad clicks) is redirected to the remote session underneath
	if (event != nil && _passthroughView != nil && [self isPointerEvent:event])
	{
		CGPoint converted = [self convertPoint:point toView:_passthroughView];
		return [_passthroughView hitTest:converted withEvent:event];
	}

	return [super hitTest:point withEvent:event];
}

@end
