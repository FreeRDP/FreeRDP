/*
 Controller to specify application wide settings
 
 Copyright 2013 Thinstuff Technologies GmbH, Author: Martin Fleisz
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#import "AppSettingsController.h"
#import "Utils.h"
#import "Toast+UIView.h"

@implementation AppSettingsController

// keep this up-to-date for correct section display/hidding
#define SECTION_UI_SETTINGS 0
#define SECTION_CERTIFICATE_HANDLING_SETTINGS 1
#define SECTION_NUM_SECTIONS 2

#pragma mark -
#pragma mark Initialization


- (id)initWithStyle:(UITableViewStyle)style
{
    if ((self = [super initWithStyle:style])) 
	{                   
        UIImage* tabBarIcon = [UIImage imageWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"tabbar_icon_settings" ofType:@"png"]];
        [self setTabBarItem:[[[UITabBarItem alloc] initWithTitle:NSLocalizedString(@"Settings", @"Tabbar item settings") image:tabBarIcon tag:0] autorelease]];
    }
    return self;
}

#pragma mark -
#pragma mark View lifecycle

- (void)viewDidLoad {
    [super viewDidLoad];

    // set title
    [self setTitle:NSLocalizedString(@"Settings", @"App Settings title")];
}


- (void)viewWillDisappear:(BOOL)animated {
    [super viewWillDisappear:animated];
}

// Override to allow orientations other than the default portrait orientation.
- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation {
    return YES;
}


- (void)dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [super dealloc];
}

#pragma mark -
#pragma mark Table view data source


- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
    // Return the number of sections.
    return SECTION_NUM_SECTIONS;
}


- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    // Return the number of rows in the section.
	switch (section)    
	{
        case SECTION_UI_SETTINGS: // UI settings
            return 5;
        case SECTION_CERTIFICATE_HANDLING_SETTINGS: // certificate handling settings
            return 2;
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
        case SECTION_UI_SETTINGS:
			return NSLocalizedString(@"User Interface", @"UI settings section title");
        case SECTION_CERTIFICATE_HANDLING_SETTINGS:
			return NSLocalizedString(@"Server Certificate Handling", @"Server Certificate Handling section title");
        default:
			return nil;
	}
	return @"unknown";
}

// Customize the appearance of table view cells.
- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    
	// determine the required cell type
	NSString* cellIdentifier = nil;
	switch([indexPath section])
	{
        case SECTION_UI_SETTINGS:
        {
            switch([indexPath row])
            {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                    cellIdentifier = TableCellIdentifierYesNo;
                    break;
            }
            break;
        }
        case SECTION_CERTIFICATE_HANDLING_SETTINGS:
        {
            switch([indexPath row])
            {
                case 0:
                    cellIdentifier = TableCellIdentifierYesNo;
                    break;
                case 1:
                    cellIdentifier = TableCellIdentifierSubEditor;
                    break;
            }
            break;
        }
	}	
	NSAssert(cellIdentifier != nil, @"Couldn't determine cell type");	
	
	// get the table view cell
	UITableViewCell *cell = [self tableViewCellFromIdentifier:cellIdentifier];
	NSAssert(cell, @"Invalid cell");	
    
	// set cell values
	switch([indexPath section]) 
	{		
        case SECTION_UI_SETTINGS:
            [self initUISettings:indexPath cell:cell];
            break;
            
        case SECTION_CERTIFICATE_HANDLING_SETTINGS:
            [self initCertificateHandlingSettings:indexPath cell:cell];
            break;      
            
		default:
			break;
	}	
    
    return cell;
}

#pragma mark - Initialization helpers

// updates UI settings in the UI
- (void)initUISettings:(NSIndexPath*)indexPath cell:(UITableViewCell*)cell
{
	switch([indexPath row]) 
	{
		case 0:
        {
            EditFlagTableViewCell* flagCell = (EditFlagTableViewCell*)cell;            
            [[flagCell label] setText:NSLocalizedString(@"Hide Status Bar", "Show/Hide Phone Status Bar setting")];
            [[flagCell toggle] setTag:GET_TAG_FROM_PATH(indexPath)];
            [[flagCell toggle] setOn:[[NSUserDefaults standardUserDefaults] boolForKey:@"ui.hide_status_bar"]];
            [[flagCell toggle] addTarget:self action:@selector(toggleSettingValue:) forControlEvents:UIControlEventValueChanged];
			break;            
        }
		case 1:
        {
            EditFlagTableViewCell* flagCell = (EditFlagTableViewCell*)cell;
            [[flagCell label] setText:NSLocalizedString(@"Hide Tool Bar", "Show/Hide Tool Bar setting")];
            [[flagCell toggle] setTag:GET_TAG_FROM_PATH(indexPath)];
            [[flagCell toggle] setOn:[[NSUserDefaults standardUserDefaults] boolForKey:@"ui.hide_tool_bar"]];
            [[flagCell toggle] addTarget:self action:@selector(toggleSettingValue:) forControlEvents:UIControlEventValueChanged];
			break;
        }
		case 2:
        {
            EditFlagTableViewCell* flagCell = (EditFlagTableViewCell*)cell;            
            [[flagCell label] setText:NSLocalizedString(@"Swap Mouse Buttons", "Swap Mouse Button UI setting")];
            [[flagCell toggle] setTag:GET_TAG_FROM_PATH(indexPath)];
            [[flagCell toggle] setOn:[[NSUserDefaults standardUserDefaults] boolForKey:@"ui.swap_mouse_buttons"]];
            [[flagCell toggle] addTarget:self action:@selector(toggleSettingValue:) forControlEvents:UIControlEventValueChanged];
			break;            
        }
		case 3:
        {
            EditFlagTableViewCell* flagCell = (EditFlagTableViewCell*)cell;            
            [[flagCell label] setText:NSLocalizedString(@"Invert Scrolling", "Invert Scrolling UI setting")];
            [[flagCell toggle] setTag:GET_TAG_FROM_PATH(indexPath)];
            [[flagCell toggle] setOn:[[NSUserDefaults standardUserDefaults] boolForKey:@"ui.invert_scrolling"]];
            [[flagCell toggle] addTarget:self action:@selector(toggleSettingValue:) forControlEvents:UIControlEventValueChanged];
			break;            
        }
		case 4:
        {
            EditFlagTableViewCell* flagCell = (EditFlagTableViewCell*)cell;            
            [[flagCell label] setText:NSLocalizedString(@"Touch Pointer Auto Scroll", "Touch Pointer Auto Scroll UI setting")];
            [[flagCell toggle] setTag:GET_TAG_FROM_PATH(indexPath)];
            [[flagCell toggle] setOn:[[NSUserDefaults standardUserDefaults] boolForKey:@"ui.auto_scroll_touchpointer"]];
            [[flagCell toggle] addTarget:self action:@selector(toggleSettingValue:) forControlEvents:UIControlEventValueChanged];
			break;            
        }
		default:
			NSLog(@"Invalid row index in settings table!");
			break;
	}
}

// updates certificate handling settings in the UI
- (void)initCertificateHandlingSettings:(NSIndexPath*)indexPath cell:(UITableViewCell*)cell
{
	switch([indexPath row]) 
	{
		case 0:
        {
            EditFlagTableViewCell* flagCell = (EditFlagTableViewCell*)cell;            
            [[flagCell label] setText:NSLocalizedString(@"Accept all Certificates", "Accept All Certificates setting")];
            [[flagCell toggle] setTag:GET_TAG_FROM_PATH(indexPath)];
            [[flagCell toggle] setOn:[[NSUserDefaults standardUserDefaults] boolForKey:@"security.accept_certificates"]];
            [[flagCell toggle] addTarget:self action:@selector(toggleSettingValue:) forControlEvents:UIControlEventValueChanged];
			break;            
        }
		case 1:
        {
            EditSubEditTableViewCell* subCell = (EditSubEditTableViewCell*)cell;
            [[subCell label] setText:NSLocalizedString(@"Erase Certificate Cache", @"Erase certificate cache button")];
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

    // deselect any row to fake a button-pressed like effect
    [tableView deselectRowAtIndexPath:indexPath animated:YES];

    // ensure everything is stored in our settings before we proceed
    [[self view] endEditing:NO];
    
    // clear certificate cache
    if([indexPath section] == SECTION_CERTIFICATE_HANDLING_SETTINGS && [indexPath row] == 1)
    {
        // delete certificates cache
        NSError* err;
        if ([[NSFileManager defaultManager] removeItemAtPath:[[NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) lastObject] stringByAppendingPathComponent:@"/.freerdp"] error:&err])
            [[self view] makeToast:NSLocalizedString(@"Certificate Cache cleared!", @"Clear Certificate cache success message") duration:ToastDurationNormal position:@"center"];
        else
            [[self view] makeToast:NSLocalizedString(@"Error clearing the Certificate Cache!", @"Clear Certificate cache failed message") duration:ToastDurationNormal position:@"center"];
    }
}

#pragma mark -
#pragma mark Action Handlers

- (void)toggleSettingValue:(id)sender
{
    UISwitch* valueSwitch = (UISwitch*)sender;
    switch([valueSwitch tag])
    {
        case GET_TAG(SECTION_UI_SETTINGS, 0):
            [[NSUserDefaults standardUserDefaults] setBool:[valueSwitch isOn] forKey:@"ui.hide_status_bar"];
            break;

        case GET_TAG(SECTION_UI_SETTINGS, 1):
            [[NSUserDefaults standardUserDefaults] setBool:[valueSwitch isOn] forKey:@"ui.hide_tool_bar"];
            break;

        case GET_TAG(SECTION_UI_SETTINGS, 2):
            [[NSUserDefaults standardUserDefaults] setBool:[valueSwitch isOn] forKey:@"ui.swap_mouse_buttons"];
            SetSwapMouseButtonsFlag([valueSwitch isOn]);
            break;

        case GET_TAG(SECTION_UI_SETTINGS, 3):
            [[NSUserDefaults standardUserDefaults] setBool:[valueSwitch isOn] forKey:@"ui.invert_scrolling"];
            SetInvertScrollingFlag([valueSwitch isOn]);
            break;

        case GET_TAG(SECTION_UI_SETTINGS, 4):
            [[NSUserDefaults standardUserDefaults] setBool:[valueSwitch isOn] forKey:@"ui.auto_scroll_touchpointer"];
            SetInvertScrollingFlag([valueSwitch isOn]);
            break;

        case GET_TAG(SECTION_CERTIFICATE_HANDLING_SETTINGS, 0):
            [[NSUserDefaults standardUserDefaults] setBool:[valueSwitch isOn] forKey:@"security.accept_certificates"];
            break;
            
        default:
            break;
    }    
}

@end

