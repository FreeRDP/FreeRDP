/*
 Custom table cell with toggle switch

 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import "EditFlagTableViewCell.h"

@implementation EditFlagTableViewCell

@synthesize title = _title, toggle = _toggle;

- (id)initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString *)reuseIdentifier
{
	self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
	if (self)
	{
		[self setSelectionStyle:UITableViewCellSelectionStyleNone];

		_toggle = [[UISwitch alloc] init];
		[self setAccessoryView:_toggle];
	}

	return self;
}

- (void)dealloc
{
	[_title release];
	[_toggle release];
	[super dealloc];
}

- (void)setTitle:(NSString *)title
{
	if (_title != title)
	{
		[_title release];
		_title = [title copy];
	}

	UIListContentConfiguration *content = [self defaultContentConfiguration];
	[content setText:title];
	[self setContentConfiguration:content];
}

@end
