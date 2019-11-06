/*
 Application info controller

 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import "AboutController.h"
#import "Utils.h"
#import "BlockAlertView.h"

@implementation AboutController

// The designated initializer.  Override if you create the controller programmatically and want to
// perform customization that is not appropriate for viewDidLoad.
- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
	if ((self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil]))
	{

		// set title and tab-bar image
		[self setTitle:NSLocalizedString(@"About", @"About Controller title")];
		UIImage *tabBarIcon = [UIImage
		    imageWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"tabbar_icon_about"
		                                                            ofType:@"png"]];
		[self setTabBarItem:[[[UITabBarItem alloc]
		                        initWithTitle:NSLocalizedString(@"About", @"Tabbar item about")
		                                image:tabBarIcon
		                                  tag:0] autorelease]];

		last_link_clicked = nil;
	}
	return self;
}

- (void)dealloc
{
	[super dealloc];
	[last_link_clicked release];
}

// Implement loadView to create a view hierarchy programmatically, without using a nib.
- (void)loadView
{
	webView = [[[UIWebView alloc] initWithFrame:CGRectZero] autorelease];
	[webView
	    setAutoresizingMask:(UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight)];
	[webView setAutoresizesSubviews:YES];
	[webView setDelegate:self];
	[webView setDataDetectorTypes:UIDataDetectorTypeNone];
	[self setView:webView];
}

// Implement viewDidLoad to do additional setup after loading the view, typically from a nib.
- (void)viewDidLoad
{
	[super viewDidLoad];

	NSString *filename = (IsPhone() ? @"about_phone" : @"about");
	NSString *htmlString = [[[NSString alloc]
	    initWithContentsOfFile:[[NSBundle mainBundle] pathForResource:filename
	                                                           ofType:@"html"
	                                                      inDirectory:@"about_page"]
	                  encoding:NSUTF8StringEncoding
	                     error:nil] autorelease];

	[webView
	    loadHTMLString:[NSString stringWithFormat:htmlString, TSXAppFullVersion(),
	                                              [[UIDevice currentDevice] systemName],
	                                              [[UIDevice currentDevice] systemVersion],
	                                              [[UIDevice currentDevice] model]]
	           baseURL:[NSURL fileURLWithPath:[[[NSBundle mainBundle] bundlePath]
	                                              stringByAppendingPathComponent:@"about_page"]]];
}

// Override to allow orientations other than the default portrait orientation.
- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
	return YES;
}

#pragma mark -
#pragma mark UIWebView callbacks
- (BOOL)webView:(UIWebView *)webView
    shouldStartLoadWithRequest:(NSURLRequest *)request
                navigationType:(UIWebViewNavigationType)navigationType
{
	if ([[request URL] isFileURL])
		return YES;

	if (navigationType == UIWebViewNavigationTypeLinkClicked)
	{
		[last_link_clicked release];
		last_link_clicked = [[[request URL] absoluteString] retain];
		BlockAlertView *alert = [BlockAlertView
		    alertWithTitle:NSLocalizedString(@"External Link", @"External Link Alert Title")
		           message:[NSString stringWithFormat:
		                                 NSLocalizedString(
		                                     @"Open [%@] in Browser?",
		                                     @"Open link in browser (with link as parameter)"),
		                                 last_link_clicked]];

		[alert setCancelButtonWithTitle:NSLocalizedString(@"No", @"No Button") block:nil];
		[alert addButtonWithTitle:NSLocalizedString(@"OK", @"OK Button")
		                    block:^{
			                    [[UIApplication sharedApplication]
			                        openURL:[NSURL URLWithString:last_link_clicked]];
		                    }];

		[alert show];

		return NO;
	}
	return YES;
}

@end
