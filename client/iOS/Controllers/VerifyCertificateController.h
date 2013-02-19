/*
 Certificate verification controller
 
 Copyright 2013 Thinstuff Technologies GmbH, Author: Martin Fleisz
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#import <UIKit/UIKit.h>

@class RDPSession;

@interface VerifyCertificateController : UIViewController
{
@private
    IBOutlet UILabel* _label_issuer;    
    IBOutlet UIButton* _btn_accept;
    IBOutlet UIButton* _btn_decline;
    IBOutlet UILabel* _label_message;
    IBOutlet UILabel* _label_for_issuer;

    RDPSession* _session;
    NSMutableDictionary* _params;
}

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil session:(RDPSession*)session params:(NSMutableDictionary*)params;

@end
