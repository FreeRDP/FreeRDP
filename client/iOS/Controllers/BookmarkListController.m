/*
 bookmarks and active session view controller

 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import "BookmarkListController.h"
#import "Utils.h"
#import "BookmarkEditorController.h"
#import "RDPSessionViewController.h"
#import "Toast+UIView.h"
#import "Reachability.h"
#import "GlobalDefaults.h"
#import "BlockAlertView.h"

#define SECTION_SESSIONS 0
#define SECTION_BOOKMARKS 1
#define NUM_SECTIONS 2

@interface BookmarkListController (Private)
#pragma mark misc functions
- (UIButton *)disclosureButtonWithImage:(UIImage *)image;
- (void)performSearch:(NSString *)searchText;
#pragma mark Persisting bookmarks
- (void)scheduleWriteBookmarksToDataStore;
- (void)writeBookmarksToDataStore;
- (void)scheduleWriteManualBookmarksToDataStore;
- (void)writeManualBookmarksToDataStore;
- (void)readManualBookmarksFromDataStore;
- (void)writeArray:(NSArray *)bookmarks toDataStoreURL:(NSURL *)url;
- (NSMutableArray *)arrayFromDataStoreURL:(NSURL *)url;
- (NSURL *)manualBookmarksDataStoreURL;
- (NSURL *)connectionHistoryDataStoreURL;
@end

@implementation BookmarkListController

@synthesize searchBar = _searchBar, tableView = _tableView, bmTableCell = _bmTableCell,
            sessTableCell = _sessTableCell;

// The designated initializer.  Override if you create the controller programmatically and want to
// perform customization that is not appropriate for viewDidLoad.
- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
	if ((self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil]))
	{
		// load bookmarks
		[self readManualBookmarksFromDataStore];

		// load connection history
		[self readConnectionHistoryFromDataStore];

		// init search result array
		_manual_search_result = nil;

		// register for session notifications
		[[NSNotificationCenter defaultCenter] addObserver:self
		                                         selector:@selector(sessionDisconnected:)
		                                             name:TSXSessionDidDisconnectNotification
		                                           object:nil];
		[[NSNotificationCenter defaultCenter] addObserver:self
		                                         selector:@selector(sessionFailedToConnect:)
		                                             name:TSXSessionDidFailToConnectNotification
		                                           object:nil];

		// set title and tabbar controller image
		[self setTitle:NSLocalizedString(@"Connections",
		                                 @"'Connections': bookmark controller title")];
		[self setTabBarItem:[[[UITabBarItem alloc]
		                        initWithTabBarSystemItem:UITabBarSystemItemBookmarks
		                                             tag:0] autorelease]];

		// load images
		_star_on_img = [[UIImage
		    imageWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"icon_accessory_star_on"
		                                                            ofType:@"png"]] retain];
		_star_off_img =
		    [[UIImage imageWithContentsOfFile:[[NSBundle mainBundle]
		                                          pathForResource:@"icon_accessory_star_off"
		                                                   ofType:@"png"]] retain];

		// init reachability detection
		[[NSNotificationCenter defaultCenter] addObserver:self
		                                         selector:@selector(reachabilityChanged:)
		                                             name:kReachabilityChangedNotification
		                                           object:nil];

		// init other properties
		_active_sessions = [[NSMutableArray alloc] init];
		_temporary_bookmark = nil;
	}
	return self;
}

- (void)loadView
{
	[super loadView];
}

// Implement viewDidLoad to do additional setup after loading the view, typically from a nib.
- (void)viewDidLoad
{
	[super viewDidLoad];

	// set edit button to allow bookmark list editing
	[[self navigationItem] setRightBarButtonItem:[self editButtonItem]];
}

- (void)viewWillAppear:(BOOL)animated
{
	[super viewWillAppear:animated];

	// in case we had a search - search again cause the bookmark searchable items could have changed
	if ([[_searchBar text] length] > 0)
		[self performSearch:[_searchBar text]];

	// to reflect any bookmark changes - reload table
	[_tableView reloadData];
}

- (void)viewWillDisappear:(BOOL)animated
{
	[super viewWillDisappear:animated];

	// clear any search
	[_searchBar setText:@""];
	[_searchBar resignFirstResponder];
	[self performSearch:@""];
}

// Override to allow orientations other than the default portrait orientation.
- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
	// Return YES for supported orientations
	return YES;
}

- (void)didReceiveMemoryWarning
{
	// Releases the view if it doesn't have a superview.
	[super didReceiveMemoryWarning];

	// Release any cached data, images, etc that aren't in use.
}

- (void)viewDidUnload
{
	[super viewDidUnload];
	// Release any retained subviews of the main view.
	// e.g. self.myOutlet = nil;
}

- (void)dealloc
{
	[[NSNotificationCenter defaultCenter] removeObserver:self];

	[_temporary_bookmark release];
	[_connection_history release];
	[_active_sessions release];
	[_manual_search_result release];
	[_manual_bookmarks release];

	[_star_on_img release];
	[_star_off_img release];

	[super dealloc];
}

#pragma mark -
#pragma mark Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
	// Return the number of sections.
	return NUM_SECTIONS;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{

	switch (section)
	{
		case SECTION_SESSIONS:
			return 0;
			break;

		case SECTION_BOOKMARKS:
		{
			// (+1 for Add Bookmark entry)
			if (_manual_search_result != nil)
				return ([_manual_search_result count] + [_history_search_result count] + 1);
			return ([_manual_bookmarks count] + 1);
		}
		break;

		default:
			break;
	}
	return 0;
}

- (UITableViewCell *)cellForGenericListEntry
{
	static NSString *CellIdentifier = @"BookmarkListCell";
	UITableViewCell *cell = [[self tableView] dequeueReusableCellWithIdentifier:CellIdentifier];
	if (cell == nil)
	{
		cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
		                              reuseIdentifier:CellIdentifier];
		[cell setSelectionStyle:UITableViewCellSelectionStyleNone];
		[cell setAccessoryView:[self disclosureButtonWithImage:_star_off_img]];
	}

	return cell;
}

- (BookmarkTableCell *)cellForBookmark
{
	static NSString *BookmarkCellIdentifier = @"BookmarkCell";
	BookmarkTableCell *cell = (BookmarkTableCell *)[[self tableView]
	    dequeueReusableCellWithIdentifier:BookmarkCellIdentifier];
	if (cell == nil)
	{
		[[NSBundle mainBundle] loadNibNamed:@"BookmarkTableViewCell" owner:self options:nil];
		[_bmTableCell setAccessoryView:[self disclosureButtonWithImage:_star_on_img]];
		cell = _bmTableCell;
		_bmTableCell = nil;
	}

	return cell;
}

// Customize the appearance of table view cells.
- (UITableViewCell *)tableView:(UITableView *)tableView
         cellForRowAtIndexPath:(NSIndexPath *)indexPath
{

	switch ([indexPath section])
	{
		case SECTION_SESSIONS:
		{
			// get custom session cell
			static NSString *SessionCellIdentifier = @"SessionCell";
			SessionTableCell *cell = (SessionTableCell *)[tableView
			    dequeueReusableCellWithIdentifier:SessionCellIdentifier];
			if (cell == nil)
			{
				[[NSBundle mainBundle] loadNibNamed:@"SessionTableViewCell" owner:self options:nil];
				cell = _sessTableCell;
				_sessTableCell = nil;
			}

			// set cell data
			RDPSession *session = [_active_sessions objectAtIndex:[indexPath row]];
			[[cell title] setText:[session sessionName]];
			[[cell server] setText:[[session params] StringForKey:@"hostname"]];
			[[cell username] setText:[[session params] StringForKey:@"username"]];
			[[cell screenshot]
			    setImage:[session getScreenshotWithSize:[[cell screenshot] bounds].size]];
			[[cell disconnectButton] setTag:[indexPath row]];
			return cell;
		}

		case SECTION_BOOKMARKS:
		{
			// special handling for first cell - quick connect/quick create Bookmark cell
			if ([indexPath row] == 0)
			{
				// if a search text is entered the cell becomes a quick connect/quick create
				// bookmark cell - otherwise it's just an add bookmark cell
				UITableViewCell *cell = [self cellForGenericListEntry];
				if ([[_searchBar text] length] == 0)
				{
					[[cell textLabel]
					    setText:[@"  " stringByAppendingString:
					                       NSLocalizedString(@"Add Connection",
					                                         @"'Add Connection': button label")]];
					[((UIButton *)[cell accessoryView]) setHidden:YES];
				}
				else
				{
					[[cell textLabel] setText:[@"  " stringByAppendingString:[_searchBar text]]];
					[((UIButton *)[cell accessoryView]) setHidden:NO];
				}

				return cell;
			}
			else
			{
				// do we have a history cell or bookmark cell?
				if ([self isIndexPathToHistoryItem:indexPath])
				{
					UITableViewCell *cell = [self cellForGenericListEntry];
					[[cell textLabel]
					    setText:[@"  " stringByAppendingString:
					                       [_history_search_result
					                           objectAtIndex:
					                               [self historyIndexFromIndexPath:indexPath]]]];
					[((UIButton *)[cell accessoryView]) setHidden:NO];
					return cell;
				}
				else
				{
					// set cell properties
					ComputerBookmark *entry;
					BookmarkTableCell *cell = [self cellForBookmark];
					if (_manual_search_result == nil)
						entry = [_manual_bookmarks
						    objectAtIndex:[self bookmarkIndexFromIndexPath:indexPath]];
					else
						entry = [[_manual_search_result
						    objectAtIndex:[self bookmarkIndexFromIndexPath:indexPath]]
						    valueForKey:@"bookmark"];

					[[cell title] setText:[entry label]];
					[[cell subTitle] setText:[[entry params] StringForKey:@"hostname"]];
					return cell;
				}
			}
		}

		default:
			break;
	}

	NSAssert(0, @"Failed to create cell");
	return nil;
}

// Override to support conditional editing of the table view.
- (BOOL)tableView:(UITableView *)tableView canEditRowAtIndexPath:(NSIndexPath *)indexPath
{
	// dont allow to edit Add Bookmark item
	if ([indexPath section] == SECTION_SESSIONS)
		return NO;
	if ([indexPath section] == SECTION_BOOKMARKS && [indexPath row] == 0)
		return NO;
	return YES;
}

// Override to support editing the table view.
- (void)tableView:(UITableView *)tableView
    commitEditingStyle:(UITableViewCellEditingStyle)editingStyle
     forRowAtIndexPath:(NSIndexPath *)indexPath
{
	if (editingStyle == UITableViewCellEditingStyleDelete)
	{
		// Delete the row from the data source
		switch ([indexPath section])
		{
			case SECTION_BOOKMARKS:
			{
				if (_manual_search_result == nil)
					[_manual_bookmarks
					    removeObjectAtIndex:[self bookmarkIndexFromIndexPath:indexPath]];
				else
				{
					// history item or bookmark?
					if ([self isIndexPathToHistoryItem:indexPath])
					{
						[_connection_history
						    removeObject:
						        [_history_search_result
						            objectAtIndex:[self historyIndexFromIndexPath:indexPath]]];
						[_history_search_result
						    removeObjectAtIndex:[self historyIndexFromIndexPath:indexPath]];
					}
					else
					{
						[_manual_bookmarks
						    removeObject:
						        [[_manual_search_result
						            objectAtIndex:[self bookmarkIndexFromIndexPath:indexPath]]
						            valueForKey:@"bookmark"]];
						[_manual_search_result
						    removeObjectAtIndex:[self bookmarkIndexFromIndexPath:indexPath]];
					}
				}
				[self scheduleWriteManualBookmarksToDataStore];
				break;
			}
		}

		[tableView reloadSections:[NSIndexSet indexSetWithIndex:[indexPath section]]
		         withRowAnimation:UITableViewRowAnimationNone];
	}
}

// Override to support rearranging the table view.
- (void)tableView:(UITableView *)tableView
    moveRowAtIndexPath:(NSIndexPath *)fromIndexPath
           toIndexPath:(NSIndexPath *)toIndexPath
{
	if ([fromIndexPath compare:toIndexPath] != NSOrderedSame)
	{
		switch ([fromIndexPath section])
		{
			case SECTION_BOOKMARKS:
			{
				int fromIdx = [self bookmarkIndexFromIndexPath:fromIndexPath];
				int toIdx = [self bookmarkIndexFromIndexPath:toIndexPath];
				ComputerBookmark *temp_bookmark =
				    [[_manual_bookmarks objectAtIndex:fromIdx] retain];
				[_manual_bookmarks removeObjectAtIndex:fromIdx];
				if (toIdx >= [_manual_bookmarks count])
					[_manual_bookmarks addObject:temp_bookmark];
				else
					[_manual_bookmarks insertObject:temp_bookmark atIndex:toIdx];
				[temp_bookmark release];

				[self scheduleWriteManualBookmarksToDataStore];
				break;
			}
		}
	}
}

// prevent that an item is moved befoer the Add Bookmark item
- (NSIndexPath *)tableView:(UITableView *)tableView
    targetIndexPathForMoveFromRowAtIndexPath:(NSIndexPath *)sourceIndexPath
                         toProposedIndexPath:(NSIndexPath *)proposedDestinationIndexPath
{
	// don't allow to move:
	//  - items between sections
	//  - the quick connect/quick create bookmark cell
	//  - any item while a search is applied
	if ([proposedDestinationIndexPath row] == 0 ||
	    ([sourceIndexPath section] != [proposedDestinationIndexPath section]) ||
	    _manual_search_result != nil)
	{
		return sourceIndexPath;
	}
	else
	{
		return proposedDestinationIndexPath;
	}
}

// Override to support conditional rearranging of the table view.
- (BOOL)tableView:(UITableView *)tableView canMoveRowAtIndexPath:(NSIndexPath *)indexPath
{
	// dont allow to reorder Add Bookmark item
	if ([indexPath section] == SECTION_BOOKMARKS && [indexPath row] == 0)
		return NO;
	return YES;
}

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section
{
	if (section == SECTION_SESSIONS && [_active_sessions count] > 0)
		return NSLocalizedString(@"My Sessions", @"'My Session': section sessions header");
	if (section == SECTION_BOOKMARKS)
		return NSLocalizedString(@"Manual Connections",
		                         @"'Manual Connections': section manual bookmarks header");
	return nil;
}

- (NSString *)tableView:(UITableView *)tableView titleForFooterInSection:(NSInteger)section
{
	return nil;
}

- (CGFloat)tableView:(UITableView *)tableView heightForRowAtIndexPath:(NSIndexPath *)indexPath
{
	if ([indexPath section] == SECTION_SESSIONS)
		return 72;
	return [tableView rowHeight];
}

#pragma mark -
#pragma mark Table view delegate

- (void)setEditing:(BOOL)editing animated:(BOOL)animated
{
	[super setEditing:editing animated:animated];
	[[self tableView] setEditing:editing animated:animated];
}

- (void)accessoryButtonTapped:(UIControl *)button withEvent:(UIEvent *)event
{
	// forward a tap on our custom accessory button to the real accessory button handler
	NSIndexPath *indexPath =
	    [[self tableView] indexPathForRowAtPoint:[[[event touchesForView:button] anyObject]
	                                                 locationInView:[self tableView]]];
	if (indexPath == nil)
		return;

	[[[self tableView] delegate] tableView:[self tableView]
	    accessoryButtonTappedForRowWithIndexPath:indexPath];
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
	if ([indexPath section] == SECTION_SESSIONS)
	{
		// resume session
		RDPSession *session = [_active_sessions objectAtIndex:[indexPath row]];
		UIViewController *ctrl =
		    [[[RDPSessionViewController alloc] initWithNibName:@"RDPSessionView"
		                                                bundle:nil
		                                               session:session] autorelease];
		[ctrl setHidesBottomBarWhenPushed:YES];
		[[self navigationController] pushViewController:ctrl animated:YES];
	}
	else
	{
		ComputerBookmark *bookmark = nil;
		if ([indexPath section] == SECTION_BOOKMARKS)
		{
			// first row has either quick connect or add bookmark item
			if ([indexPath row] == 0)
			{
				if ([[_searchBar text] length] == 0)
				{
					// show add bookmark controller
					ComputerBookmark *bookmark =
					    [[[ComputerBookmark alloc] initWithBaseDefaultParameters] autorelease];
					BookmarkEditorController *bookmarkEditorController =
					    [[[BookmarkEditorController alloc] initWithBookmark:bookmark] autorelease];
					[bookmarkEditorController
					    setTitle:NSLocalizedString(@"Add Connection", @"Add Connection title")];
					[bookmarkEditorController setDelegate:self];
					[bookmarkEditorController setHidesBottomBarWhenPushed:YES];
					[[self navigationController] pushViewController:bookmarkEditorController
					                                       animated:YES];
				}
				else
				{
					// create a quick connect bookmark and add an entry to the quick connect history
					// (if not already in the history)
					bookmark = [self bookmarkForQuickConnectTo:[_searchBar text]];
					if (![_connection_history containsObject:[_searchBar text]])
					{
						[_connection_history addObject:[_searchBar text]];
						[self scheduleWriteConnectionHistoryToDataStore];
					}
				}
			}
			else
			{
				if (_manual_search_result != nil)
				{
					if ([self isIndexPathToHistoryItem:indexPath])
					{
						// create a quick connect bookmark for a history item
						NSString *item = [_history_search_result
						    objectAtIndex:[self historyIndexFromIndexPath:indexPath]];
						bookmark = [self bookmarkForQuickConnectTo:item];
					}
					else
						bookmark = [[_manual_search_result
						    objectAtIndex:[self bookmarkIndexFromIndexPath:indexPath]]
						    valueForKey:@"bookmark"];
				}
				else
					bookmark = [_manual_bookmarks
					    objectAtIndex:[self bookmarkIndexFromIndexPath:
					                            indexPath]]; // -1 because of ADD BOOKMARK entry
			}

			// set reachability status
			WakeUpWWAN();
			[bookmark
			    setConntectedViaWLAN:[[Reachability
			                             reachabilityWithHostName:[[bookmark params]
			                                                          StringForKey:@"hostname"]]
			                             currentReachabilityStatus] == ReachableViaWiFi];
		}

		if (bookmark != nil)
		{
			// create rdp session
			RDPSession *session = [[[RDPSession alloc] initWithBookmark:bookmark] autorelease];
			UIViewController *ctrl =
			    [[[RDPSessionViewController alloc] initWithNibName:@"RDPSessionView"
			                                                bundle:nil
			                                               session:session] autorelease];
			[ctrl setHidesBottomBarWhenPushed:YES];
			[[self navigationController] pushViewController:ctrl animated:YES];
			[_active_sessions addObject:session];
		}
	}
}

- (void)tableView:(UITableView *)tableView
    accessoryButtonTappedForRowWithIndexPath:(NSIndexPath *)indexPath
{
	// get the bookmark
	NSString *bookmark_editor_title =
	    NSLocalizedString(@"Edit Connection", @"Edit Connection title");
	ComputerBookmark *bookmark = nil;
	if ([indexPath section] == SECTION_BOOKMARKS)
	{
		if ([indexPath row] == 0)
		{
			// create a new bookmark and init hostname and label
			bookmark = [self bookmarkForQuickConnectTo:[_searchBar text]];
			bookmark_editor_title = NSLocalizedString(@"Add Connection", @"Add Connection title");
		}
		else
		{
			if (_manual_search_result != nil)
			{
				if ([self isIndexPathToHistoryItem:indexPath])
				{
					// create a new bookmark and init hostname and label
					NSString *item = [_history_search_result
					    objectAtIndex:[self historyIndexFromIndexPath:indexPath]];
					bookmark = [self bookmarkForQuickConnectTo:item];
					bookmark_editor_title =
					    NSLocalizedString(@"Add Connection", @"Add Connection title");
				}
				else
					bookmark = [[_manual_search_result
					    objectAtIndex:[self bookmarkIndexFromIndexPath:indexPath]]
					    valueForKey:@"bookmark"];
			}
			else
				bookmark = [_manual_bookmarks
				    objectAtIndex:[self bookmarkIndexFromIndexPath:indexPath]]; // -1 because of ADD
				                                                                // BOOKMARK entry
		}
	}

	// bookmark found? - start the editor
	if (bookmark != nil)
	{
		BookmarkEditorController *editBookmarkController =
		    [[[BookmarkEditorController alloc] initWithBookmark:bookmark] autorelease];
		[editBookmarkController setHidesBottomBarWhenPushed:YES];
		[editBookmarkController setTitle:bookmark_editor_title];
		[editBookmarkController setDelegate:self];
		[[self navigationController] pushViewController:editBookmarkController animated:YES];
	}
}

#pragma mark -
#pragma mark Search Bar Delegates

- (BOOL)searchBarShouldBeginEditing:(UISearchBar *)searchBar
{
	// show cancel button
	[searchBar setShowsCancelButton:YES animated:YES];
	return YES;
}

- (void)searchBarCancelButtonClicked:(UISearchBar *)searchBar
{
	// clear search result
	[_manual_search_result release];
	_manual_search_result = nil;

	// clear text and remove cancel button
	[searchBar setText:@""];
	[searchBar resignFirstResponder];
}

- (BOOL)searchBarShouldEndEditing:(UISearchBar *)searchBar
{
	[searchBar setShowsCancelButton:NO animated:YES];

	// re-enable table selection
	[_tableView setAllowsSelection:YES];

	return YES;
}

- (void)searchBarSearchButtonClicked:(UISearchBar *)searchBar
{
	[_searchBar resignFirstResponder];
}

- (void)searchBar:(UISearchBar *)searchBar textDidChange:(NSString *)searchText
{
	[self performSearch:searchText];
	[_tableView reloadData];
}

#pragma mark - Session handling

// session was added
- (void)sessionDisconnected:(NSNotification *)notification
{
	// remove session from active sessions
	RDPSession *session = (RDPSession *)[notification object];
	[_active_sessions removeObject:session];

	// if this view is currently active refresh entries
	if ([[self navigationController] visibleViewController] == self)
		[_tableView reloadSections:[NSIndexSet indexSetWithIndex:SECTION_SESSIONS]
		          withRowAnimation:UITableViewRowAnimationNone];

	// if session's bookmark is not in the bookmark list ask the user if he wants to add it
	// (this happens if the session is created using the quick connect feature)
	if (![_manual_bookmarks containsObject:[session bookmark]])
	{
		// retain the bookmark in case we want to save it later
		_temporary_bookmark = [[session bookmark] retain];

		// ask the user if he wants to save the bookmark
		NSString *title =
		    NSLocalizedString(@"Save Connection Settings?", @"Save connection settings title");
		NSString *message = NSLocalizedString(
		    @"Your Connection Settings have not been saved. Do you want to save them?",
		    @"Save connection settings message");
		BlockAlertView *alert = [BlockAlertView alertWithTitle:title message:message];
		[alert setCancelButtonWithTitle:NSLocalizedString(@"No", @"No Button") block:nil];
		[alert addButtonWithTitle:NSLocalizedString(@"Yes", @"Yes Button")
		                    block:^{
			                    if (_temporary_bookmark)
			                    {
				                    [_manual_bookmarks addObject:_temporary_bookmark];
				                    [_tableView
				                          reloadSections:[NSIndexSet
				                                             indexSetWithIndex:SECTION_BOOKMARKS]
				                        withRowAnimation:UITableViewRowAnimationNone];
				                    [_temporary_bookmark autorelease];
				                    _temporary_bookmark = nil;
			                    }
		                    }];
		[alert show];
	}
}

- (void)sessionFailedToConnect:(NSNotification *)notification
{
	// remove session from active sessions
	RDPSession *session = (RDPSession *)[notification object];
	[_active_sessions removeObject:session];

	// display error toast
	[[self view] makeToast:NSLocalizedString(@"Failed to connect to session!",
	                                         @"Failed to connect error message")
	              duration:ToastDurationNormal
	              position:@"center"];
}

#pragma mark - Reachability notification
- (void)reachabilityChanged:(NSNotification *)notification
{
	// no matter how the network changed - we will disconnect
	// disconnect session (if there is any)
	if ([_active_sessions count] > 0)
	{
		RDPSession *session = [_active_sessions objectAtIndex:0];
		[session disconnect];
	}
}

#pragma mark - BookmarkEditorController delegate

- (void)commitBookmark:(ComputerBookmark *)bookmark
{
	// if we got a manual bookmark that is not in the list yet - add it otherwise replace it
	BOOL found = NO;
	for (int idx = 0; idx < [_manual_bookmarks count]; ++idx)
	{
		if ([[bookmark uuid] isEqualToString:[[_manual_bookmarks objectAtIndex:idx] uuid]])
		{
			[_manual_bookmarks replaceObjectAtIndex:idx withObject:bookmark];
			found = YES;
			break;
		}
	}
	if (!found)
		[_manual_bookmarks addObject:bookmark];

	// remove any quick connect history entry with the same hostname
	NSString *hostname = [[bookmark params] StringForKey:@"hostname"];
	if ([_connection_history containsObject:hostname])
	{
		[_connection_history removeObject:hostname];
		[self scheduleWriteConnectionHistoryToDataStore];
	}

	[self scheduleWriteManualBookmarksToDataStore];
}

- (IBAction)disconnectButtonPressed:(id)sender
{
	// disconnect session and refresh table view
	RDPSession *session = [_active_sessions objectAtIndex:[sender tag]];
	[session disconnect];
}

#pragma mark - Misc functions

- (BOOL)hasNoBookmarks
{
	return ([_manual_bookmarks count] == 0);
}

- (UIButton *)disclosureButtonWithImage:(UIImage *)image
{
	// we make the button a little bit bigger (image widht * 2, height + 10) so that the user
	// doesn't accidentally connect to the bookmark ...
	UIButton *button = [UIButton buttonWithType:UIButtonTypeCustom];
	[button setFrame:CGRectMake(0, 0, [image size].width * 2, [image size].height + 10)];
	[button setImage:image forState:UIControlStateNormal];
	[button addTarget:self
	              action:@selector(accessoryButtonTapped:withEvent:)
	    forControlEvents:UIControlEventTouchUpInside];
	[button setUserInteractionEnabled:YES];
	return button;
}

- (void)performSearch:(NSString *)searchText
{
	[_manual_search_result autorelease];

	if ([searchText length] > 0)
	{
		_manual_search_result = [FilterBookmarks(
		    _manual_bookmarks,
		    [searchText componentsSeparatedByCharactersInSet:[NSCharacterSet
		                                                         whitespaceAndNewlineCharacterSet]])
		    retain];
		_history_search_result = [FilterHistory(_connection_history, searchText) retain];
	}
	else
	{
		_history_search_result = nil;
		_manual_search_result = nil;
	}
}

- (int)bookmarkIndexFromIndexPath:(NSIndexPath *)indexPath
{
	return [indexPath row] -
	       ((_history_search_result != nil) ? [_history_search_result count] : 0) - 1;
}

- (int)historyIndexFromIndexPath:(NSIndexPath *)indexPath
{
	return [indexPath row] - 1;
}

- (BOOL)isIndexPathToHistoryItem:(NSIndexPath *)indexPath
{
	return (([indexPath row] - 1) < [_history_search_result count]);
}

- (ComputerBookmark *)bookmarkForQuickConnectTo:(NSString *)host
{
	ComputerBookmark *bookmark =
	    [[[ComputerBookmark alloc] initWithBaseDefaultParameters] autorelease];
	[bookmark setLabel:host];
	[[bookmark params] setValue:host forKey:@"hostname"];
	return bookmark;
}

#pragma mark - Persisting bookmarks

- (void)scheduleWriteBookmarksToDataStore
{
	[[NSOperationQueue mainQueue] addOperationWithBlock:^{
		[self writeBookmarksToDataStore];
	}];
}

- (void)writeBookmarksToDataStore
{
	[self writeManualBookmarksToDataStore];
}

- (void)scheduleWriteManualBookmarksToDataStore
{
	[[NSOperationQueue mainQueue]
	    addOperation:[[[NSInvocationOperation alloc]
	                     initWithTarget:self
	                           selector:@selector(writeManualBookmarksToDataStore)
	                             object:nil] autorelease]];
}

- (void)writeManualBookmarksToDataStore
{
	[self writeArray:_manual_bookmarks toDataStoreURL:[self manualBookmarksDataStoreURL]];
}

- (void)scheduleWriteConnectionHistoryToDataStore
{
	[[NSOperationQueue mainQueue]
	    addOperation:[[[NSInvocationOperation alloc]
	                     initWithTarget:self
	                           selector:@selector(writeConnectionHistoryToDataStore)
	                             object:nil] autorelease]];
}

- (void)writeConnectionHistoryToDataStore
{
	[self writeArray:_connection_history toDataStoreURL:[self connectionHistoryDataStoreURL]];
}

- (void)writeArray:(NSArray *)bookmarks toDataStoreURL:(NSURL *)url
{
	NSData *archived_data = [NSKeyedArchiver archivedDataWithRootObject:bookmarks];
	[archived_data writeToURL:url atomically:YES];
}

- (void)readManualBookmarksFromDataStore
{
	[_manual_bookmarks autorelease];
	_manual_bookmarks = [self arrayFromDataStoreURL:[self manualBookmarksDataStoreURL]];

	if (_manual_bookmarks == nil)
	{
		_manual_bookmarks = [[NSMutableArray alloc] init];
		[_manual_bookmarks
		    addObject:[[[GlobalDefaults sharedGlobalDefaults] newTestServerBookmark] autorelease]];
	}
}

- (void)readConnectionHistoryFromDataStore
{
	[_connection_history autorelease];
	_connection_history = [self arrayFromDataStoreURL:[self connectionHistoryDataStoreURL]];

	if (_connection_history == nil)
		_connection_history = [[NSMutableArray alloc] init];
}

- (NSMutableArray *)arrayFromDataStoreURL:(NSURL *)url
{
	NSData *archived_data = [NSData dataWithContentsOfURL:url];

	if (!archived_data)
		return nil;

	return [[NSKeyedUnarchiver unarchiveObjectWithData:archived_data] retain];
}

- (NSURL *)manualBookmarksDataStoreURL
{
	return [NSURL
	    fileURLWithPath:[NSString stringWithFormat:@"%@/%@",
	                                               [NSSearchPathForDirectoriesInDomains(
	                                                   NSDocumentDirectory, NSUserDomainMask, YES)
	                                                   lastObject],
	                                               @"com.freerdp.ifreerdp.bookmarks.plist"]];
}

- (NSURL *)connectionHistoryDataStoreURL
{
	return [NSURL
	    fileURLWithPath:[NSString
	                        stringWithFormat:@"%@/%@",
	                                         [NSSearchPathForDirectoriesInDomains(
	                                             NSDocumentDirectory, NSUserDomainMask, YES)
	                                             lastObject],
	                                         @"com.freerdp.ifreerdp.connection_history.plist"]];
}

@end
