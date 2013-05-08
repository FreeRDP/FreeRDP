//
//  BookmarkGatewaySettingsController.m
//  FreeRDP
//
//  Created by Thinstuff Developer on 4/30/13.
//
//

#import "BookmarkGatewaySettingsController.h"
#import "Bookmark.h"
#import "Utils.h"
#import "EditorSelectionController.h"

#define SECTION_TSGATEWAY_SETTINGS 0
#define SECTION_COUNT 1

@interface BookmarkGatewaySettingsController ()

@end

@implementation BookmarkGatewaySettingsController

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
    [self setTitle:NSLocalizedString(@"TS Gateway Settings", @"TS Gateway Settings title")];
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

- (void)dealloc
{
    [super dealloc];
    [_bookmark release];
}

#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
    // Return the number of sections.
    return SECTION_COUNT;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    // Return the number of rows in the section.
	switch (section)
	{
		case SECTION_TSGATEWAY_SETTINGS: // ts gateway settings
			return 5;
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
		case SECTION_TSGATEWAY_SETTINGS:
			return NSLocalizedString(@"TS Gateway", @"'TS Gateway': ts gateway settings header");
	}
	return @"unknown";
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
	// determine the required cell type
	NSString* cellType = nil;
	switch([indexPath section])
	{
        case SECTION_TSGATEWAY_SETTINGS: // advanced settings
        {
            switch([indexPath row])
            {
                case 0: // hostname
                case 1:	// port
                case 2: // username
                case 4: // domain
                    cellType = TableCellIdentifierText;
                    break;
                case 3: // password
                    cellType = TableCellIdentifierSecretText;
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
		case SECTION_TSGATEWAY_SETTINGS:
			[self initGatewaySettings:indexPath cell:cell];
			break;
            
		default:
			break;
	}
    
    return cell;
}

// updates server settings in the UI
- (void)initGatewaySettings:(NSIndexPath*)indexPath cell:(UITableViewCell*)cell
{
    EditTextTableViewCell* textCell = (EditTextTableViewCell*)cell;
	[[textCell textfield] setTag:GET_TAG_FROM_PATH(indexPath)];
	switch([indexPath row])
	{
		case 0:
        {
			[[textCell label] setText:NSLocalizedString(@"Host", @"'Host': Bookmark hostname")];
			[[textCell textfield] setText:[_params StringForKey:@"tsg_hostname"]];
            [[textCell textfield] setPlaceholder:NSLocalizedString(@"not set", @"not set placeholder")];
			break;
        }
		case 1:
        {
            int port = [_params intForKey:@"tsg_port"];
            if (port == 0) port = 443;
			[[textCell label] setText:NSLocalizedString(@"Port", @"'Port': Bookmark port")];
			[[textCell textfield] setText:[NSString stringWithFormat:@"%d", port]];
            [[textCell textfield] setKeyboardType:UIKeyboardTypeNumberPad];
			break;            
        }
		case 2:
        {
            [[textCell textfield] setTag:GET_TAG_FROM_PATH(indexPath)];
            [[textCell label] setText:NSLocalizedString(@"Username", @"'Username': Bookmark username")];
			[[textCell textfield] setText:[_params StringForKey:@"tsg_username"]];
            [[textCell textfield] setPlaceholder:NSLocalizedString(@"not set", @"not set placeholder")];
			break;
        }
		case 3:
        {
            [[textCell textfield] setTag:GET_TAG_FROM_PATH(indexPath)];
			[[textCell label] setText:NSLocalizedString(@"Password", @"'Password': Bookmark password")];
			[[textCell textfield] setText:[_params StringForKey:@"tsg_password"]];
            [[textCell textfield] setPlaceholder:NSLocalizedString(@"not set", @"not set placeholder")];
			break;
        }
		case 4:
        {
            [[textCell textfield] setTag:GET_TAG_FROM_PATH(indexPath)];
			[[textCell label] setText:NSLocalizedString(@"Domain", @"'Domain': Bookmark domain")];
			[[textCell textfield] setText:[_params StringForKey:@"tsg_domain"]];
            [[textCell textfield] setPlaceholder:NSLocalizedString(@"not set", @"not set placeholder")];
			break;
        }
		default:
			NSLog(@"Invalid row index in settings table!");
			break;
	}
    
    [self adjustEditTextTableViewCell:textCell];
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
		case GET_TAG(SECTION_TSGATEWAY_SETTINGS, 0):
			[_params setValue:[textField text] forKey:@"tsg_hostname"];
			break;
            
		case GET_TAG(SECTION_TSGATEWAY_SETTINGS, 1):
			[_params setInt:[[textField text] intValue] forKey:@"tsg_port"];
			break;
            
		case GET_TAG(SECTION_TSGATEWAY_SETTINGS, 2):
			[_params setValue:[textField text] forKey:@"tsg_username"];
			break;
            
		case GET_TAG(SECTION_TSGATEWAY_SETTINGS, 3):
			[_params setValue:[textField text] forKey:@"tsg_password"];
			break;
            
		case GET_TAG(SECTION_TSGATEWAY_SETTINGS, 4):
			[_params setValue:[textField text] forKey:@"tsg_domain"];
			break;            
            
		default:
			break;
	}
	return YES;
}


@end
