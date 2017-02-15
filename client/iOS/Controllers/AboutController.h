/*
 Application info controller
 
 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#import <UIKit/UIKit.h>


@interface AboutController : UIViewController <UIWebViewDelegate>
{
    NSString* last_link_clicked;
	UIWebView* webView;
}

@end
