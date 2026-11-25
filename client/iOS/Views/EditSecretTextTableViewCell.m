/*
 Custom table cell with secret edit text field

 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import "EditSecretTextTableViewCell.h"

@implementation EditSecretTextTableViewCell

- (id)initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString *)reuseIdentifier
{
	self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
	if (self)
	{
		// Initialization code

		// just set 'password' field
		[_textfield setSecureTextEntry:YES];
	}
	return self;
}

- (void)prepareForReuse
{
	[super prepareForReuse];
	[_textfield setSecureTextEntry:YES];
}

@end
