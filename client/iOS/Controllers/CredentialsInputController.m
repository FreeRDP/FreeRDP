/*
 Credentials input controller

 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import "CredentialsInputController.h"
#import "RDPSession.h"

@interface CredentialsInputController ()
- (void)updateScrollViewContentSize;

- (UIView *)activeCredentialField;
- (void)setKeyboardOverlap:(CGFloat)overlap notification:(NSNotification *)notification;
@end

@implementation CredentialsInputController

- (id)initWithNibName:(NSString *)nibNameOrNil
               bundle:(NSBundle *)nibBundleOrNil
              session:(RDPSession *)session
               params:(NSMutableDictionary *)params
{
	self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
	if (self)
	{
		_session = session;
		_params = params;
		[self setModalPresentationStyle:UIModalPresentationFormSheet];

		[[NSNotificationCenter defaultCenter] addObserver:self
		                                         selector:@selector(keyboardWillShow:)
		                                             name:UIKeyboardWillShowNotification
		                                           object:nil];
		[[NSNotificationCenter defaultCenter] addObserver:self
		                                         selector:@selector(keyboardWillHide:)
		                                             name:UIKeyboardWillHideNotification
		                                           object:nil];
	}
	return self;
}

- (void)viewDidLoad
{
	[super viewDidLoad];

	// set localized strings
	[_lbl_message
	    setText:NSLocalizedString(
	                @"Please provide the missing user information in order to proceed and login.",
	                @"Credentials input view message")];
	[_textfield_username
	    setPlaceholder:NSLocalizedString(@"Username", @"Credentials Input Username hint")];
	[_textfield_password
	    setPlaceholder:NSLocalizedString(@"Password", @"Credentials Input Password hint")];
	[_textfield_domain
	    setPlaceholder:NSLocalizedString(@"Domain", @"Credentials Input Domain hint")];
	[_btn_login setTitle:NSLocalizedString(@"Login", @"Login Button")
	            forState:UIControlStateNormal];
	[_btn_cancel setTitle:NSLocalizedString(@"Cancel", @"Cancel Button")
	             forState:UIControlStateNormal];

	[_scroll_view setKeyboardDismissMode:UIScrollViewKeyboardDismissModeInteractive];
	[self updateScrollViewContentSize];

	// set params in the view
	[_textfield_username setText:[_params valueForKey:@"username"]];
	[_textfield_password setText:[_params valueForKey:@"password"]];
	[_textfield_domain setText:[_params valueForKey:@"domain"]];
}

- (void)viewDidLayoutSubviews
{
	[super viewDidLayoutSubviews];
	[self updateScrollViewContentSize];
}

- (void)viewDidUnload
{
	[super viewDidUnload];
}

- (void)viewDidDisappear:(BOOL)animated
{
	[super viewDidDisappear:animated];
	// set signal
	[[_session uiRequestCompleted] signal];
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
	return YES;
}

- (void)dealloc
{
	[[NSNotificationCenter defaultCenter] removeObserver:self];
	[super dealloc];
}

#pragma mark -
#pragma mark iOS Keyboard Notification Handlers

- (void)keyboardWillShow:(NSNotification *)notification
{
	CGRect keyboardScreenFrame =
	    [[[notification userInfo] objectForKey:UIKeyboardFrameEndUserInfoKey] CGRectValue];
	CGRect keyboardViewFrame = [[self view] convertRect:keyboardScreenFrame fromView:nil];
	CGRect overlapRect = CGRectIntersection([[self view] bounds], keyboardViewFrame);
	CGFloat overlap = CGRectIsNull(overlapRect) ? 0.0f : CGRectGetHeight(overlapRect);
	[self setKeyboardOverlap:overlap notification:notification];
}

- (void)keyboardWillHide:(NSNotification *)notification
{
	[self setKeyboardOverlap:0.0f notification:notification];
}

- (void)updateScrollViewContentSize
{
	CGFloat contentBottom = 0.0f;
	for (UIView *subview in [_scroll_view subviews])
		contentBottom = MAX(contentBottom, CGRectGetMaxY([subview frame]));

	CGSize contentSize = [_scroll_view bounds].size;
	contentSize.height = MAX(contentSize.height, contentBottom + 20.0f);
	[_scroll_view setContentSize:contentSize];
}

- (UIView *)activeCredentialField
{
	if ([_textfield_username isFirstResponder])
		return _textfield_username;
	if ([_textfield_password isFirstResponder])
		return _textfield_password;
	if ([_textfield_domain isFirstResponder])
		return _textfield_domain;
	return nil;
}

- (void)setKeyboardOverlap:(CGFloat)overlap notification:(NSNotification *)notification
{
	NSTimeInterval duration =
	    [[[notification userInfo] objectForKey:UIKeyboardAnimationDurationUserInfoKey] doubleValue];
	UIViewAnimationCurve curve = (UIViewAnimationCurve)[
	    [[notification userInfo] objectForKey:UIKeyboardAnimationCurveUserInfoKey] integerValue];
	UIViewAnimationOptions options =
	    UIViewAnimationOptionBeginFromCurrentState | (UIViewAnimationOptions)(curve << 16);

	[UIView animateWithDuration:duration
	                      delay:0.0
	                    options:options
	                 animations:^{
		                 UIEdgeInsets insets = [_scroll_view contentInset];
		                 insets.bottom = overlap;
		                 [_scroll_view setContentInset:insets];
		                 [_scroll_view setScrollIndicatorInsets:insets];

		                 UIView *activeField = [self activeCredentialField];
		                 if (activeField)
		                 {
			                 CGRect visibleRect = [activeField convertRect:[activeField bounds]
			                                                        toView:_scroll_view];
			                 visibleRect = CGRectInset(visibleRect, -12.0f, -16.0f);
			                 [_scroll_view scrollRectToVisible:visibleRect animated:NO];
		                 }
	                 }
	                 completion:nil];
}

#pragma mark - Action handlers

- (IBAction)loginPressed:(id)sender
{
	// read input back in
	[_params setValue:[_textfield_username text] forKey:@"username"];
	[_params setValue:[_textfield_password text] forKey:@"password"];
	[_params setValue:[_textfield_domain text] forKey:@"domain"];
	[_params setValue:[NSNumber numberWithBool:YES] forKey:@"result"];

	// dismiss controller
	[self dismissModalViewControllerAnimated:YES];
}

- (IBAction)cancelPressed:(id)sender
{
	[_params setValue:[NSNumber numberWithBool:NO] forKey:@"result"];

	// dismiss controller
	[self dismissModalViewControllerAnimated:YES];
}

@end
