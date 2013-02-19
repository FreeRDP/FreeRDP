/*
 RDP Session View
 
 Copyright 2013 Thinstuff Technologies GmbH, Author: Martin Fleisz
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#import <UIKit/UIKit.h>
#import "RDPSession.h"

@interface RDPSessionView : UIView
{
    RDPSession* _session;
}

- (void)setSession:(RDPSession*)session;

@end
