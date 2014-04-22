/*
 RDP Session View Controller
 
 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#import <QuartzCore/QuartzCore.h>
#import "RDPSessionViewController.h"
#import "RDPKeyboard.h"
#import "Utils.h"
#import "Toast+UIView.h"
#import "ConnectionParams.h"
#import "CredentialsInputController.h"
#import "VerifyCertificateController.h"
#import "BlockAlertView.h"

#define TOOLBAR_HEIGHT 30

#define AUTOSCROLLDISTANCE 20
#define AUTOSCROLLTIMEOUT 0.05

@interface RDPSessionViewController (Private)
-(void)showSessionToolbar:(BOOL)show;
-(UIToolbar*)keyboardToolbar;
-(void)initGestureRecognizers;
- (void)suspendSession;
- (NSDictionary*)eventDescriptorForMouseEvent:(int)event position:(CGPoint)position;
- (void)handleMouseMoveForPosition:(CGPoint)position;
@end


@implementation RDPSessionViewController

#pragma mark class methods

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil session:(RDPSession *)session
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) 
    {
        _session = [session retain];
        [_session setDelegate:self];
        _session_initilized = NO;
        
        _mouse_move_events_skipped = 0;
        _mouse_move_event_timer = nil;

        _advanced_keyboard_view = nil;
        _advanced_keyboard_visible = NO;
        _requesting_advanced_keyboard = NO;
		_keyboard_last_height = 0;

        _session_toolbar_visible = NO;
        
        _toggle_mouse_button = NO;
        
        _autoscroll_with_touchpointer = [[NSUserDefaults standardUserDefaults] boolForKey:@"ui.auto_scroll_touchpointer"];
        _is_autoscrolling = NO;
        
        [UIView setAnimationDelegate:self];
        [UIView setAnimationDidStopSelector:@selector(animationStopped:finished:context:)];
    }
    
    return self;
}

// Implement loadView to create a view hierarchy programmatically, without using a nib.
- (void)loadView 
{
	// load default view and set background color and resizing mask
	[super loadView];

    // init keyboard handling vars
    _keyboard_visible = NO;

    // init keyboard toolbar
    _keyboard_toolbar = [[self keyboardToolbar] retain];
    [_dummy_textfield setInputAccessoryView:_keyboard_toolbar];
    
    // init gesture recognizers
    [self initGestureRecognizers];
    
    // hide session toolbar
    [_session_toolbar setFrame:CGRectMake(0.0, -TOOLBAR_HEIGHT, [[self view] bounds].size.width, TOOLBAR_HEIGHT)];
}


// Implement viewDidLoad to do additional setup after loading the view, typically from a nib.
- (void)viewDidLoad 
{
    [super viewDidLoad];
}


- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation {
    return YES;
}

- (void)didRotateFromInterfaceOrientation:(UIInterfaceOrientation)fromInterfaceOrientation
{
    if (![_touchpointer_view isHidden])
        [_touchpointer_view ensurePointerIsVisible];
}

- (void)didReceiveMemoryWarning {
    // Releases the view if it doesn't have a superview.
    [super didReceiveMemoryWarning];
    
    // Release any cached data, images, etc. that aren't in use.
}

- (void)viewDidUnload {
    [super viewDidUnload];
    // Release any retained subviews of the main view.
    // e.g. self.myOutlet = nil;
}

- (void)viewWillAppear:(BOOL)animated 
{
	[super viewWillAppear:animated];

    // hide navigation bar and (if enabled) the status bar
    if ([[NSUserDefaults standardUserDefaults] boolForKey:@"ui.hide_status_bar"])
    {
        if(animated == YES)
            [[UIApplication sharedApplication] setStatusBarHidden:YES withAnimation:UIStatusBarAnimationSlide];
        else
            [[UIApplication sharedApplication] setStatusBarHidden:YES withAnimation:UIStatusBarAnimationNone];
    }
    [[self navigationController] setNavigationBarHidden:YES animated:animated];        
	
    // if sesssion is suspended - notify that we got a new bitmap context
    if ([_session isSuspended]) 
        [self sessionBitmapContextWillChange:_session];

    // init keyboard
    [[RDPKeyboard getSharedRDPKeyboard] initWithSession:_session delegate:self];
}

- (void)viewDidAppear:(BOOL)animated
{
	[super viewDidAppear:animated];
        
    if (!_session_initilized)
    {
        if ([_session isSuspended]) 
        {
            [_session resume];
            [self sessionBitmapContextDidChange:_session];
            [_session_view setNeedsDisplay];
        }
        else
            [_session connect];
        
        _session_initilized = YES;
    }
}

- (void)viewWillDisappear:(BOOL)animated 
{
    [super viewWillDisappear:animated];

    // show navigation and status bar again
	if(animated == YES)
		[[UIApplication sharedApplication] setStatusBarHidden:NO withAnimation:UIStatusBarAnimationSlide];
	else
		[[UIApplication sharedApplication] setStatusBarHidden:NO withAnimation:UIStatusBarAnimationNone];
	[[self navigationController] setNavigationBarHidden:NO animated:animated];
	
    // reset all modifier keys on rdp keyboard
    [[RDPKeyboard getSharedRDPKeyboard] reset];
    
	// hide toolbar and keyboard
    [self showSessionToolbar:NO];
	[_dummy_textfield resignFirstResponder];
}


- (void)dealloc {
    // remove any observers
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    
    // the session lives on longer so set the delegate to nil
    [_session setDelegate:nil];

    [_advanced_keyboard_view release];
    [_keyboard_toolbar release];
    [_session release];
    [super dealloc];
}

#pragma mark -
#pragma mark ScrollView delegate methods

- (UIView *)viewForZoomingInScrollView:(UIScrollView *)scrollView
{	
    return _session_view;	
}

-(void)scrollViewDidEndZooming:(UIScrollView *)scrollView withView:(UIView *)view atScale:(float)scale
{
    NSLog(@"New zoom scale: %f", scale);
	[_session_view setNeedsDisplay];
}

#pragma mark -
#pragma mark TextField delegate methods
-(BOOL)textFieldShouldBeginEditing:(UITextField *)textField
{
	_keyboard_visible = YES;
    _advanced_keyboard_visible = NO;
	return YES;
}

-(BOOL)textFieldShouldEndEditing:(UITextField *)textField
{
	_keyboard_visible = NO;
    _advanced_keyboard_visible = NO;
	return YES;
}

- (BOOL)textField:(UITextField *)textField shouldChangeCharactersInRange:(NSRange)range replacementString:(NSString *)string
{	
	if([string length] > 0)
	{
		for(int i = 0 ; i < [string length] ; i++)
		{
            unichar curChar = [string characterAtIndex:i];

            // special handling for return/enter key
            if(curChar == '\n')
                [[RDPKeyboard getSharedRDPKeyboard] sendEnterKeyStroke];
            else
                [[RDPKeyboard getSharedRDPKeyboard] sendUnicode:curChar];
		}
	}
	else
	{
		[[RDPKeyboard getSharedRDPKeyboard] sendBackspaceKeyStroke];
	}		
	
	return NO;
}

#pragma mark -
#pragma mark AdvancedKeyboardDelegate functions
-(void)advancedKeyPressedVKey:(int)key
{
    [[RDPKeyboard getSharedRDPKeyboard] sendVirtualKeyCode:key];
}

-(void)advancedKeyPressedUnicode:(int)key
{
    [[RDPKeyboard getSharedRDPKeyboard] sendUnicode:key];
}

#pragma mark - RDP keyboard handler

- (void)modifiersChangedForKeyboard:(RDPKeyboard *)keyboard
{
    UIBarButtonItem* curItem;
    
    // shift button (only on iPad)   
    int objectIdx = 0;
    if (IsPad())
    {
        objectIdx = 2;
        curItem = (UIBarButtonItem*)[[_keyboard_toolbar items] objectAtIndex:objectIdx];
        [curItem setStyle:[keyboard shiftPressed] ? UIBarButtonItemStyleDone : UIBarButtonItemStyleBordered];
    }
    
    // ctrl button
    objectIdx += 2;
    curItem = (UIBarButtonItem*)[[_keyboard_toolbar items] objectAtIndex:objectIdx];
    [curItem setStyle:[keyboard ctrlPressed] ? UIBarButtonItemStyleDone : UIBarButtonItemStyleBordered];
    
    // win button
    objectIdx += 2;
    curItem = (UIBarButtonItem*)[[_keyboard_toolbar items] objectAtIndex:objectIdx];
    [curItem setStyle:[keyboard winPressed] ? UIBarButtonItemStyleDone : UIBarButtonItemStyleBordered];
    
    // alt button
    objectIdx += 2;
    curItem = (UIBarButtonItem*)[[_keyboard_toolbar items] objectAtIndex:objectIdx];
    [curItem setStyle:[keyboard altPressed] ? UIBarButtonItemStyleDone : UIBarButtonItemStyleBordered];    
}

#pragma mark -
#pragma mark RDPSessionDelegate functions

- (void)session:(RDPSession*)session didFailToConnect:(int)reason
{
    // remove and release connecting view
    [_connecting_indicator_view stopAnimating];
    [_connecting_view removeFromSuperview];
    [_connecting_view autorelease];          

    // return to bookmark list
    [[self navigationController] popViewControllerAnimated:YES];
}

- (void)sessionWillConnect:(RDPSession*)session
{
    // load connecting view
    [[NSBundle mainBundle] loadNibNamed:@"RDPConnectingView" owner:self options:nil];
    
    // set strings
    [_lbl_connecting setText:NSLocalizedString(@"Connecting", @"Connecting progress view - label")];
    [_cancel_connect_button setTitle:NSLocalizedString(@"Cancel", @"Cancel Button") forState:UIControlStateNormal];
    
    // center view and give it round corners
    [_connecting_view setCenter:[[self view] center]];
    [[_connecting_view layer] setCornerRadius:10];

    // display connecting view and start indicator
    [[self view] addSubview:_connecting_view];
    [_connecting_indicator_view startAnimating];
}

- (void)sessionDidConnect:(RDPSession*)session
{
    // register keyboard notification handlers
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(keyboardWillShow:) name: UIKeyboardWillShowNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(keyboardDidShow:) name: UIKeyboardDidShowNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(keyboardWillHide:) name: UIKeyboardWillHideNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(keyboardDidHide:) name: UIKeyboardDidHideNotification object:nil];

    // remove and release connecting view
    [_connecting_indicator_view stopAnimating];
    [_connecting_view removeFromSuperview];
    [_connecting_view autorelease];   
    
    // check if session settings changed ...
    // The 2nd width check is to ignore changes in resolution settings due to the RDVH display bug (refer to RDPSEssion.m for more details)
    ConnectionParams* orig_params = [session params];
    rdpSettings* sess_params = [session getSessionParams];
    if (([orig_params intForKey:@"width"] != sess_params->DesktopWidth && [orig_params intForKey:@"width"] != (sess_params->DesktopWidth + 1)) ||
        [orig_params intForKey:@"height"] != sess_params->DesktopHeight || [orig_params intForKey:@"colors"] != sess_params->ColorDepth)
    {
        // display notification that the session params have been changed by the server
        NSString* message = [NSString stringWithFormat:NSLocalizedString(@"The server changed the screen settings to %dx%dx%d", @"Screen settings not supported message with width, height and colors parameter"), sess_params->DesktopWidth, sess_params->DesktopHeight, sess_params->ColorDepth];
        [[self view] makeToast:message duration:ToastDurationNormal position:@"bottom"];        
    }
}

- (void)sessionWillDisconnect:(RDPSession*)session
{

}

- (void)sessionDidDisconnect:(RDPSession*)session
{
    // return to bookmark list
    [[self navigationController] popViewControllerAnimated:YES];    
}

- (void)sessionBitmapContextWillChange:(RDPSession*)session
{
    // calc new view frame
    rdpSettings* sess_params = [session getSessionParams];
    CGRect view_rect = CGRectMake(0, 0, sess_params->DesktopWidth, sess_params->DesktopHeight);

    // reset  zoom level and update content size
    [_session_scrollview setZoomScale:1.0];
    [_session_scrollview setContentSize:view_rect.size];

    // set session view size
    [_session_view setFrame:view_rect];
    
    // show/hide toolbar
    [_session setToolbarVisible:![[NSUserDefaults standardUserDefaults] boolForKey:@"ui.hide_tool_bar"]];
    [self showSessionToolbar:[_session toolbarVisible]];
}

- (void)sessionBitmapContextDidChange:(RDPSession*)session
{
    // associate view with session
    [_session_view setSession:session];

    // issue an update (this might be needed in case we had a resize for instance)
    [_session_view setNeedsDisplay];
}

- (void)session:(RDPSession*)session needsRedrawInRect:(CGRect)rect
{
    [_session_view setNeedsDisplayInRect:rect];
}

- (void)session:(RDPSession *)session requestsAuthenticationWithParams:(NSMutableDictionary *)params
{
    CredentialsInputController* view_controller = [[[CredentialsInputController alloc] initWithNibName:@"CredentialsInputView" bundle:nil session:_session params:params] autorelease];
    [self presentModalViewController:view_controller animated:YES];
}

- (void)session:(RDPSession *)session verifyCertificateWithParams:(NSMutableDictionary *)params
{
    VerifyCertificateController* view_controller = [[[VerifyCertificateController alloc] initWithNibName:@"VerifyCertificateView" bundle:nil session:_session params:params] autorelease];
    [self presentModalViewController:view_controller animated:YES];
}

- (CGSize)sizeForFitScreenForSession:(RDPSession*)session
{
    if (IsPad())
        return [self view].bounds.size;
    else
    {
        // on phones make a resolution that has a 16:10 ratio with the phone's height
        CGSize size = [self view].bounds.size;
        CGFloat maxSize = (size.width > size.height) ? size.width : size.height;
        return CGSizeMake(maxSize * 1.6f, maxSize);
    }
}

#pragma mark - Keyboard Toolbar Handlers

-(void)showAdvancedKeyboardAnimated
{
    // calc initial and final rect of the advanced keyboard view
    CGRect rect = [[_keyboard_toolbar superview] bounds];    
    rect.origin.y = [_keyboard_toolbar bounds].size.height;
    rect.size.height -= rect.origin.y;
    
    // create new view (hidden) and add to host-view of keyboard toolbar
    _advanced_keyboard_view = [[AdvancedKeyboardView alloc] initWithFrame:CGRectMake(rect.origin.x, 
                                                                                     [[_keyboard_toolbar superview] bounds].size.height, 
                                                                                     rect.size.width, rect.size.height) delegate:self];
    [[_keyboard_toolbar superview] addSubview:_advanced_keyboard_view];
    // we set autoresize to YES for the keyboard toolbar's superview so that our adv. keyboard view gets properly resized
    [[_keyboard_toolbar superview] setAutoresizesSubviews:YES];
    
    // show view with animation
    [UIView beginAnimations:nil context:NULL];
    [_advanced_keyboard_view setFrame:rect];
    [UIView commitAnimations]; 
}

-(IBAction)toggleKeyboardWhenOtherVisible:(id)sender
{       
    if(_advanced_keyboard_visible == NO)
    {
        [self showAdvancedKeyboardAnimated];
    }
    else
    {
        // hide existing view
        [UIView beginAnimations:@"hide_advanced_keyboard_view" context:NULL];
        CGRect rect = [_advanced_keyboard_view frame];
        rect.origin.y = [[_keyboard_toolbar superview] bounds].size.height;
        [_advanced_keyboard_view setFrame:rect];
        [UIView commitAnimations];        
        
        // the view is released in the animationDidStop selector registered in init
    }
    
    // toggle flag
    _advanced_keyboard_visible = !_advanced_keyboard_visible;
}

-(IBAction)toggleWinKey:(id)sender
{
    [[RDPKeyboard getSharedRDPKeyboard] toggleWinKey];
}

-(IBAction)toggleShiftKey:(id)sender
{
    [[RDPKeyboard getSharedRDPKeyboard] toggleShiftKey];
}

-(IBAction)toggleCtrlKey:(id)sender
{
    [[RDPKeyboard getSharedRDPKeyboard] toggleCtrlKey];
}

-(IBAction)toggleAltKey:(id)sender
{
    [[RDPKeyboard getSharedRDPKeyboard] toggleAltKey];
}

-(IBAction)pressEscKey:(id)sender
{
    [[RDPKeyboard getSharedRDPKeyboard] sendEscapeKeyStroke];
}

#pragma mark -
#pragma mark event handlers

- (void)animationStopped:(NSString*)animationID finished:(NSNumber*)finished context:(void*)context
{
    if ([animationID isEqualToString:@"hide_advanced_keyboard_view"])
    {
        // cleanup advanced keyboard view
        [_advanced_keyboard_view removeFromSuperview];
        [_advanced_keyboard_view autorelease];
        _advanced_keyboard_view = nil;        
    }
}

- (IBAction)switchSession:(id)sender
{
    [self suspendSession];
}

- (IBAction)toggleKeyboard:(id)sender
{
	if(!_keyboard_visible)
		[_dummy_textfield becomeFirstResponder];
	else
		[_dummy_textfield resignFirstResponder];
}

- (IBAction)toggleExtKeyboard:(id)sender
{
    // if the sys kb is shown but not the advanced kb then toggle the advanced kb
    if(_keyboard_visible && !_advanced_keyboard_visible)
        [self toggleKeyboardWhenOtherVisible:nil];
    else
    {
        // if not visible request the advanced keyboard view
        if(_advanced_keyboard_visible == NO)
            _requesting_advanced_keyboard = YES;
        [self toggleKeyboard:nil];        
    }    
}

- (IBAction)toggleTouchPointer:(id)sender
{
    BOOL toggle_visibilty = ![_touchpointer_view isHidden];
    [_touchpointer_view setHidden:toggle_visibilty];
    if(toggle_visibilty)
        [_session_scrollview setContentInset:UIEdgeInsetsZero];
    else
        [_session_scrollview setContentInset:[_touchpointer_view getEdgeInsets]];
}

- (IBAction)disconnectSession:(id)sender
{
    [_session disconnect];        
}


-(IBAction)cancelButtonPressed:(id)sender
{
    [_session disconnect];        
}

#pragma mark -
#pragma mark iOS Keyboard Notification Handlers

// the keyboard is given in a portrait frame of reference
- (BOOL)isLandscape {
	
	UIInterfaceOrientation ori = [[UIApplication sharedApplication] statusBarOrientation];
	return ( ori == UIInterfaceOrientationLandscapeLeft || ori == UIInterfaceOrientationLandscapeRight );
	
}

- (void)shiftKeyboard: (NSNotification*)notification {
	
	CGRect keyboardEndFrame = [[[notification userInfo] objectForKey:UIKeyboardFrameEndUserInfoKey] CGRectValue];
	
	CGFloat previousHeight = _keyboard_last_height;
	
	if( [self isLandscape] ) {
		// landscape has the keyboard based on x, so x can go negative
		_keyboard_last_height = keyboardEndFrame.size.width + keyboardEndFrame.origin.x;
	} else {
		// portrait has the keyboard based on the difference of the height and the frames y.
		CGFloat height = [[UIScreen mainScreen] bounds].size.height;
		_keyboard_last_height = height - keyboardEndFrame.origin.y;
	}
	
	CGFloat shiftHeight = _keyboard_last_height - previousHeight;
	
	[UIView beginAnimations:nil context:NULL];
    [UIView setAnimationCurve:[[[notification userInfo] objectForKey:UIKeyboardAnimationCurveUserInfoKey] intValue]];
    [UIView setAnimationDuration:[[[notification userInfo] objectForKey:UIKeyboardAnimationDurationUserInfoKey] doubleValue]];
	CGRect frame = [_session_scrollview frame];
	frame.size.height -= shiftHeight;
	[_session_scrollview setFrame:frame];
    [_touchpointer_view setFrame:frame];
	[UIView commitAnimations];
	
}

- (void)keyboardWillShow:(NSNotification *)notification
{
	[self shiftKeyboard: notification];
	
    [_touchpointer_view ensurePointerIsVisible];
}

- (void)keyboardDidShow:(NSNotification *)notification
{
    if(_requesting_advanced_keyboard)
    {
        [self showAdvancedKeyboardAnimated];
        _advanced_keyboard_visible = YES;
        _requesting_advanced_keyboard = NO;
    }            
}

- (void)keyboardWillHide:(NSNotification *)notification
{
	
	[self shiftKeyboard: notification];
	
}

- (void)keyboardDidHide:(NSNotification*)notification
{
    // release adanced keyboard view
    if(_advanced_keyboard_visible == YES)
    {
        _advanced_keyboard_visible = NO;
        [_advanced_keyboard_view removeFromSuperview];
        [_advanced_keyboard_view autorelease];
        _advanced_keyboard_view = nil;
    }    
}

#pragma mark -
#pragma mark Gesture handlers

- (void)handleSingleTap:(UITapGestureRecognizer*)gesture
{
	CGPoint pos = [gesture locationInView:_session_view];
    if (_toggle_mouse_button)
    {
        [_session sendInputEvent:[self eventDescriptorForMouseEvent:GetRightMouseButtonClickEvent(YES) position:pos]];	
        [_session sendInputEvent:[self eventDescriptorForMouseEvent:GetRightMouseButtonClickEvent(NO) position:pos]];	        
    }
    else
    {
        [_session sendInputEvent:[self eventDescriptorForMouseEvent:GetLeftMouseButtonClickEvent(YES) position:pos]];	
        [_session sendInputEvent:[self eventDescriptorForMouseEvent:GetLeftMouseButtonClickEvent(NO) position:pos]];	        
    }

    _toggle_mouse_button = NO;
}

- (void)handleDoubleTap:(UITapGestureRecognizer*)gesture
{
	CGPoint pos = [gesture locationInView:_session_view];	
    [_session sendInputEvent:[self eventDescriptorForMouseEvent:GetLeftMouseButtonClickEvent(YES) position:pos]];	
    [_session sendInputEvent:[self eventDescriptorForMouseEvent:GetLeftMouseButtonClickEvent(NO) position:pos]];	
    [_session sendInputEvent:[self eventDescriptorForMouseEvent:GetLeftMouseButtonClickEvent(YES) position:pos]];	
    [_session sendInputEvent:[self eventDescriptorForMouseEvent:GetLeftMouseButtonClickEvent(NO) position:pos]];	
    _toggle_mouse_button = NO;
}

- (void)handleLongPress:(UILongPressGestureRecognizer*)gesture
{    
	CGPoint pos = [gesture locationInView:_session_view];
    
	if([gesture state] == UIGestureRecognizerStateBegan) 
        [_session sendInputEvent:[self eventDescriptorForMouseEvent:GetLeftMouseButtonClickEvent(YES) position:pos]];	
	else if([gesture state] == UIGestureRecognizerStateChanged)
        [self handleMouseMoveForPosition:pos];
	else if([gesture state] == UIGestureRecognizerStateEnded)
        [_session sendInputEvent:[self eventDescriptorForMouseEvent:GetLeftMouseButtonClickEvent(NO) position:pos]];	
}


- (void)handleDoubleLongPress:(UILongPressGestureRecognizer*)gesture
{
    // this point is mapped against the scroll view because we want to have relative movement to the screen/scrollview
	CGPoint pos = [gesture locationInView:_session_scrollview];
    
	if([gesture state] == UIGestureRecognizerStateBegan) 
        _prev_long_press_position = pos;
	else if([gesture state] == UIGestureRecognizerStateChanged)
    {
        int delta = _prev_long_press_position.y - pos.y;
        
        if(delta > GetScrollGestureDelta())
        {
            [_session sendInputEvent:[self eventDescriptorForMouseEvent:GetMouseWheelEvent(YES) position:pos]];	
            _prev_long_press_position = pos;
        }
        else if(delta < -GetScrollGestureDelta())
        {            
            [_session sendInputEvent:[self eventDescriptorForMouseEvent:GetMouseWheelEvent(NO) position:pos]];	
            _prev_long_press_position = pos;
        }
    }
}

-(void)handleSingle2FingersTap:(UITapGestureRecognizer*)gesture
{
    _toggle_mouse_button = !_toggle_mouse_button;
}

-(void)handleSingle3FingersTap:(UITapGestureRecognizer*)gesture
{
    [_session setToolbarVisible:![_session toolbarVisible]];
    [self showSessionToolbar:[_session toolbarVisible]];
}

#pragma mark -
#pragma mark Touch Pointer delegates
// callback if touch pointer should be closed
-(void)touchPointerClose
{
    [self toggleTouchPointer:nil];
}

// callback for a left click action
-(void)touchPointerLeftClick:(CGPoint)pos down:(BOOL)down
{
    CGPoint session_view_pos = [_touchpointer_view convertPoint:pos toView:_session_view];
    [_session sendInputEvent:[self eventDescriptorForMouseEvent:GetLeftMouseButtonClickEvent(down) position:session_view_pos]];	
}

// callback for a right click action
-(void)touchPointerRightClick:(CGPoint)pos down:(BOOL)down
{
    CGPoint session_view_pos = [_touchpointer_view convertPoint:pos toView:_session_view];
    [_session sendInputEvent:[self eventDescriptorForMouseEvent:GetRightMouseButtonClickEvent(down) position:session_view_pos]];
}

- (void)doAutoScrolling
{
    int scrollX = 0;
    int scrollY = 0;
    CGPoint curPointerPos = [_touchpointer_view getPointerPosition];
    CGRect viewBounds = [_touchpointer_view bounds];
    CGRect scrollBounds = [_session_view bounds];

    // add content insets to scroll bounds
    scrollBounds.size.width += [_session_scrollview contentInset].right;
    scrollBounds.size.height += [_session_scrollview contentInset].bottom;
    
    // add zoom factor
    scrollBounds.size.width *= [_session_scrollview zoomScale];
    scrollBounds.size.height *= [_session_scrollview zoomScale];
    
    if (curPointerPos.x > (viewBounds.size.width - [_touchpointer_view getPointerWidth]))
        scrollX = AUTOSCROLLDISTANCE;
    else if (curPointerPos.x < 0)
        scrollX = -AUTOSCROLLDISTANCE;

    if (curPointerPos.y > (viewBounds.size.height - [_touchpointer_view getPointerHeight]))
        scrollY = AUTOSCROLLDISTANCE;
    else if (curPointerPos.y < (_session_toolbar_visible ? TOOLBAR_HEIGHT : 0))
        scrollY = -AUTOSCROLLDISTANCE;

    CGPoint newOffset = [_session_scrollview contentOffset];
    newOffset.x += scrollX;
    newOffset.y += scrollY;

    // if offset is going off screen - stop scrolling in that direction
    if (newOffset.x < 0)
    {
        scrollX = 0;
        newOffset.x = 0;
    }
    else if (newOffset.x > (scrollBounds.size.width - viewBounds.size.width))
    {
        scrollX = 0;
        newOffset.x = MAX(scrollBounds.size.width - viewBounds.size.width, 0);
    }
    if (newOffset.y < 0)
    {
        scrollY = 0;
        newOffset.y = 0;
    }
    else if (newOffset.y > (scrollBounds.size.height - viewBounds.size.height))
    {
        scrollY = 0;
        newOffset.y = MAX(scrollBounds.size.height - viewBounds.size.height, 0);
    }

    // perform scrolling
    [_session_scrollview setContentOffset:newOffset];

    // continue scrolling?
    if (scrollX != 0 || scrollY != 0)
        [self performSelector:@selector(doAutoScrolling) withObject:nil afterDelay:AUTOSCROLLTIMEOUT];
    else
        _is_autoscrolling = NO;    
}

// callback for a right click action
-(void)touchPointerMove:(CGPoint)pos
{
    CGPoint session_view_pos = [_touchpointer_view convertPoint:pos toView:_session_view];
    [self handleMouseMoveForPosition:session_view_pos];
    
    if (_autoscroll_with_touchpointer && !_is_autoscrolling)
    {
        _is_autoscrolling = YES;
        [self performSelector:@selector(doAutoScrolling) withObject:nil afterDelay:AUTOSCROLLTIMEOUT];
    }
}

// callback if scrolling is performed
-(void)touchPointerScrollDown:(BOOL)down
{   
    [_session sendInputEvent:[self eventDescriptorForMouseEvent:GetMouseWheelEvent(down) position:CGPointZero]];
}

// callback for toggling the standard keyboard
-(void)touchPointerToggleKeyboard
{
    if(_advanced_keyboard_visible)
        [self toggleKeyboardWhenOtherVisible:nil];
    else
        [self toggleKeyboard:nil];
}

// callback for toggling the extended keyboard
-(void)touchPointerToggleExtendedKeyboard
{
    [self toggleExtKeyboard:nil];
}

// callback for reset view
-(void)touchPointerResetSessionView
{
    [_session_scrollview setZoomScale:1.0 animated:YES];
}

@end


@implementation RDPSessionViewController (Private)

#pragma mark -
#pragma mark Helper functions

-(void)showSessionToolbar:(BOOL)show
{
    // already shown or hidden?
    if (_session_toolbar_visible == show)
        return;
    
    if(show)
    {
        [UIView beginAnimations:@"showToolbar" context:nil];
        [UIView setAnimationDuration:.4];
        [UIView setAnimationCurve:UIViewAnimationCurveLinear];
        [_session_toolbar setFrame:CGRectMake(0.0, 0.0, [[self view] bounds].size.width, TOOLBAR_HEIGHT)];
        [UIView commitAnimations];		
        _session_toolbar_visible = YES;        
    }
    else
    {
        [UIView beginAnimations:@"hideToolbar" context:nil];
        [UIView setAnimationDuration:.4];
        [UIView setAnimationCurve:UIViewAnimationCurveLinear];
        [_session_toolbar setFrame:CGRectMake(0.0, -TOOLBAR_HEIGHT, [[self view] bounds].size.width, TOOLBAR_HEIGHT)];
        [UIView commitAnimations];		
        _session_toolbar_visible = NO;
    }
}

-(UIToolbar*)keyboardToolbar
{
	UIToolbar* keyboard_toolbar = [[[UIToolbar alloc] initWithFrame:CGRectNull] autorelease];
	[keyboard_toolbar setBarStyle:UIBarStyleBlackOpaque];
    
	UIBarButtonItem* esc_btn = [[[UIBarButtonItem alloc] initWithTitle:@"Esc" style:UIBarButtonItemStyleBordered target:self action:@selector(pressEscKey:)] autorelease];
    UIImage* win_icon = [UIImage imageWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"toolbar_icon_win" ofType:@"png"]];
	UIBarButtonItem* win_btn = [[[UIBarButtonItem alloc] initWithImage:win_icon style:UIBarButtonItemStyleBordered target:self action:@selector(toggleWinKey:)] autorelease];
	UIBarButtonItem* ctrl_btn = [[[UIBarButtonItem alloc] initWithTitle:@"Ctrl" style:UIBarButtonItemStyleBordered target:self action:@selector(toggleCtrlKey:)] autorelease];
	UIBarButtonItem* alt_btn = [[[UIBarButtonItem alloc] initWithTitle:@"Alt" style:UIBarButtonItemStyleBordered target:self action:@selector(toggleAltKey:)] autorelease];
	UIBarButtonItem* ext_btn = [[[UIBarButtonItem alloc] initWithTitle:@"Ext" style:UIBarButtonItemStyleBordered target:self action:@selector(toggleKeyboardWhenOtherVisible:)] autorelease];
	UIBarButtonItem* done_btn = [[[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone target:self action:@selector(toggleKeyboard:)] autorelease];
	UIBarButtonItem* flex_spacer = [[[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace target:nil action:nil] autorelease];
    
    // iPad gets a shift button, iphone doesn't (there's just not enough space ...)
    NSArray* items;
    if(IsPad())
    {
        UIBarButtonItem* shift_btn = [[[UIBarButtonItem alloc] initWithTitle:@"Shift" style:UIBarButtonItemStyleBordered target:self action:@selector(toggleShiftKey:)] autorelease];
        items = [NSArray arrayWithObjects:esc_btn, flex_spacer, 
                 shift_btn, flex_spacer, 
                 ctrl_btn, flex_spacer, 
                 win_btn, flex_spacer, 
                 alt_btn, flex_spacer, 
                 ext_btn, flex_spacer, done_btn, nil];
    }
    else
    {
        items = [NSArray arrayWithObjects:esc_btn, flex_spacer, ctrl_btn, flex_spacer, win_btn, flex_spacer, alt_btn, flex_spacer, ext_btn, flex_spacer, done_btn, nil];        
    }
    
	[keyboard_toolbar setItems:items];
    [keyboard_toolbar sizeToFit];
    return keyboard_toolbar;
}

- (void)initGestureRecognizers
{        
	// single and double tap recognizer 
    UITapGestureRecognizer* doubleTapRecognizer = [[[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(handleDoubleTap:)] autorelease];
    [doubleTapRecognizer setNumberOfTouchesRequired:1];
	[doubleTapRecognizer setNumberOfTapsRequired:2];	
    
	UITapGestureRecognizer* singleTapRecognizer = [[[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(handleSingleTap:)] autorelease];
	[singleTapRecognizer requireGestureRecognizerToFail:doubleTapRecognizer];	
    [singleTapRecognizer setNumberOfTouchesRequired:1];
	[singleTapRecognizer setNumberOfTapsRequired:1];
    
    // 2 fingers - tap recognizer 
	UITapGestureRecognizer* single2FingersTapRecognizer = [[[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(handleSingle2FingersTap:)] autorelease];
    [single2FingersTapRecognizer setNumberOfTouchesRequired:2];
	[single2FingersTapRecognizer setNumberOfTapsRequired:1];
    
	// long press gesture recognizer
	UILongPressGestureRecognizer* longPressRecognizer = [[[UILongPressGestureRecognizer alloc] initWithTarget:self action:@selector(handleLongPress:)] autorelease];
	[longPressRecognizer setMinimumPressDuration:0.5];
    
    // double long press gesture recognizer
	UILongPressGestureRecognizer* doubleLongPressRecognizer = [[[UILongPressGestureRecognizer alloc] initWithTarget:self action:@selector(handleDoubleLongPress:)] autorelease];
    [doubleLongPressRecognizer setNumberOfTouchesRequired:2];
	[doubleLongPressRecognizer setMinimumPressDuration:0.5];
    
    // 3 finger, single tap gesture for showing/hiding the toolbar
    UITapGestureRecognizer* single3FingersTapRecognizer = [[[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(handleSingle3FingersTap:)] autorelease];
    [single3FingersTapRecognizer setNumberOfTapsRequired:1];
    [single3FingersTapRecognizer setNumberOfTouchesRequired:3];
    
    // add gestures to scroll view
	[_session_scrollview addGestureRecognizer:singleTapRecognizer];
	[_session_scrollview addGestureRecognizer:doubleTapRecognizer];
	[_session_scrollview addGestureRecognizer:single2FingersTapRecognizer];
	[_session_scrollview addGestureRecognizer:longPressRecognizer];
	[_session_scrollview addGestureRecognizer:doubleLongPressRecognizer];
    [_session_scrollview addGestureRecognizer:single3FingersTapRecognizer];
}

- (void)suspendSession
{
	// suspend session and pop navigation controller
    [_session suspend];
    
    // pop current view controller
    [[self navigationController] popViewControllerAnimated:YES];
}

- (NSDictionary*)eventDescriptorForMouseEvent:(int)event position:(CGPoint)position
{
    return [NSDictionary dictionaryWithObjectsAndKeys:	
                        @"mouse", @"type",
                        [NSNumber numberWithUnsignedShort:event], @"flags",
                        [NSNumber numberWithUnsignedShort:lrintf(position.x)], @"coord_x",
                        [NSNumber numberWithUnsignedShort:lrintf(position.y)], @"coord_y",
                        nil];
}

- (void)sendDelayedMouseEventWithTimer:(NSTimer*)timer
{
    _mouse_move_event_timer = nil;
    NSDictionary* event = [timer userInfo];
    [_session sendInputEvent:event];
    [timer autorelease];
}

- (void)handleMouseMoveForPosition:(CGPoint)position
{
    NSDictionary* event = [self eventDescriptorForMouseEvent:PTR_FLAGS_MOVE position:position];
    
    // cancel pending mouse move events
    [_mouse_move_event_timer invalidate];
    _mouse_move_events_skipped++;
    
    if (_mouse_move_events_skipped >= 5)
    {
        [_session sendInputEvent:event];
        _mouse_move_events_skipped = 0;
    }
    else
    {
        [_mouse_move_event_timer autorelease];
        _mouse_move_event_timer = [[NSTimer scheduledTimerWithTimeInterval:0.05 target:self selector:@selector(sendDelayedMouseEventWithTimer:) userInfo:event repeats:NO] retain];        
    }    
}

@end
