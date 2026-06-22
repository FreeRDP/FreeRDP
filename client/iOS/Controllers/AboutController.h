/*
 Application info controller

 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

// UIWebView (deprecated) --> WKWebView
@interface AboutController : UIViewController <WKNavigationDelegate>
{
	NSString *last_link_clicked;
	WKWebView *webView;
}

@end
