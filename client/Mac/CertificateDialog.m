/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * MacFreeRDP
 *
 * Copyright 2018 Armin Novak <armin.novak@thincast.com>
 * Copyright 2018 Thicast Technologies GmbH
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

#import "CertificateDialog.h"
#import <freerdp/client/cmdline.h>

#import <CoreGraphics/CoreGraphics.h>

@interface CertificateDialog ()

@property int result;

@end

@implementation CertificateDialog

@synthesize textCommonName;
@synthesize textFingerprint;
@synthesize textIssuer;
@synthesize textSubject;
@synthesize textMismatch;
@synthesize messageLabel;
@synthesize serverHostname;
@synthesize commonName;
@synthesize fingerprint;
@synthesize issuer;
@synthesize subject;
@synthesize hostMismatch;
@synthesize changed;
@synthesize result;

- (id)init
{
	return [self initWithWindowNibName:@"CertificateDialog"];
}

- (void)windowDidLoad
{
	[super windowDidLoad];
	// Implement this method to handle any initialization after your window controller's window has
	// been loaded from its nib file.
	[self.window setTitle:self.serverHostname];
	if (self.changed)
		[self.messageLabel setStringValue:[NSString stringWithFormat:@"Changed certificate for %@",
		                                                             self.serverHostname]];
	else
		[self.messageLabel setStringValue:[NSString stringWithFormat:@"New Certificate for %@",
		                                                             self.serverHostname]];

	if (!self.hostMismatch)
		[self.textMismatch
		    setStringValue:[NSString stringWithFormat:
		                                 @"NOTE: The server name matches the certificate, good."]];
	else
		[self.textMismatch
		    setStringValue:[NSString
		                       stringWithFormat:
		                           @"ATTENTION: The common name does not match the server name!"]];
	[self.textCommonName setStringValue:self.commonName];
	[self.textFingerprint setStringValue:self.fingerprint];
	[self.textIssuer setStringValue:self.issuer];
	[self.textSubject setStringValue:self.subject];
}

- (IBAction)onAccept:(NSObject *)sender
{
	[NSApp stopModalWithCode:1];
}

- (IBAction)onTemporary:(NSObject *)sender
{
	[NSApp stopModalWithCode:2];
}

- (IBAction)onCancel:(NSObject *)sender
{
	[NSApp stopModalWithCode:0];
}

- (int)runModal:(NSWindow *)mainWindow
{
	if ([mainWindow respondsToSelector:@selector(beginSheet:completionHandler:)])
	{
		[mainWindow beginSheet:self.window completionHandler:nil];
		self.result = [NSApp runModalForWindow:self.window];
		[mainWindow endSheet:self.window];
	}
	else
	{
		[NSApp beginSheet:self.window
		    modalForWindow:mainWindow
		     modalDelegate:nil
		    didEndSelector:nil
		       contextInfo:nil];
		self.result = [NSApp runModalForWindow:self.window];
		[NSApp endSheet:self.window];
	}

	[self.window orderOut:nil];
	return self.result;
}

- (void)dealloc
{
	[textCommonName release];
	[textFingerprint release];
	[textIssuer release];
	[textSubject release];
	[messageLabel release];
	[serverHostname release];
	[commonName release];
	[fingerprint release];
	[issuer release];
	[subject release];
	[super dealloc];
}

@end
