/*
 Controller to edit bookmark credentials
 
 Copyright 2013 Thinstuff Technologies GmbH, Author: Martin Fleisz
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#import "CredentialsEditorController.h"
#import "Bookmark.h"
#import "Utils.h"

@interface CredentialsEditorController ()

@end

#define SECTION_CREDENTIALS 0
#define SECTION_COUNT 1

@implementation CredentialsEditorController

- (id)initWithBookmark:(ComputerBookmark*)bookmark
{
    if ((self = [super initWithStyle:UITableViewStyleGrouped])) 
	{
		// set additional settings state according to bookmark data
		_bookmark = [bookmark retain];
        _params = [bookmark params];        
    }
    return self;
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    [self setTitle:NSLocalizedString(@"Credentials", @"Credentials title")];
}

- (void)viewDidUnload
{
    [super viewDidUnload];
    // Release any retained subviews of the main view.
}

- (void)viewWillDisappear:(BOOL)animated
{
    [super viewWillDisappear:animated];
    
    // foce any active editing to stop
    [[self view] endEditing:NO];
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
    return YES;
}

- (void)dealloc
{
    [super dealloc];
    [_bookmark release];
}

#pragma mark -
#pragma mark Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
    // Return the number of sections.
    return SECTION_COUNT;
}


- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    // Return the number of rows in the section.
	switch (section)
	{
		case SECTION_CREDENTIALS: // credentials
			return 3;
		default:
			break;
	}
	
    return 0;
}


// set section headers
- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section
{
	switch(section)
	{
		case SECTION_CREDENTIALS:
			return NSLocalizedString(@"Credentials", @"'Credentials': credentials settings header");
	}
	return @"unknown";
}


// Customize the appearance of table view cells.
- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    
	// determine the required cell type
	NSString* cellType = nil;
	switch([indexPath section])
	{
		case SECTION_CREDENTIALS: // credentials
			if([indexPath row] == 1)
                cellType = TableCellIdentifierSecretText; // password field
            else
                cellType = TableCellIdentifierText;
			break;
                        
        default:
            break;
	}	
	NSAssert(cellType != nil, @"Couldn't determine cell type");	
	
	// get the table view cell
	UITableViewCell *cell = [self tableViewCellFromIdentifier:cellType];
	NSAssert(cell, @"Invalid cell");	
    
	// set cell values
	switch([indexPath section]) 
	{			
            // credentials
		case SECTION_CREDENTIALS:
			[self initCredentialSettings:indexPath cell:cell];
			break;            
            
		default:
			break;
	}	
    
    return cell;
}


// updates credentials in the UI
- (void)initCredentialSettings:(NSIndexPath*)indexPath cell:(UITableViewCell*)cell
{
	switch(indexPath.row) 
	{
		case 0:
        {
            EditTextTableViewCell* textCell = (EditTextTableViewCell*)cell;
            [[textCell textfield] setTag:GET_TAG_FROM_PATH(indexPath)];
            [[textCell label] setText:NSLocalizedString(@"Username", @"'Username': Bookmark username")];
			[[textCell textfield] setText:[_params StringForKey:@"username"]];
            [[textCell textfield] setPlaceholder:NSLocalizedString(@"not set", @"not set placeholder")];
			break;            
        }
		case 1:
        {
            EditSecretTextTableViewCell* textCell = (EditSecretTextTableViewCell*)cell;
            [[textCell textfield] setTag:GET_TAG_FROM_PATH(indexPath)];            
			[[textCell label] setText:NSLocalizedString(@"Password", @"'Password': Bookmark password")];
			[[textCell textfield] setText:[_params StringForKey:@"password"]];
            [[textCell textfield] setPlaceholder:NSLocalizedString(@"not set", @"not set placeholder")];
			break;
        }
		case 2:
        {
            EditTextTableViewCell* textCell = (EditTextTableViewCell*)cell;
            [[textCell textfield] setTag:GET_TAG_FROM_PATH(indexPath)];
			[[textCell label] setText:NSLocalizedString(@"Domain", @"'Domain': Bookmark domain")];
			[[textCell textfield] setText:[_params StringForKey:@"domain"]];
            [[textCell textfield] setPlaceholder:NSLocalizedString(@"not set", @"not set placeholder")];
			break;            
        }
		default:
			NSLog(@"Invalid row index in settings table!");
			break;
	}    
}

#pragma mark -
#pragma mark Text Field delegate

- (BOOL)textFieldShouldReturn:(UITextField*)textField
{
	[textField resignFirstResponder];
	return NO;
}

- (BOOL)textFieldShouldEndEditing:(UITextField *)textField
{
	switch(textField.tag)
	{            
            // update credentials settings
		case GET_TAG(SECTION_CREDENTIALS, 0):
			[_params setValue:[textField text] forKey:@"username"];
			break;
            			
		case GET_TAG(SECTION_CREDENTIALS, 1):
			[_params setValue:[textField text] forKey:@"password"];
			break;

		case GET_TAG(SECTION_CREDENTIALS, 2):
			[_params setValue:[textField text] forKey:@"domain"];
			break;
            
		default:
			break;
	}
	return YES;
}

@end
