/*
 RDP Touch Pointer View

 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import "TouchPointerView.h"
#import "Utils.h"

#define RESET_DEFAULT_POINTER_IMAGE_DELAY 0.15

#define POINTER_ACTION_CURSOR 0
#define POINTER_ACTION_CLOSE 3
#define POINTER_ACTION_RCLICK 2
#define POINTER_ACTION_LCLICK 4
#define POINTER_ACTION_MOVE 4
#define POINTER_ACTION_SCROLL 5
#define POINTER_ACTION_KEYBOARD 7
#define POINTER_ACTION_EXTKEYBOARD 8
#define POINTER_ACTION_RESET 6

@interface TouchPointerView (Private)
- (void)setCurrentPointerImage:(UIImage *)image;
- (void)displayPointerActionImage:(UIImage *)image;
- (BOOL)pointInsidePointer:(CGPoint)point;
- (BOOL)pointInsidePointerArea:(int)area point:(CGPoint)point;
- (CGPoint)getCursorPosition;
- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent *)event;
- (void)handleSingleTap:(UITapGestureRecognizer *)gesture;
- (void)handlerForGesture:(UIGestureRecognizer *)gesture sendClick:(BOOL)sendClick;
@end

@implementation TouchPointerView

@synthesize delegate = _delegate;

- (void)awakeFromNib
{
	[super awakeFromNib];

	// set content mode when rotating (keep aspect ratio)
	[self setContentMode:UIViewContentModeTopLeft];

	// load touchPointerImage
	_default_pointer_img = [[UIImage
	    imageWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"touch_pointer_default"
	                                                            ofType:@"png"]] retain];
	_active_pointer_img = [[UIImage
	    imageWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"touch_pointer_active"
	                                                            ofType:@"png"]] retain];
	_lclick_pointer_img = [[UIImage
	    imageWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"touch_pointer_lclick"
	                                                            ofType:@"png"]] retain];
	_rclick_pointer_img = [[UIImage
	    imageWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"touch_pointer_rclick"
	                                                            ofType:@"png"]] retain];
	_scroll_pointer_img = [[UIImage
	    imageWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"touch_pointer_scroll"
	                                                            ofType:@"png"]] retain];
	_extkeyboard_pointer_img = [[UIImage
	    imageWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"touch_pointer_ext_keyboard"
	                                                            ofType:@"png"]] retain];
	_keyboard_pointer_img = [[UIImage
	    imageWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"touch_pointer_keyboard"
	                                                            ofType:@"png"]] retain];
	_reset_pointer_img = [[UIImage
	    imageWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"touch_pointer_reset"
	                                                            ofType:@"png"]] retain];
	_cur_pointer_img = _default_pointer_img;
	_pointer_transformation = CGAffineTransformMake(1, 0, 0, 1, 0, 0);

	// init flags
	_pointer_moving = NO;
	_pointer_scrolling = NO;

	// create areas array
	int i, j;
	CGFloat area_width = [_cur_pointer_img size].width / 3.0f;
	CGFloat area_height = [_cur_pointer_img size].height / 3.0f;
	for (i = 0; i < 3; i++)
	{
		for (j = 0; j < 3; j++)
		{
			_pointer_areas[j + i * 3] =
			    CGRectMake(j * area_width, i * area_height, area_width, area_height);
		}
	}

	// init gesture recognizers
	UITapGestureRecognizer *singleTapRecognizer =
	    [[[UITapGestureRecognizer alloc] initWithTarget:self
	                                             action:@selector(handleSingleTap:)] autorelease];
	[singleTapRecognizer setNumberOfTouchesRequired:1];
	[singleTapRecognizer setNumberOfTapsRequired:1];

	UILongPressGestureRecognizer *dragDropRecognizer = [[[UILongPressGestureRecognizer alloc]
	    initWithTarget:self
	            action:@selector(handleDragDrop:)] autorelease];
	dragDropRecognizer.minimumPressDuration = 0.4;
	//        dragDropRecognizer.allowableMovement = 1000.0;

	UILongPressGestureRecognizer *pointerMoveScrollRecognizer =
	    [[[UILongPressGestureRecognizer alloc] initWithTarget:self
	                                                   action:@selector(handlePointerMoveScroll:)]
	        autorelease];
	pointerMoveScrollRecognizer.minimumPressDuration = 0.15;
	pointerMoveScrollRecognizer.allowableMovement = 1000.0;
	[pointerMoveScrollRecognizer requireGestureRecognizerToFail:dragDropRecognizer];

	[self addGestureRecognizer:singleTapRecognizer];
	[self addGestureRecognizer:dragDropRecognizer];
	[self addGestureRecognizer:pointerMoveScrollRecognizer];
}

- (void)dealloc
{
	[super dealloc];
	[_default_pointer_img autorelease];
	[_active_pointer_img autorelease];
	[_lclick_pointer_img autorelease];
	[_rclick_pointer_img autorelease];
	[_scroll_pointer_img autorelease];
	[_extkeyboard_pointer_img autorelease];
	[_keyboard_pointer_img autorelease];
	[_reset_pointer_img autorelease];
}

#pragma mark - Public interface

// positions the pointer on screen if it got offscreen after an orentation change
- (void)ensurePointerIsVisible
{
	CGRect bounds = [self bounds];
	if (_pointer_transformation.tx > (bounds.size.width - _cur_pointer_img.size.width))
		_pointer_transformation.tx = bounds.size.width - _cur_pointer_img.size.width;
	if (_pointer_transformation.ty > (bounds.size.height - _cur_pointer_img.size.height))
		_pointer_transformation.ty = bounds.size.height - _cur_pointer_img.size.height;
	[self setNeedsDisplay];
}

// show/hides the touch pointer
- (void)setHidden:(BOOL)hidden
{
	[super setHidden:hidden];

	// if shown center pointer in view
	if (!hidden)
	{
		_pointer_transformation = CGAffineTransformMakeTranslation(
		    ([self bounds].size.width - [_cur_pointer_img size].width) / 2,
		    ([self bounds].size.height - [_cur_pointer_img size].height) / 2);
		[self setNeedsDisplay];
	}
}

- (UIEdgeInsets)getEdgeInsets
{
	return UIEdgeInsetsMake(0, 0, [_cur_pointer_img size].width, [_cur_pointer_img size].height);
}

- (CGPoint)getPointerPosition
{
	return CGPointMake(_pointer_transformation.tx, _pointer_transformation.ty);
}

- (int)getPointerWidth
{
	return [_cur_pointer_img size].width;
}

- (int)getPointerHeight
{
	return [_cur_pointer_img size].height;
}

@end

@implementation TouchPointerView (Private)

- (void)setCurrentPointerImage:(UIImage *)image
{
	_cur_pointer_img = image;
	[self setNeedsDisplay];
}

- (void)displayPointerActionImage:(UIImage *)image
{
	[self setCurrentPointerImage:image];
	[self performSelector:@selector(setCurrentPointerImage:)
	           withObject:_default_pointer_img
	           afterDelay:RESET_DEFAULT_POINTER_IMAGE_DELAY];
}

// Only override drawRect: if you perform custom drawing.
// An empty implementation adversely affects performance during animation.
- (void)drawRect:(CGRect)rect
{
	// Drawing code
	CGContextRef context = UIGraphicsGetCurrentContext();
	CGContextSaveGState(context);
	CGContextConcatCTM(context, _pointer_transformation);
	CGContextDrawImage(
	    context, CGRectMake(0, 0, [_cur_pointer_img size].width, [_cur_pointer_img size].height),
	    [_cur_pointer_img CGImage]);
	CGContextRestoreGState(context);
}

// helper that returns YES if the given point is within the pointer
- (BOOL)pointInsidePointer:(CGPoint)point
{
	CGRect rec = CGRectMake(0, 0, [_cur_pointer_img size].width, [_cur_pointer_img size].height);
	return CGRectContainsPoint(CGRectApplyAffineTransform(rec, _pointer_transformation), point);
}

// helper that returns YES if the given point is within the given pointer area
- (BOOL)pointInsidePointerArea:(int)area point:(CGPoint)point
{
	CGRect rec = _pointer_areas[area];
	return CGRectContainsPoint(CGRectApplyAffineTransform(rec, _pointer_transformation), point);
}

// returns the position of the cursor
- (CGPoint)getCursorPosition
{
	CGRect transPointerArea =
	    CGRectApplyAffineTransform(_pointer_areas[POINTER_ACTION_CURSOR], _pointer_transformation);
	return CGPointMake(CGRectGetMidX(transPointerArea), CGRectGetMidY(transPointerArea));
}

// this filters events - if the pointer was clicked the scrollview won't get any events
- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent *)event
{
	return [self pointInsidePointer:point];
}

#pragma mark - Action handlers

// handles single tap gestures, returns YES if the event was handled by the pointer, NO otherwise
- (void)handleSingleTap:(UITapGestureRecognizer *)gesture
{
	// get touch position within our view
	CGPoint touchPos = [gesture locationInView:self];

	// look if pointer was in one of our action areas
	if ([self pointInsidePointerArea:POINTER_ACTION_CLOSE point:touchPos])
		[[self delegate] touchPointerClose];
	else if ([self pointInsidePointerArea:POINTER_ACTION_LCLICK point:touchPos])
	{
		[self displayPointerActionImage:_lclick_pointer_img];
		[[self delegate] touchPointerLeftClick:[self getCursorPosition] down:YES];
		[[self delegate] touchPointerLeftClick:[self getCursorPosition] down:NO];
	}
	else if ([self pointInsidePointerArea:POINTER_ACTION_RCLICK point:touchPos])
	{
		[self displayPointerActionImage:_rclick_pointer_img];
		[[self delegate] touchPointerRightClick:[self getCursorPosition] down:YES];
		[[self delegate] touchPointerRightClick:[self getCursorPosition] down:NO];
	}
	else if ([self pointInsidePointerArea:POINTER_ACTION_KEYBOARD point:touchPos])
	{
		[self displayPointerActionImage:_keyboard_pointer_img];
		[[self delegate] touchPointerToggleKeyboard];
	}
	else if ([self pointInsidePointerArea:POINTER_ACTION_EXTKEYBOARD point:touchPos])
	{
		[self displayPointerActionImage:_extkeyboard_pointer_img];
		[[self delegate] touchPointerToggleExtendedKeyboard];
	}
	else if ([self pointInsidePointerArea:POINTER_ACTION_RESET point:touchPos])
	{
		[self displayPointerActionImage:_reset_pointer_img];
		[[self delegate] touchPointerResetSessionView];
	}
}

- (void)handlerForGesture:(UIGestureRecognizer *)gesture sendClick:(BOOL)sendClick
{
	if ([gesture state] == UIGestureRecognizerStateBegan)
	{
		CGPoint touchPos = [gesture locationInView:self];
		if ([self pointInsidePointerArea:POINTER_ACTION_LCLICK point:touchPos])
		{
			_prev_touch_location = touchPos;
			_pointer_moving = YES;
			if (sendClick == YES)
			{
				[[self delegate] touchPointerLeftClick:[self getCursorPosition] down:YES];
				[self setCurrentPointerImage:_active_pointer_img];
			}
		}
		else if ([self pointInsidePointerArea:POINTER_ACTION_SCROLL point:touchPos])
		{
			[self setCurrentPointerImage:_scroll_pointer_img];
			_prev_touch_location = touchPos;
			_pointer_scrolling = YES;
		}
	}
	else if ([gesture state] == UIGestureRecognizerStateChanged)
	{
		if (_pointer_moving)
		{
			CGPoint touchPos = [gesture locationInView:self];
			_pointer_transformation = CGAffineTransformTranslate(
			    _pointer_transformation, touchPos.x - _prev_touch_location.x,
			    touchPos.y - _prev_touch_location.y);
			[[self delegate] touchPointerMove:[self getCursorPosition]];
			_prev_touch_location = touchPos;
			[self setNeedsDisplay];
		}
		else if (_pointer_scrolling)
		{
			CGPoint touchPos = [gesture locationInView:self];
			float delta = touchPos.y - _prev_touch_location.y;
			if (delta > GetScrollGestureDelta())
			{
				[[self delegate] touchPointerScrollDown:YES];
				_prev_touch_location = touchPos;
			}
			else if (delta < -GetScrollGestureDelta())
			{
				[[self delegate] touchPointerScrollDown:NO];
				_prev_touch_location = touchPos;
			}
		}
	}
	else if ([gesture state] == UIGestureRecognizerStateEnded)
	{
		if (_pointer_moving)
		{
			if (sendClick == YES)
				[[self delegate] touchPointerLeftClick:[self getCursorPosition] down:NO];
			_pointer_moving = NO;
			[self setCurrentPointerImage:_default_pointer_img];
		}

		if (_pointer_scrolling)
		{
			[self setCurrentPointerImage:_default_pointer_img];
			_pointer_scrolling = NO;
		}
	}
}

// handles long press gestures
- (void)handleDragDrop:(UILongPressGestureRecognizer *)gesture
{
	[self handlerForGesture:gesture sendClick:YES];
}

- (void)handlePointerMoveScroll:(UILongPressGestureRecognizer *)gesture
{
	[self handlerForGesture:gesture sendClick:NO];
}

@end
