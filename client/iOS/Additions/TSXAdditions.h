/*
 Additions to Cocoa touch classes
 
 Copyright 2013 Thinstuff Technologies GmbH, Authors: Dorian Johnson, Martin Fleisz
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#import <Foundation/Foundation.h>

@interface NSObject (TSXAdditions)
- (void)setValuesForKeyPathsWithDictionary:(NSDictionary *)keyedValues;
@end

#pragma mark -
@interface NSString (TSXAdditions)
+ (NSString*)stringWithUUID;
- (NSData*)dataFromHexString;
+ (NSString*)hexStringFromData:(const unsigned char *)data ofSize:(unsigned int)size withSeparator:(NSString *)sep afterNthChar:(int)sepnth;
@end

@interface NSDictionary (TSXAdditions)
- (id)mutableDeepCopy;
@end

@interface NSData (TSXAdditions)
- (NSString *)hexadecimalString;
- (NSString *)base64EncodedString;
@end
