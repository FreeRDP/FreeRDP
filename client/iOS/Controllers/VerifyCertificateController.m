/*
 Certificate verification controller

 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import "VerifyCertificateController.h"
#import "RDPSession.h"

@implementation VerifyCertificateController

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
	}
	return self;
}

- (void)viewDidLoad
{
	[super viewDidLoad];

	NSString *message = NSLocalizedString(
	    @"The identity of the remote computer cannot be verified. Do you want to connect anyway?",
	    @"Verify certificate view message");

	// init strings
	[_label_message setText:message];
	[_label_for_issuer
	    setText:NSLocalizedString(@"Issuer:", @"Verify certificate view issuer label")];
	[_btn_accept setTitle:NSLocalizedString(@"Yes", @"Yes Button") forState:UIControlStateNormal];
	[_btn_decline setTitle:NSLocalizedString(@"No", @"No Button") forState:UIControlStateNormal];

	[_label_issuer setText:[_params valueForKey:@"issuer"]];
}

- (void)viewDidUnload
{
	[super viewDidUnload];
	// Release any retained subviews of the main view.
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

#pragma mark - Action handlers

- (IBAction)acceptPressed:(id)sender
{
	[_params setValue:[NSNumber numberWithBool:YES] forKey:@"result"];

	// dismiss controller
	[self dismissModalViewControllerAnimated:YES];
}

- (IBAction)declinePressed:(id)sender
{
	[_params setValue:[NSNumber numberWithBool:NO] forKey:@"result"];

	// dismiss controller
	[self dismissModalViewControllerAnimated:YES];
}

@end
