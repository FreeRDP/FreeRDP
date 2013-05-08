/*
 Controller to edit advanced bookmark settings
 
 Copyright 2013 Thinstuff Technologies GmbH, Author: Martin Fleisz
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#import "AdvancedBookmarkEditorController.h"
#import "Bookmark.h"
#import "Utils.h"
#import "EditorSelectionController.h"
#import "ScreenSelectionController.h"
#import "PerformanceEditorController.h"
#import "BookmarkGatewaySettingsController.h"

@interface AdvancedBookmarkEditorController ()

@end

#define SECTION_ADVANCED_SETTINGS 0
#define SECTION_COUNT 1

@implementation AdvancedBookmarkEditorController

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
    [self setTitle:NSLocalizedString(@"Advanced Settings", @"Advanced Settings title")];
}

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];
    
    // we need to reload the table view data here to have up-to-date data for the 
    // advanced settings accessory items (like for resolution/color mode settings)
    [[self tableView] reloadData];
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
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
		case SECTION_ADVANCED_SETTINGS: // advanced settings
			return 9;
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
		case SECTION_ADVANCED_SETTINGS:
			return NSLocalizedString(@"Advanced", @"'Advanced': advanced settings header");
	}
	return @"unknown";
}


// Customize the appearance of table view cells.
- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    
	// determine the required cell type
	NSString* cellType = nil;
	switch([indexPath section])
	{
        case SECTION_ADVANCED_SETTINGS: // advanced settings
        {
            switch([indexPath row]) 
            {
                case 0: // Enable/Disable TSG Settings
                    cellType = TableCellIdentifierYesNo;
                    break;
                case 1:	// TS Gateway Settings
                    cellType = TableCellIdentifierSubEditor;
                    break;
                case 2: // 3G Settings
                    cellType = TableCellIdentifierYesNo;
                    break;
                case 3:	// 3G screen/color depth
                    cellType = TableCellIdentifierSelection;
                    break;
                case 4:	// 3G performance settings
                    cellType = TableCellIdentifierSubEditor;
                    break;
                case 5: // security mode
                    cellType = TableCellIdentifierSelection;
                    break;
                case 6:	// remote program
                case 7:	// work dir
                    cellType = TableCellIdentifierText;
                    break;
                case 8:	// console mode
                    cellType = TableCellIdentifierYesNo;
                    break;
                default:						
                    break;
            }
			break;
        }
	}	
	NSAssert(cellType != nil, @"Couldn't determine cell type");	
	
	// get the table view cell
	UITableViewCell *cell = [self tableViewCellFromIdentifier:cellType];
	NSAssert(cell, @"Invalid cell");	
    
	// set cell values
	switch([indexPath section]) 
	{
            // advanced settings
		case SECTION_ADVANCED_SETTINGS:
			[self initAdvancedSettings:indexPath cell:cell];
			break;
            
		default:
			break;
	}	
    
    return cell;
}

// updates advanced settings in the UI
- (void)initAdvancedSettings:(NSIndexPath*)indexPath cell:(UITableViewCell*)cell
{
    BOOL enable_3G_settings = [_params boolForKey:@"enable_3g_settings"];
	switch(indexPath.row) 
	{
		case 0:
        {
            EditFlagTableViewCell* flagCell = (EditFlagTableViewCell*)cell;
            [[flagCell label] setText:NSLocalizedString(@"Enable TS Gateway", @"'Enable TS Gateway': Bookmark enable TSG settings")];
            [[flagCell toggle] setTag:GET_TAG_FROM_PATH(indexPath)];
            [[flagCell toggle] setOn:[_params boolForKey:@"enable_tsg_settings"]];
            [[flagCell toggle] addTarget:self action:@selector(toggleSettingValue:) forControlEvents:UIControlEventValueChanged];
            break;
        }
        case 1:
        {
            BOOL enable_tsg_settings = [_params boolForKey:@"enable_tsg_settings"];
            EditSubEditTableViewCell* editCell = (EditSubEditTableViewCell*)cell;
            [[editCell label] setText:NSLocalizedString(@"TS Gateway Settings", @"'TS Gateway Settings': Bookmark TS Gateway Settings")];
            [[editCell label] setEnabled:enable_tsg_settings];
            [editCell setSelectionStyle:enable_tsg_settings ? UITableViewCellSelectionStyleBlue : UITableViewCellSelectionStyleNone];
            break;
        }
		case 2:
        {
            EditFlagTableViewCell* flagCell = (EditFlagTableViewCell*)cell;
            [[flagCell label] setText:NSLocalizedString(@"3G Settings", @"'3G Settings': Bookmark enable 3G settings")];
            [[flagCell toggle] setTag:GET_TAG_FROM_PATH(indexPath)];
            [[flagCell toggle] setOn:[_params boolForKey:@"enable_3g_settings"]];
            [[flagCell toggle] addTarget:self action:@selector(toggleSettingValue:) forControlEvents:UIControlEventValueChanged];
            break;
        }
		case 3:
        {
            EditSelectionTableViewCell* selCell = (EditSelectionTableViewCell*)cell;
            [[selCell label] setText:NSLocalizedString(@"3G Screen", @"'3G Screen': Bookmark 3G Screen settings")];
            NSString* resolution = ScreenResolutionDescription([_params intForKeyPath:@"settings_3g.screen_resolution_type"], [_params intForKeyPath:@"settings_3g.width"], [_params intForKeyPath:@"settings_3g.height"]);
            int colorBits = [_params intForKeyPath:@"settings_3g.colors"];
            [[selCell selection] setText:[NSString stringWithFormat:@"%@@%d", resolution, colorBits]];
            [[selCell label] setEnabled:enable_3G_settings];
            [[selCell selection] setEnabled:enable_3G_settings];
            [selCell setSelectionStyle:enable_3G_settings ? UITableViewCellSelectionStyleBlue : UITableViewCellSelectionStyleNone];
            break;
        }
		case 4:
        {
            EditSubEditTableViewCell* editCell = (EditSubEditTableViewCell*)cell;
            [[editCell label] setText:NSLocalizedString(@"3G Performance", @"'3G Performance': Bookmark 3G Performance Settings")];
            [[editCell label] setEnabled:enable_3G_settings];
            [editCell setSelectionStyle:enable_3G_settings ? UITableViewCellSelectionStyleBlue : UITableViewCellSelectionStyleNone];
            break;
        }			
		case 5:
        {
            EditSelectionTableViewCell* selCell = (EditSelectionTableViewCell*)cell;
            [[selCell label] setText:NSLocalizedString(@"Security", @"'Security': Bookmark protocl security settings")];
            [[selCell selection] setText:ProtocolSecurityDescription([_params intForKey:@"security"])];
            break;
        }                                    
		case 6:
        {
            EditTextTableViewCell* textCell = (EditTextTableViewCell*)cell;
            [[textCell label] setText:NSLocalizedString(@"Remote Program", @"'Remote Program': Bookmark remote program settings")];
            [[textCell textfield] setText:[_params StringForKey:@"remote_program"]];
            [[textCell textfield] setTag:GET_TAG_FROM_PATH(indexPath)];
            [[textCell textfield] setPlaceholder:NSLocalizedString(@"not set", @"not set placeholder")];
            [self adjustEditTextTableViewCell:textCell];
            break;
        }            
		case 7:
        {
            EditTextTableViewCell* textCell = (EditTextTableViewCell*)cell;
            [[textCell label] setText:NSLocalizedString(@"Working Directory", @"'Working Directory': Bookmark working directory settings")];
            [[textCell textfield] setText:[_params StringForKey:@"working_dir"]];
            [[textCell textfield] setTag:GET_TAG_FROM_PATH(indexPath)];
            [[textCell textfield] setPlaceholder:NSLocalizedString(@"not set", @"not set placeholder")];
            [self adjustEditTextTableViewCell:textCell];
            break;
        }            
		case 8:
        {
            EditFlagTableViewCell* flagCell = (EditFlagTableViewCell*)cell;
            [[flagCell label] setText:NSLocalizedString(@"Console Mode", @"'Console Mode': Bookmark console mode settings")];
            [[flagCell toggle] setTag:GET_TAG_FROM_PATH(indexPath)];
            [[flagCell toggle] setOn:[_params boolForKey:@"console"]];
            [[flagCell toggle] addTarget:self action:@selector(toggleSettingValue:) forControlEvents:UIControlEventValueChanged];
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
    switch ([indexPath row])
    {
        case 1:
            if ([_params boolForKey:@"enable_tsg_settings"])
                viewCtrl = [[[BookmarkGatewaySettingsController alloc] initWithBookmark:_bookmark] autorelease];
            break;
        case 3:
            if ([_params boolForKey:@"enable_3g_settings"])
                viewCtrl = [[[ScreenSelectionController alloc] initWithConnectionParams:_params keyPath:@"settings_3g"] autorelease];
            break;
        case 4:
            if ([_params boolForKey:@"enable_3g_settings"])
                viewCtrl = [[[PerformanceEditorController alloc] initWithConnectionParams:_params keyPath:@"settings_3g"] autorelease];
            break;                    
        case 5:
            viewCtrl = [[[EditorSelectionController alloc] initWithConnectionParams:_params entries:[NSArray arrayWithObject:@"security"] selections:[NSArray arrayWithObject:SelectionForSecuritySetting()]] autorelease];
            break;                                    
        default:
            break;
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
        // update remote program/work dir settings
        case GET_TAG(SECTION_ADVANCED_SETTINGS, 6):
        {
            [_params setValue:[textField text] forKey:@"remote_program"];
            break;
        }
            
        case GET_TAG(SECTION_ADVANCED_SETTINGS, 7):
        {
            [_params setValue:[textField text] forKey:@"working_dir"];
            break;
        }
			
		default:
			break;
	}
	return YES;
}

#pragma mark - Action handlers

- (void)toggleSettingValue:(id)sender
{
    UISwitch* valueSwitch = (UISwitch*)sender;
    switch(valueSwitch.tag)
    {
        case GET_TAG(SECTION_ADVANCED_SETTINGS, 0):
        {
            [_params setBool:[valueSwitch isOn] forKey:@"enable_tsg_settings"];
            NSArray* indexPaths = [NSArray arrayWithObjects:[NSIndexPath indexPathForRow:1 inSection:SECTION_ADVANCED_SETTINGS], [NSIndexPath indexPathForRow:2 inSection:SECTION_ADVANCED_SETTINGS], nil];
            [[self tableView] reloadRowsAtIndexPaths:indexPaths withRowAnimation:UITableViewRowAnimationNone];
            break;
        }

        case GET_TAG(SECTION_ADVANCED_SETTINGS, 2):
        {
            [_params setBool:[valueSwitch isOn] forKey:@"enable_3g_settings"];
            NSArray* indexPaths = [NSArray arrayWithObjects:[NSIndexPath indexPathForRow:3 inSection:SECTION_ADVANCED_SETTINGS], [NSIndexPath indexPathForRow:2 inSection:SECTION_ADVANCED_SETTINGS], nil];
            [[self tableView] reloadRowsAtIndexPaths:indexPaths withRowAnimation:UITableViewRowAnimationNone];
            break;
        }
            
        case GET_TAG(SECTION_ADVANCED_SETTINGS, 8):
            [_params setBool:[valueSwitch isOn] forKey:@"console"];
            break;
            
        default:
            break;
    }    
}

@end
