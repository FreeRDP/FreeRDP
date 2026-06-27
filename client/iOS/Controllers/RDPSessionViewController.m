/*
 RDP Session View Controller

 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import <QuartzCore/QuartzCore.h>
#import <GameController/GameController.h>
#import <objc/runtime.h>
#import "RDPSessionViewController.h"
#import "RDPSessionToolbar.h"
#import "RDPKeyboard.h"
#import "Utils.h"
#import "Toast+UIView.h"
#import "ConnectionParams.h"
#import "CredentialsInputController.h"
#import "VerifyCertificateController.h"
#import "BlockAlertView.h"

#define TOOLBAR_HEIGHT 44

@interface RDPSessionViewController (Private)
- (void)showSessionToolbar:(BOOL)show;
- (UIToolbar *)keyboardToolbar;
- (void)initGestureRecognizers;
- (void)suspendSession;
- (void)fitSessionViewToViewport;
- (void)centerSessionViewInViewport;
- (CGPoint)remotePositionForSessionViewPosition:(CGPoint)position;
- (CGPoint)sessionViewPositionForRemotePosition:(CGPoint)position;
- (CGPoint)clampedSessionViewCursorPosition:(CGPoint)position;
- (NSDictionary *)eventDescriptorForMouseEvent:(int)event position:(CGPoint)position;
- (void)handleMouseMoveForPosition:(CGPoint)position;
- (CGPoint)currentCursorViewPosition;
- (void)moveCursorByViewportDelta:(CGPoint)delta;
- (void)moveCursorToSessionViewPosition:(CGPoint)position;
- (void)sendMouseButtonEvent:(int)event;
@end

@implementation RDPSessionViewController

#pragma mark class methods

- (id)initWithNibName:(NSString *)nibNameOrNil
               bundle:(NSBundle *)nibBundleOrNil
              session:(RDPSession *)session
{
	self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
	if (self)
	{
		_session = [session retain];
		[_session setDelegate:self];
		_session_initilized = NO;

		_advanced_keyboard_view = nil;
		_advanced_keyboard_visible = NO;
		_requesting_advanced_keyboard = NO;
		_last_session_viewport_size = CGSizeZero;

		_session_toolbar_visible = NO;

		_cursor_view_position = CGPointZero;
		_last_mouse_pan_location = CGPointZero;
		_has_cursor_view_position = NO;
		_has_user_moved_cursor = NO;
		_mouse_pan_active = NO;
		_long_press_active = NO;
		_mouse_drag_active = NO;
		_pointer_is_indirect = NO;

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

	// let pointer input pass through the session toolbar to the remote session
	object_setClass(_session_toolbar, [RDPSessionToolbar class]);
	[(RDPSessionToolbar *)_session_toolbar setPassthroughView:_session_scrollview];

	// init keyboard handling vars
	_keyboard_visible = NO;

	// init keyboard toolbar
	_keyboard_toolbar = [[self keyboardToolbar] retain];
	[_dummy_textfield setInputAccessoryView:_keyboard_toolbar];

	// init gesture recognizers
	[self initGestureRecognizers];

	// hide session toolbar
	[_session_toolbar
	    setFrame:CGRectMake(0.0, -TOOLBAR_HEIGHT, [[self view] bounds].size.width, TOOLBAR_HEIGHT)];
}

// Implement viewDidLoad to do additional setup after loading the view, typically from a nib.
- (void)viewDidLoad
{
	[super viewDidLoad];

	[_session_scrollview setContentInsetAdjustmentBehavior:UIScrollViewContentInsetAdjustmentNever];
	[_session_scrollview setContentInset:UIEdgeInsetsZero];
	[_session_scrollview setScrollIndicatorInsets:UIEdgeInsetsZero];
	[_session_scrollview setShowsHorizontalScrollIndicator:NO];
	[_session_scrollview setShowsVerticalScrollIndicator:NO];
	[_session_scrollview setAlwaysBounceHorizontal:NO];
	[_session_scrollview setAlwaysBounceVertical:NO];
	[_session_scrollview setBounces:NO];
}

- (void)viewDidLayoutSubviews
{
	[super viewDidLayoutSubviews];

	CGRect viewportFrame = [[self view] bounds];
	[_session_scrollview setFrame:viewportFrame];

	CGSize viewportSize = [_session_scrollview bounds].size;
	if (!CGSizeEqualToSize(viewportSize, _last_session_viewport_size))
		[self fitSessionViewToViewport];
	else
		[self centerSessionViewInViewport];
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
	return YES;
}

- (BOOL)prefersStatusBarHidden
{
	return YES;
}

- (UIStatusBarAnimation)preferredStatusBarUpdateAnimation
{
	return UIStatusBarAnimationSlide;
}

- (void)didRotateFromInterfaceOrientation:(UIInterfaceOrientation)fromInterfaceOrientation
{
	(void)fromInterfaceOrientation;
	[self centerSessionViewInViewport];
}

- (void)didReceiveMemoryWarning
{
	// Releases the view if it doesn't have a superview.
	[super didReceiveMemoryWarning];

	// Release any cached data, images, etc. that aren't in use.
}

- (void)viewDidUnload
{
	[super viewDidUnload];
	// Release any retained subviews of the main view.
	// e.g. self.myOutlet = nil;
}

- (void)viewWillAppear:(BOOL)animated
{
	[super viewWillAppear:animated];

	// remote screen always fit in device screen
	[self setNeedsStatusBarAppearanceUpdate];
	[[self navigationController] setNeedsStatusBarAppearanceUpdate];
	[[self navigationController] setNavigationBarHidden:YES animated:animated];
	if (@available(iOS 18.0, *))
		[[self tabBarController] setTabBarHidden:YES animated:animated];
	else
		[[[self tabBarController] tabBar] setHidden:YES];

	// if session is suspended - notify that we got a new bitmap context
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
		}
		else
			[_session connect];

		_session_initilized = YES;
	}
}

- (void)viewWillDisappear:(BOOL)animated
{
	[super viewWillDisappear:animated];
	if (_mouse_drag_active)
		[self sendMouseButtonEvent:GetLeftMouseButtonClickEvent(NO)];
	_mouse_drag_active = NO;
	_mouse_pan_active = NO;
	_long_press_active = NO;

	[[self navigationController] setNeedsStatusBarAppearanceUpdate];
	[[self navigationController] setNavigationBarHidden:NO animated:animated];
	if (@available(iOS 18.0, *))
		[[self tabBarController] setTabBarHidden:NO animated:animated];
	else
		[[[self tabBarController] tabBar] setHidden:NO];

	// reset all modifier keys on rdp keyboard
	[[RDPKeyboard getSharedRDPKeyboard] reset];

	// hide toolbar and keyboard
	[self showSessionToolbar:NO];
	[_dummy_textfield resignFirstResponder];
}

- (void)dealloc
{
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

- (void)scrollViewDidZoom:(UIScrollView *)scrollView
{
	[self centerSessionViewInViewport];
}

- (void)scrollViewDidEndZooming:(UIScrollView *)scrollView
                       withView:(UIView *)view
                        atScale:(CGFloat)scale
{
	NSLog(@"New zoom scale: %f", scale);
	[self centerSessionViewInViewport];
	[_session_view setNeedsDisplayInRemoteRect:[_session_view bounds]];
}

#pragma mark -
#pragma mark TextField delegate methods
- (BOOL)textFieldShouldBeginEditing:(UITextField *)textField
{
	_keyboard_visible = YES;
	_advanced_keyboard_visible = NO;
	return YES;
}

- (BOOL)textFieldShouldEndEditing:(UITextField *)textField
{
	_keyboard_visible = NO;
	_advanced_keyboard_visible = NO;
	return YES;
}

- (BOOL)textField:(UITextField *)textField
    shouldChangeCharactersInRange:(NSRange)range
                replacementString:(NSString *)string
{
	if ([string length] > 0)
	{
		for (int i = 0; i < [string length]; i++)
		{
			unichar curChar = [string characterAtIndex:i];

			// special handling for return/enter key
			if (curChar == '\n')
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
- (void)advancedKeyPressedVKey:(int)key
{
	[[RDPKeyboard getSharedRDPKeyboard] sendVirtualKeyCode:key];
}

- (void)advancedKeyPressedUnicode:(int)key
{
	[[RDPKeyboard getSharedRDPKeyboard] sendUnicode:key];
}

#pragma mark - RDP keyboard handler

- (void)modifiersChangedForKeyboard:(RDPKeyboard *)keyboard
{
	UIBarButtonItem *curItem;

	// shift button (only on iPad)
	int objectIdx = 0;
	if (IsPad())
	{
		objectIdx = 2;
		curItem = (UIBarButtonItem *)[[_keyboard_toolbar items] objectAtIndex:objectIdx];
		[curItem setStyle:[keyboard shiftPressed] ? UIBarButtonItemStyleDone
		                                          : UIBarButtonItemStyleBordered];
	}

	// ctrl button
	objectIdx += 2;
	curItem = (UIBarButtonItem *)[[_keyboard_toolbar items] objectAtIndex:objectIdx];
	[curItem
	    setStyle:[keyboard ctrlPressed] ? UIBarButtonItemStyleDone : UIBarButtonItemStyleBordered];

	// win button
	objectIdx += 2;
	curItem = (UIBarButtonItem *)[[_keyboard_toolbar items] objectAtIndex:objectIdx];
	[curItem
	    setStyle:[keyboard winPressed] ? UIBarButtonItemStyleDone : UIBarButtonItemStyleBordered];

	// alt button
	objectIdx += 2;
	curItem = (UIBarButtonItem *)[[_keyboard_toolbar items] objectAtIndex:objectIdx];
	[curItem
	    setStyle:[keyboard altPressed] ? UIBarButtonItemStyleDone : UIBarButtonItemStyleBordered];
}

#pragma mark -
#pragma mark RDPSessionDelegate functions

- (void)session:(RDPSession *)session didFailToConnect:(int)reason
{
	// remove and release connecting view
	[_connecting_indicator_view stopAnimating];
	[_connecting_view removeFromSuperview];
	[_connecting_view autorelease];

	// return to bookmark list
	[[self navigationController] popViewControllerAnimated:YES];
}

- (void)sessionWillConnect:(RDPSession *)session
{
	// load connecting view
	[[NSBundle mainBundle] loadNibNamed:@"RDPConnectingView" owner:self options:nil];

	// set strings
	[_lbl_connecting setText:NSLocalizedString(@"Connecting", @"Connecting progress view - label")];
	[_cancel_connect_button setTitle:NSLocalizedString(@"Cancel", @"Cancel Button")
	                        forState:UIControlStateNormal];

	// center view and give it round corners
	[_connecting_view setCenter:[[self view] center]];
	[[_connecting_view layer] setCornerRadius:10];

	// display connecting view and start indicator
	[[self view] addSubview:_connecting_view];
	[_connecting_indicator_view startAnimating];
}

- (void)sessionDidConnect:(RDPSession *)session
{
	// register keyboard notification handlers
	[[NSNotificationCenter defaultCenter] addObserver:self
	                                         selector:@selector(keyboardWillShow:)
	                                             name:UIKeyboardWillShowNotification
	                                           object:nil];
	[[NSNotificationCenter defaultCenter] addObserver:self
	                                         selector:@selector(keyboardDidShow:)
	                                             name:UIKeyboardDidShowNotification
	                                           object:nil];
	[[NSNotificationCenter defaultCenter] addObserver:self
	                                         selector:@selector(keyboardWillHide:)
	                                             name:UIKeyboardWillHideNotification
	                                           object:nil];
	[[NSNotificationCenter defaultCenter] addObserver:self
	                                         selector:@selector(keyboardDidHide:)
	                                             name:UIKeyboardDidHideNotification
	                                           object:nil];

	// register hardware keyboard connection handlers (for ipad magic keyboard)
	[[NSNotificationCenter defaultCenter] addObserver:self
	                                         selector:@selector(hardwareKeyboardChanged:)
	                                             name:GCKeyboardDidConnectNotification
	                                           object:nil];
	[[NSNotificationCenter defaultCenter] addObserver:self
	                                         selector:@selector(hardwareKeyboardChanged:)
	                                             name:GCKeyboardDidDisconnectNotification
	                                           object:nil];
	[self hardwareKeyboardChanged:nil];

	// remove and release connecting view
	[_connecting_indicator_view stopAnimating];
	[_connecting_view removeFromSuperview];
	[_connecting_view autorelease];

	// check if session settings changed ...
	// The 2nd width check is to ignore changes in resolution settings due to the RDVH display bug
	// (refer to RDPSEssion.m for more details)
	ConnectionParams *orig_params = [session params];
	rdpSettings *sess_params = [session getSessionParams];
	if (([orig_params intForKey:@"width"] != sess_params->DesktopWidth &&
	     [orig_params intForKey:@"width"] != (sess_params->DesktopWidth + 1)) ||
	    [orig_params intForKey:@"height"] != sess_params->DesktopHeight ||
	    [orig_params intForKey:@"colors"] != sess_params->ColorDepth)
	{
		// display notification that the session params have been changed by the server
		NSString *message =
		    [NSString stringWithFormat:NSLocalizedString(
		                                   @"The server changed the screen settings to %dx%dx%d",
		                                   @"Screen settings not supported message with width, "
		                                   @"height and colors parameter"),
		                               sess_params->DesktopWidth, sess_params->DesktopHeight,
		                               sess_params->ColorDepth];
		[[self view] makeToast:message duration:ToastDurationNormal position:@"bottom"];
	}
}

- (void)sessionWillDisconnect:(RDPSession *)session
{
}

- (void)sessionDidDisconnect:(RDPSession *)session
{
	// return to bookmark list
	[[self navigationController] popViewControllerAnimated:YES];
}

- (void)sessionBitmapContextWillChange:(RDPSession *)session
{
	[_session_view prepareForBitmapContextChange];

	// calc new view frame
	rdpSettings *sess_params = [session getSessionParams];
	CGRect view_rect = CGRectMake(0, 0, sess_params->DesktopWidth, sess_params->DesktopHeight);

	// set session view to its native (remote) size and update content size
	[_session_scrollview setZoomScale:1.0];
	[_session_view setFrame:view_rect];
	[_session_scrollview setContentSize:view_rect.size];
	_has_cursor_view_position = NO;
	_has_user_moved_cursor = NO;
	_last_session_viewport_size = CGSizeZero;
	[self fitSessionViewToViewport];

	// show/hide toolbar
	[_session
	    setToolbarVisible:![[NSUserDefaults standardUserDefaults] boolForKey:@"ui.hide_tool_bar"]];
	[self showSessionToolbar:[_session toolbarVisible]];
}

- (void)sessionBitmapContextDidChange:(RDPSession *)session
{
	// associate view with session
	[_session_view setSession:session];
	[_session_view setDefaultRemoteCursor];
	if (!_has_cursor_view_position)
		(void)[self currentCursorViewPosition];

	// Upload the new desktop once; subsequent EndPaint callbacks update only dirty regions.
	[_session_view setNeedsDisplayInRemoteRect:[_session_view bounds]];
}

- (void)session:(RDPSession *)session needsRedrawInRect:(CGRect)rect
{
	[_session_view setNeedsDisplayInRemoteRect:rect];
}

- (void)session:(RDPSession *)session didSetRemoteCursor:(RDPCursor *)cursor
{
	(void)session;
	[_session_view setRemoteCursor:cursor];

	if (!_has_user_moved_cursor &&
	    (!_has_cursor_view_position || CGPointEqualToPoint(_cursor_view_position, CGPointZero)))
	{
		_has_cursor_view_position = NO;
		(void)[self currentCursorViewPosition];
	}
}

- (void)session:(RDPSession *)session didMoveRemoteCursor:(CGPoint)position
{
	(void)session;
	if (_mouse_pan_active || _long_press_active)
		return;
	CGPoint viewPosition = [self sessionViewPositionForRemotePosition:position];
	if (!_has_user_moved_cursor && CGPointEqualToPoint(position, CGPointZero) &&
	    _has_cursor_view_position && !CGPointEqualToPoint(_cursor_view_position, CGPointZero))
		return;

	_cursor_view_position = viewPosition;
	_has_cursor_view_position = YES;
	[_session_view setRemoteCursorPosition:viewPosition];
}

- (void)sessionDidHideRemoteCursor:(RDPSession *)session
{
	(void)session;
	[_session_view hideRemoteCursor];
}

- (void)sessionDidSetDefaultRemoteCursor:(RDPSession *)session
{
	(void)session;
	[_session_view setDefaultRemoteCursor];
}

- (void)session:(RDPSession *)session requestsAuthenticationWithParams:(NSMutableDictionary *)params
{
	CredentialsInputController *view_controller =
	    [[[CredentialsInputController alloc] initWithNibName:@"CredentialsInputView"
	                                                  bundle:nil
	                                                 session:_session
	                                                  params:params] autorelease];
	[self presentModalViewController:view_controller animated:YES];
}

- (void)session:(RDPSession *)session verifyCertificateWithParams:(NSMutableDictionary *)params
{
	VerifyCertificateController *view_controller =
	    [[[VerifyCertificateController alloc] initWithNibName:@"VerifyCertificateView"
	                                                   bundle:nil
	                                                  session:_session
	                                                   params:params] autorelease];
	[self presentModalViewController:view_controller animated:YES];
}

- (CGSize)sizeForFitScreenForSession:(RDPSession *)session
{
	// set remote resolution that matches the on-screen viewport.
	CGSize size = [self view].bounds.size;
	UIScreen *screen = [[self view] window] ? [[[self view] window] screen] : [UIScreen mainScreen];
	CGFloat scale =
	    [screen respondsToSelector:@selector(nativeScale)] ? [screen nativeScale] : [screen scale];
	if (scale <= 0.0f)
		scale = 1.0f;

	size.width = ceilf(size.width * scale);
	size.height = ceilf(size.height * scale);

	CGFloat maxDimension = MAX(size.width, size.height);
	if (maxDimension > 4096.0f)
	{
		CGFloat downscale = 4096.0f / maxDimension;
		size.width = floorf(size.width * downscale);
		size.height = floorf(size.height * downscale);
	}

	size.width = MAX(64.0f, size.width);
	size.height = MAX(64.0f, size.height);
	return size;
}

#pragma mark - Keyboard Toolbar Handlers

- (void)showAdvancedKeyboardAnimated
{
	// calc initial and final rect of the advanced keyboard view
	CGRect rect = [[_keyboard_toolbar superview] bounds];
	rect.origin.y = [_keyboard_toolbar bounds].size.height;
	rect.size.height -= rect.origin.y;

	// create new view (hidden) and add to host-view of keyboard toolbar
	_advanced_keyboard_view = [[AdvancedKeyboardView alloc]
	    initWithFrame:CGRectMake(rect.origin.x, [[_keyboard_toolbar superview] bounds].size.height,
	                             rect.size.width, rect.size.height)
	         delegate:self];
	[[_keyboard_toolbar superview] addSubview:_advanced_keyboard_view];
	// we set autoresize to YES for the keyboard toolbar's superview so that our adv. keyboard view
	// gets properly resized
	[[_keyboard_toolbar superview] setAutoresizesSubviews:YES];

	// show view with animation
	[UIView beginAnimations:nil context:NULL];
	[_advanced_keyboard_view setFrame:rect];
	[UIView commitAnimations];
}

- (IBAction)toggleKeyboardWhenOtherVisible:(id)sender
{
	if (_advanced_keyboard_visible == NO)
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

- (IBAction)toggleWinKey:(id)sender
{
	[[RDPKeyboard getSharedRDPKeyboard] toggleWinKey];
}

- (IBAction)toggleShiftKey:(id)sender
{
	[[RDPKeyboard getSharedRDPKeyboard] toggleShiftKey];
}

- (IBAction)toggleCtrlKey:(id)sender
{
	[[RDPKeyboard getSharedRDPKeyboard] toggleCtrlKey];
}

- (IBAction)toggleAltKey:(id)sender
{
	[[RDPKeyboard getSharedRDPKeyboard] toggleAltKey];
}

- (IBAction)pressEscKey:(id)sender
{
	[[RDPKeyboard getSharedRDPKeyboard] sendEscapeKeyStroke];
}

#pragma mark -
#pragma mark event handlers

- (void)animationStopped:(NSString *)animationID
                finished:(NSNumber *)finished
                 context:(void *)context
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
	if (!_keyboard_visible)
		[_dummy_textfield becomeFirstResponder];
	else
		[_dummy_textfield resignFirstResponder];
}

- (IBAction)toggleExtKeyboard:(id)sender
{
	// if the sys kb is shown but not the advanced kb then toggle the advanced kb
	if (_keyboard_visible && !_advanced_keyboard_visible)
		[self toggleKeyboardWhenOtherVisible:nil];
	else
	{
		// if not visible request the advanced keyboard view
		if (_advanced_keyboard_visible == NO)
			_requesting_advanced_keyboard = YES;
		[self toggleKeyboard:nil];
	}
}

- (IBAction)disconnectSession:(id)sender
{
	[_session disconnect];
}

- (IBAction)cancelButtonPressed:(id)sender
{
	[_session disconnect];
}

#pragma mark -
#pragma mark iOS Keyboard Notification Handlers

- (void)keyboardWillShow:(NSNotification *)notification
{
	(void)notification;
	[self centerSessionViewInViewport];
}

- (void)keyboardDidShow:(NSNotification *)notification
{
	if (_requesting_advanced_keyboard)
	{
		[self showAdvancedKeyboardAnimated];
		_advanced_keyboard_visible = YES;
		_requesting_advanced_keyboard = NO;
	}
}

- (void)keyboardWillHide:(NSNotification *)notification
{
	(void)notification;
	[self centerSessionViewInViewport];
}

- (void)keyboardDidHide:(NSNotification *)notification
{
	// release adanced keyboard view
	if (_advanced_keyboard_visible == YES)
	{
		_advanced_keyboard_visible = NO;
		[_advanced_keyboard_view removeFromSuperview];
		[_advanced_keyboard_view autorelease];
		_advanced_keyboard_view = nil;
	}

	// resume capturing the hardware keyboard once the on-screen keyboard is gone
	if ([_session_view hardwareKeyboardActive])
		[_session_view becomeFirstResponder];
}

- (void)hardwareKeyboardChanged:(NSNotification *)notification
{
	BOOL connected = (GCKeyboard.coalescedKeyboard != nil);
	[_session_view setHardwareKeyboardActive:connected];

	if (connected && !_keyboard_visible)
		[_session_view becomeFirstResponder];
	else if (!connected)
		[_session_view resignFirstResponder];
}

#pragma mark -
#pragma mark Gesture handlers

- (void)handleSingleTap:(UITapGestureRecognizer *)gesture
{
	if (_pointer_is_indirect)
		[self moveCursorToSessionViewPosition:[gesture locationInView:_session_view]];
	[self sendMouseButtonEvent:GetLeftMouseButtonClickEvent(YES)];
	[self sendMouseButtonEvent:GetLeftMouseButtonClickEvent(NO)];
}

- (void)handleSecondaryTap:(UITapGestureRecognizer *)gesture
{
	[self moveCursorToSessionViewPosition:[gesture locationInView:_session_view]];
	[self sendMouseButtonEvent:GetRightMouseButtonClickEvent(YES)];
	[self sendMouseButtonEvent:GetRightMouseButtonClickEvent(NO)];
}

- (void)handleLongPress:(UILongPressGestureRecognizer *)gesture
{
	if ([gesture state] == UIGestureRecognizerStateBegan)
	{
		_long_press_active = YES;
		_mouse_drag_active = NO;
	}
	else if ([gesture state] == UIGestureRecognizerStateEnded ||
	         [gesture state] == UIGestureRecognizerStateCancelled ||
	         [gesture state] == UIGestureRecognizerStateFailed)
	{
		if (_mouse_drag_active)
			[self sendMouseButtonEvent:GetLeftMouseButtonClickEvent(NO)];
		else if ([gesture state] == UIGestureRecognizerStateEnded)
		{
			[self sendMouseButtonEvent:GetRightMouseButtonClickEvent(YES)];
			[self sendMouseButtonEvent:GetRightMouseButtonClickEvent(NO)];
		}

		_mouse_drag_active = NO;
		_long_press_active = NO;
	}
}

- (void)handleMousePan:(UIPanGestureRecognizer *)gesture
{
	// A mouse/trackpad drag holds a button down, so move the cursor absolutely and
	// keep the left button pressed for the duration of the drag.
	if (_pointer_is_indirect)
	{
		[self moveCursorToSessionViewPosition:[gesture locationInView:_session_view]];

		if ([gesture state] == UIGestureRecognizerStateBegan)
		{
			[self sendMouseButtonEvent:GetLeftMouseButtonClickEvent(YES)];
			_mouse_drag_active = YES;
		}
		else if ([gesture state] == UIGestureRecognizerStateEnded ||
		         [gesture state] == UIGestureRecognizerStateCancelled ||
		         [gesture state] == UIGestureRecognizerStateFailed)
		{
			if (_mouse_drag_active)
				[self sendMouseButtonEvent:GetLeftMouseButtonClickEvent(NO)];
			_mouse_drag_active = NO;
		}
		return;
	}

	CGPoint location = [gesture locationInView:_session_scrollview];

	if ([gesture state] == UIGestureRecognizerStateBegan)
	{
		_mouse_pan_active = YES;
		_last_mouse_pan_location = location;
		if (_long_press_active && !_mouse_drag_active)
		{
			[self sendMouseButtonEvent:GetLeftMouseButtonClickEvent(YES)];
			_mouse_drag_active = YES;
		}
	}
	else if ([gesture state] == UIGestureRecognizerStateChanged)
	{
		if (_long_press_active && !_mouse_drag_active)
		{
			[self sendMouseButtonEvent:GetLeftMouseButtonClickEvent(YES)];
			_mouse_drag_active = YES;
		}

		CGPoint delta = CGPointMake(location.x - _last_mouse_pan_location.x,
		                            location.y - _last_mouse_pan_location.y);
		[self moveCursorByViewportDelta:delta];
		_last_mouse_pan_location = location;
	}
	else if ([gesture state] == UIGestureRecognizerStateEnded ||
	         [gesture state] == UIGestureRecognizerStateCancelled ||
	         [gesture state] == UIGestureRecognizerStateFailed)
	{
		_mouse_pan_active = NO;
	}
}

- (void)handleHover:(UIHoverGestureRecognizer *)gesture
{
	if ([gesture state] != UIGestureRecognizerStateBegan &&
	    [gesture state] != UIGestureRecognizerStateChanged)
		return;

	[self moveCursorToSessionViewPosition:[gesture locationInView:_session_view]];
}

- (void)handleDoubleLongPress:(UILongPressGestureRecognizer *)gesture
{
	// this point is mapped against the scroll view because we want to have relative movement to the
	// screen/scrollview
	CGPoint pos = [gesture locationInView:_session_scrollview];
	CGPoint session_view_pos = [self currentCursorViewPosition];

	if ([gesture state] == UIGestureRecognizerStateBegan)
		_prev_long_press_position = pos;
	else if ([gesture state] == UIGestureRecognizerStateChanged)
	{
		int delta = _prev_long_press_position.y - pos.y;

		if (delta > GetScrollGestureDelta())
		{
			[_session sendInputEvent:[self eventDescriptorForMouseEvent:GetMouseWheelEvent(YES)
			                                                   position:session_view_pos]];
			_prev_long_press_position = pos;
		}
		else if (delta < -GetScrollGestureDelta())
		{
			[_session sendInputEvent:[self eventDescriptorForMouseEvent:GetMouseWheelEvent(NO)
			                                                   position:session_view_pos]];
			_prev_long_press_position = pos;
		}
	}
}

- (void)handleScroll:(UIPanGestureRecognizer *)gesture
{
	CGFloat delta = [gesture translationInView:_session_view].y;
	if (fabs(delta) < GetScrollGestureDelta())
		return;

	CGPoint position = [self currentCursorViewPosition];
	[_session sendInputEvent:[self eventDescriptorForMouseEvent:GetMouseWheelEvent(delta > 0)
	                                                   position:position]];
	[gesture setTranslation:CGPointZero inView:_session_view];
}

- (void)handleSingle3FingersTap:(UITapGestureRecognizer *)gesture
{
	[_session setToolbarVisible:![_session toolbarVisible]];
	[self showSessionToolbar:[_session toolbarVisible]];
}

- (UIPointerStyle *)pointerInteraction:(UIPointerInteraction *)interaction
                        styleForRegion:(UIPointerRegion *)region
{
	return [UIPointerStyle hiddenPointerStyle];
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer
       shouldReceiveTouch:(UITouch *)touch
{
	// the scroll recognizer only handles pointer scroll events, never touches
	if ([gestureRecognizer isKindOfClass:[UIPanGestureRecognizer class]] &&
	    [(UIPanGestureRecognizer *)gestureRecognizer allowedScrollTypesMask] != 0)
		return NO;

	_pointer_is_indirect = ([touch type] == UITouchTypeIndirectPointer);

	// the long press maps to a right-click / drag for touch input, which a connected
	// mouse handles through its own buttons instead
	if (_pointer_is_indirect &&
	    [gestureRecognizer isKindOfClass:[UILongPressGestureRecognizer class]])
		return NO;

	return YES;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
	BOOL panAndHold =
	    ([gestureRecognizer isKindOfClass:[UIPanGestureRecognizer class]] &&
	     [otherGestureRecognizer isKindOfClass:[UILongPressGestureRecognizer class]]) ||
	    ([gestureRecognizer isKindOfClass:[UILongPressGestureRecognizer class]] &&
	     [otherGestureRecognizer isKindOfClass:[UIPanGestureRecognizer class]]);
	return panAndHold;
}

@end

@implementation RDPSessionViewController (Private)

#pragma mark -
#pragma mark Helper functions

- (void)fitSessionViewToViewport
{
	CGSize viewportSize = [_session_scrollview bounds].size;
	CGSize remoteSize = [_session_view bounds].size;
	if (viewportSize.width <= 0.0f || viewportSize.height <= 0.0f || remoteSize.width <= 0.0f ||
	    remoteSize.height <= 0.0f)
		return;

	CGFloat fitScale =
	    MIN(viewportSize.width / remoteSize.width, viewportSize.height / remoteSize.height);
	[_session_scrollview setMinimumZoomScale:fitScale];
	[_session_scrollview setMaximumZoomScale:MAX(2.0f, fitScale)];
	[_session_scrollview setZoomScale:fitScale animated:NO];
	[_session_scrollview setContentOffset:CGPointZero animated:NO];
	_last_session_viewport_size = viewportSize;
	[self centerSessionViewInViewport];
}

- (void)centerSessionViewInViewport
{
	CGSize viewportSize = [_session_scrollview bounds].size;
	CGRect sessionFrame = [_session_view frame];
	if (viewportSize.width <= 0.0f || viewportSize.height <= 0.0f ||
	    sessionFrame.size.width <= 0.0f || sessionFrame.size.height <= 0.0f)
		return;

	sessionFrame.origin.x = MAX((viewportSize.width - sessionFrame.size.width) * 0.5f, 0.0f);
	sessionFrame.origin.y = MAX((viewportSize.height - sessionFrame.size.height) * 0.5f, 0.0f);
	[_session_view setFrame:sessionFrame];

	[_session_scrollview
	    setContentSize:CGSizeMake(MAX(viewportSize.width, sessionFrame.size.width),
	                              MAX(viewportSize.height, sessionFrame.size.height))];
}

- (void)showSessionToolbar:(BOOL)show
{
	// already shown or hidden?
	if (_session_toolbar_visible == show)
		return;

	// Offset by the safe-area insets so the toolbar/buttons are not hidden behind
	// the status bar / notch / Dynamic Island on modern devices (and side notch
	// in landscape).
	UIEdgeInsets safe = [[self view] safeAreaInsets];
	CGFloat toolbarWidth = [[self view] bounds].size.width - safe.left - safe.right;

	if (show)
	{
		[UIView beginAnimations:@"showToolbar" context:nil];
		[UIView setAnimationDuration:.4];
		[UIView setAnimationCurve:UIViewAnimationCurveLinear];
		[_session_toolbar setFrame:CGRectMake(safe.left, safe.top, toolbarWidth, TOOLBAR_HEIGHT)];
		[UIView commitAnimations];
		_session_toolbar_visible = YES;
	}
	else
	{
		[UIView beginAnimations:@"hideToolbar" context:nil];
		[UIView setAnimationDuration:.4];
		[UIView setAnimationCurve:UIViewAnimationCurveLinear];
		[_session_toolbar
		    setFrame:CGRectMake(safe.left, -TOOLBAR_HEIGHT, toolbarWidth, TOOLBAR_HEIGHT)];
		[UIView commitAnimations];
		_session_toolbar_visible = NO;
	}
}

- (UIToolbar *)keyboardToolbar
{
	UIToolbar *keyboard_toolbar = [[[UIToolbar alloc] initWithFrame:CGRectNull] autorelease];
	[keyboard_toolbar setBarStyle:UIBarStyleBlackOpaque];

	UIBarButtonItem *esc_btn =
	    [[[UIBarButtonItem alloc] initWithTitle:@"Esc"
	                                      style:UIBarButtonItemStyleBordered
	                                     target:self
	                                     action:@selector(pressEscKey:)] autorelease];
	UIImage *win_icon =
	    [UIImage imageWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"toolbar_icon_win"
	                                                                     ofType:@"png"]];
	UIBarButtonItem *win_btn =
	    [[[UIBarButtonItem alloc] initWithImage:win_icon
	                                      style:UIBarButtonItemStyleBordered
	                                     target:self
	                                     action:@selector(toggleWinKey:)] autorelease];
	UIBarButtonItem *ctrl_btn =
	    [[[UIBarButtonItem alloc] initWithTitle:@"Ctrl"
	                                      style:UIBarButtonItemStyleBordered
	                                     target:self
	                                     action:@selector(toggleCtrlKey:)] autorelease];
	UIBarButtonItem *alt_btn =
	    [[[UIBarButtonItem alloc] initWithTitle:@"Alt"
	                                      style:UIBarButtonItemStyleBordered
	                                     target:self
	                                     action:@selector(toggleAltKey:)] autorelease];
	UIBarButtonItem *ext_btn = [[[UIBarButtonItem alloc]
	    initWithTitle:@"Ext"
	            style:UIBarButtonItemStyleBordered
	           target:self
	           action:@selector(toggleKeyboardWhenOtherVisible:)] autorelease];
	UIBarButtonItem *done_btn = [[[UIBarButtonItem alloc]
	    initWithBarButtonSystemItem:UIBarButtonSystemItemDone
	                         target:self
	                         action:@selector(toggleKeyboard:)] autorelease];
	UIBarButtonItem *flex_spacer =
	    [[[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
	                                                   target:nil
	                                                   action:nil] autorelease];

	// iPad gets a shift button, iphone doesn't (there's just not enough space ...)
	NSArray *items;
	if (IsPad())
	{
		UIBarButtonItem *shift_btn =
		    [[[UIBarButtonItem alloc] initWithTitle:@"Shift"
		                                      style:UIBarButtonItemStyleBordered
		                                     target:self
		                                     action:@selector(toggleShiftKey:)] autorelease];
		items = [NSArray arrayWithObjects:esc_btn, flex_spacer, shift_btn, flex_spacer, ctrl_btn,
		                                  flex_spacer, win_btn, flex_spacer, alt_btn, flex_spacer,
		                                  ext_btn, flex_spacer, done_btn, nil];
	}
	else
	{
		items = [NSArray arrayWithObjects:esc_btn, flex_spacer, ctrl_btn, flex_spacer, win_btn,
		                                  flex_spacer, alt_btn, flex_spacer, ext_btn, flex_spacer,
		                                  done_btn, nil];
	}

	[keyboard_toolbar setItems:items];
	[keyboard_toolbar sizeToFit];
	return keyboard_toolbar;
}

- (void)initGestureRecognizers
{
	// single tap recognizer. A double-click is just two quick taps, which the server
	// coalesces on its own, so we avoid the latency of waiting for a double-tap to fail.
	UITapGestureRecognizer *singleTapRecognizer =
	    [[[UITapGestureRecognizer alloc] initWithTarget:self
	                                             action:@selector(handleSingleTap:)] autorelease];
	[singleTapRecognizer setNumberOfTouchesRequired:1];
	[singleTapRecognizer setNumberOfTapsRequired:1];
	[singleTapRecognizer setDelegate:self];

	// long press gesture recognizer
	UILongPressGestureRecognizer *longPressRecognizer = [[[UILongPressGestureRecognizer alloc]
	    initWithTarget:self
	            action:@selector(handleLongPress:)] autorelease];
	[longPressRecognizer setMinimumPressDuration:0.45];
	[longPressRecognizer setAllowableMovement:12.0];
	[longPressRecognizer setDelegate:self];

	// One-finger movement behaves like a trackpad and moves the remote cursor relatively.
	UIPanGestureRecognizer *mousePanRecognizer =
	    [[[UIPanGestureRecognizer alloc] initWithTarget:self
	                                             action:@selector(handleMousePan:)] autorelease];
	[mousePanRecognizer setMinimumNumberOfTouches:1];
	[mousePanRecognizer setMaximumNumberOfTouches:1];
	[mousePanRecognizer setDelegate:self];

	// A connected mouse/trackpad moves the remote cursor absolutely while hovering.
	UIHoverGestureRecognizer *hoverRecognizer =
	    [[[UIHoverGestureRecognizer alloc] initWithTarget:self
	                                               action:@selector(handleHover:)] autorelease];
	[hoverRecognizer setDelegate:self];

	// Secondary mouse/trackpad button maps to a right-click. Standard tap recognizers
	// only track the primary button, so this needs its own recognizer.
	UITapGestureRecognizer *secondaryTapRecognizer = [[[UITapGestureRecognizer alloc]
	    initWithTarget:self
	            action:@selector(handleSecondaryTap:)] autorelease];
	[secondaryTapRecognizer setButtonMaskRequired:UIEventButtonMaskSecondary];
	[secondaryTapRecognizer setAllowedTouchTypes:@[@(UITouchTypeIndirectPointer)]];
	[secondaryTapRecognizer setDelegate:self];

	// Mouse wheel and trackpad scrolling are forwarded as remote wheel events. Setting a
	// scroll type mask lets this pan recognizer receive pointer scroll device events.
	UIPanGestureRecognizer *scrollRecognizer =
	    [[[UIPanGestureRecognizer alloc] initWithTarget:self
	                                             action:@selector(handleScroll:)] autorelease];
	[scrollRecognizer setAllowedScrollTypesMask:UIScrollTypeMaskAll];
	[scrollRecognizer setDelegate:self];

	// double long press gesture recognizer
	UILongPressGestureRecognizer *doubleLongPressRecognizer = [[[UILongPressGestureRecognizer alloc]
	    initWithTarget:self
	            action:@selector(handleDoubleLongPress:)] autorelease];
	[doubleLongPressRecognizer setNumberOfTouchesRequired:2];
	[doubleLongPressRecognizer setMinimumPressDuration:0.5];

	// 3 finger, single tap gesture for showing/hiding the toolbar
	UITapGestureRecognizer *single3FingersTapRecognizer = [[[UITapGestureRecognizer alloc]
	    initWithTarget:self
	            action:@selector(handleSingle3FingersTap:)] autorelease];
	[single3FingersTapRecognizer setNumberOfTapsRequired:1];
	[single3FingersTapRecognizer setNumberOfTouchesRequired:3];
	[singleTapRecognizer requireGestureRecognizerToFail:longPressRecognizer];

	// Reserve one finger for the mouse; two fingers still pan/zoom the viewport.
	[[_session_scrollview panGestureRecognizer] setMinimumNumberOfTouches:2];

	// add gestures to scroll view
	[_session_scrollview addGestureRecognizer:singleTapRecognizer];
	[_session_scrollview addGestureRecognizer:longPressRecognizer];
	[_session_scrollview addGestureRecognizer:mousePanRecognizer];
	[_session_scrollview addGestureRecognizer:hoverRecognizer];
	[_session_scrollview addGestureRecognizer:secondaryTapRecognizer];
	[_session_scrollview addGestureRecognizer:scrollRecognizer];
	[_session_scrollview addGestureRecognizer:doubleLongPressRecognizer];
	[_session_scrollview addGestureRecognizer:single3FingersTapRecognizer];

	// Hide the system pointer over the session so only the remote cursor is visible.
	UIPointerInteraction *pointerInteraction =
	    [[[UIPointerInteraction alloc] initWithDelegate:self] autorelease];
	[_session_view addInteraction:pointerInteraction];
}

- (void)suspendSession
{
	// suspend session and pop navigation controller
	[_session suspend];

	// pop current view controller
	[[self navigationController] popViewControllerAnimated:YES];
}

- (NSDictionary *)eventDescriptorForMouseEvent:(int)event position:(CGPoint)position
{
	CGPoint remote_position = [self remotePositionForSessionViewPosition:position];
	return [NSDictionary
	    dictionaryWithObjectsAndKeys:@"mouse", @"type", [NSNumber numberWithUnsignedShort:event],
	                                 @"flags",
	                                 [NSNumber numberWithUnsignedShort:lrintf(remote_position.x)],
	                                 @"coord_x",
	                                 [NSNumber numberWithUnsignedShort:lrintf(remote_position.y)],
	                                 @"coord_y", nil];
}

- (CGPoint)remotePositionForSessionViewPosition:(CGPoint)position
{
	rdpSettings *settings = [_session getSessionParams];
	CGSize viewSize = [_session_view bounds].size;
	CGFloat desktopWidth =
	    settings ? freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth) : 0;
	CGFloat desktopHeight =
	    settings ? freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight) : 0;

	if ((viewSize.width > 0.0f) && (desktopWidth > 0.0f))
		position.x = position.x * desktopWidth / viewSize.width;
	if ((viewSize.height > 0.0f) && (desktopHeight > 0.0f))
		position.y = position.y * desktopHeight / viewSize.height;

	if (desktopWidth > 0.0f)
		position.x = MIN(MAX(position.x, 0.0f), desktopWidth - 1.0f);
	if (desktopHeight > 0.0f)
		position.y = MIN(MAX(position.y, 0.0f), desktopHeight - 1.0f);

	return position;
}

- (CGPoint)sessionViewPositionForRemotePosition:(CGPoint)position
{
	rdpSettings *settings = [_session getSessionParams];
	CGSize viewSize = [_session_view bounds].size;
	CGFloat desktopWidth =
	    settings ? freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth) : 0;
	CGFloat desktopHeight =
	    settings ? freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight) : 0;

	if ((desktopWidth > 0.0f) && (viewSize.width > 0.0f))
		position.x = position.x * viewSize.width / desktopWidth;
	if ((desktopHeight > 0.0f) && (viewSize.height > 0.0f))
		position.y = position.y * viewSize.height / desktopHeight;

	return [self clampedSessionViewCursorPosition:position];
}

- (CGPoint)clampedSessionViewCursorPosition:(CGPoint)position
{
	CGSize viewSize = [_session_view bounds].size;
	if (viewSize.width > 0.0f)
		position.x = MIN(MAX(position.x, 0.0f), viewSize.width - 1.0f);
	if (viewSize.height > 0.0f)
		position.y = MIN(MAX(position.y, 0.0f), viewSize.height - 1.0f);
	return position;
}

- (CGPoint)currentCursorViewPosition
{
	if (!_has_cursor_view_position)
	{
		CGSize viewSize = [_session_view bounds].size;
		_cursor_view_position = CGPointMake(MAX(viewSize.width - 1.0, 0.0) * 0.5,
		                                    MAX(viewSize.height - 1.0, 0.0) * 0.5);
		_has_cursor_view_position = YES;
		[_session_view setRemoteCursorPosition:_cursor_view_position];
	}
	[_session_view showRemoteCursor];

	return _cursor_view_position;
}

- (void)moveCursorByViewportDelta:(CGPoint)delta
{
	CGPoint position = [self currentCursorViewPosition];
	CGFloat zoomScale = [_session_scrollview zoomScale];
	if (zoomScale <= 0.0)
		zoomScale = 1.0;

	position.x += delta.x / zoomScale;
	position.y += delta.y / zoomScale;
	position = [self clampedSessionViewCursorPosition:position];
	_cursor_view_position = position;
	_has_cursor_view_position = YES;
	_has_user_moved_cursor = YES;

	// Local prediction keeps the cursor responsive while the RDP event is in flight.
	[_session_view setRemoteCursorPosition:position];
	[self handleMouseMoveForPosition:position];
}

- (void)moveCursorToSessionViewPosition:(CGPoint)position
{
	position = [self clampedSessionViewCursorPosition:position];
	_cursor_view_position = position;
	_has_cursor_view_position = YES;
	_has_user_moved_cursor = YES;

	// Local prediction keeps the cursor responsive while the RDP event is in flight.
	[_session_view setRemoteCursorPosition:position];
	[_session_view showRemoteCursor];
	[self handleMouseMoveForPosition:position];
}

- (void)sendMouseButtonEvent:(int)event
{
	CGPoint position = [self currentCursorViewPosition];
	[_session sendInputEvent:[self eventDescriptorForMouseEvent:event position:position]];
}

- (void)handleMouseMoveForPosition:(CGPoint)position
{
	[_session sendInputEvent:[self eventDescriptorForMouseEvent:PTR_FLAGS_MOVE position:position]];
}

@end
