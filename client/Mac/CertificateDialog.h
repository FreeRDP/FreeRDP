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

#import <Cocoa/Cocoa.h>

@interface CertificateDialog : NSWindowController
{
  @public
	NSTextField *textCommonName;
	NSTextField *textSubject;
	NSTextField *textIssuer;
	NSTextField *textFingerprint;
	NSTextField *textMismatch;
	NSTextField *messageLabel;
	NSString *serverHostname;

	BOOL hostMismatch;
	BOOL changed;
	int result;
}
@property(retain) IBOutlet NSTextField *textCommonName;
@property(retain) IBOutlet NSTextField *textSubject;
@property(retain) IBOutlet NSTextField *textIssuer;
@property(retain) IBOutlet NSTextField *textFingerprint;
@property(retain) IBOutlet NSTextField *textMismatch;
@property(retain) IBOutlet NSTextField *messageLabel;

- (IBAction)onAccept:(NSObject *)sender;
- (IBAction)onTemporary:(NSObject *)sender;
- (IBAction)onCancel:(NSObject *)sender;

@property(retain) NSString *serverHostname;
@property(retain) NSString *commonName;
@property(retain) NSString *subject;
@property(retain) NSString *issuer;
@property(retain) NSString *fingerprint;
@property BOOL hostMismatch;
@property BOOL changed;
@property(readonly) int result;

- (int)runModal:(NSWindow *)mainWindow;

@end
