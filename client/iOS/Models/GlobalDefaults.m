/*
 Global default bookmark settings
 
 Copyright 2013 Thinstuff Technologies GmbH, Author: Dorian Johnson
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#import "GlobalDefaults.h"
#import "Bookmark.h"
#import "ConnectionParams.h"

@implementation GlobalDefaults

+ (GlobalDefaults*)sharedGlobalDefaults
{
	static GlobalDefaults* _shared_global_defaults = nil;
	
	if (_shared_global_defaults == nil)
	{
		@synchronized(self)
		{
			if (_shared_global_defaults == nil)
				_shared_global_defaults = [[GlobalDefaults alloc] init];		
		}
	}
	
	return _shared_global_defaults;	
}

- (id)init
{
	if (!(self = [super init]))
		return nil;
	
	ComputerBookmark* bookmark = nil;
	NSData* bookmark_data = [[NSUserDefaults standardUserDefaults] objectForKey:@"TSXSharedGlobalDefaultBookmark"];
	
	if (bookmark_data && [bookmark_data length])
		bookmark = [NSKeyedUnarchiver unarchiveObjectWithData:bookmark_data];
	
	if (!bookmark)
		bookmark = [[[ComputerBookmark alloc] initWithBaseDefaultParameters] autorelease];
	
	_default_bookmark = [bookmark retain];
	return self;
}

- (void)dealloc
{
	[_default_bookmark release];
	[super dealloc];
}

#pragma mark -

@synthesize bookmark=_default_bookmark;

- (ComputerBookmark*)newBookmark
{
	return [[ComputerBookmark alloc] initWithConnectionParameters:[[self newParams] autorelease]];
}

- (ConnectionParams*)newParams
{
	ConnectionParams* param_copy = [[[self bookmark] params] copy];
	return param_copy;
}

- (ComputerBookmark*)newTestServerBookmark
{
    ComputerBookmark* bm = [self newBookmark];
    [bm setLabel:@"Test Server"];
    [[bm params] setValue:@"testservice.ifreerdp.com" forKey:@"hostname"];
    [[bm params] setInt:0 forKey:@"screen_resolution_type"];
    [[bm params] setInt:1024 forKey:@"width"];
    [[bm params] setInt:768 forKey:@"height"];
    [[bm params] setInt:32 forKey:@"colors"];
    [[bm params] setBool:YES forKey:@"perf_remotefx"];
	return bm;
}

@end
