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
		EditTextTableViewCell *textCell =
		    [[[EditTextTableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
		                                  reuseIdentifier:identifier] autorelease];
		[[textCell textfield] setDelegate:self];
		cell = textCell;
	}
	else if ([identifier isEqualToString:TableCellIdentifierSecretText])
	{
		EditSecretTextTableViewCell *secretCell =
		    [[[EditSecretTextTableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
		                                        reuseIdentifier:identifier] autorelease];
		[[secretCell textfield] setDelegate:self];
		cell = secretCell;
	}
	else if ([identifier isEqualToString:TableCellIdentifierYesNo])
	{
		cell = [[[EditFlagTableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
		                                     reuseIdentifier:identifier] autorelease];
	}
	else if ([identifier isEqualToString:TableCellIdentifierSelection])
	{
		cell = [[[EditSelectionTableViewCell alloc] initWithStyle:UITableViewCellStyleValue1
		                                          reuseIdentifier:identifier] autorelease];
	}
	else if ([identifier isEqualToString:TableCellIdentifierSubEditor])
	{
		cell = [[[EditSubEditTableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
		                                        reuseIdentifier:identifier] autorelease];
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

@end
