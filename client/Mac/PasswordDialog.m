/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * MacFreeRDP
 *
 * Copyright 2013 Christian Hofstaedtler
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#import "PasswordDialog.h"
#import <freerdp/client/cmdline.h>

#import <CoreGraphics/CoreGraphics.h>

@interface PasswordDialog ()

@property BOOL modalCode;

@end

@implementation PasswordDialog

@synthesize usernameText;
@synthesize passwordText;
@synthesize messageLabel;
@synthesize serverHostname;
@synthesize username;
@synthesize password;
@synthesize domain;
@synthesize modalCode;

- (id)init
{
	return [self initWithWindowNibName:@"PasswordDialog"];
}

- (void)windowDidLoad
{
	[super windowDidLoad];
	// Implement this method to handle any initialization after your window controller's window has
	// been loaded from its nib file.
	[self.window setTitle:self.serverHostname];
	[self.messageLabel
	    setStringValue:[NSString stringWithFormat:@"Authenticate to %@", self.serverHostname]];
	NSMutableString *domainUser = [[NSMutableString alloc] initWithString:@""];

	if (self.domain != nil &&
	    [[self.domain stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]]
	        length] > 0)
	{
		[domainUser appendFormat:@"%@\\", self.domain];
	}

	if (self.username != nil)
	{
		[domainUser appendString:self.username];
		[self.window makeFirstResponder:self.passwordText];
	}

	[self.usernameText setStringValue:domainUser];
}

- (IBAction)onOK:(NSObject *)sender
{
	char *submittedUser = NULL;
	char *submittedDomain = NULL;

	if (freerdp_parse_username(
	        [self.usernameText.stringValue cStringUsingEncoding:NSUTF8StringEncoding],
	        &submittedUser, &submittedDomain))
	{
		self.username = [NSString stringWithCString:submittedUser encoding:NSUTF8StringEncoding];
		self.domain = [NSString stringWithCString:submittedDomain encoding:NSUTF8StringEncoding];
	}
	else
	{
		self.username = self.usernameText.stringValue;
	}

	self.password = self.passwordText.stringValue;
	[NSApp stopModalWithCode:TRUE];
}

- (IBAction)onCancel:(NSObject *)sender
{
	[NSApp stopModalWithCode:FALSE];
}

- (BOOL)runModal:(NSWindow *)mainWindow
{
	if ([mainWindow respondsToSelector:@selector(beginSheet:completionHandler:)])
	{
		[mainWindow beginSheet:self.window completionHandler:nil];
		self.modalCode = [NSApp runModalForWindow:self.window];
		[mainWindow endSheet:self.window];
	}
	else
	{
		[NSApp beginSheet:self.window
		    modalForWindow:mainWindow
		     modalDelegate:nil
		    didEndSelector:nil
		       contextInfo:nil];
		self.modalCode = [NSApp runModalForWindow:self.window];
		[NSApp endSheet:self.window];
	}

	[self.window orderOut:nil];
	return self.modalCode;
}

- (void)dealloc
{
	[usernameText release];
	[passwordText release];
	[messageLabel release];
	[serverHostname release];
	[username release];
	[password release];
	[domain release];
	[super dealloc];
}

@end
