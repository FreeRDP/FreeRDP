/*
 App delegate
 
 Copyright 2013 Thinstuff Technologies GmbH, Author: Martin Fleisz
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#import "AppDelegate.h"

#import "AboutController.h"
#import "HelpController.h"
#import "BookmarkListController.h"
#import "AppSettingsController.h"
#import "MainTabBarController.h"
#import "Utils.h"

@implementation AppDelegate


@synthesize window = _window, tabBarController = _tabBarController;


- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{    
	// Set default values for most NSUserDefaults
	[[NSUserDefaults standardUserDefaults] registerDefaults:[NSDictionary dictionaryWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"Defaults" ofType:@"plist"]]];

    // init global settings
    SetSwapMouseButtonsFlag([[NSUserDefaults standardUserDefaults] boolForKey:@"ui.swap_mouse_buttons"]);
    SetInvertScrollingFlag([[NSUserDefaults standardUserDefaults] boolForKey:@"ui.invert_scrolling"]);
    
    // create bookmark view and navigation controller
    BookmarkListController* bookmarkListController = [[[BookmarkListController alloc] initWithNibName:@"BookmarkListView" bundle:nil] autorelease];
    UINavigationController* bookmarkNavigationController = [[[UINavigationController alloc] initWithRootViewController:bookmarkListController] autorelease];	

    // create app settings view and navigation controller
    AppSettingsController* appSettingsController = [[[AppSettingsController alloc] initWithStyle:UITableViewStyleGrouped] autorelease];
    UINavigationController* appSettingsNavigationController = [[[UINavigationController alloc] initWithRootViewController:appSettingsController] autorelease];	
     
    // create help view controller
    HelpController* helpViewController = [[[HelpController alloc] initWithNibName:nil bundle:nil] autorelease];

    // create about view controller
    AboutController* aboutViewController = [[[AboutController alloc] initWithNibName:nil bundle:nil] autorelease];
     
    // add tab-bar controller to the main window and display everything
    NSArray* tabItems = [NSArray arrayWithObjects:bookmarkNavigationController, appSettingsNavigationController, helpViewController, aboutViewController, nil];
    [_tabBarController setViewControllers:tabItems];
    if ([_window respondsToSelector:@selector(setRootViewController:)])
        [_window setRootViewController:_tabBarController];
    else
        [_window addSubview:[_tabBarController view]];
    [_window makeKeyAndVisible];	   

    return YES;
}

- (void)applicationWillResignActive:(UIApplication *)application
{
    /*
     Sent when the application is about to move from active to inactive state. This can occur for certain types of temporary interruptions (such as an incoming phone call or SMS message) or when the user quits the application and it begins the transition to the background state.
     Use this method to pause ongoing tasks, disable timers, and throttle down OpenGL ES frame rates. Games should use this method to pause the game.
     */
}

- (void)applicationDidEnterBackground:(UIApplication *)application
{
    /*
     Use this method to release shared resources, save user data, invalidate timers, and store enough application state information to restore your application to its current state in case it is terminated later. 
     If your application supports background execution, this method is called instead of applicationWillTerminate: when the user quits.
     */
}

- (void)applicationWillEnterForeground:(UIApplication *)application
{
    /*
     Called as part of the transition from the background to the inactive state; here you can undo many of the changes made on entering the background.
     */
    // cancel disconnect timer
}

- (void)applicationDidBecomeActive:(UIApplication *)application
{
    /*
     Restart any tasks that were paused (or not yet started) while the application was inactive. If the application was previously in the background, optionally refresh the user interface.
     */
}

- (void)applicationWillTerminate:(UIApplication *)application
{
    /*
     Called when the application is about to terminate.
     Save data if appropriate.
     See also applicationDidEnterBackground:.
     */
}

- (void)dealloc
{
    [_window release];
    [super dealloc];
}

@end
