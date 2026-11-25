/*
 Custom table cell with edit text field

 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import <UIKit/UIKit.h>

@interface EditTextTableViewCell : UITableViewCell
{
	UILabel *_label;
	UITextField *_textfield;
	BOOL _enabled;
}

@property(readonly, nonatomic) UILabel *label;
@property(readonly, nonatomic) UITextField *textfield;
@property(assign, nonatomic) BOOL enabled;

@end
