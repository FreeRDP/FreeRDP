//
//  PasswordDialog.m
//  FreeRDP
//
//  Created by Christian Hofstaedtler on 3/10/13.
//
//

#import "PasswordDialog.h"

@interface PasswordDialog ()

@end

@implementation PasswordDialog

@synthesize userNameText;
@synthesize passwordText;
@synthesize messageLabel;
@synthesize serverName;
@synthesize userName;
@synthesize password;

- (id)init {
    return [self initWithWindowNibName:@"PasswordDialog"];
}

- (void)windowDidLoad
{
    [super windowDidLoad];
    // Implement this method to handle any initialization after your window controller's window has been loaded from its nib file.
    [self.window setTitle:self.serverName];
    [messageLabel setStringValue:[NSString stringWithFormat:@"Authenticate to %@", self.serverName]];
    if (self.userName != nil) {
        [userNameText setStringValue:self.userName];
        [self.window makeFirstResponder:passwordText];
    }
}

- (IBAction)onOK:(NSObject *)sender {
    self.userName = self.userNameText.stringValue;
    self.password = self.passwordText.stringValue;
    [self.window orderOut:nil];
    [NSApp stopModalWithCode:TRUE];
}

- (IBAction)onCancel:(NSObject *)sender {
    [self.window orderOut:nil];
    [NSApp stopModalWithCode:FALSE];
}

- (BOOL) runModal {
    return [NSApp runModalForWindow:self.window];
}

@end
