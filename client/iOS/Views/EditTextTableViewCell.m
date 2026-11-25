/*
 Custom table cell with edit text field

 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import "EditTextTableViewCell.h"

@implementation EditTextTableViewCell

@synthesize label = _label, textfield = _textfield, enabled = _enabled;

- (id)initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString *)reuseIdentifier
{
	self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
	if (self)
	{
		// Initialization code
		[self setSelectionStyle:UITableViewCellSelectionStyleNone];

		_label = [[UILabel alloc] init];
		[_label setFont:[UIFont preferredFontForTextStyle:UIFontTextStyleBody]];
		[_label setAdjustsFontForContentSizeCategory:YES];
		[_label setTranslatesAutoresizingMaskIntoConstraints:NO];
		[_label setContentHuggingPriority:UILayoutPriorityRequired
		                          forAxis:UILayoutConstraintAxisHorizontal];
		[_label setContentCompressionResistancePriority:UILayoutPriorityRequired
		                                        forAxis:UILayoutConstraintAxisHorizontal];

		// right editable field
		_textfield = [[UITextField alloc] init];
		[_textfield setFont:[UIFont preferredFontForTextStyle:UIFontTextStyleBody]];
		[_textfield setAdjustsFontForContentSizeCategory:YES];
		[_textfield setTextAlignment:NSTextAlignmentRight];
		[_textfield setClearButtonMode:UITextFieldViewModeWhileEditing];
		[_textfield setAutocorrectionType:UITextAutocorrectionTypeNo];
		[_textfield setAutocapitalizationType:UITextAutocapitalizationTypeNone];
		[_textfield setSpellCheckingType:UITextSpellCheckingTypeNo];
		[_textfield setTranslatesAutoresizingMaskIntoConstraints:NO];

		UIView *content = [self contentView];
		[content addSubview:_label];
		[content addSubview:_textfield];

		UILayoutGuide *margins = [content layoutMarginsGuide];
		[NSLayoutConstraint activateConstraints:@[
			[[_label leadingAnchor] constraintEqualToAnchor:[margins leadingAnchor]],
			[[_label topAnchor] constraintGreaterThanOrEqualToAnchor:[margins topAnchor]],
			[[_label bottomAnchor] constraintLessThanOrEqualToAnchor:[margins bottomAnchor]],
			[[_label centerYAnchor] constraintEqualToAnchor:[margins centerYAnchor]],
			[[_textfield leadingAnchor]
			    constraintEqualToSystemSpacingAfterAnchor:[_label trailingAnchor]
			                                   multiplier:1.0],
			[[_textfield trailingAnchor] constraintEqualToAnchor:[margins trailingAnchor]],
			[[_textfield topAnchor] constraintGreaterThanOrEqualToAnchor:[margins topAnchor]],
			[[_textfield bottomAnchor] constraintLessThanOrEqualToAnchor:[margins bottomAnchor]],
			[[_textfield centerYAnchor] constraintEqualToAnchor:[margins centerYAnchor]]
		]];

		[self setEnabled:YES];
	}
	return self;
}

- (void)dealloc
{
	[_label release];
	[_textfield release];
	[super dealloc];
}

- (void)prepareForReuse
{
	[super prepareForReuse];

	[_textfield setKeyboardType:UIKeyboardTypeDefault];
	[self setEnabled:YES];
}

- (void)setEnabled:(BOOL)enabled
{
	_enabled = enabled;

	// if 'disabled' set grayed out color
	[_label setTextColor:enabled ? [UIColor labelColor] : [UIColor tertiaryLabelColor]];
	[_textfield
	    setTextColor:enabled ? [UIColor secondaryLabelColor] : [UIColor tertiaryLabelColor]];
	[_textfield setEnabled:enabled];
}

@end
