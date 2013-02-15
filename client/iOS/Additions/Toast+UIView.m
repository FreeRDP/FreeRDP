/***************************************************************************
 
 Toast+UIView.h
 Toast
 Version 0.1
 
 Copyright (c) 2011 Charles Scalesse.
 
 Permission is hereby granted, free of charge, to any person obtaining a
 copy of this software and associated documentation files (the
 "Software"), to deal in the Software without restriction, including
 without limitation the rights to use, copy, modify, merge, publish,
 distribute, sublicense, and/or sell copies of the Software, and to
 permit persons to whom the Software is furnished to do so, subject to
 the following conditions:
 
 The above copyright notice and this permission notice shall be included
 in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 
 ***************************************************************************/

#import "Toast+UIView.h"
#import <QuartzCore/QuartzCore.h>
#import <objc/runtime.h>

#define kMaxWidth           0.8
#define kMaxHeight          0.8

#define kHorizontalPadding  10.0
#define kVerticalPadding    10.0
#define kCornerRadius       10.0
#define kOpacity            0.8
#define kFontSize           16.0
#define kMaxTitleLines      999
#define kMaxMessageLines    999

#define kFadeDuration       0.2

#define kDefaultLength      3
#define kDefaultPosition    @"bottom"

#define kImageWidth         80.0
#define kImageHeight        80.0

static NSString *kDurationKey = @"duration";


@interface UIView (ToastPrivate)

-(CGPoint)getPositionFor:(id)position toast:(UIView *)toast;
-(UIView *)makeViewForMessage:(NSString *)message title:(NSString *)title image:(UIImage *)image;

@end


@implementation UIView (Toast)

#pragma mark -
#pragma mark Toast Methods

-(void)makeToast:(NSString *)message {
    [self makeToast:message duration:kDefaultLength position:kDefaultPosition];
}

-(void)makeToast:(NSString *)message duration:(float)interval position:(id)point {
    UIView *toast = [self makeViewForMessage:message title:nil image:nil];
    [self showToast:toast duration:interval position:point];  
}

-(void)makeToast:(NSString *)message duration:(float)interval position:(id)point title:(NSString *)title {
    UIView *toast = [self makeViewForMessage:message title:title image:nil];
    [self showToast:toast duration:interval position:point];  
}

-(void)makeToast:(NSString *)message duration:(float)interval position:(id)point image:(UIImage *)image {
    UIView *toast = [self makeViewForMessage:message title:nil image:image];
    [self showToast:toast duration:interval position:point];  
}

-(void)makeToast:(NSString *)message duration:(float)interval  position:(id)point title:(NSString *)title image:(UIImage *)image {
    UIView *toast = [self makeViewForMessage:message title:title image:image];
    [self showToast:toast duration:interval position:point];  
}

-(void)showToast:(UIView *)toast {
    [self showToast:toast duration:kDefaultLength position:kDefaultPosition];
}

-(void)showToast:(UIView *)toast duration:(float)interval position:(id)point {
    
    /****************************************************
     *                                                  *
     * Displays a view for a given duration & position. *
     *                                                  *
     ****************************************************/
    
    CGPoint toastPoint = [self getPositionFor:point toast:toast];
    
    //use an associative reference to associate the toast view with the display interval
    objc_setAssociatedObject (toast, &kDurationKey, [NSNumber numberWithFloat:interval], OBJC_ASSOCIATION_RETAIN);
    
    [toast setCenter:toastPoint];
    [toast setAlpha:0.0];
    [self addSubview:toast];
    
    [UIView beginAnimations:@"fade_in" context:toast];
    [UIView setAnimationDuration:kFadeDuration];
    [UIView setAnimationDelegate:self];
    [UIView setAnimationDidStopSelector:@selector(animationDidStop:finished:context:)];
    [UIView setAnimationCurve:UIViewAnimationCurveEaseOut];
    [toast setAlpha:1.0];
    [UIView commitAnimations];
    
}

#pragma mark -
#pragma mark Animation Delegate Method

- (void)animationDidStop:(NSString*)animationID finished:(BOOL)finished context:(void *)context {
    
    UIView *toast = (UIView *)context;
    
    // retrieve the display interval associated with the view
    float interval = [(NSNumber *)objc_getAssociatedObject(toast, &kDurationKey) floatValue];
    
    if([animationID isEqualToString:@"fade_in"]) {
        
        [UIView beginAnimations:@"fade_out" context:toast];
        [UIView setAnimationDelay:interval];
        [UIView setAnimationDuration:kFadeDuration];
        [UIView setAnimationDelegate:self];
        [UIView setAnimationDidStopSelector:@selector(animationDidStop:finished:context:)];
        [UIView setAnimationCurve:UIViewAnimationCurveEaseIn];
        [toast setAlpha:0.0];
        [UIView commitAnimations];
        
    } else if ([animationID isEqualToString:@"fade_out"]) {
        
        [toast removeFromSuperview];
        
    }
    
}

#pragma mark -
#pragma mark Private Methods

-(CGPoint)getPositionFor:(id)point toast:(UIView *)toast {
    
    /*************************************************************************************
     *                                                                                   *
     * Converts string literals @"top", @"bottom", @"center", or any point wrapped in an *
     * NSValue object into a CGPoint                                                     *
     *                                                                                   *
     *************************************************************************************/
    
    if([point isKindOfClass:[NSString class]]) {
        
        if( [point caseInsensitiveCompare:@"top"] == NSOrderedSame ) {
            return CGPointMake(self.bounds.size.width/2, (toast.frame.size.height / 2) + kVerticalPadding);
        } else if( [point caseInsensitiveCompare:@"bottom"] == NSOrderedSame ) {
            return CGPointMake(self.bounds.size.width/2, (self.bounds.size.height - (toast.frame.size.height / 2)) - kVerticalPadding);
        } else if( [point caseInsensitiveCompare:@"center"] == NSOrderedSame ) {
            return CGPointMake(self.bounds.size.width / 2, self.bounds.size.height / 2);
        }
        
    } else if ([point isKindOfClass:[NSValue class]]) {
        return [point CGPointValue];
    }
    
    NSLog(@"Error: Invalid position for toast.");
    return [self getPositionFor:kDefaultPosition toast:toast];
}

-(UIView *)makeViewForMessage:(NSString *)message title:(NSString *)title image:(UIImage *)image {
    
    /***********************************************************************************
     *                                                                                 *
     * Dynamically build a toast view with any combination of message, title, & image. *
     *                                                                                 *
     ***********************************************************************************/
    
    if((message == nil) && (title == nil) && (image == nil)) return nil;

    UILabel *messageLabel = nil;
    UILabel *titleLabel = nil;
    UIImageView *imageView = nil;
    
    // create the parent view
    UIView *wrapperView = [[[UIView alloc] init] autorelease];
    [wrapperView.layer setCornerRadius:kCornerRadius];
    [wrapperView setBackgroundColor:[[UIColor blackColor] colorWithAlphaComponent:kOpacity]];
    wrapperView.autoresizingMask = UIViewAutoresizingFlexibleTopMargin | UIViewAutoresizingFlexibleLeftMargin | UIViewAutoresizingFlexibleRightMargin | UIViewAutoresizingFlexibleBottomMargin;

    if(image != nil) {
        imageView = [[[UIImageView alloc] initWithImage:image] autorelease];
        [imageView setContentMode:UIViewContentModeScaleAspectFit];
        [imageView setFrame:CGRectMake(kHorizontalPadding, kVerticalPadding, kImageWidth, kImageHeight)];
    }
    
    float imageWidth, imageHeight, imageLeft;
    
    // the imageView frame values will be used to size & position the other views
    if(imageView != nil) {
        imageWidth = imageView.bounds.size.width;
        imageHeight = imageView.bounds.size.height;
        imageLeft = kHorizontalPadding;
    } else {
        imageWidth = imageHeight = imageLeft = 0;
    }
    
    if (title != nil) {
        titleLabel = [[[UILabel alloc] init] autorelease];
        [titleLabel setNumberOfLines:kMaxTitleLines];
        [titleLabel setFont:[UIFont boldSystemFontOfSize:kFontSize]];
        [titleLabel setTextAlignment:UITextAlignmentLeft];
        [titleLabel setLineBreakMode:UILineBreakModeWordWrap];
        [titleLabel setTextColor:[UIColor whiteColor]];
        [titleLabel setBackgroundColor:[UIColor clearColor]];
        [titleLabel setAlpha:1.0];
        [titleLabel setText:title];
        
        // size the title label according to the length of the text
        CGSize maxSizeTitle = CGSizeMake((self.bounds.size.width * kMaxWidth) - imageWidth, self.bounds.size.height * kMaxHeight);
        CGSize expectedSizeTitle = [title sizeWithFont:titleLabel.font constrainedToSize:maxSizeTitle lineBreakMode:titleLabel.lineBreakMode]; 
        [titleLabel setFrame:CGRectMake(0, 0, expectedSizeTitle.width, expectedSizeTitle.height)];
    }
    
    if (message != nil) {
        messageLabel = [[[UILabel alloc] init] autorelease];
        [messageLabel setNumberOfLines:kMaxMessageLines];
        [messageLabel setFont:[UIFont systemFontOfSize:kFontSize]];
        [messageLabel setLineBreakMode:UILineBreakModeWordWrap];
        [messageLabel setTextColor:[UIColor whiteColor]];
        [messageLabel setTextAlignment:UITextAlignmentCenter];
        [messageLabel setBackgroundColor:[UIColor clearColor]];
        [messageLabel setAlpha:1.0];
        [messageLabel setText:message];
        
        // size the message label according to the length of the text
        CGSize maxSizeMessage = CGSizeMake((self.bounds.size.width * kMaxWidth) - imageWidth, self.bounds.size.height * kMaxHeight);
        CGSize expectedSizeMessage = [message sizeWithFont:messageLabel.font constrainedToSize:maxSizeMessage lineBreakMode:messageLabel.lineBreakMode]; 
        [messageLabel setFrame:CGRectMake(0, 0, expectedSizeMessage.width, expectedSizeMessage.height)];
    }
    
    // titleLabel frame values
    float titleWidth, titleHeight, titleTop, titleLeft;
    
    if(titleLabel != nil) {
        titleWidth = titleLabel.bounds.size.width;
        titleHeight = titleLabel.bounds.size.height;
        titleTop = kVerticalPadding;
        titleLeft = imageLeft + imageWidth + kHorizontalPadding;
    } else {
        titleWidth = titleHeight = titleTop = titleLeft = 0;
    }
    
    // messageLabel frame values
    float messageWidth, messageHeight, messageLeft, messageTop;

    if(messageLabel != nil) {
        messageWidth = messageLabel.bounds.size.width;
        messageHeight = messageLabel.bounds.size.height;
        messageLeft = imageLeft + imageWidth + kHorizontalPadding;
        messageTop = titleTop + titleHeight + kVerticalPadding;
    } else {
        messageWidth = messageHeight = messageLeft = messageTop = 0;
    }
    
    // compare the title & message widths and use the longer value to calculate the size of the wrapper width
    // the same logic applies to the x value (left)
    float longerWidth = (messageWidth < titleWidth) ? titleWidth : messageWidth;
    float longerLeft = (messageLeft < titleLeft) ? titleLeft : messageLeft;
    
    // if the image width is larger than longerWidth, use the image width to calculate the wrapper width.
    // the same logic applies to the wrapper height
    float wrapperWidth = ((longerLeft + longerWidth + kHorizontalPadding) < imageWidth + (kHorizontalPadding * 2)) ? imageWidth + (kHorizontalPadding * 2) : (longerLeft + longerWidth + kHorizontalPadding);
    float wrapperHeight = ((messageTop + messageHeight + kVerticalPadding) < imageHeight + (kVerticalPadding * 2)) ? imageHeight + (kVerticalPadding * 2) : (messageTop + messageHeight + kVerticalPadding);
                         
    [wrapperView setFrame:CGRectMake(0, 0, wrapperWidth, wrapperHeight)];
    
    if(titleLabel != nil) {
        [titleLabel setFrame:CGRectMake(titleLeft, titleTop, titleWidth, titleHeight)];
        [wrapperView addSubview:titleLabel];
    }
    
    if(messageLabel != nil) {
        [messageLabel setFrame:CGRectMake(messageLeft, messageTop, messageWidth, messageHeight)];
        [wrapperView addSubview:messageLabel];
    }
    
    if(imageView != nil) {
        [wrapperView addSubview:imageView];
    }
    
    return wrapperView;
}

@end
