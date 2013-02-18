/*
 Generic controller to select a single item from a list of options
 
 Copyright 2013 Thinstuff Technologies GmbH, Author: Martin Fleisz
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#import "EditorSelectionController.h"
#import "ConnectionParams.h"
#import "OrderedDictionary.h"

@interface EditorSelectionController (Private)
- (OrderedDictionary*)selectionForIndex:(int)index;
@end

@implementation EditorSelectionController

- (id)initWithConnectionParams:(ConnectionParams*)params entries:(NSArray *)entries selections:(NSArray *)selections
{
    self = [super initWithStyle:UITableViewStyleGrouped];
    if (self)
    {
        _params = [params retain];
        _entries = [entries retain];
        _selections = [selections retain];
        
        // allocate and init current selections array
        _cur_selections = [[NSMutableArray alloc] initWithCapacity:[_entries count]];        
        for (int i = 0; i < [entries count]; ++i)
        {
            NSString* entry = [entries objectAtIndex:i];            
            if([_params hasValueForKeyPath:entry])
            {
                NSUInteger idx = [(OrderedDictionary*)[selections objectAtIndex:i] indexForValue:[NSNumber numberWithInt:[_params intForKeyPath:entry]]];
                [_cur_selections addObject:[NSNumber numberWithInt:(idx != NSNotFound ? idx : 0)]];
            }
            else
                [_cur_selections addObject:[NSNumber numberWithInt:0]];
        }
    }
    return self;
}

- (void)didReceiveMemoryWarning
{
    // Releases the view if it doesn't have a superview.
    [super didReceiveMemoryWarning];
    
    // Release any cached data, images, etc that aren't in use.
    [_params autorelease];
    [_entries autorelease];
    [_selections autorelease];    
    [_cur_selections autorelease];
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
    // Return the number of sections.
    return [_entries count];
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    // Return the number of rows in the section.
    return [[self selectionForIndex:section] count];
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    UITableViewCell *cell = [self tableViewCellFromIdentifier:TableCellIdentifierMultiChoice];
    
    // get selection
    OrderedDictionary* selection = [self selectionForIndex:[indexPath section]];
    
    // set cell properties
	[[cell textLabel] setText:[selection keyAtIndex:[indexPath row]]];
	
	// set default checkmark
	if([indexPath row] == [[_cur_selections objectAtIndex:[indexPath section]] intValue])
		[cell setAccessoryType:UITableViewCellAccessoryCheckmark];
	else
		[cell setAccessoryType:UITableViewCellAccessoryNone];
	
    return cell;
}

#pragma mark - Table view delegate

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
	// has selection change?
    int cur_selection = [[_cur_selections objectAtIndex:[indexPath section]] intValue];
	if([indexPath row] != cur_selection)
	{
		[tableView deselectRowAtIndexPath:indexPath animated:NO];
		
		NSIndexPath* oldIndexPath = [NSIndexPath indexPathForRow:cur_selection inSection:[indexPath section]];
        
		// clear old checkmark
		UITableViewCell* old_sel_cell = [tableView cellForRowAtIndexPath:oldIndexPath];
		old_sel_cell.accessoryType = UITableViewCellAccessoryNone;
        
		// set new checkmark
		UITableViewCell* new_sel_cell = [tableView cellForRowAtIndexPath:indexPath];
		new_sel_cell.accessoryType = UITableViewCellAccessoryCheckmark;

        // get value from selection dictionary
        OrderedDictionary* dict = [self selectionForIndex:[indexPath section]];
        int sel_value = [[dict valueForKey:[dict keyAtIndex:[indexPath row]]] intValue];

		// update selection index and params value
        [_cur_selections replaceObjectAtIndex:[indexPath section] withObject:[NSNumber numberWithInt:[indexPath row]]];
        [_params setInt:sel_value forKeyPath:[_entries objectAtIndex:[indexPath section]]];
    }    
}

#pragma mark - Convenience functions

- (OrderedDictionary*)selectionForIndex:(int)index
{
    return (OrderedDictionary*)[_selections objectAtIndex:index];
}

@end
