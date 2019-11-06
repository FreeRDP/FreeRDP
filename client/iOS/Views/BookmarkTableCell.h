/*
 Custom bookmark table cell

 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import <UIKit/UIKit.h>

@interface BookmarkTableCell : UITableViewCell
{
	IBOutlet UILabel *_title;
	IBOutlet UILabel *_sub_title;
	IBOutlet UIImageView *_connection_state_icon;
}

@property(retain, nonatomic) UILabel *title;
@property(retain, nonatomic) UILabel *subTitle;
@property(retain, nonatomic) UIImageView *connectionStateIcon;

@end
