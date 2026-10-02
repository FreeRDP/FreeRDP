/*
 Custom table cell with a label on the right, showing the current selection

 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import "EditSelectionTableViewCell.h"

@interface EditSelectionTableViewCell (Private)
- (void)updateContent;
@end

@implementation EditSelectionTableViewCell

@synthesize title = _title, value = _value, enabled = _enabled;

- (id)initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString *)reuseIdentifier
{
	self = [super initWithStyle:UITableViewCellStyleValue1 reuseIdentifier:reuseIdentifier];
	if (self)
	{
		// Initialization code
		_enabled = YES;
		[self setAccessoryType:UITableViewCellAccessoryDisclosureIndicator];
	}

	return self;
}

- (void)dealloc
{
	[_title release];
	[_value release];
	[super dealloc];
}

- (void)prepareForReuse
{
	[super prepareForReuse];

	[_title release];
	_title = nil;
	[_value release];
	_value = nil;
	_enabled = YES;
	[self setSelectionStyle:UITableViewCellSelectionStyleDefault];
	[self updateContent];
}

- (void)setTitle:(NSString *)title
{
	if (_title != title)
	{
		[_title release];
		_title = [title copy];
	}

	[self updateContent];
}

- (void)setValue:(NSString *)value
{
	if (_value != value)
	{
		[_value release];
		_value = [value copy];
	}

	[self updateContent];
}

- (void)setEnabled:(BOOL)enabled
{
	_enabled = enabled;
	[self updateContent];
}

- (void)updateContent
{
	UIListContentConfiguration *content = [self defaultContentConfiguration];
	[content setText:_title];
	[content setSecondaryText:_value];
	if (!_enabled)
	{
		[[content textProperties] setColor:[UIColor tertiaryLabelColor]];
		[[content secondaryTextProperties] setColor:[UIColor tertiaryLabelColor]];
	}

	[self setContentConfiguration:content];
}

@end
