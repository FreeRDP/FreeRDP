//
//  AppDelegate.m
//  MacFreeRDP
//
//  Created by Thomas Goddard on 5/8/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#import "AppDelegate.h"

@implementation AppDelegate

@synthesize window = _window;

@synthesize mrdpView;

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
	rdp_connect();
}

- (void) applicationWillTerminate:(NSNotification *)notification
{
	//[mrdpView releaseResources];
}

@end
