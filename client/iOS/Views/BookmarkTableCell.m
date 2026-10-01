/*
 Custom bookmark table cell

 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import "BookmarkTableCell.h"

@interface BookmarkTableCell (Private)
- (void)updateContent;
@end

@implementation BookmarkTableCell

@synthesize title = _title, subTitle = _sub_title;

- (id)initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString *)reuseIdentifier
{
	self = [super initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:reuseIdentifier];
	if (self)
	{
		// Initialization code
		[self setAccessoryType:UITableViewCellAccessoryDetailButton];
	}
	return self;
}

- (void)dealloc
{
	[_title release];
	[_sub_title release];
	[super dealloc];
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

- (void)setSubTitle:(NSString *)subTitle
{
	if (_sub_title != subTitle)
	{
		[_sub_title release];
		_sub_title = [subTitle copy];
	}

	[self updateContent];
}

- (void)updateContent
{
	UIListContentConfiguration *content = [UIListContentConfiguration subtitleCellConfiguration];
	[content setText:_title];
	[content setSecondaryText:_sub_title];
	[[content secondaryTextProperties] setColor:[UIColor secondaryLabelColor]];
	[self setContentConfiguration:content];
}

@end
