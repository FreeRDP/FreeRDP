/*
 Custom table cell with secret edit text field

 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import "EditSecretTextTableViewCell.h"

@implementation EditSecretTextTableViewCell

@synthesize label = _label, textfield = _textfield;

- (void)awakeFromNib
{
	[super awakeFromNib];
	[_unhide_button setTitle:NSLocalizedString(@"Unhide", @"Button title 'Unhide'")
	                forState:UIControlStateNormal];
	[_unhide_button addTarget:self
	                   action:@selector(togglePasswordMode:)
	         forControlEvents:UIControlEventTouchUpInside];
}

- (void)setEnabled:(BOOL)enabled
{
	[_label setEnabled:enabled];
	[_textfield setEnabled:enabled];
	[_unhide_button setEnabled:enabled];
}

#pragma mark - action handlers
- (void)togglePasswordMode:(id)sender
{
	BOOL isSecure = [_textfield isSecureTextEntry];

	if (isSecure)
	{
		[_unhide_button setTitle:NSLocalizedString(@"Hide", @"Button title 'Hide'")
		                forState:UIControlStateNormal];
		[_textfield setSecureTextEntry:NO];
	}
	else
	{
		BOOL first_responder = [_textfield isFirstResponder];
		// little trick to make non-secure to secure transition working - this seems to be an ios
		// bug:
		// http://stackoverflow.com/questions/6710019/uitextfield-securetextentry-works-going-from-yes-to-no-but-changing-back-to-y
		[_textfield setEnabled:NO];
		[_unhide_button setTitle:NSLocalizedString(@"Unhide", @"Button title 'Unhide'")
		                forState:UIControlStateNormal];
		[_textfield setSecureTextEntry:YES];
		[_textfield setEnabled:YES];
		if (first_responder)
			[_textfield becomeFirstResponder];
	}
}

@end
