/*
 Bookmark model abstraction
 
 Copyright 2013 Thinstuff Technologies GmbH, Author: Dorian Johnson
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */


#import "Bookmark.h"
#import "TSXAdditions.h"
#import "Utils.h"

#import "GlobalDefaults.h"

@interface ComputerBookmark (Private)
- (void)willChangeValueForKeyPath:(NSString *)keyPath;
- (void)didChangeValueForKeyPath:(NSString *)keyPath;
@end

@implementation ComputerBookmark

@synthesize parent=_parent, uuid=_uuid, label=_label, image=_image;
@synthesize params=_connection_params, conntectedViaWLAN = _connected_via_wlan;


- (id)init
{
    if (!(self = [super init]))
		return nil;
	
	_uuid = [[NSString stringWithUUID] retain];
	_label = @"";
    _connected_via_wlan = NO;
    return self;
}

// Designated initializer.
- (id)initWithConnectionParameters:(ConnectionParams*)params
{
	if (!(self = [self init]))
		return nil;
	
	_connection_params = [params copy];
    _connected_via_wlan = NO;
	return self;
}


- (id)initWithCoder:(NSCoder *)decoder
{
	if (!(self = [self init]))
		return nil;
	
	if (![decoder allowsKeyedCoding])
		[NSException raise:NSInvalidArgumentException format:@"coder must support keyed archiving"];
		
	if ([decoder containsValueForKey:@"uuid"])
	{
		[_uuid release];
		_uuid = [[decoder decodeObjectForKey:@"uuid"] retain]; 
	}
	
	if ([decoder containsValueForKey:@"label"])
		[self setLabel:[decoder decodeObjectForKey:@"label"]];
    
	if ([decoder containsValueForKey:@"connectionParams"])
	{
		[_connection_params release];
		_connection_params = [[decoder decodeObjectForKey:@"connectionParams"] retain];
	}
	
	return self;
}

- (id)initWithBaseDefaultParameters
{
	return [self initWithConnectionParameters:[[[ConnectionParams alloc] initWithBaseDefaultParameters] autorelease]];
}

- (id)copy
{
    ComputerBookmark* copy = [[[self class] alloc] init];
	[copy setLabel:[self label]];
	copy->_connection_params = [_connection_params copy];
	return copy;
}

- (id)copyWithUUID
{
    ComputerBookmark* copy = [self copy];
    copy->_uuid = [[self uuid] copy];
    return copy;
}

- (void)encodeWithCoder:(NSCoder *)coder
{
	if (![coder allowsKeyedCoding])
		[NSException raise:NSInvalidArgumentException format:@"coder must support keyed archiving"];
	
	[coder encodeObject:_uuid forKey:@"uuid"];
	[coder encodeObject:_label forKey:@"label"];
	[coder encodeObject:_connection_params forKey:@"connectionParams"];
}

- (void)dealloc
{	
	_parent = nil;
	[_label release]; _label = nil;
	[_uuid release]; _uuid = nil;
	[_connection_params release]; _connection_params = nil;
	[super dealloc];
}

- (UIImage*)image
{
    return nil;
}

- (BOOL)isEqual:(id)object
{
	return [object respondsToSelector:@selector(uuid)] && [[object uuid] isEqual:_uuid];
}

- (NSString*)description
{
 	return ([self label] != nil) ? [self label] : _uuid;
}

- (BOOL)validateValue:(id *)val forKey:(NSString *)key error:(NSError **)error
{
	NSString* string_value = *val;
	
	if ([key isEqualToString:@"label"])
	{
		if (![[string_value stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]] length])
		{
			if (error)
				*error = [NSError errorWithDomain:@"" code:NSKeyValueValidationError userInfo:
                          [NSDictionary dictionaryWithObjectsAndKeys:
                           NSLocalizedString(@"Connection labels cannot be blank", @"Bookmark data validation: label blank title."), NSLocalizedDescriptionKey, 
                           NSLocalizedString(@"Please enter the short description of this Connection that will appear in the Connection list.", @"Bookmark data validation: label blank message."), NSLocalizedRecoverySuggestionErrorKey, 
                           nil]];
			return NO;
		}
	}
    
	return YES;
}

- (BOOL)validateValue:(id *)val forKeyPath:(NSString *)keyPath error:(NSError **)error
{
	// Could be used to validate params.hostname, params.password, params.port, etc.
	return [super validateValue:val forKeyPath:keyPath error:error];
}

- (BOOL)isDeletable { return YES; }
- (BOOL)isMovable   { return YES; }
- (BOOL)isRenamable { return YES; }
- (BOOL)hasImmutableHost { return NO; }


#pragma mark Custom KVC

- (void)setValue:(id)value forKeyPath:(NSString *)keyPath
{
	if ([keyPath isEqualToString:@"params.resolution"])
	{
		int width, height;
		TSXScreenOptions type;
		if (ScanScreenResolution(value, &width, &height, &type))
		{
			[_connection_params willChangeValueForKey:@"resolution"];
			[[self params] setInt:type forKey:@"screen_resolution_type"];
			
			if (type == TSXScreenOptionFixed)
			{
				[[self params] setInt:width forKey:@"width"];
				[[self params] setInt:height forKey:@"height"];
			}
			[_connection_params didChangeValueForKey:@"resolution"];
		}
		else
			[NSException raise:NSInvalidArgumentException format:@"%s got invalid screen resolution '%@'", __func__, value];
	}
	else
	{
		[self willChangeValueForKeyPath:keyPath];
		[super setValue:value forKeyPath:keyPath];	
		[self didChangeValueForKeyPath:keyPath];
	}
}

- (id)valueForKeyPath:(NSString *)keyPath
{
	if ([keyPath isEqualToString:@"params.resolution"])
		return ScreenResolutionDescription([[self params] intForKey:@"screen_resolution_type"], [[self params] intForKey:@"width"], [[self params] intForKey:@"height"]);
	
	return [super valueForKeyPath:keyPath];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{	
	if ([[change objectForKey:NSKeyValueChangeNotificationIsPriorKey] boolValue])
		[self willChangeValueForKeyPath:keyPath];
	else
		[self didChangeValueForKeyPath:keyPath];	
}

- (NSDictionary*)targetForChangeNotificationForKeyPath:(NSString*)keyPath
{
	NSString* changed_key = keyPath;
	NSObject* changed_object = self;
	
	if ([keyPath rangeOfString:@"params."].location == 0)
	{
		changed_key = [keyPath substringFromIndex:[@"params." length]];
		changed_object = _connection_params;
	}
	
	return [NSDictionary dictionaryWithObjectsAndKeys:changed_key, @"key", changed_object, @"object", nil];
}

- (void)willChangeValueForKeyPath:(NSString *)keyPath
{
	NSDictionary* target = [self targetForChangeNotificationForKeyPath:keyPath];	
	[[target objectForKey:@"object"] willChangeValueForKey:[target objectForKey:@"key"]];
}

- (void)didChangeValueForKeyPath:(NSString *)keyPath
{
	NSDictionary* target = [self targetForChangeNotificationForKeyPath:keyPath];	
	[[target objectForKey:@"object"] didChangeValueForKey:[target objectForKey:@"key"]];
}

- (ConnectionParams*)copyMarkedParams
{
	ConnectionParams* param_copy = [[self params] copy];
	[param_copy setValue:[self uuid] forKey:@"_bookmark_uuid"];
	return param_copy;
}


#pragma mark No children
- (NSUInteger)numberOfChildren { return 0; }
- (NSUInteger)numberOfDescendants { return 1; }
- (BookmarkBase *)childAtIndex:(NSUInteger)index { return nil; }
- (NSUInteger)indexOfChild:(BookmarkBase *)child { return 0; }
- (void)removeChild:(BookmarkBase *)child { }
- (void)addChild:(BookmarkBase *)child { }
- (void)addChild:(BookmarkBase *)child afterExistingChild:(BookmarkBase *)existingChild { }
- (void)addChild:(BookmarkBase *)child atIndex:(NSInteger)index { }
- (BOOL)hasDescendant:(BookmarkBase *)needle { return NO; }
- (BOOL)canContainChildren { return NO; }
@end

