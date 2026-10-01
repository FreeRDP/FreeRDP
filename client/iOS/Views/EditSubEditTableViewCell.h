/*
 Custom table cell indicating a switch to a sub-view

 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import <UIKit/UIKit.h>

@interface EditSubEditTableViewCell : UITableViewCell
{
	NSString *_title;
	BOOL _enabled;
}

@property(copy, nonatomic) NSString *title;
@property(assign, nonatomic) BOOL enabled;

@end
