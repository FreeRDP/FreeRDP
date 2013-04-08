//
//  BlockAlertView.m
//
//

#import "BlockAlertView.h"
#import "BlockBackground.h"

@implementation BlockAlertView

@synthesize view = _view;

static UIImage *background = nil;
static UIFont *titleFont = nil;
static UIFont *messageFont = nil;
static UIFont *buttonFont = nil;

#define kBounce         20
#define kBorder         10
#define kButtonHeight   44

#define kAlertFontColor    [UIColor colorWithWhite:244.0/255.0 alpha:1.0]

#define kAlertViewBackground   @"alert-window.png"
#define kAlertViewBackgroundCapHeight  38

#pragma mark - init

+ (void)initialize
{
    if (self == [BlockAlertView class])
    {
        background = [UIImage imageNamed:kAlertViewBackground];
        background = [[background stretchableImageWithLeftCapWidth:0 topCapHeight:kAlertViewBackgroundCapHeight] retain];
        titleFont = [[UIFont boldSystemFontOfSize:20] retain];
        messageFont = [[UIFont systemFontOfSize:18] retain];
        buttonFont = [[UIFont boldSystemFontOfSize:18] retain];
    }
}

+ (BlockAlertView *)alertWithTitle:(NSString *)title message:(NSString *)message
{
    return [[[BlockAlertView alloc] initWithTitle:title message:message] autorelease];
}

///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark - NSObject

- (id)initWithTitle:(NSString *)title message:(NSString *)message 
{
    if ((self = [super init]))
    {
        BlockBackground* blockBackground = [BlockBackground sharedInstance];
        [blockBackground sizeToFill];
        [self setViewTransform:blockBackground forOrientation:blockBackground.orientation];

        CGRect frame = blockBackground.bounds;
        frame.origin.x = (frame.size.width - background.size.width) * 0.5;
        frame.size.width = background.size.width;
        
        _view = [[UIView alloc] initWithFrame:frame];
        _blocks = [[NSMutableArray alloc] init];
        _height = kBorder + 15;

        if (title)
        {
            CGSize size = [title sizeWithFont:titleFont
                            constrainedToSize:CGSizeMake(frame.size.width-kBorder*2, 1000)
                                lineBreakMode:NSLineBreakByWordWrapping];

            UILabel *labelView = [[UILabel alloc] initWithFrame:CGRectMake(kBorder, _height, frame.size.width-kBorder*2, size.height)];
            labelView.font = titleFont;
            labelView.numberOfLines = 0;
            labelView.lineBreakMode = NSLineBreakByWordWrapping;
            labelView.textColor = kAlertFontColor;
            labelView.backgroundColor = [UIColor clearColor];
            labelView.textAlignment = NSTextAlignmentCenter;
            labelView.shadowColor = [UIColor blackColor];
            labelView.shadowOffset = CGSizeMake(0, -1);
            labelView.text = title;
            [_view addSubview:labelView];
            [labelView release];
            
            _height += size.height + kBorder;
        }
        
        if (message)
        {
            CGSize size = [message sizeWithFont:messageFont
                              constrainedToSize:CGSizeMake(frame.size.width-kBorder*2, 1000)
                                  lineBreakMode:NSLineBreakByWordWrapping];
            
            UILabel *labelView = [[UILabel alloc] initWithFrame:CGRectMake(kBorder, _height, frame.size.width-kBorder*2, size.height)];
            labelView.font = messageFont;
            labelView.numberOfLines = 0;
            labelView.lineBreakMode = NSLineBreakByWordWrapping;
            labelView.textColor = kAlertFontColor;
            labelView.backgroundColor = [UIColor clearColor];
            labelView.textAlignment = NSTextAlignmentCenter;
            labelView.shadowColor = [UIColor blackColor];
            labelView.shadowOffset = CGSizeMake(0, -1);
            labelView.text = message;
            [_view addSubview:labelView];
            [labelView release];
            
            _height += size.height + kBorder;
        }
    }
    
    return self;
}

- (void)dealloc 
{
    [_view release];
    [_blocks release];
    [super dealloc];
}

- (void)setViewTransform:(UIView*)view forOrientation:(UIInterfaceOrientation)orientation
{
    switch(orientation)
    {
        case UIInterfaceOrientationPortrait:
            view.transform = CGAffineTransformMakeRotation(0);
            break;
            
        case UIInterfaceOrientationPortraitUpsideDown:
            view.transform = CGAffineTransformMakeRotation((-2) *M_PI/2);
            break;
            
        case UIInterfaceOrientationLandscapeRight:
            view.transform = CGAffineTransformMakeRotation(M_PI/2);
            break;
            
        case UIInterfaceOrientationLandscapeLeft:
            view.transform = CGAffineTransformMakeRotation((-1) * M_PI/2);
            break;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark - Public

- (void)addButtonWithTitle:(NSString *)title color:(NSString*)color block:(void (^)())block 
{
    [_blocks addObject:[NSArray arrayWithObjects:
                        block ? [[block copy] autorelease] : [NSNull null],
                        title,
                        color,
                        nil]];
}

- (void)addButtonWithTitle:(NSString *)title block:(void (^)())block 
{
    [self addButtonWithTitle:title color:@"gray" block:block];
}

- (void)setCancelButtonWithTitle:(NSString *)title block:(void (^)())block 
{
    [self addButtonWithTitle:title color:@"black" block:block];
}

- (void)setDestructiveButtonWithTitle:(NSString *)title block:(void (^)())block
{
    [self addButtonWithTitle:title color:@"red" block:block];
}

- (void)show
{
    BOOL isSecondButton = NO;
    NSUInteger index = 0;
    for (NSUInteger i = 0; i < _blocks.count; i++)
    {
        NSArray *block = [_blocks objectAtIndex:i];
        NSString *title = [block objectAtIndex:1];
        NSString *color = [block objectAtIndex:2];

        UIImage *image = [UIImage imageNamed:[NSString stringWithFormat:@"alert-%@-button.png", color]];
        image = [image stretchableImageWithLeftCapWidth:(int)(image.size.width+1)>>1 topCapHeight:0];
        
        CGFloat maxHalfWidth = floorf((_view.bounds.size.width-kBorder*3)*0.5);
        CGFloat width = _view.bounds.size.width-kBorder*2;
        CGFloat xOffset = kBorder;
        if (isSecondButton)
        {
            width = maxHalfWidth;
            xOffset = width + kBorder * 2;
            isSecondButton = NO;
        }
        else if (i + 1 < _blocks.count)
        {
            // In this case there's another button.
            // Let's check if they fit on the same line.
            CGSize size = [title sizeWithFont:buttonFont 
                                  minFontSize:10 
                               actualFontSize:nil
                                     forWidth:_view.bounds.size.width-kBorder*2 
                                lineBreakMode:NSLineBreakByClipping];
            
            if (size.width < maxHalfWidth - kBorder)
            {
                // It might fit. Check the next Button
                NSArray *block2 = [_blocks objectAtIndex:i+1];
                NSString *title2 = [block2 objectAtIndex:1];
                size = [title2 sizeWithFont:buttonFont 
                                minFontSize:10 
                             actualFontSize:nil
                                   forWidth:_view.bounds.size.width-kBorder*2 
                              lineBreakMode:NSLineBreakByClipping];
                
                if (size.width < maxHalfWidth - kBorder)
                {
                    // They'll fit!
                    isSecondButton = YES;  // For the next iteration
                    width = maxHalfWidth;
                }
            }
        }
        
        UIButton *button = [UIButton buttonWithType:UIButtonTypeCustom];
        button.frame = CGRectMake(xOffset, _height, width, kButtonHeight);
        button.titleLabel.font = buttonFont;
        if ([button.titleLabel respondsToSelector:@selector(setMinimumScaleFactor:)])
            button.titleLabel.minimumScaleFactor = 10;
        button.titleLabel.textAlignment = NSTextAlignmentCenter;
        button.titleLabel.shadowOffset = CGSizeMake(0, -1);
        button.backgroundColor = [UIColor clearColor];
        button.tag = i+1;
        
        [button setBackgroundImage:image forState:UIControlStateNormal];
        [button setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
        [button setTitleShadowColor:[UIColor blackColor] forState:UIControlStateNormal];
        [button setTitle:title forState:UIControlStateNormal];
        button.accessibilityLabel = title;
        
        [button addTarget:self action:@selector(buttonClicked:) forControlEvents:UIControlEventTouchUpInside];
        
        [_view addSubview:button];
        
        if (!isSecondButton)
            _height += kButtonHeight + kBorder;
        
        index++;
    }

    CGRect frame = _view.frame;
    frame.origin.y = - _height;
    frame.size.height = _height;
    _view.frame = frame;
    
    UIImageView *modalBackground = [[UIImageView alloc] initWithFrame:_view.bounds];
    modalBackground.image = background;
    modalBackground.contentMode = UIViewContentModeScaleToFill;
    [_view insertSubview:modalBackground atIndex:0];
    [modalBackground release];
    
    [[BlockBackground sharedInstance] addToMainWindow:_view];

    __block CGPoint center = _view.center;
    center.y = floorf([BlockBackground sharedInstance].bounds.size.height * 0.5) + kBounce;
    
    [UIView animateWithDuration:0.4
                          delay:0.0
                        options:UIViewAnimationOptionCurveEaseOut
                     animations:^{
                         [BlockBackground sharedInstance].alpha = 1.0f;
                         _view.center = center;
                     } 
                     completion:^(BOOL finished) {
                         [UIView animateWithDuration:0.1
                                               delay:0.0
                                             options:0
                                          animations:^{
                                              center.y -= kBounce;
                                              _view.center = center;
                                          } 
                                          completion:nil];
                     }];
    
    [self retain];
}

- (void)dismissWithClickedButtonIndex:(NSInteger)buttonIndex animated:(BOOL)animated 
{
    if (buttonIndex >= 0 && buttonIndex < [_blocks count])
    {
        id obj = [[_blocks objectAtIndex: buttonIndex] objectAtIndex:0];
        if (![obj isEqual:[NSNull null]])
        {
            ((void (^)())obj)();
        }
    }
    
    if (animated)
    {
        [UIView animateWithDuration:0.1
                              delay:0.0
                            options:0
                         animations:^{
                             CGPoint center = _view.center;
                             center.y += 20;
                             _view.center = center;
                         } 
                         completion:^(BOOL finished) {
                             [UIView animateWithDuration:0.4
                                                   delay:0.0 
                                                 options:UIViewAnimationOptionCurveEaseIn
                                              animations:^{
                                                  CGRect frame = _view.frame;
                                                  frame.origin.y = -frame.size.height;
                                                  _view.frame = frame;
                                                  [[BlockBackground sharedInstance] reduceAlphaIfEmpty];
                                              } 
                                              completion:^(BOOL finished) {
                                                  [[BlockBackground sharedInstance] removeView:_view];
                                                  [_view release]; _view = nil;
                                                  [self autorelease];
                                              }];
                         }];
    }
    else
    {
        [[BlockBackground sharedInstance] removeView:_view];
        [_view release]; _view = nil;
        [self autorelease];
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark - Action

- (void)buttonClicked:(id)sender 
{
    /* Run the button's block */
    int buttonIndex = [sender tag] - 1;
    [self dismissWithClickedButtonIndex:buttonIndex animated:YES];
}

@end
