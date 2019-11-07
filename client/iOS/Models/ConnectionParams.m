/*
 Connection Parameters abstraction

 Copyright 2013 Thincast Technologies GmbH, Author: Dorian Johnson

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import "ConnectionParams.h"
#import "GlobalDefaults.h"
#import "EncryptionController.h"
#import "Utils.h"
#import "TSXAdditions.h"

@interface ConnectionParams (Private)
- (id)initWithConnectionParams:(ConnectionParams *)params;
@end

@implementation ConnectionParams

// Designated initializer.
- (id)initWithDictionary:(NSDictionary *)dict
{
	if (!(self = [super init]))
		return nil;

	_connection_params = [dict mutableDeepCopy];

	[self decryptPasswordForKey:@"password"];
	[self decryptPasswordForKey:@"tsg_password"];

	return self;
}

- (void)decryptPasswordForKey:(NSString *)key
{
	if ([[_connection_params objectForKey:key] isKindOfClass:[NSData class]])
	{
		NSString *plaintext_password = [[[EncryptionController sharedEncryptionController]
		    decryptor] decryptString:[_connection_params objectForKey:key]];
		[self setValue:plaintext_password forKey:key];
	}
}

- (id)initWithBaseDefaultParameters
{
	return [self initWithDictionary:[[NSUserDefaults standardUserDefaults]
	                                    dictionaryForKey:@"TSXDefaultComputerBookmarkSettings"]];
}

- (id)init
{
	return [self initWithDictionary:[NSDictionary dictionary]];
}

- (id)initWithConnectionParams:(ConnectionParams *)params
{
	return [self initWithDictionary:params->_connection_params];
}
- (void)dealloc
{
	[_connection_params release];
	_connection_params = nil;
	[super dealloc];
}

- (id)copyWithZone:(NSZone *)zone
{
	return [[ConnectionParams alloc] initWithDictionary:_connection_params];
}

- (NSString *)description
{
	return [NSString stringWithFormat:@"ConnectionParams: %@", [_connection_params description]];
}

#pragma mark -
#pragma mark NSCoder

- (id)initWithCoder:(NSCoder *)decoder
{
	if ([decoder containsValueForKey:@"connectionParams"])
		return [self initWithDictionary:[decoder decodeObjectForKey:@"connectionParams"]];

	return [self init];
}

- (void)encodeWithCoder:(NSCoder *)coder
{
	NSSet *unserializable_keys = [NSSet setWithObjects:@"view", nil];
	NSMutableDictionary *serializable_params =
	    [[NSMutableDictionary alloc] initWithCapacity:[_connection_params count]];

	for (NSString *k in _connection_params)
		if (([k characterAtIndex:0] != '_') && ![unserializable_keys containsObject:k])
			[serializable_params setObject:[_connection_params objectForKey:k] forKey:k];

	if ([serializable_params objectForKey:@"password"] != nil)
		[self serializeDecryptedForKey:@"password" forParams:serializable_params];
	if ([serializable_params objectForKey:@"tsg_password"] != nil)
		[self serializeDecryptedForKey:@"tsg_password" forParams:serializable_params];

	[coder encodeObject:serializable_params forKey:@"connectionParams"];
	[serializable_params release];
}

- (void)serializeDecryptedForKey:(NSString *)key forParams:(NSMutableDictionary *)params
{
	NSData *encrypted_password = [[[EncryptionController sharedEncryptionController] encryptor]
	    encryptString:[params objectForKey:key]];

	if (encrypted_password)
		[params setObject:encrypted_password forKey:key];
	else
		[params removeObjectForKey:key];
}

#pragma mark -
#pragma mark NSKeyValueCoding

- (void)setValue:(id)value forKey:(NSString *)key
{
	[self willChangeValueForKey:key];

	if (value == nil)
		[self setNilValueForKey:key];
	else
		[_connection_params setValue:value forKey:key];

	[self didChangeValueForKey:key];
}

- (void)setValue:(id)value forKeyPath:(NSString *)key
{
	[self willChangeValueForKey:key];

	if (value == nil)
		[self setNilValueForKey:key];
	else
		[_connection_params setValue:value forKeyPath:key];

	[self didChangeValueForKey:key];
}

- (void)setNilValueForKey:(NSString *)key
{
	[_connection_params removeObjectForKey:key];
}

- (id)valueForKey:(NSString *)key
{
	return [_connection_params valueForKey:key];
}

- (NSArray *)allKeys
{
	return [_connection_params allKeys];
}

#pragma mark -
#pragma mark KV convenience

- (BOOL)hasValueForKey:(NSString *)key
{
	return [_connection_params objectForKey:key] != nil;
}

- (void)setInt:(int)integer forKey:(NSString *)key
{
	[self setValue:[NSNumber numberWithInteger:integer] forKey:key];
}
- (int)intForKey:(NSString *)key
{
	return [[self valueForKey:key] intValue];
}

- (void)setBool:(BOOL)v forKey:(NSString *)key
{
	[self setValue:[NSNumber numberWithBool:v] forKey:key];
}
- (BOOL)boolForKey:(NSString *)key
{
	return [[_connection_params objectForKey:key] boolValue];
}

- (const char *)UTF8StringForKey:(NSString *)key
{
	id val = [self valueForKey:key];
	const char *str;

	if ([val respondsToSelector:@selector(UTF8String)] && (str = [val UTF8String]))
		return str;

	return "";
}

- (NSString *)StringForKey:(NSString *)key
{
	return [self valueForKey:key];
}

- (BOOL)hasValueForKeyPath:(NSString *)key
{
	return [_connection_params valueForKeyPath:key] != nil;
}

- (void)setInt:(int)integer forKeyPath:(NSString *)key
{
	[self setValue:[NSNumber numberWithInteger:integer] forKeyPath:key];
}
- (int)intForKeyPath:(NSString *)key
{
	return [[self valueForKeyPath:key] intValue];
}

- (void)setBool:(BOOL)v forKeyPath:(NSString *)key
{
	[self setValue:[NSNumber numberWithBool:v] forKeyPath:key];
}

- (BOOL)boolForKeyPath:(NSString *)key
{
	return [[self valueForKeyPath:key] boolValue];
}

- (const char *)UTF8StringForKeyPath:(NSString *)key
{
	id val = [self valueForKeyPath:key];
	const char *str;

	if ([val respondsToSelector:@selector(UTF8String)] && (str = [val UTF8String]))
		return str;

	return "";
}

- (NSString *)StringForKeyPath:(NSString *)key
{
	return [self valueForKeyPath:key];
}

- (int)intForKey:(NSString *)key with3GEnabled:(BOOL)enabled
{
	if (enabled && [self boolForKey:@"enable_3g_settings"])
		return [self intForKeyPath:[NSString stringWithFormat:@"settings_3g.%@", key]];
	return [self intForKeyPath:key];
}

- (BOOL)boolForKey:(NSString *)key with3GEnabled:(BOOL)enabled
{
	if (enabled && [self boolForKey:@"enable_3g_settings"])
		return [self boolForKeyPath:[NSString stringWithFormat:@"settings_3g.%@", key]];
	return [self boolForKeyPath:key];
}

@end
