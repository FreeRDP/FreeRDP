/*
 Credentials input controller
 
 Copyright 2013 Thinstuff Technologies GmbH, Author: Martin Fleisz
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#import "CredentialsInputController.h"
#import "RDPSession.h"
#import "Utils.h"

@implementation CredentialsInputController

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil session:(RDPSession *)session params:(NSMutableDictionary *)params
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        _session = session;
        _params = params;
        [self setModalPresentationStyle:UIModalPresentationFormSheet];

        // on iphone we have the problem that the buttons are hidden by the keyboard
        // we solve this issue by registering keyboard notification handlers and adjusting the scrollview accordingly
        if (IsPhone())
        {
            [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(keyboardWillShow:) name: UIKeyboardWillShowNotification object:nil];
            [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(keyboardWillHide:) name: UIKeyboardWillHideNotification object:nil];            
        }
    }
    return self;
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    // set localized strings
    [_lbl_message setText:NSLocalizedString(@"Please provide the missing user information in order to proceed and login.", @"Credentials input view message")];
    [_textfield_username setPlaceholder:NSLocalizedString(@"Username", @"Credentials Input Username hint")];
    [_textfield_password setPlaceholder:NSLocalizedString(@"Password", @"Credentials Input Password hint")];
    [_textfield_domain setPlaceholder:NSLocalizedString(@"Domain", @"Credentials Input Domain hint")];
    [_btn_login setTitle:NSLocalizedString(@"Login", @"Login Button") forState:UIControlStateNormal];
    [_btn_cancel setTitle:NSLocalizedString(@"Cancel", @"Cancel Button") forState:UIControlStateNormal];

    // init scrollview content size
    [_scroll_view setContentSize:[_scroll_view frame].size];
    
    // set params in the view
    [_textfield_username setText:[_params valueForKey:@"username"]];
    [_textfield_password setText:[_params valueForKey:@"password"]];
    [_textfield_domain setText:[_params valueForKey:@"domain"]];
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
    [super dealloc];
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

#pragma mark -
#pragma mark iOS Keyboard Notification Handlers

- (void)keyboardWillShow:(NSNotification *)notification
{   
	CGRect keyboardEndFrame = [[[notification userInfo] objectForKey:UIKeyboardFrameEndUserInfoKey] CGRectValue];
    CGRect keyboardFrame = [[self view] convertRect:keyboardEndFrame toView:nil];
    
    [UIView beginAnimations:nil context:NULL];
    [UIView setAnimationCurve:[[[notification userInfo] objectForKey:UIKeyboardAnimationCurveUserInfoKey] intValue]];
    [UIView setAnimationDuration:[[[notification userInfo] objectForKey:UIKeyboardAnimationDurationUserInfoKey] doubleValue]];
	CGRect frame = [_scroll_view frame];
	frame.size.height -=  keyboardFrame.size.height;
	[_scroll_view setFrame:frame];
	[UIView commitAnimations];    
}

- (void)keyboardWillHide:(NSNotification *)notification
{
	CGRect keyboardEndFrame = [[[notification userInfo] objectForKey:UIKeyboardFrameEndUserInfoKey] CGRectValue];
    CGRect keyboardFrame = [[self view] convertRect:keyboardEndFrame toView:nil];

    [UIView beginAnimations:nil context:NULL];
    [UIView setAnimationCurve:[[[notification userInfo] objectForKey:UIKeyboardAnimationCurveUserInfoKey] intValue]];
    [UIView setAnimationDuration:[[[notification userInfo] objectForKey:UIKeyboardAnimationDurationUserInfoKey] doubleValue]];
	CGRect frame = [_scroll_view frame];
	frame.size.height +=  keyboardFrame.size.height;
	[_scroll_view setFrame:frame];
    [UIView commitAnimations];
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
