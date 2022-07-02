/*
 Additions to Cocoa touch classes

 Copyright 2013 Thincast Technologies GmbH, Authors: Dorian Johnson, Martin Fleisz

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import "TSXAdditions.h"
#include <openssl/bio.h>
#include <openssl/evp.h>

@implementation NSObject (TSXAdditions)

- (void)setValuesForKeyPathsWithDictionary:(NSDictionary *)keyedValues
{
	for (id keyPath in keyedValues)
		[self setValue:[keyedValues objectForKey:keyPath] forKeyPath:keyPath];
}

- mutableDeepCopy
{
	if ([self respondsToSelector:@selector(mutableCopyWithZone:)])
		return [self mutableCopy];
	else if ([self respondsToSelector:@selector(copyWithZone:)])
		return [self copy];
	else
		return [self retain];
}

@end

#pragma mark -

@implementation NSString (TSXAdditions)

#pragma mark Creation routines
+ (NSString *)stringWithUUID
{
	CFUUIDRef uuidObj = CFUUIDCreate(nil);
	NSString *uuidString = (NSString *)CFUUIDCreateString(nil, uuidObj);
	CFRelease(uuidObj);
	return [uuidString autorelease];
}

/* Code from
 * http://code.google.com/p/google-toolbox-for-mac/source/browse/trunk/Foundation/GTMNSData%2BHex.m?r=344
 */
- (NSData *)dataFromHexString
{
	NSData *hexData = [self dataUsingEncoding:NSASCIIStringEncoding];
	const char *hexBuf = [hexData bytes];
	NSUInteger hexLen = [hexData length];

	// This indicates an error converting to ASCII.
	if (!hexData)
		return nil;

	if ((hexLen % 2) != 0)
	{
		return nil;
	}

	NSMutableData *binaryData = [NSMutableData dataWithLength:(hexLen / 2)];
	unsigned char *binaryPtr = [binaryData mutableBytes];
	unsigned char value = 0;
	for (NSUInteger i = 0; i < hexLen; i++)
	{
		char c = hexBuf[i];

		if (!isxdigit(c))
		{
			return nil;
		}

		if (isdigit(c))
		{
			value += c - '0';
		}
		else if (islower(c))
		{
			value += 10 + c - 'a';
		}
		else
		{
			value += 10 + c - 'A';
		}

		if (i & 1)
		{
			*binaryPtr++ = value;
			value = 0;
		}
		else
		{
			value <<= 4;
		}
	}

	return [NSData dataWithData:binaryData];
}

+ (NSString *)hexStringFromData:(const unsigned char *)data
                         ofSize:(unsigned int)size
                  withSeparator:(NSString *)sep
                   afterNthChar:(int)sepnth
{
	int i;
	NSMutableString *result;
	NSString *immutableResult;

	result = [[NSMutableString alloc] init];
	for (i = 0; i < size; i++)
	{
		if (i && sep && sepnth && i % sepnth == 0)
			[result appendString:sep];
		[result appendFormat:@"%02X", data[i]];
	}

	immutableResult = [NSString stringWithString:result];
	[result release];
	return immutableResult;
}

@end

#pragma mark Mutable deep copy for dicionary, array and set

@implementation NSDictionary (TSXAdditions)

- mutableDeepCopy
{
	NSMutableDictionary *newDictionary = [[NSMutableDictionary alloc] init];
	NSEnumerator *enumerator = [self keyEnumerator];
	id key;
	while ((key = [enumerator nextObject]))
	{
		id obj = [[self objectForKey:key] mutableDeepCopy];
		[newDictionary setObject:obj forKey:key];
		[obj release];
	}
	return newDictionary;
}

@end

@implementation NSArray (TSXAdditions)

- mutableDeepCopy
{
	NSMutableArray *newArray = [[NSMutableArray alloc] init];
	NSEnumerator *enumerator = [self objectEnumerator];
	id obj;
	while ((obj = [enumerator nextObject]))
	{
		obj = [obj mutableDeepCopy];
		[newArray addObject:obj];
		[obj release];
	}
	return newArray;
}

@end

@implementation NSSet (TSXAdditions)

- mutableDeepCopy
{
	NSMutableSet *newSet = [[NSMutableSet alloc] init];
	NSEnumerator *enumerator = [self objectEnumerator];
	id obj;
	while ((obj = [enumerator nextObject]))
	{
		obj = [obj mutableDeepCopy];
		[newSet addObject:obj];
		[obj release];
	}
	return newSet;
}

@end

#pragma mark -

/* Code from
 * http://stackoverflow.com/questions/1305225/best-way-to-serialize-a-nsdata-into-an-hexadeximal-string
 */
@implementation NSData (TSXAdditions)

#pragma mark - String Conversion
- (NSString *)hexadecimalString
{
	/* Returns hexadecimal string of NSData. Empty string if data is empty.   */

	const unsigned char *dataBuffer = (const unsigned char *)[self bytes];

	if (!dataBuffer)
		return [NSString string];

	NSUInteger dataLength = [self length];
	NSMutableString *hexString = [NSMutableString stringWithCapacity:(dataLength * 2)];

	for (int i = 0; i < dataLength; ++i)
		[hexString appendString:[NSString stringWithFormat:@"%02lx", (unsigned long)dataBuffer[i]]];

	return [NSString stringWithString:hexString];
}

/* Code from http://cocoawithlove.com/2009/06/base64-encoding-options-on-mac-and.html */
- (NSString *)base64EncodedString
{
	// Construct an OpenSSL context
	BIO *context = BIO_new(BIO_s_mem());

	// Tell the context to encode base64
	BIO *command = BIO_new(BIO_f_base64());
	context = BIO_push(command, context);
	BIO_set_flags(context, BIO_FLAGS_BASE64_NO_NL);

	// Encode all the data
	ERR_clear_error();
	BIO_write(context, [self bytes], [self length]);
	(void)BIO_flush(context);

	// Get the data out of the context
	char *outputBuffer;
	long outputLength = BIO_get_mem_data(context, &outputBuffer);
	NSString *encodedString = [[NSString alloc] initWithBytes:outputBuffer
	                                                   length:outputLength
	                                                 encoding:NSASCIIStringEncoding];

	BIO_free_all(context);

	return encodedString;
}

@end
