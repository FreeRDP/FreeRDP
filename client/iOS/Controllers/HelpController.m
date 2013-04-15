/*
 Application help controller
 
 Copyright 2013 Thinstuff Technologies GmbH, Author: Martin Fleisz
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#import "HelpController.h"
#import "Utils.h"

@implementation HelpController

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
		// set title and tab-bar image
		[self setTitle:NSLocalizedString(@"Help", @"Help Controller title")];
        UIImage* tabBarIcon = [UIImage imageWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"tabbar_icon_help" ofType:@"png"]];
        [self setTabBarItem:[[[UITabBarItem alloc] initWithTitle:NSLocalizedString(@"Help", @"Tabbar item help") image:tabBarIcon tag:0] autorelease]];
    }
    return self;
}

// Implement loadView to create a view hierarchy programmatically, without using a nib.
- (void)loadView 
{
	webView = [[[UIWebView alloc] initWithFrame:CGRectZero] autorelease];
	[webView setAutoresizingMask:(UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight)];
	[webView setAutoresizesSubviews:YES];
	[webView setDelegate:self];	
	[webView setDataDetectorTypes:UIDataDetectorTypeNone];	
	[self setView:webView];
}

- (void)dealloc {
    [super dealloc];
}

// Implement viewDidLoad to do additional setup after loading the view, typically from a nib.
- (void)viewDidLoad 
{
    [super viewDidLoad];
	
    NSString *filename = (IsPhone() ? @"gestures_phone" : @"gestures");
	NSString *htmlString = [[[NSString alloc] initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:filename ofType:@"html" inDirectory:@"help_page"]  encoding:NSUTF8StringEncoding error:nil] autorelease];
	
	[webView loadHTMLString:htmlString baseURL:[NSURL fileURLWithPath:[[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@"help_page"]]];    
}

// Override to allow orientations other than the default portrait orientation.
- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation {
    return YES;
}

@end
