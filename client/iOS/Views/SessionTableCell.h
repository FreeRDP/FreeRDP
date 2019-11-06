/*
 Custom session table cell

 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import <UIKit/UIKit.h>

@interface SessionTableCell : UITableViewCell
{
	IBOutlet UILabel *_title;
	IBOutlet UILabel *_server;
	IBOutlet UILabel *_username;
	IBOutlet UIImageView *_screenshot;
	IBOutlet UIButton *_disconnect_button;
}

@property(retain, nonatomic) UILabel *title;
@property(retain, nonatomic) UILabel *server;
@property(retain, nonatomic) UILabel *username;
@property(retain, nonatomic) UIImageView *screenshot;
@property(retain, nonatomic) UIButton *disconnectButton;

@end
