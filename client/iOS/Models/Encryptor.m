/*
 Password Encryptor
 
 Copyright 2013 Thinstuff Technologies GmbH, Author: Dorian Johnson
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* We try to use CommonCrypto as much as possible. PBKDF2 was added to CommonCrypto in iOS 5, so use OpenSSL only as a fallback to do PBKDF2 on pre iOS 5 systems. */

#import "Encryptor.h"
#import <CommonCrypto/CommonKeyDerivation.h> 
#import <CommonCrypto/CommonCryptor.h> 
#import <CommonCrypto/CommonDigest.h> 
#import <openssl/evp.h> // For PBKDF2 on < 5.0
#include <fcntl.h>

#pragma mark -

@interface Encryptor (Private)
- (NSData*)randomInitializationVector;
@end

@implementation Encryptor
@synthesize plaintextPassword=_plaintext_password;

- (id)initWithPassword:(NSString*)plaintext_password
{
	if (plaintext_password == nil)
		return nil; 
    
	if ( !(self = [super init]) )
		return nil;
	
	_plaintext_password = [plaintext_password retain];
	const char* plaintext_password_data = [plaintext_password length] ? [plaintext_password UTF8String] : " ";
	
	if (!plaintext_password_data || !strlen(plaintext_password_data))
		[NSException raise:NSInternalInconsistencyException format:@"%s: plaintext password data is zero length!", __func__];
	
	
	uint8_t* derived_key = calloc(1, TSXEncryptorPBKDF2KeySize);
    
	if (CCKeyDerivationPBKDF != NULL)
	{
		int ret = CCKeyDerivationPBKDF(kCCPBKDF2, plaintext_password_data, strlen(plaintext_password_data)-1, (const uint8_t*)TSXEncryptorPBKDF2Salt, TSXEncryptorPBKDF2SaltLen, kCCPRFHmacAlgSHA1, TSXEncryptorPBKDF2Rounds, derived_key, TSXEncryptorPBKDF2KeySize);
		//NSLog(@"CCKeyDerivationPBKDF ret = %d; key: %@", ret, [NSData dataWithBytesNoCopy:derived_key length:TWEncryptorPBKDF2KeySize freeWhenDone:NO]);
		
		if (ret)
		{
			NSLog(@"%s: CCKeyDerivationPBKDF ret == %d, indicating some sort of failure.", __func__, ret);
            free(derived_key);
			[self autorelease];
			return nil;
		}
	}
	else
	{
		// iOS 4.x or earlier -- use OpenSSL
		unsigned long ret = PKCS5_PBKDF2_HMAC_SHA1(plaintext_password_data, (int)strlen(plaintext_password_data)-1, (const unsigned char*)TSXEncryptorPBKDF2Salt, TSXEncryptorPBKDF2SaltLen, TSXEncryptorPBKDF2Rounds, TSXEncryptorPBKDF2KeySize, derived_key);
		//NSLog(@"PKCS5_PBKDF2_HMAC_SHA1 ret = %lu; key: %@", ret, [NSData dataWithBytesNoCopy:derived_key length:TWEncryptorPBKDF2KeySize freeWhenDone:NO]);
		
		if (ret != 1)
		{
			NSLog(@"%s: PKCS5_PBKDF2_HMAC_SHA1 ret == %lu, indicating some sort of failure.", __func__, ret);
            free(derived_key);
			[self release];
			return nil;
		}
	}	
    
    
	_encryption_key = [[NSData alloc] initWithBytesNoCopy:derived_key length:TSXEncryptorPBKDF2KeySize freeWhenDone:YES];
	return self;
}


#pragma mark -
#pragma mark Encrypting/Decrypting data

- (NSData*)encryptData:(NSData*)plaintext_data
{
	if (![plaintext_data length])
		return nil;
    
	NSData* iv = [self randomInitializationVector];
	NSMutableData* encrypted_data = [NSMutableData dataWithLength:[iv length] + [plaintext_data length] + TSXEncryptorBlockCipherBlockSize];
	[encrypted_data replaceBytesInRange:NSMakeRange(0, [iv length]) withBytes:[iv bytes]];
	
	size_t data_out_moved = 0;
	int ret = CCCrypt(kCCEncrypt, TSXEncryptorBlockCipherAlgo, TSXEncryptorBlockCipherOptions, [_encryption_key bytes], TSXEncryptorBlockCipherKeySize, [iv bytes], [plaintext_data bytes], [plaintext_data length], [encrypted_data mutableBytes]+[iv length], [encrypted_data length]-[iv length], &data_out_moved);
    
	switch (ret)
	{
		case kCCSuccess:
			[encrypted_data setLength:[iv length] + data_out_moved];
			return encrypted_data;
            
		default:
			NSLog(@"%s: uncaught error, ret CCCryptorStatus = %d (plaintext len = %lu; buffer size = %lu)", __func__, ret, (unsigned long)[plaintext_data length], (unsigned long)([encrypted_data length]-[iv length]));
			return nil;
	}
	
	return nil;
}

- (NSData*)decryptData:(NSData*)encrypted_data
{
	if ([encrypted_data length] <= TSXEncryptorBlockCipherBlockSize)
		return nil;
	
	NSMutableData* plaintext_data = [NSMutableData dataWithLength:[encrypted_data length] + TSXEncryptorBlockCipherBlockSize];
	size_t data_out_moved = 0;
	
	int ret = CCCrypt(kCCDecrypt, TSXEncryptorBlockCipherAlgo, TSXEncryptorBlockCipherOptions, [_encryption_key bytes], TSXEncryptorBlockCipherKeySize, [encrypted_data bytes], [encrypted_data bytes] + TSXEncryptorBlockCipherBlockSize, [encrypted_data length] - TSXEncryptorBlockCipherBlockSize, [plaintext_data mutableBytes], [plaintext_data length], &data_out_moved);
    
	switch (ret)
	{
		case kCCSuccess:
			[plaintext_data setLength:data_out_moved];
			return plaintext_data;
			
		case kCCBufferTooSmall: // Our output buffer is big enough to decrypt valid data. This return code indicates malformed data.
		case kCCAlignmentError: // Shouldn't get this, since we're using padding.
		case kCCDecodeError:	// Wrong key.
			return nil;
			
		default:
			NSLog(@"%s: uncaught error, ret CCCryptorStatus = %d (encrypted data len = %lu; buffer size = %lu; dom = %lu)", __func__, ret, (unsigned long)[encrypted_data length], (unsigned long)[plaintext_data length], data_out_moved);
			return nil;
	}
    
	return nil;
}


- (NSData*)encryptString:(NSString*)plaintext_string
{
	return [self encryptData:[plaintext_string dataUsingEncoding:NSUTF8StringEncoding]];
}

- (NSString*)decryptString:(NSData*)encrypted_string
{
	return [[[NSString alloc] initWithData:[self decryptData:encrypted_string] encoding:NSUTF8StringEncoding] autorelease];
}

- (NSData*)randomInitializationVector
{
	NSMutableData* iv = [NSMutableData dataWithLength:TSXEncryptorBlockCipherBlockSize];
	int fd;
	
	if ( (fd = open("/dev/urandom", O_RDONLY)) < 0)
		return nil;
    
	NSInteger bytes_needed = [iv length];
	char* p = [iv mutableBytes];
	
	while (bytes_needed)
	{
		long bytes_read = read(fd, p, bytes_needed);
		
		if (bytes_read < 0)
			continue;
        
		p += bytes_read;
		bytes_needed -= bytes_read;
	}
    
	close(fd);
	return iv;
}

@end
