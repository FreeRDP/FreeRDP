/*
 Bookmark editor controller
 
 Copyright 2013 Thinstuff Technologies GmbH, Author: Martin Fleisz
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#import "BookmarkEditorController.h"
#import "Bookmark.h"
#import "Utils.h"
#import "ScreenSelectionController.h"
#import "PerformanceEditorController.h"
#import "CredentialsEditorController.h"
#import "AdvancedBookmarkEditorController.h"
#import "BlockAlertView.h"

@implementation BookmarkEditorController

@synthesize delegate;

#define SECTION_SERVER 0
#define SECTION_CREDENTIALS 1
#define SECTION_SETTINGS 2
#define SECTION_COUNT 3

#pragma mark -
#pragma mark Initialization	

- (id)initWithBookmark:(ComputerBookmark*)bookmark
{
    if ((self = [super initWithStyle:UITableViewStyleGrouped])) 
	{
		// set additional settings state according to bookmark data
        if ([[bookmark uuid] length] == 0)
            _bookmark = [bookmark copy];
        else
            _bookmark = [bookmark copyWithUUID];
        _params = [_bookmark params];

		// if this is a TSX Connect bookmark - disable server settings
		if([_bookmark isKindOfClass:NSClassFromString(@"TSXConnectComputerBookmark")])
			_display_server_settings = NO;
		else
			_display_server_settings = YES;
    }
    return self;
}


#pragma mark -
#pragma mark View lifecycle


- (void)viewDidLoad {
    [super viewDidLoad];

    // replace back button with a custom handler that checks if the required bookmark settings were specified
    UIBarButtonItem* saveButton = [[[UIBarButtonItem alloc] initWithTitle:NSLocalizedString(@"Save", @"Save Button title") style:UIBarButtonItemStyleDone target:self action:@selector(handleSave:)] autorelease];
    UIBarButtonItem* cancelButton = [[[UIBarButtonItem alloc] initWithTitle:NSLocalizedString(@"Cancel", @"Cancel Button title") style:UIBarButtonItemStyleBordered target:self action:@selector(handleCancel:)] autorelease];
    [[self navigationItem] setLeftBarButtonItem:cancelButton];    
    [[self navigationItem] setRightBarButtonItem:saveButton];    
}

- (void)viewWillAppear:(BOOL)animated {
    [super viewWillAppear:animated];

    // we need to reload the table view data here to have up-to-date data for the 
    // advanced settings accessory items (like for resolution/color mode settings)
    [[self tableView] reloadData];
}

// Override to allow orientations other than the default portrait orientation.
- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation {
    return YES;
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
		case SECTION_SERVER: // server settings
			return (_display_server_settings ? 3 : 0);
		case SECTION_CREDENTIALS: // credentials
			return 1;
		case SECTION_SETTINGS: // session settings
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
		case SECTION_SERVER:
			return (_display_server_settings ? NSLocalizedString(@"Host", @"'Host': host settings header") : nil);
		case SECTION_CREDENTIALS:
			return NSLocalizedString(@"Credentials", @"'Credentials': credentials settings header");
		case SECTION_SETTINGS:
			return NSLocalizedString(@"Settings", @"'Session Settings': session settings header");
	}
	return @"unknown";
}


// Customize the appearance of table view cells.
- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {

	// determine the required cell type
	NSString* cellType = nil;
	switch([indexPath section])
	{
		case SECTION_SERVER:
            cellType = TableCellIdentifierText;
			break;

        case SECTION_CREDENTIALS:
            cellType = TableCellIdentifierSelection;
            break;
            
		case SECTION_SETTINGS: // settings
			{
				switch([indexPath row]) 
				{
					case 0:	// screen/color depth
						cellType = TableCellIdentifierSelection;
						break;
					case 1:	// performance settings
                    case 2: // advanced settings
						cellType = TableCellIdentifierSubEditor;
						break;		
					default:						
						break;
				}
			}
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
		// server settings
		case SECTION_SERVER:
			[self initServerSettings:indexPath cell:cell];
			break;
			
		// credentials
		case SECTION_CREDENTIALS:
			[self initCredentialSettings:indexPath cell:cell];
			break;

		// session settings
		case SECTION_SETTINGS:
			[self initSettings:indexPath cell:cell];
			break;

		default:
			break;
	}	
    
    return cell;
}

// updates server settings in the UI
- (void)initServerSettings:(NSIndexPath*)indexPath cell:(UITableViewCell*)cell
{
    EditTextTableViewCell* textCell = (EditTextTableViewCell*)cell;
	[[textCell textfield] setTag:GET_TAG_FROM_PATH(indexPath)];
	switch([indexPath row]) 
	{
		case 0:
			[[textCell label] setText:NSLocalizedString(@"Label", @"'Label': Bookmark label")];
			[[textCell textfield] setText:[_bookmark label]];
            [[textCell textfield] setPlaceholder:NSLocalizedString(@"not set", @"not set placeholder")];
			break;
		case 1:
			[[textCell label] setText:NSLocalizedString(@"Host", @"'Host': Bookmark hostname")];
			[[textCell textfield] setText:[_params StringForKey:@"hostname"]];
            [[textCell textfield] setPlaceholder:NSLocalizedString(@"not set", @"not set placeholder")];
			break;
		case 2:
			[[textCell label] setText:NSLocalizedString(@"Port", @"'Port': Bookmark port")];
			[[textCell textfield] setText:[NSString stringWithFormat:@"%d", [_params intForKey:@"port"]]];
            [[textCell textfield] setKeyboardType:UIKeyboardTypeNumberPad];
			break;
		default:
			NSLog(@"Invalid row index in settings table!");
			break;
	}	

    [self adjustEditTextTableViewCell:textCell];
}

// updates credentials in the UI
- (void)initCredentialSettings:(NSIndexPath*)indexPath cell:(UITableViewCell*)cell
{
    EditSelectionTableViewCell* selCell = (EditSelectionTableViewCell*)cell;
	switch(indexPath.row) 
	{
		case 0:
            [[selCell label] setText:NSLocalizedString(@"Credentials", @"'Credentials': Bookmark credentials")];
            [[selCell selection] setText:[_params StringForKey:@"username"]];
			break;
		default:
			NSLog(@"Invalid row index in settings table!");
			break;
	}
}

// updates session settings in the UI
- (void)initSettings:(NSIndexPath*)indexPath cell:(UITableViewCell*)cell
{	
	switch(indexPath.row) 
	{
		case 0:
        {
            EditSelectionTableViewCell* selCell = (EditSelectionTableViewCell*)cell;
            [[selCell label] setText:NSLocalizedString(@"Screen", @"'Screen': Bookmark Screen settings")];
            NSString* resolution = ScreenResolutionDescription([_params intForKey:@"screen_resolution_type"], [_params intForKey:@"width"], [_params intForKey:@"height"]);
            int colorBits = [_params intForKey:@"colors"];
            [[selCell selection] setText:[NSString stringWithFormat:@"%@@%d", resolution, colorBits]];
            break;
        }            
		case 1:
        {
            EditSubEditTableViewCell* editCell = (EditSubEditTableViewCell*)cell;
            [[editCell label] setText:NSLocalizedString(@"Performance", @"'Performance': Bookmark Performance Settings")];
            break;
        }			
		case 2:
        {
            EditSubEditTableViewCell* editCell = (EditSubEditTableViewCell*)cell;
            [[editCell label] setText:NSLocalizedString(@"Advanced", @"'Advanced': Bookmark Advanced Settings")];
            break;
        }			
		default:
			NSLog(@"Invalid row index in settings table!");
			break;
	}			
}


#pragma mark -
#pragma mark Table view delegate

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
    UIViewController* viewCtrl = nil;

    // determine view
    switch([indexPath section])
    {
        case SECTION_CREDENTIALS:
        {
            if ([indexPath row] == 0)
                viewCtrl = [[[CredentialsEditorController alloc] initWithBookmark:_bookmark] autorelease];
            break;
        }
            
        case SECTION_SETTINGS:
        {            
            switch ([indexPath row])
            {
                case 0:
                    viewCtrl = [[[ScreenSelectionController alloc] initWithConnectionParams:_params] autorelease];
                    break;
                case 1:
                    viewCtrl = [[[PerformanceEditorController alloc] initWithConnectionParams:_params] autorelease];
                    break;    
                case 2:
                    viewCtrl = [[[AdvancedBookmarkEditorController alloc] initWithBookmark:_bookmark] autorelease];
                    break;
                default:
                    break;
            }            
            
            break;
        }            
    }

    // display view
    if(viewCtrl)
        [[self navigationController] pushViewController:viewCtrl animated:YES];
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
		// update server settings
		case GET_TAG(SECTION_SERVER, 0):
			[_bookmark setLabel:[textField text]];
			break;

		case GET_TAG(SECTION_SERVER, 1):
			[_params setValue:[textField text] forKey:@"hostname"];
			break;

		case GET_TAG(SECTION_SERVER, 2):
			[_params setInt:[[textField text] intValue] forKey:@"port"];
			break;
		            
		default:
			break;
	}
	return YES;
}

#pragma mark -
#pragma mark Action Handlers

- (void)handleSave:(id)sender
{
    // resign any first responder (so that we finish editing any bookmark parameter that might be currently edited)
    [[self view] endEditing:NO];
    
    // verify that bookmark is complete (only for manual bookmarks)
    if (![_bookmark isKindOfClass:NSClassFromString(@"TSXConnectComputerBookmark")])
    {
        if ([[_bookmark label] length] == 0 || [[_params StringForKey:@"hostname"] length] == 0 || [_params intForKey:@"port"] == 0)        
        {
            BlockAlertView* alertView = [BlockAlertView alertWithTitle:NSLocalizedString(@"Cancel without saving?", @"Incomplete bookmark error title") message:NSLocalizedString(@"Press 'Cancel' to abort!\nPress 'Continue' to specify the required fields!", @"Incomplete bookmark error message")];
            [alertView setCancelButtonWithTitle:NSLocalizedString(@"Cancel", @"Cancel Button") block:^{
                // cancel bookmark editing and return to previous view controller
                [[self navigationController] popViewControllerAnimated:YES];
            }];
            [alertView addButtonWithTitle:NSLocalizedString(@"Continue", @"Continue Button") block:nil];
            [alertView show];
            return;
        }        
    }        
    
    // commit bookmark
    if ([[self delegate] respondsToSelector:@selector(commitBookmark:)])
        [[self delegate] commitBookmark:_bookmark];
    
    // return to previous view controller
    [[self navigationController] popViewControllerAnimated:YES];
}

- (void)handleCancel:(id)sender
{
    // return to previous view controller
    [[self navigationController] popViewControllerAnimated:YES];    
}

#pragma mark -
#pragma mark Memory management

- (void)didReceiveMemoryWarning {
    // Releases the view if it doesn't have a superview.
    [super didReceiveMemoryWarning];
    
    // Relinquish ownership any cached data, images, etc that aren't in use.
}

- (void)dealloc {
    [super dealloc];
    [_bookmark autorelease];
}


@end

