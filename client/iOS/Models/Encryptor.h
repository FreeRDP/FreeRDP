/*
 Password Encryptor
 
 Copyright 2013 Thinstuff Technologies GmbH, Author: Dorian Johnson
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* Encrypts data using AES 128 with a 256 bit key derived using PBKDF2-HMAC-SHA1  */

#import <Foundation/Foundation.h>

// Encryption block cipher config
#define TSXEncryptorBlockCipherAlgo kCCAlgorithmAES128
#define TSXEncryptorBlockCipherKeySize kCCKeySizeAES256
#define TSXEncryptorBlockCipherOptions kCCOptionPKCS7Padding
#define TSXEncryptorBlockCipherBlockSize 16

// Key generation: If any of these are changed, existing password stores will no longer work
#define TSXEncryptorPBKDF2Rounds 100
#define TSXEncryptorPBKDF2Salt "9D¶3L}S¿lA[e€3C«"
#define TSXEncryptorPBKDF2SaltLen TSXEncryptorBlockCipherOptions
#define TSXEncryptorPBKDF2KeySize TSXEncryptorBlockCipherKeySize


@interface Encryptor : NSObject {
@private
	NSData* _encryption_key;
	NSString* _plaintext_password;
}

@property(readonly) NSString* plaintextPassword;

- (id)initWithPassword:(NSString*)plaintext_password;

- (NSData*)encryptData:(NSData*)plaintext_data;
- (NSData*)decryptData:(NSData*)encrypted_data;
- (NSData*)encryptString:(NSString*)plaintext_string;
- (NSString*)decryptString:(NSData*)encrypted_string;

@end
