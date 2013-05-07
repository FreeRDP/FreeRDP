/*
 Password Encryption Controller
 
 Copyright 2013 Thinstuff Technologies GmbH, Author: Dorian Johnson
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#import "EncryptionController.h"
#import "SFHFKeychainUtils.h"
#import "TSXAdditions.h"

@interface EncryptionController (Private)

- (BOOL)verifyPassword:(Encryptor*)decryptor;
- (NSData*)encryptedVerificationData;
- (void)setEncryptedVerificationData:(Encryptor*)encryptor;

- (NSString*)keychainServerName;
- (NSString*)keychainUsername;
- (void)setKeychainPassword:(NSString*)password;
- (NSString*)keychainPassword;
- (NSString*)keychainDefaultPassword;

@end

static EncryptionController* _shared_encryption_controller = nil;


#pragma mark -

@implementation EncryptionController

+ (EncryptionController*)sharedEncryptionController
{
	@synchronized(self)
	{
		if (_shared_encryption_controller == nil)
			_shared_encryption_controller = [[EncryptionController alloc] init];		
	}
	
	return _shared_encryption_controller;	
}

#pragma mark Getting an encryptor or decryptor

- (Encryptor*)encryptor
{
	if (_shared_encryptor)
		return _shared_encryptor;
	
	NSString* saved_password = [self keychainPassword];    
	if (saved_password == nil)
    {
        saved_password = [self keychainDefaultPassword];        
        Encryptor* encryptor = [[[Encryptor alloc] initWithPassword:saved_password] autorelease];
        [self setEncryptedVerificationData:encryptor];        
        _shared_encryptor = [encryptor retain];
    }
    else
    {
        Encryptor* encryptor = [[[Encryptor alloc] initWithPassword:saved_password] autorelease];
        if ([self verifyPassword:encryptor])
            _shared_encryptor = [encryptor retain];
    }    
    
    return _shared_encryptor;
}

// For the current implementation, decryptors and encryptors are equivilant.
- (Encryptor*)decryptor { return [self encryptor]; }

@end

#pragma mark -

@implementation EncryptionController (Private)

#pragma mark -
#pragma mark Keychain password storage

- (NSString*)keychainServerName
{
	return [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleName"];
}

- (NSString*)keychainUsername
{
	return @"master.password";
}

- (void)setKeychainPassword:(NSString*)password
{	
    NSError* error;
	if (password == nil)
	{
        [SFHFKeychainUtils deleteItemForUsername:[self keychainUsername] andServerName:[self keychainServerName] error:&error];
		return;
	}

	[SFHFKeychainUtils storeUsername:[self keychainUsername] andPassword:password forServerName:[self keychainServerName] updateExisting:YES error:&error];
}

- (NSString*)keychainPassword
{
    NSError* error;
    return [SFHFKeychainUtils getPasswordForUsername:[self keychainUsername] andServerName:[self keychainServerName] error:&error];
}

- (NSString*)keychainDefaultPassword
{
    NSString* password = [[NSUserDefaults standardUserDefaults] stringForKey:@"UUID"];
    if ([password length] == 0)
    {
        password = [NSString stringWithUUID];
        [[NSUserDefaults standardUserDefaults] setObject:password forKey:@"UUID"];
        [[NSUserDefaults standardUserDefaults] removeObjectForKey:@"TSXMasterPasswordVerification"];
    }
    return password;
}

#pragma mark -
#pragma mark Verification of encryption key against verification data

- (BOOL)verifyPassword:(Encryptor*)decryptor 
{
	return [[decryptor plaintextPassword] isEqualToString:[decryptor decryptString:[self encryptedVerificationData]]];
}

- (NSData*)encryptedVerificationData
{
	return [[NSUserDefaults standardUserDefaults] dataForKey:@"TSXMasterPasswordVerification"];
}

- (void)setEncryptedVerificationData:(Encryptor*)encryptor
{
	[[NSUserDefaults standardUserDefaults] setObject:[encryptor encryptString:[encryptor plaintextPassword]] forKey:@"TSXMasterPasswordVerification"];
}

@end
