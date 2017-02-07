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

#import <Cocoa/Cocoa.h>

@interface PasswordDialog : NSWindowController
{
@public
	NSTextField* usernameText;
	NSTextField* passwordText;
	NSTextField* messageLabel;
	NSString* serverHostname;
	NSString* username;
	NSString* password;
	NSString* domain;
	BOOL modalCode;
}
@property(retain) IBOutlet NSTextField* usernameText;
@property(retain) IBOutlet NSTextField* passwordText;
@property(retain) IBOutlet NSTextField* messageLabel;

- (IBAction)onOK:(NSObject*)sender;
- (IBAction)onCancel:(NSObject*)sender;

@property(retain) NSString* serverHostname;
@property(retain) NSString* username;
@property(retain) NSString* password;
@property(retain) NSString* domain;
@property(readonly) BOOL modalCode;

- (BOOL) runModal:(NSWindow*)mainWindow;

@end
