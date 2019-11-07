/*
 controller for screen settings selection

 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import "ScreenSelectionController.h"
#import "Utils.h"
#import "OrderedDictionary.h"
#import "ConnectionParams.h"

@interface ScreenSelectionController (Private)
- (NSString *)keyPathForKey:(NSString *)key;
@end

@implementation ScreenSelectionController

- (id)initWithConnectionParams:(ConnectionParams *)params
{
	return [self initWithConnectionParams:params keyPath:nil];
}

- (id)initWithConnectionParams:(ConnectionParams *)params keyPath:(NSString *)keyPath
{
	self = [super initWithStyle:UITableViewStyleGrouped];
	if (self)
	{
		_params = [params retain];
		_keyPath = (keyPath != nil ? [keyPath retain] : nil);

		_color_options = (OrderedDictionary *)[SelectionForColorSetting() retain];
		_resolution_modes = [ResolutionModes() retain];

		// init current selections
		NSUInteger idx = [_color_options
		    indexForValue:[NSNumber
		                      numberWithInt:[_params
		                                        intForKeyPath:[self keyPathForKey:@"colors"]]]];
		_selection_color = (idx != NSNotFound) ? idx : 0;

		idx = [_resolution_modes
		    indexOfObject:ScreenResolutionDescription(
		                      [_params
		                          intForKeyPath:[self keyPathForKey:@"screen_resolution_type"]],
		                      [_params intForKeyPath:[self keyPathForKey:@"width"]],
		                      [_params intForKeyPath:[self keyPathForKey:@"height"]])];
		_selection_resolution = (idx != NSNotFound) ? idx : 0;
	}
	return self;
}

- (void)dealloc
{
	[super dealloc];
	[_params autorelease];
	[_keyPath autorelease];
	[_color_options autorelease];
	[_resolution_modes autorelease];
}

- (NSString *)keyPathForKey:(NSString *)key
{
	if (_keyPath)
		return [_keyPath stringByAppendingFormat:@".%@", key];
	return key;
}

#pragma mark - View lifecycle

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
	// Return YES for supported orientations
	return YES;
}

#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
	return 2;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
	// Return the number of rows in the section.
	if (section == 0)
		return [_color_options count];
	return [_resolution_modes count] + 2; // +2 for custom width/height input fields
}

- (UITableViewCell *)tableView:(UITableView *)tableView
         cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
	UITableViewCell *cell = nil;
	switch ([indexPath section])
	{
		case 0:
			cell = [self tableViewCellFromIdentifier:TableCellIdentifierMultiChoice];
			[[cell textLabel] setText:[_color_options keyAtIndex:[indexPath row]]];
			break;

		case 1:
			if ([indexPath row] < [_resolution_modes count])
			{
				cell = [self tableViewCellFromIdentifier:TableCellIdentifierMultiChoice];
				[[cell textLabel] setText:[_resolution_modes objectAtIndex:[indexPath row]]];
			}
			else
				cell = [self tableViewCellFromIdentifier:TableCellIdentifierText];
			break;

		default:
			break;
	}

	if ([indexPath section] == 1)
	{
		BOOL enabled = ([_params intForKeyPath:[self keyPathForKey:@"screen_resolution_type"]] ==
		                TSXScreenOptionCustom);
		if ([indexPath row] == [_resolution_modes count])
		{
			int value = [_params intForKeyPath:[self keyPathForKey:@"width"]];
			EditTextTableViewCell *textCell = (EditTextTableViewCell *)cell;
			[[textCell label] setText:NSLocalizedString(@"Width", @"Custom Screen Width")];
			[[textCell textfield] setText:[NSString stringWithFormat:@"%d", value ? value : 800]];
			[[textCell textfield] setKeyboardType:UIKeyboardTypeNumberPad];
			[[textCell label] setEnabled:enabled];
			[[textCell textfield] setEnabled:enabled];
			[[textCell textfield] setTag:1];
		}
		else if ([indexPath row] == ([_resolution_modes count] + 1))
		{
			int value = [_params intForKeyPath:[self keyPathForKey:@"height"]];
			EditTextTableViewCell *textCell = (EditTextTableViewCell *)cell;
			[[textCell label] setText:NSLocalizedString(@"Height", @"Custom Screen Height")];
			[[textCell textfield] setText:[NSString stringWithFormat:@"%d", value ? value : 600]];
			[[textCell textfield] setKeyboardType:UIKeyboardTypeNumberPad];
			[[textCell label] setEnabled:enabled];
			[[textCell textfield] setEnabled:enabled];
			[[textCell textfield] setTag:2];
		}
	}

	// set default checkmark
	if ([indexPath row] == ([indexPath section] == 0 ? _selection_color : _selection_resolution))
		[cell setAccessoryType:UITableViewCellAccessoryCheckmark];
	else
		[cell setAccessoryType:UITableViewCellAccessoryNone];

	return cell;
}

#pragma mark - Table view delegate

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
	// custom widht/height cells are not selectable
	if ([indexPath section] == 1 && [indexPath row] >= [_resolution_modes count])
		return;

	// has selection change?
	int cur_selection = ([indexPath section] == 0 ? _selection_color : _selection_resolution);
	if ([indexPath row] != cur_selection)
	{
		[tableView deselectRowAtIndexPath:indexPath animated:NO];

		NSIndexPath *oldIndexPath = [NSIndexPath indexPathForRow:cur_selection
		                                               inSection:[indexPath section]];

		// clear old checkmark
		UITableViewCell *old_sel_cell = [tableView cellForRowAtIndexPath:oldIndexPath];
		old_sel_cell.accessoryType = UITableViewCellAccessoryNone;

		// set new checkmark
		UITableViewCell *new_sel_cell = [tableView cellForRowAtIndexPath:indexPath];
		new_sel_cell.accessoryType = UITableViewCellAccessoryCheckmark;

		if ([indexPath section] == 0)
		{
			// get value from color dictionary
			int sel_value =
			    [[_color_options valueForKey:[_color_options keyAtIndex:[indexPath row]]] intValue];

			// update selection index and params value
			[_params setInt:sel_value forKeyPath:[self keyPathForKey:@"colors"]];
			_selection_color = [indexPath row];
		}
		else
		{
			// update selection index and params value
			int width, height;
			TSXScreenOptions mode;
			ScanScreenResolution([_resolution_modes objectAtIndex:[indexPath row]], &width, &height,
			                     &mode);
			[_params setInt:mode forKeyPath:[self keyPathForKey:@"screen_resolution_type"]];
			if (mode != TSXScreenOptionCustom)
			{
				[_params setInt:width forKeyPath:[self keyPathForKey:@"width"]];
				[_params setInt:height forKeyPath:[self keyPathForKey:@"height"]];
			}
			_selection_resolution = [indexPath row];

			// refresh width/height edit fields if custom selection changed
			NSArray *indexPaths = [NSArray
			    arrayWithObjects:[NSIndexPath indexPathForRow:[_resolution_modes count]
			                                        inSection:1],
			                     [NSIndexPath indexPathForRow:([_resolution_modes count] + 1)
			                                        inSection:1],
			                     nil];
			[[self tableView] reloadRowsAtIndexPaths:indexPaths
			                        withRowAnimation:UITableViewRowAnimationNone];
		}
	}
}

#pragma mark -
#pragma mark Text Field delegate

- (BOOL)textFieldShouldReturn:(UITextField *)textField
{
	[textField resignFirstResponder];
	return NO;
}

- (BOOL)textFieldShouldEndEditing:(UITextField *)textField
{

	switch ([textField tag])
	{
			// update resolution settings (and check for invalid input)
		case 1:
			if ([[textField text] intValue] < 640)
				[textField setText:@"640"];
			[_params setInt:[[textField text] intValue] forKeyPath:[self keyPathForKey:@"width"]];
			break;

		case 2:
			if ([[textField text] intValue] < 480)
				[textField setText:@"480"];
			[_params setInt:[[textField text] intValue] forKeyPath:[self keyPathForKey:@"height"]];
			break;

		default:
			break;
	}
	return YES;
}

@end
