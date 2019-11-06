/*
 Credentials input controller

 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import <UIKit/UIKit.h>

@class RDPSession;

@interface CredentialsInputController : UIViewController
{
  @private
	IBOutlet UITextField *_textfield_username;
	IBOutlet UITextField *_textfield_password;
	IBOutlet UITextField *_textfield_domain;
	IBOutlet UIButton *_btn_login;
	IBOutlet UIButton *_btn_cancel;
	IBOutlet UIScrollView *_scroll_view;
	IBOutlet UILabel *_lbl_message;

	RDPSession *_session;
	NSMutableDictionary *_params;
}

- (id)initWithNibName:(NSString *)nibNameOrNil
               bundle:(NSBundle *)nibBundleOrNil
              session:(RDPSession *)session
               params:(NSMutableDictionary *)params;

@end
