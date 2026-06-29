/*
 Custom table cell with toggle switch

 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import "EditFlagTableViewCell.h"

@implementation EditFlagTableViewCell

@synthesize label = _label, toggle = _toggle;

- (id)initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString *)reuseIdentifier
{
	self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
	if (self)
	{
		// Initialization code
	}
	return self;
}

- (void)setSelected:(BOOL)selected animated:(BOOL)animated
{
	[super setSelected:selected animated:animated];

	// Configure the view for the selected state
}

// set toggle switch layout margin
- (void)layoutSubviews
{
	[super layoutSubviews];

	if (!_toggle)
		return;

	CGRect bounds = [[self contentView] bounds];
	UIEdgeInsets margins = [[self contentView] layoutMargins];
	CGSize sw = [_toggle frame].size;

	[_toggle setFrame:CGRectMake(CGRectGetMaxX(bounds) - margins.right - sw.width,
	                             (bounds.size.height - sw.height) / 2.0, sw.width, sw.height)];

	if (_label)
	{
		CGRect lf = [_label frame];
		lf.origin.x = margins.left;
		lf.origin.y = (bounds.size.height - lf.size.height) / 2.0;
		lf.size.width = CGRectGetMinX([_toggle frame]) - margins.left - 8.0;
		[_label setFrame:lf];
	}
}

@end
