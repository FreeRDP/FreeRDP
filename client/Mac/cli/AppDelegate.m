//
//  AppDelegate.m
//  MacClient2
//
//  Created by Benoît et Kathy on 2013-05-08.
//
//

#import "AppDelegate.h"

@implementation AppDelegate

- (void)dealloc
{
    [super dealloc];
}

@synthesize window = window;

@synthesize mrdpView = mrdpView;

- (void)applicationDidFinishLaunching:(NSNotification*)aNotification
{
	[mrdpView rdpConnect];
}

- (void) applicationWillTerminate:(NSNotification*)notification
{
    [mrdpView releaseResources];
}

@end
