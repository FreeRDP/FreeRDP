/*
 Global default bookmark settings

 Copyright 2013 Thincast Technologies GmbH, Author: Dorian Johnson

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import <Foundation/Foundation.h>

@class ConnectionParams, ComputerBookmark;

@interface GlobalDefaults : NSObject
{
  @private
	ComputerBookmark *_default_bookmark;
}

+ (GlobalDefaults *)sharedGlobalDefaults;

// The same object is always returned from this method.
@property(readonly, nonatomic) ComputerBookmark *bookmark;

- (ConnectionParams *)newParams;
- (ComputerBookmark *)newBookmark;
- (ComputerBookmark *)newTestServerBookmark;

@end
