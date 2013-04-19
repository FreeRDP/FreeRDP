/*
 Bookmark model abstraction
 
 Copyright 2013 Thinstuff Technologies GmbH, Authors: Dorian Johnson
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */


#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import "ConnectionParams.h"

@interface ComputerBookmark : NSObject <NSCoding> {
@protected
	ComputerBookmark* _parent;
	NSString* _uuid, * _label;
	UIImage* _image;
    ConnectionParams* _connection_params;
    BOOL _connected_via_wlan;
}

@property (nonatomic,assign)   ComputerBookmark* parent;
@property (nonatomic,readonly) NSString* uuid;
@property (nonatomic,copy)     NSString* label;
@property (nonatomic,retain)   UIImage* image;
@property (readonly, nonatomic) ConnectionParams* params;
@property (nonatomic, assign) BOOL conntectedViaWLAN;

// Creates a copy of this object, with a new UUID
- (id)copy;
- (id)copyWithUUID;

// Whether user can delete, move, or rename this entry
- (BOOL)isDeletable;
- (BOOL)isMovable;
- (BOOL)isRenamable;
- (BOOL)hasImmutableHost;

- (id)initWithConnectionParameters:(ConnectionParams*)params;
- (id)initWithBaseDefaultParameters;

// A copy of @params, with _bookmark_uuid set.
- (ConnectionParams*)copyMarkedParams;

@end

