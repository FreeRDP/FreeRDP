//
//  PasswordDialog.h
//  FreeRDP
//
//  Created by Christian Hofstaedtler on 3/10/13.
//
//

#import <Cocoa/Cocoa.h>

@interface PasswordDialog : NSWindowController

@property (retain) IBOutlet NSTextField* userNameText;
@property (retain) IBOutlet NSTextField* passwordText;
@property (retain) IBOutlet NSTextField* messageLabel;

- (IBAction)onOK:(NSObject*)sender;
- (IBAction)onCancel:(NSObject*)sender;

@property (retain) NSString* serverName;
@property (retain) NSString* userName;
@property (retain) NSString* password;

- (BOOL) runModal;

@end
