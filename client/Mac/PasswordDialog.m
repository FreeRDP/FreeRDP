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

@interface PasswordDialog ()

@end

@implementation PasswordDialog

@synthesize usernameText;
@synthesize passwordText;
@synthesize messageLabel;
@synthesize serverHostname;
@synthesize username;
@synthesize password;

- (id)init
{
	return [self initWithWindowNibName:@"PasswordDialog"];
}

- (void)windowDidLoad
{
	[super windowDidLoad];
	// Implement this method to handle any initialization after your window controller's window has been loaded from its nib file.
	[self.window setTitle:self.serverHostname];
	[messageLabel setStringValue:[NSString stringWithFormat:@"Authenticate to %@", self.serverHostname]];
	if (self.username != nil)
	{
		[usernameText setStringValue:self.username];
		[self.window makeFirstResponder:passwordText];
	}
}

- (IBAction)onOK:(NSObject *)sender
{
	self.username = self.usernameText.stringValue;
	self.password = self.passwordText.stringValue;
	[self.window orderOut:nil];
	[NSApp stopModalWithCode:TRUE];
}

- (IBAction)onCancel:(NSObject *)sender
{
	[self.window orderOut:nil];
	[NSApp stopModalWithCode:FALSE];
}

- (BOOL)runModal
{
	return [NSApp runModalForWindow:self.window];
}

- (void)dealloc
{
	[usernameText release];
	[passwordText release];
	[messageLabel release];
	[serverHostname release];
	[username release];
	[password release];

	[super dealloc];
}

@end
