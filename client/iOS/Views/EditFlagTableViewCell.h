/*
 Custom table cell with toggle switch

 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import <UIKit/UIKit.h>

@interface EditFlagTableViewCell : UITableViewCell
{
	NSString *_title;
	UISwitch *_toggle;
}

@property(copy, nonatomic) NSString *title;
@property(readonly, nonatomic) UISwitch *toggle;

@end
