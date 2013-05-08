/*
 controller for performance settings selection
 
 Copyright 2013 Thinstuff Technologies GmbH, Author: Martin Fleisz
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#import "PerformanceEditorController.h"
#import "ConnectionParams.h"
#import "Utils.h"

@interface PerformanceEditorController (Private)
-(NSString*)keyPathForKey:(NSString*)key;
@end

@implementation PerformanceEditorController

- (id)initWithConnectionParams:(ConnectionParams*)params
{
    return [self initWithConnectionParams:params keyPath:nil];
}

- (id)initWithConnectionParams:(ConnectionParams*)params keyPath:(NSString*)keyPath;
{
    self = [super initWithStyle:UITableViewStyleGrouped];
    if (self) {
        _params = [params retain];
        _keyPath = (keyPath != nil ? [keyPath retain] : nil);
    }
    return self;
}

- (void)viewDidLoad
{
    [super viewDidLoad];
	// Do any additional setup after loading the view.
}

- (void)viewDidUnload
{
    [super viewDidUnload];
    // Release any retained subviews of the main view.
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
    return YES;
}

-(NSString*)keyPathForKey:(NSString*)key
{
    if (_keyPath)
        return [_keyPath stringByAppendingFormat:@".%@", key];
    return key;
}

- (void)dealloc
{
    [super dealloc];
    [_params release];
}

#pragma mark -
#pragma mark Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
    // Return the number of sections.
    return 1;
}


- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    return 7;
}


// set section headers
- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section
{
	return NSLocalizedString(@"Performance Settings", @"'Performance Settings': performance settings header");
}

// Customize the appearance of table view cells.
- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    
    // get the table view cell
	EditFlagTableViewCell *cell = (EditFlagTableViewCell*)[self tableViewCellFromIdentifier:TableCellIdentifierYesNo];
	NSAssert(cell, @"Invalid cell");	
    
    switch ([indexPath row])
    {
        case 0:
        {
            [[cell label] setText:NSLocalizedString(@"RemoteFX", @"RemoteFX performance setting")];
            [[cell toggle] setOn:[_params boolForKeyPath:[self keyPathForKey:@"perf_remotefx"]]];
            break;
        }

        case 1:
        {
            [[cell label] setText:NSLocalizedString(@"Desktop Background", @"Desktop background performance setting")];
            [[cell toggle] setOn:[_params boolForKeyPath:[self keyPathForKey:@"perf_show_desktop"]]];
            break;
        }

        case 2:
        {
            [[cell label] setText:NSLocalizedString(@"Font Smoothing", @"Font smoothing performance setting")];
            [[cell toggle] setOn:[_params boolForKeyPath:[self keyPathForKey:@"perf_font_smoothing"]]];
            break;
        }
            
        case 3:
        {
            [[cell label] setText:NSLocalizedString(@"Desktop Composition", @"Desktop composition performance setting")];
            [[cell toggle] setOn:[_params boolForKeyPath:[self keyPathForKey:@"perf_desktop_composition"]]];
            break;
        }            
            
        case 4:
        {
            [[cell label] setText:NSLocalizedString(@"Window contents while dragging", @"Window Dragging performance setting")];
            [[cell toggle] setOn:[_params boolForKeyPath:[self keyPathForKey:@"perf_window_dragging"]]];
            break;
        }
            
        case 5:
        {
            [[cell label] setText:NSLocalizedString(@"Menu Animation", @"Menu Animations performance setting")];
            [[cell toggle] setOn:[_params boolForKeyPath:[self keyPathForKey:@"perf_menu_animation"]]];
            break;
        }

        case 6:
        {
            [[cell label] setText:NSLocalizedString(@"Visual Styles", @"Use Themes performance setting")];
            [[cell toggle] setOn:[_params boolForKeyPath:[self keyPathForKey:@"perf_windows_themes"]]];
            break;
        }

        default:
            break;
    }
    
    [[cell toggle] setTag:GET_TAG_FROM_PATH(indexPath)];
    [[cell toggle] addTarget:self action:@selector(togglePerformanceSetting:) forControlEvents:UIControlEventValueChanged];
    return cell;
}

#pragma mark -
#pragma mark Action Handlers

- (void)togglePerformanceSetting:(id)sender
{
    UISwitch* valueSwitch = (UISwitch*)sender;
    switch(valueSwitch.tag)
    {
        case GET_TAG(0, 0):
            [_params setBool:[valueSwitch isOn] forKeyPath:[self keyPathForKey:@"perf_remotefx"]];
            break;

        case GET_TAG(0, 1):
            [_params setBool:[valueSwitch isOn] forKeyPath:[self keyPathForKey:@"perf_show_desktop"]];
            break;
            
        case GET_TAG(0, 2):
            [_params setBool:[valueSwitch isOn] forKeyPath:[self keyPathForKey:@"perf_font_smoothing"]];
            break;
            
        case GET_TAG(0, 3):
            [_params setBool:[valueSwitch isOn] forKeyPath:[self keyPathForKey:@"perf_desktop_composition"]];
            break;

        case GET_TAG(0, 4):
            [_params setBool:[valueSwitch isOn] forKeyPath:[self keyPathForKey:@"perf_window_dragging"]];
            break;
            
        case GET_TAG(0, 5):
            [_params setBool:[valueSwitch isOn] forKeyPath:[self keyPathForKey:@"perf_menu_animation"]];
            break;

        case GET_TAG(0, 6):
            [_params setBool:[valueSwitch isOn] forKeyPath:[self keyPathForKey:@"perf_windows_themes"]];
            break;

        default:
            break;
    }    
}

@end
