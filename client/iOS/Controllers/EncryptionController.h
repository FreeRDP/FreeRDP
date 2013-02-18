/*
 Password Encryption Controller
 
 Copyright 2013 Thinstuff Technologies GmbH, Author: Dorian Johnson
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#import <Foundation/Foundation.h>
#import "Encryptor.h"

@interface EncryptionController : NSObject
{
	Encryptor* _shared_encryptor;
}

+ (EncryptionController*)sharedEncryptionController;

// Return a Encryptor suitable for encrypting or decrypting with the master password
- (Encryptor*)decryptor;
- (Encryptor*)encryptor;

@end
