/*
 Basic interface for settings editors

 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import "EditorBaseController.h"

@interface EditorBaseController ()

@end

NSString *TableCellIdentifierText = @"cellIdText";
NSString *TableCellIdentifierSecretText = @"cellIdSecretText";
NSString *TableCellIdentifierYesNo = @"cellIdYesNo";
NSString *TableCellIdentifierSelection = @"cellIdSelection";
NSString *TableCellIdentifierSubEditor = @"cellIdSubEditor";
NSString *TableCellIdentifierMultiChoice = @"cellIdMultiChoice";
NSString *TableCellIdentifierButton = @"cellIdButton";

@implementation EditorBaseController

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
	return YES;
}

#pragma mark - Create table view cells
- (UITableViewCell *)tableViewCellFromIdentifier:(NSString *)identifier
{
	// try to reuse a cell
	UITableViewCell *cell = [[self tableView] dequeueReusableCellWithIdentifier:identifier];
	if (cell != nil)
		return cell;

	// we have to create a new cell
	if ([identifier isEqualToString:TableCellIdentifierText])
	{
		[[NSBundle mainBundle] loadNibNamed:@"EditTextTableViewCell" owner:self options:nil];
		cell = _textTableViewCell;
		_textTableViewCell = nil;
	}
	else if ([identifier isEqualToString:TableCellIdentifierSecretText])
	{
		[[NSBundle mainBundle] loadNibNamed:@"EditSecretTextTableViewCell" owner:self options:nil];
		cell = _secretTextTableViewCell;
		_secretTextTableViewCell = nil;
	}
	else if ([identifier isEqualToString:TableCellIdentifierYesNo])
	{
		[[NSBundle mainBundle] loadNibNamed:@"EditFlagTableViewCell" owner:self options:nil];
		cell = _flagTableViewCell;
		_flagTableViewCell = nil;
	}
	else if ([identifier isEqualToString:TableCellIdentifierSelection])
	{
		[[NSBundle mainBundle] loadNibNamed:@"EditSelectionTableViewCell" owner:self options:nil];
		cell = _selectionTableViewCell;
		_selectionTableViewCell = nil;
	}
	else if ([identifier isEqualToString:TableCellIdentifierSubEditor])
	{
		[[NSBundle mainBundle] loadNibNamed:@"EditSubEditTableViewCell" owner:self options:nil];
		cell = _subEditTableViewCell;
		_subEditTableViewCell = nil;
	}
	else if ([identifier isEqualToString:TableCellIdentifierButton])
	{
		[[NSBundle mainBundle] loadNibNamed:@"EditButtonTableViewCell" owner:self options:nil];
		cell = _buttonTableViewCell;
		_buttonTableViewCell = nil;
	}
	else if ([identifier isEqualToString:TableCellIdentifierMultiChoice])
	{
		cell = [[[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1
		                               reuseIdentifier:identifier] autorelease];
	}
	else
	{
		NSAssert(false, @"Unknown table cell identifier");
	}

	return cell;
}

#pragma mark - Utility functions
- (void)adjustEditTextTableViewCell:(EditTextTableViewCell *)cell
{
	UILabel *label = [cell label];
	UITextField *textField = [cell textfield];

	// adjust label
	CGFloat width = [[label text] sizeWithFont:[label font]].width;
	CGRect frame = [label frame];
	CGFloat delta = width - frame.size.width;
	frame.size.width = width;
	[label setFrame:frame];

	// adjust text field
	frame = [textField frame];
	frame.origin.x += delta;
	frame.size.width -= delta;
	[textField setFrame:frame];
}

@end
