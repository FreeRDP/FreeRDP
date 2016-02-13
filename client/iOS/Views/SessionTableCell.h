/*
 Custom session table cell
 
 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#import <UIKit/UIKit.h>


@interface SessionTableCell : UITableViewCell 
{
	IBOutlet UILabel* _title;
	IBOutlet UILabel* _server;
	IBOutlet UILabel* _username;
	IBOutlet UIImageView* _screenshot;
	IBOutlet UIButton* _disconnect_button;
}

@property (strong, nonatomic) UILabel* title;
@property (strong, nonatomic) UILabel* server;
@property (strong, nonatomic) UILabel* username;
@property (strong, nonatomic) UIImageView* screenshot;
@property (strong, nonatomic) UIButton* disconnectButton;

@end
