/*
 Custom table cell indicating a switch to a sub-view

 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import "EditSubEditTableViewCell.h"

@interface EditSubEditTableViewCell (Private)
- (void)updateContent;
@end

@implementation EditSubEditTableViewCell

@synthesize title = _title, enabled = _enabled;

- (id)initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString *)reuseIdentifier
{
	self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
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
	[super dealloc];
}

- (void)prepareForReuse
{
	[super prepareForReuse];

	[_title release];
	_title = nil;
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

- (void)setEnabled:(BOOL)enabled
{
	_enabled = enabled;
	[self updateContent];
}

- (void)updateContent
{
	UIListContentConfiguration *content = [self defaultContentConfiguration];
	[content setText:_title];
	if (!_enabled)
		[[content textProperties] setColor:[UIColor tertiaryLabelColor]];
	[self setContentConfiguration:content];
}

@end
