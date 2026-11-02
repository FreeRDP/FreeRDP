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
#import "RDPOverlayStyle.h"
#import "RDPKeyboard.h"
#import "Utils.h"
#import "Toast+UIView.h"
#import "ConnectionParams.h"

#define TOOLBAR_HEIGHT 44
#define KEYBOARD_CURSOR_MARGIN 44.0

@interface RDPSessionViewController (Private)
- (void)showSessionToolbar:(BOOL)show;
- (void)initGestureRecognizers;
- (void)suspendSession;
- (void)fitSessionViewToViewport;
- (void)centerSessionViewInViewport;
- (void)setKeyboardOverlap:(CGFloat)overlap;
- (CGRect)sessionViewportFrame;
- (void)showConnectingViewWithText:(NSString *)text;
- (void)hideConnectingView;
- (void)endBackgroundTask;
- (CGPoint)remotePositionForSessionViewPosition:(CGPoint)position;
- (CGPoint)sessionViewPositionForRemotePosition:(CGPoint)position;
- (CGPoint)clampedSessionViewCursorPosition:(CGPoint)position;
- (NSDictionary *)eventDescriptorForMouseEvent:(int)event position:(CGPoint)position;
- (void)handleMouseMoveForPosition:(CGPoint)position;
- (CGPoint)currentCursorViewPosition;
- (void)moveCursorByViewportDelta:(CGPoint)delta;
- (void)moveCursorToSessionViewPosition:(CGPoint)position;
- (void)sendMouseButtonEvent:(int)event;
- (BOOL)isKeyboardActive;
- (BOOL)isLandscape;
- (CGFloat)advancedKeyboardHeight;
- (void)resetTextInput;
- (void)commitTextInput;
@end

// dummy text field
@interface RDPSessionTextField : UITextField
@end

@implementation RDPSessionTextField

- (void)deleteBackward
{
	// send backspace to remote session if dummy text field is empty.
	if ([[self text] length] == 0)
		[[RDPKeyboard getSharedRDPKeyboard] sendBackspaceKeyStroke];
	[super deleteBackward];
}

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
		_sent_text = [@"" retain];
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
	if (RDPSupportLiquidGlass() == NO)
	{
		// before ios 26, the gray background has been displayed in toolbar
		UIToolbarAppearance *appearance = [[[UIToolbarAppearance alloc] init] autorelease];

		// set transparent background
		[appearance configureWithTransparentBackground];
		[_session_toolbar setStandardAppearance:appearance];
		[_session_toolbar setCompactAppearance:appearance];
		[_session_toolbar setScrollEdgeAppearance:appearance];
		[_session_toolbar setCompactScrollEdgeAppearance:appearance];
	}

	// init keyboard toolbar
	_keyboard_toolbar = [[RDPKeyboardToolbarView alloc] initWithTarget:self];
	[_dummy_textfield setInputAccessoryView:_keyboard_toolbar];

	// the text field must not alter when typed
	[_dummy_textfield setAutocapitalizationType:UITextAutocapitalizationTypeNone];
	[_dummy_textfield setSpellCheckingType:UITextSpellCheckingTypeNo];
	[_dummy_textfield setSmartQuotesType:UITextSmartQuotesTypeNo];
	[_dummy_textfield setSmartDashesType:UITextSmartDashesTypeNo];
	[_dummy_textfield setSmartInsertDeleteType:UITextSmartInsertDeleteTypeNo];
	[_dummy_textfield addTarget:self
	                     action:@selector(textInputChanged:)
	           forControlEvents:UIControlEventEditingChanged];

	// the undo/redo/paste shortcuts of the iPad keyboard act on the hidden text field, not on
	// the remote session, so don't show them
	[[_dummy_textfield inputAssistantItem] setLeadingBarButtonGroups:@[]];
	[[_dummy_textfield inputAssistantItem] setTrailingBarButtonGroups:@[]];

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

	// shown around the session when it avoids the screen edges
	[[self view] setBackgroundColor:[UIColor blackColor]];

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

	CGRect viewportFrame = [self sessionViewportFrame];
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

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator
{
	[super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

	// the system keyboard changes its height with the orientation, so follow it
	[coordinator
	    animateAlongsideTransition:nil
	                    completion:^(id<UIViewControllerTransitionCoordinatorContext> context) {
		                    if (!_advanced_keyboard_view)
			                    return;
		                    CGRect frame = [_advanced_keyboard_view frame];
		                    frame.size.height = [self advancedKeyboardHeight];
		                    [_advanced_keyboard_view setFrame:frame];
		                    [_dummy_textfield reloadInputViews];
	                    }];
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
	[_sent_text release];
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
	return YES;
}

- (BOOL)textFieldShouldEndEditing:(UITextField *)textField
{
	[self destroyAdvancedKeyboard];
	return YES;
}

- (void)textFieldDidEndEditing:(UITextField *)textField
{
	[self commitTextInput];
}

- (BOOL)textFieldShouldReturn:(UITextField *)textField
{
	[self commitTextInput];
	[[RDPKeyboard getSharedRDPKeyboard] sendEnterKeyStroke];

	// turn off default action
	return NO;
}

// sends what changed in the text field since the last time
- (void)textInputChanged:(UITextField *)textField
{
	// send marked text (chinese, japanese) only once committed, sending every change
	// arrives out of order on the server
	if ([textField markedTextRange] != nil)
		return;

	// a letter typed with a toolbar modifier is a shortcut (e.g. ctrl+a). read it before
	// sending, which releases the modifiers
	RDPKeyboard *keyboard = [RDPKeyboard getSharedRDPKeyboard];
	BOOL modifierPressed = [keyboard ctrlPressed] || [keyboard altPressed] ||
	                       [keyboard winPressed] || [keyboard altGrPressed];

	NSString *text = [textField text] != nil ? [textField text] : @"";
	NSUInteger length = MIN([text length], [_sent_text length]);
	NSUInteger common = 0;

	while (common < length)
	{
		if ([text characterAtIndex:common] != [_sent_text characterAtIndex:common])
		{
			break;
		}
		common++;
	}

	// don't split a multi code point character (e.g. emoji)
	if (common < [_sent_text length])
		common = MIN(common, [_sent_text rangeOfComposedCharacterSequenceAtIndex:common].location);
	if (common < [text length])
		common = MIN(common, [text rangeOfComposedCharacterSequenceAtIndex:common].location);

	for (NSUInteger i = common; i < [_sent_text length];)
	{
		NSRange range = [_sent_text rangeOfComposedCharacterSequenceAtIndex:i];
		[keyboard sendBackspaceKeyStroke];
		i = NSMaxRange(range);
	}

	for (NSUInteger i = common; i < [text length]; i++)
	{
		[keyboard sendUnicode:[text characterAtIndex:i]];
	}

	BOOL isLastCharLetter = NO;
	if ([text length] > 0)
	{
		isLastCharLetter = [[NSCharacterSet letterCharacterSet]
		    characterIsMember:[text characterAtIndex:[text length] - 1]];
	}

	// some keyboard (korean) composes with the letters before the cursor
	// keep it
	if (modifierPressed || !isLastCharLetter)
	{
		[self resetTextInput];
	}
	else
	{
		[_sent_text release];
		_sent_text = [text copy];
	}
}

#pragma mark -
#pragma mark AdvancedKeyboardDelegate functions
- (void)advancedKeyPressedVKey:(NSInteger)key
{
	[self commitTextInput];
	[[RDPKeyboard getSharedRDPKeyboard] sendVirtualKeyCode:key];
}

#pragma mark - RDP keyboard handler

- (void)modifiersChangedForKeyboard:(RDPKeyboard *)keyboard
{
	[_keyboard_toolbar updateModifiersWithKeyboard:keyboard];
}

#pragma mark -
#pragma mark RDPSessionDelegate functions

- (void)session:(RDPSession *)session didFailToConnect:(int)reason
{
	[self hideConnectingView];

	// return to bookmark list
	[[self navigationController] popViewControllerAnimated:YES];
}

- (void)sessionWillConnect:(RDPSession *)session
{
	[self showConnectingViewWithText:NSLocalizedString(@"Connecting",
	                                                   @"Connecting progress view - label")];
}

- (void)sessionWillReconnect:(RDPSession *)session
{
	(void)session;
	[self showConnectingViewWithText:NSLocalizedString(
	                                     @"Reconnecting",
	                                     @"Connecting progress view - reconnect label")];
}

- (void)sessionDidReconnect:(RDPSession *)session
{
	(void)session;
	[self hideConnectingView];
}

- (void)sessionDidConnect:(RDPSession *)session
{
	// register keyboard notification handlers
	[[NSNotificationCenter defaultCenter] addObserver:self
	                                         selector:@selector(keyboardWillShow:)
	                                             name:UIKeyboardWillShowNotification
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

	// another app may have changed the clipboard while this one was inactive
	[[NSNotificationCenter defaultCenter] addObserver:self
	                                         selector:@selector(localClipboardChanged:)
	                                             name:UIApplicationDidBecomeActiveNotification
	                                           object:nil];
	[self localClipboardChanged:nil];

	// ios can suspends the app soon after it switched to the background and may then take its
	// socket away
	[[NSNotificationCenter defaultCenter] addObserver:self
	                                         selector:@selector(applicationDidEnterBackground:)
	                                             name:UIApplicationDidEnterBackgroundNotification
	                                           object:nil];
	[[NSNotificationCenter defaultCenter] addObserver:self
	                                         selector:@selector(applicationWillEnterForeground:)
	                                             name:UIApplicationWillEnterForegroundNotification
	                                           object:nil];

	[self hideConnectingView];

	// check if session settings changed ...
	// The 2nd width check is to ignore changes in resolution settings due to the RDVH display bug
	// (refer to RDPSEssion.m for more details)
	ConnectionParams *orig_params = [session params];
	rdpSettings *sess_params = [session getSessionParams];
	const UINT32 width = freerdp_settings_get_uint32(sess_params, FreeRDP_DesktopWidth);
	const UINT32 height = freerdp_settings_get_uint32(sess_params, FreeRDP_DesktopHeight);
	const UINT32 depth = freerdp_settings_get_uint32(sess_params, FreeRDP_ColorDepth);

	if (([orig_params intForKey:@"width"] != width &&
	     [orig_params intForKey:@"width"] != (width + 1ul)) ||
	    [orig_params intForKey:@"height"] != height || [orig_params intForKey:@"colors"] != depth)
	{
		// display notification that the session params have been changed by the server
		NSString *message =
		    [NSString stringWithFormat:NSLocalizedString(
		                                   @"The server changed the screen settings to %dx%dx%d",
		                                   @"Screen settings not supported message with width, "
		                                   @"height and colors parameter"),
		                               width, height, depth];
		[[self view] makeToast:message duration:ToastDurationNormal position:@"bottom"];
	}
}

- (void)sessionWillDisconnect:(RDPSession *)session
{
}

- (void)sessionDidDisconnect:(RDPSession *)session
{
	[self hideConnectingView];
	[self endBackgroundTask];

	// return to bookmark list
	[[self navigationController] popViewControllerAnimated:YES];
}

- (void)sessionBitmapContextWillChange:(RDPSession *)session
{
	[_session_view prepareForBitmapContextChange];

	// calc new view frame
	rdpSettings *sess_params = [session getSessionParams];
	CGRect view_rect =
	    CGRectMake(0, 0, freerdp_settings_get_uint32(sess_params, FreeRDP_DesktopWidth),
	               freerdp_settings_get_uint32(sess_params, FreeRDP_DesktopHeight));

	// set session view to its native (remote) size and update content size
	[_session_scrollview setZoomScale:1.0];
	[_session_view setFrame:view_rect];
	[_session_scrollview setContentSize:view_rect.size];
	_has_cursor_view_position = NO;
	_has_user_moved_cursor = NO;
	_last_session_viewport_size = CGSizeZero;
	[self fitSessionViewToViewport];

	// show toolbar
	[_session setToolbarVisible:YES];
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

- (void)session:(RDPSession *)session didChangeRemoteClipboardText:(NSString *)text
{
	(void)session;
	UIPasteboard *pasteboard = [UIPasteboard generalPasteboard];
	[pasteboard setString:text];

	// don't announce the server's own text back to it
	_pasteboard_change_count = [pasteboard changeCount];
}

- (void)session:(RDPSession *)session requestsClipboardTextWithParams:(NSMutableDictionary *)params
{
	(void)session;
	NSString *text = [[UIPasteboard generalPasteboard] string];
	if (text)
		[params setObject:text forKey:@"text"];
}

- (void)session:(RDPSession *)session requestsAuthenticationWithParams:(NSMutableDictionary *)params
{
	// custom view --> stock UIAlertController
	__block UIAlertController *alert = [UIAlertController
	    alertControllerWithTitle:NSLocalizedString(@"Credentials", @"Credentials title")
	                     message:NSLocalizedString(@"Please provide the missing user information.",
	                                               @"Credentials input view message")
	              preferredStyle:UIAlertControllerStyleAlert];

	// username input field
	[alert addTextFieldWithConfigurationHandler:^(UITextField *textField) {
		[textField
		    setPlaceholder:NSLocalizedString(@"Username", @"Credentials Input Username hint")];
		[textField setText:[params valueForKey:@"username"]];
		[textField setAutocapitalizationType:UITextAutocapitalizationTypeNone];
		[textField setAutocorrectionType:UITextAutocorrectionTypeNo];
	}];

	// password input field
	[alert addTextFieldWithConfigurationHandler:^(UITextField *textField) {
		[textField
		    setPlaceholder:NSLocalizedString(@"Password", @"Credentials Input Password hint")];
		[textField setText:[params valueForKey:@"password"]];
		[textField setSecureTextEntry:YES];
	}];

	// domain input field
	[alert addTextFieldWithConfigurationHandler:^(UITextField *textField) {
		[textField setPlaceholder:NSLocalizedString(@"Domain", @"Credentials Input Domain hint")];
		[textField setText:[params valueForKey:@"domain"]];
		[textField setAutocapitalizationType:UITextAutocapitalizationTypeNone];
		[textField setAutocorrectionType:UITextAutocorrectionTypeNo];
	}];

	// cancel btn
	UIAlertAction *cancelAction =
	    [UIAlertAction actionWithTitle:NSLocalizedString(@"Cancel", @"Cancel Button")
	                             style:UIAlertActionStyleCancel
	                           handler:^(UIAlertAction *action) {
		                           [params setValue:[NSNumber numberWithBool:NO] forKey:@"result"];
		                           [[session uiRequestCompleted] signal];
	                           }];
	[alert addAction:cancelAction];

	// login btn
	UIAlertAction *loginAction = [UIAlertAction
	    actionWithTitle:NSLocalizedString(@"Login", @"Login Button")
	              style:UIAlertActionStyleDefault
	            handler:^(UIAlertAction *action) {
		            NSArray *fields = [alert textFields];
		            [params setValue:[[fields objectAtIndex:0] text] forKey:@"username"];
		            [params setValue:[[fields objectAtIndex:1] text] forKey:@"password"];
		            [params setValue:[[fields objectAtIndex:2] text] forKey:@"domain"];
		            [params setValue:[NSNumber numberWithBool:YES] forKey:@"result"];
		            [[session uiRequestCompleted] signal];
	            }];
	[alert addAction:loginAction];

	[alert setPreferredAction:loginAction];

	[self presentViewController:alert animated:YES completion:nil];
}

- (void)session:(RDPSession *)session verifyCertificateWithParams:(NSMutableDictionary *)params
{
	NSString *message = [NSString
	    stringWithFormat:@"%@\n\n%@ %@",
	                     NSLocalizedString(@"The identity of the remote computer cannot be "
	                                       @"verified. Do you want to connect anyway?",
	                                       @"Verify certificate view message"),
	                     NSLocalizedString(@"Issuer:", @"Verify certificate view issuer label"),
	                     [params valueForKey:@"issuer"]];
	UIAlertController *alert =
	    [UIAlertController alertControllerWithTitle:nil
	                                        message:message
	                                 preferredStyle:UIAlertControllerStyleAlert];

	[alert addAction:[UIAlertAction actionWithTitle:NSLocalizedString(@"No", @"No Button")
	                                          style:UIAlertActionStyleCancel
	                                        handler:^(UIAlertAction *action) {
		                                        [params setValue:[NSNumber numberWithBool:NO]
		                                                  forKey:@"result"];
		                                        [[session uiRequestCompleted] signal];
	                                        }]];

	[alert addAction:[UIAlertAction actionWithTitle:NSLocalizedString(@"Yes", @"Yes Button")
	                                          style:UIAlertActionStyleDefault
	                                        handler:^(UIAlertAction *action) {
		                                        [params setValue:[NSNumber numberWithBool:YES]
		                                                  forKey:@"result"];
		                                        [[session uiRequestCompleted] signal];
	                                        }]];

	[self presentViewController:alert animated:YES completion:nil];
}

- (CGSize)sizeForFitScreenForSession:(RDPSession *)session
{
	// set remote resolution that matches the on-screen viewport.
	CGSize size = [self sessionViewportFrame].size;
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
	// already advanced keyboard activated
	if (_advanced_keyboard_view)
		return;

	CGRect rect = CGRectMake(0, 0, [[self view] bounds].size.width, [self advancedKeyboardHeight]);
	_advanced_keyboard_view = [[AdvancedKeyboardView alloc] initWithFrame:rect delegate:self];

	[_dummy_textfield setInputView:_advanced_keyboard_view];
	[_dummy_textfield reloadInputViews];
	[_keyboard_toolbar setExtendedKeyboardActive:YES];
}

- (void)destroyAdvancedKeyboard
{
	// advanced keyboard not activated
	if (!_advanced_keyboard_view)
		return;

	[_dummy_textfield setInputView:nil];
	[_advanced_keyboard_view autorelease];
	_advanced_keyboard_view = nil;
	[_keyboard_toolbar setExtendedKeyboardActive:NO];
}

// toggle advanced keyboard (not keyboard)
- (IBAction)toggleKeyboardWhenOtherVisible:(id)sender
{
	if (!_advanced_keyboard_view)
	{
		// show advanced keyboard
		[self showAdvancedKeyboardAnimated];
	}
	else
	{
		// close advanced keyboard
		[self destroyAdvancedKeyboard];
		[_dummy_textfield reloadInputViews];
	}
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

- (IBAction)toggleAltGrKey:(id)sender
{
	[[RDPKeyboard getSharedRDPKeyboard] toggleAltGrKey];
}

- (IBAction)pressEscKey:(id)sender
{
	[self commitTextInput];
	[[RDPKeyboard getSharedRDPKeyboard] sendEscapeKeyStroke];
}

- (IBAction)pressDeleteKey:(id)sender
{
	[self commitTextInput];
	[[RDPKeyboard getSharedRDPKeyboard] sendDeleteKeyStroke];
}

- (IBAction)pressTabKey:(id)sender
{
	[self commitTextInput];
	[[RDPKeyboard getSharedRDPKeyboard] sendTabKeyStroke];
}

#pragma mark -
#pragma mark event handlers

- (IBAction)switchSession:(id)sender
{
	[self suspendSession];
}

- (IBAction)toggleKeyboard:(id)sender
{
	if (![self isKeyboardActive])
		[_dummy_textfield becomeFirstResponder];
	else
		[_dummy_textfield resignFirstResponder];
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
	// remember the system keyboard's height (to show advanced keyboard view)
	if (!_advanced_keyboard_view)
	{
		CGRect frame =
		    [[[notification userInfo] objectForKey:UIKeyboardFrameEndUserInfoKey] CGRectValue];
		CGFloat height = frame.size.height - [_keyboard_toolbar bounds].size.height;

		// 100.0 is heuristic value.
		if (height >= 100.0)
		{
			if ([self isLandscape])
				_keyboard_height_landscape = height;
			else
				_keyboard_height_portrait = height;
		}
	}

	[self centerSessionViewInViewport];

	// get covered area (keyboard)
	CGRect keyboardFrame =
	    [[[notification userInfo] objectForKey:UIKeyboardFrameEndUserInfoKey] CGRectValue];
	keyboardFrame = [[self view] convertRect:keyboardFrame fromView:nil];

	CGRect coveredArea = CGRectIntersection([_session_scrollview frame], keyboardFrame);

	[self setKeyboardOverlap:CGRectGetHeight(coveredArea)];
}

- (void)keyboardWillHide:(NSNotification *)notification
{
	(void)notification;
	[self centerSessionViewInViewport];
	[self setKeyboardOverlap:0.0];
}

- (void)keyboardDidHide:(NSNotification *)notification
{
	// release adanced keyboard view
	if (![self isKeyboardActive])
		[self destroyAdvancedKeyboard];

	// resume capturing the hardware keyboard once the on-screen keyboard is gone
	if (![self isKeyboardActive] && [_session_view hardwareKeyboardActive])
		[_session_view becomeFirstResponder];
}

- (void)hardwareKeyboardChanged:(NSNotification *)notification
{
	BOOL connected = (GCKeyboard.coalescedKeyboard != nil);
	[_session_view setHardwareKeyboardActive:connected];

	if (connected && ![self isKeyboardActive])
		[_session_view becomeFirstResponder];
	else if (!connected)
		[_session_view resignFirstResponder];
}

- (void)localClipboardChanged:(NSNotification *)notification
{
	// hasStrings does not show the paste permission alert, unlike reading the string
	UIPasteboard *pasteboard = [UIPasteboard generalPasteboard];
	if ([pasteboard changeCount] == _pasteboard_change_count)
		return;

	_pasteboard_change_count = [pasteboard changeCount];
	[_session sendClipboardHasText:[pasteboard hasStrings]];
}

- (void)applicationDidEnterBackground:(NSNotification *)notification
{
	// keeps the connection alive for a short visit to another app
	// after a longer one the session will reconnect
	[self endBackgroundTask];
	_background_task =
	    [[UIApplication sharedApplication] beginBackgroundTaskWithExpirationHandler:^{
		    [self endBackgroundTask];
	    }];
}

- (void)applicationWillEnterForeground:(NSNotification *)notification
{
	[self endBackgroundTask];
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

- (void)showConnectingViewWithText:(NSString *)text
{
	[self hideConnectingView];

	// load connecting view
	[[NSBundle mainBundle] loadNibNamed:@"RDPConnectingView" owner:self options:nil];

	// set strings
	[_lbl_connecting setText:text];
	[_cancel_connect_button setTitle:NSLocalizedString(@"Cancel", @"Cancel Button")
	                        forState:UIControlStateNormal];

	// center view and give it round corners
	[_connecting_view setCenter:[[self view] center]];
	[[_connecting_view layer] setCornerRadius:10];

	// display connecting view and start indicator
	[[self view] addSubview:_connecting_view];
	[_connecting_indicator_view startAnimating];
}

- (void)hideConnectingView
{
	if (!_connecting_view)
		return;

	// remove and release connecting view
	[_connecting_indicator_view stopAnimating];
	[_connecting_view removeFromSuperview];
	[_connecting_view autorelease];
	_connecting_view = nil;
}

- (void)endBackgroundTask
{
	if (_background_task == UIBackgroundTaskInvalid)
		return;

	[[UIApplication sharedApplication] endBackgroundTask:_background_task];
	_background_task = UIBackgroundTaskInvalid;
}

- (CGRect)sessionViewportFrame
{
	// use full screen (ipad or 'Avoid Screen Edges' disabled)
	if (IsPad() || ![[NSUserDefaults standardUserDefaults] boolForKey:@"ui.avoid_screen_edges"])
		return [[self view] bounds];

	// if 'Avoid Screen Edges' enabled (only for iphone)
	return UIEdgeInsetsInsetRect([[self view] bounds], [[self view] safeAreaInsets]);
}

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

- (void)setKeyboardOverlap:(CGFloat)overlap
{
	// extend the scrollable area to push up using 2 finger
	UIEdgeInsets inset = [_session_scrollview contentInset];
	inset.bottom = overlap;
	[_session_scrollview setContentInset:inset];

	CGPoint offset = [_session_scrollview contentOffset];
	CGFloat viewportHeight = [_session_scrollview bounds].size.height;

	// cursor is usually located in typing area
	if (overlap > 0.0 && _has_cursor_view_position)
	{
		CGPoint cursor = [_session_scrollview convertPoint:_cursor_view_position
		                                          fromView:_session_view];
		CGFloat visibleBottom = offset.y + viewportHeight - overlap;
		CGFloat hiddenHeight = cursor.y + KEYBOARD_CURSOR_MARGIN - visibleBottom;
		if (hiddenHeight > 0.0)
			offset.y += hiddenHeight;
	}

	// stay within the scrollable range or scrolls back once the keyboard hides
	CGFloat maxOffsetY = [_session_scrollview contentSize].height + overlap - viewportHeight;
	offset.y = MAX(MIN(offset.y, maxOffsetY), 0.0);
	[_session_scrollview setContentOffset:offset animated:YES];
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
	// devices without a notch (e.g. iPad) have no top inset once the status bar is hidden,
	// so keep at least the system margin to the top edge
	safe.top = MAX(safe.top, [self systemMinimumLayoutMargins].leading);
	CGFloat toolbarWidth = [[self view] bounds].size.width - safe.left - safe.right;

	if (show)
	{
		[UIView animateWithDuration:0.4
		                      delay:0.0
		                    options:UIViewAnimationOptionCurveLinear
		                 animations:^{
			                 [_session_toolbar setFrame:CGRectMake(safe.left, safe.top,
			                                                       toolbarWidth, TOOLBAR_HEIGHT)];
		                 }
		                 completion:nil];
		_session_toolbar_visible = YES;
	}
	else
	{
		[UIView animateWithDuration:0.4
		                      delay:0.0
		                    options:UIViewAnimationOptionCurveLinear
		                 animations:^{
			                 [_session_toolbar setFrame:CGRectMake(safe.left, -TOOLBAR_HEIGHT,
			                                                       toolbarWidth, TOOLBAR_HEIGHT)];
		                 }
		                 completion:nil];
		_session_toolbar_visible = NO;
	}
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
	[longPressRecognizer setMinimumPressDuration:0.3];
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
	[doubleLongPressRecognizer setMinimumPressDuration:0.3];

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
	// typed text can not be changed anymore.
	[self commitTextInput];
	CGPoint position = [self currentCursorViewPosition];
	[_session sendInputEvent:[self eventDescriptorForMouseEvent:event position:position]];
}

- (void)handleMouseMoveForPosition:(CGPoint)position
{
	[_session sendInputEvent:[self eventDescriptorForMouseEvent:PTR_FLAGS_MOVE position:position]];
}

- (BOOL)isKeyboardActive
{
	return [_dummy_textfield isFirstResponder];
}

- (void)resetTextInput
{
	[_dummy_textfield setText:@""];
	[_sent_text release];
	_sent_text = [@"" retain];
}

- (void)commitTextInput
{
	if ([[_dummy_textfield text] length] == 0)
		return;

	// send the text being composed as it is
	[_dummy_textfield unmarkText];
	[self textInputChanged:_dummy_textfield];
	[self resetTextInput];
}

- (BOOL)isLandscape
{
	CGSize size = [[self view] bounds].size;
	return size.width > size.height;
}

- (CGFloat)advancedKeyboardHeight
{
	// before the system keyboard was shown in this orientation, use a typical height
	if ([self isLandscape])
		return (_keyboard_height_landscape > 0.0) ? _keyboard_height_landscape
		                                          : (IsPad() ? 400.0 : 200.0);
	return (_keyboard_height_portrait > 0.0) ? _keyboard_height_portrait
	                                         : (IsPad() ? 320.0 : 290.0);
}

@end
