/*
 Scene delegate (UIScene life cycle)

 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import "SceneDelegate.h"

#import "AboutController.h"
#import "BookmarkListController.h"
#import "AppSettingsController.h"
#import "MainTabBarController.h"

@implementation SceneDelegate

@synthesize window = _window;

- (void)scene:(UIScene *)scene
    willConnectToSession:(UISceneSession *)session
                 options:(UISceneConnectionOptions *)connectionOptions
{
	if (![scene isKindOfClass:[UIWindowScene class]])
		return;

	UIWindowScene *windowScene = (UIWindowScene *)scene;
	_window = [[UIWindow alloc] initWithWindowScene:windowScene];

	MainTabBarController *tabBarController = [[[MainTabBarController alloc] init] autorelease];

	// create bookmark view and navigation controller
	BookmarkListController *bookmarkListController =
	    [[[BookmarkListController alloc] initWithNibName:@"BookmarkListView"
	                                              bundle:nil] autorelease];
	UINavigationController *bookmarkNavigationController = [[[UINavigationController alloc]
	    initWithRootViewController:bookmarkListController] autorelease];

	// create app settings view and navigation controller
	AppSettingsController *appSettingsController =
	    [[[AppSettingsController alloc] initWithStyle:UITableViewStyleGrouped] autorelease];
	UINavigationController *appSettingsNavigationController = [[[UINavigationController alloc]
	    initWithRootViewController:appSettingsController] autorelease];

	// create about view controller
	AboutController *aboutViewController =
	    [[[AboutController alloc] initWithNibName:nil bundle:nil] autorelease];

	// add tab-bar controller to the main window and display everything
	NSArray *tabItems =
	    [NSArray arrayWithObjects:bookmarkNavigationController, appSettingsNavigationController,
	                              aboutViewController, nil];
	[tabBarController setViewControllers:tabItems];

	[_window setRootViewController:tabBarController];
	[_window makeKeyAndVisible];
}

- (void)dealloc
{
	[_window release];
	[super dealloc];
}

@end
