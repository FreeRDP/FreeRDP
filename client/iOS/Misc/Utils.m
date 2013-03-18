/*
 Utility functions
 
 Copyright 2013 Thinstuff Technologies GmbH, Authors: Martin Fleisz, Dorian Johnson
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#import "Utils.h"
#import "OrderedDictionary.h"
#import "TSXAdditions.h"

#import <freerdp/input.h>
#import "config.h"

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if_dl.h>

BOOL ScanHostNameAndPort(NSString* address, NSString** host, unsigned short* port)
{
	*host = @""; *port = 0;
	
	if (![address length])
		return NO;
	
	NSURL* url = [NSURL URLWithString:[NSString stringWithFormat:@"rdp://%@", address]];
	
	if (!url || ![[url host] length])
		return NO;
	
	*host = [url host];
	*port = [[url port] unsignedShortValue];
	return YES;
}

#pragma mark -
#pragma mark Working with Screen Resolutions

NSString* LocalizedFitScreen()
{
    return NSLocalizedString(@"Automatic", @"Screen resolution selector: Automatic resolution (Full Screen on iPad, reasonable size on iPhone)");    
}

NSString* LocalizedCustom()
{
    return NSLocalizedString(@"Custom", @"Screen resolution selector: Custom");    
}

BOOL ScanScreenResolution(NSString* description, int* width, int* height, TSXScreenOptions* type)
{
	*height = 0; *width = 0; *type = TSXScreenOptionFixed;
	
	if ([description isEqualToString:LocalizedFitScreen()])
	{
		*type = TSXScreenOptionFitScreen;
		return YES;
	}
	else if ([description isEqualToString:LocalizedCustom()])
	{
		*type = TSXScreenOptionCustom;
		return YES;
	}
    
	NSArray* resolution_components = [description componentsSeparatedByCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@"x*×"]];
	
	if ([resolution_components count] != 2)
		return NO;
	
	*width = [[resolution_components objectAtIndex:0] intValue];
	*height = [[resolution_components objectAtIndex:1] intValue];	
	return YES;
}

NSString* ScreenResolutionDescription(TSXScreenOptions type, int width, int height)
{
	if (type == TSXScreenOptionFitScreen)
		return LocalizedFitScreen();
	else if (type == TSXScreenOptionCustom)
		return LocalizedCustom();
    
	return [NSString stringWithFormat:@"%dx%d", width, height];
}


NSDictionary* SelectionForColorSetting()
{
    OrderedDictionary* dict = [OrderedDictionary dictionaryWithCapacity:3];
    [dict setValue:[NSNumber numberWithInt:16] forKey:NSLocalizedString(@"High Color (16 Bit)", @"16 bit color selection")];
    [dict setValue:[NSNumber numberWithInt:24] forKey:NSLocalizedString(@"True Color (24 Bit)", @"24 bit color selection")];
    [dict setValue:[NSNumber numberWithInt:32] forKey:NSLocalizedString(@"Highest Quality (32 Bit)", @"32 bit color selection")];
    return dict;
}

NSArray* ResolutionModes()
{
    NSArray* array = [NSArray arrayWithObjects:ScreenResolutionDescription(TSXScreenOptionFitScreen, 0, 0),
                                               ScreenResolutionDescription(TSXScreenOptionFixed, 640, 480),                      
                                               ScreenResolutionDescription(TSXScreenOptionFixed, 800, 600),
                                               ScreenResolutionDescription(TSXScreenOptionFixed, 1024, 768),
                                               ScreenResolutionDescription(TSXScreenOptionFixed, 1280, 1024),
                                               ScreenResolutionDescription(TSXScreenOptionFixed, 1440, 900),
                                               ScreenResolutionDescription(TSXScreenOptionFixed, 1440, 1050),
                                               ScreenResolutionDescription(TSXScreenOptionFixed, 1600, 1200),
                                               ScreenResolutionDescription(TSXScreenOptionFixed, 1920, 1080),
                                               ScreenResolutionDescription(TSXScreenOptionFixed, 1920, 1200),
                                               ScreenResolutionDescription(TSXScreenOptionCustom, 0, 0), nil];
    return array;    
}

#pragma mark Working with Security Protocols

NSString* LocalizedAutomaticSecurity()
{
    return NSLocalizedString(@"Automatic", @"Automatic protocl security selection");    
}

NSString* ProtocolSecurityDescription(TSXProtocolSecurityOptions type)
{
	if (type == TSXProtocolSecurityNLA)
		return @"NLA";
	else if (type == TSXProtocolSecurityTLS)
		return @"TLS";
	else if (type == TSXProtocolSecurityRDP)
		return @"RDP";
    
	return LocalizedAutomaticSecurity();
}

BOOL ScanProtocolSecurity(NSString* description, TSXProtocolSecurityOptions* type)
{
	*type = TSXProtocolSecurityRDP;
	
	if ([description isEqualToString:@"NLA"])
	{
		*type = TSXProtocolSecurityNLA;
		return YES;
	}
	else if ([description isEqualToString:@"TLS"])
	{
		*type = TSXProtocolSecurityTLS;
		return YES;
	}
	else if ([description isEqualToString:@"RDP"])
	{
		*type = TSXProtocolSecurityRDP;
		return YES;
	}
	else if ([description isEqualToString:LocalizedAutomaticSecurity()])
	{
		*type = TSXProtocolSecurityAutomatic;
		return YES;
	}
    
	return NO;
}

NSDictionary* SelectionForSecuritySetting()
{
    OrderedDictionary* dict = [OrderedDictionary dictionaryWithCapacity:4];
    [dict setValue:[NSNumber numberWithInt:TSXProtocolSecurityAutomatic] forKey:ProtocolSecurityDescription(TSXProtocolSecurityAutomatic)];
    [dict setValue:[NSNumber numberWithInt:TSXProtocolSecurityRDP] forKey:ProtocolSecurityDescription(TSXProtocolSecurityRDP)];
    [dict setValue:[NSNumber numberWithInt:TSXProtocolSecurityTLS] forKey:ProtocolSecurityDescription(TSXProtocolSecurityTLS)];
    [dict setValue:[NSNumber numberWithInt:TSXProtocolSecurityNLA] forKey:ProtocolSecurityDescription(TSXProtocolSecurityNLA)];
    return dict;
}


#pragma mark -
#pragma mark Bookmarks

#import "Bookmark.h"

NSMutableArray* FilterBookmarks(NSArray* bookmarks, NSArray* filter_words)
{
	NSMutableArray* matching_items = [NSMutableArray array];
    NSArray* searched_keys = [NSArray arrayWithObjects:@"label", @"params.hostname", @"params.username", @"params.domain", nil];
    
    for (ComputerBookmark* cur_bookmark in bookmarks)
    {
        double match_score = 0.0; 
        for (int i = 0; i < [searched_keys count]; i++)
        {
            NSString* val = [cur_bookmark valueForKeyPath:[searched_keys objectAtIndex:i]];
            
            if (![val isKindOfClass:[NSString class]] || ![val length])
                continue;
            
            for (NSString* word in filter_words)
                if ([val rangeOfString:word options:(NSCaseInsensitiveSearch | NSWidthInsensitiveSearch)].location != NSNotFound)
                    match_score += (1.0 / [filter_words count]) * pow(2, [searched_keys count] - i);
        }
        
        if (match_score > 0.001)
            [matching_items addObject:[NSDictionary dictionaryWithObjectsAndKeys:
                                       cur_bookmark, @"bookmark",
                                       [NSNumber numberWithFloat:match_score], @"score",
                                       nil]];
    }
    	
	[matching_items sortUsingComparator:^NSComparisonResult(NSDictionary* obj1, NSDictionary* obj2) {
		return [[obj2 objectForKey:@"score"] compare:[obj1 objectForKey:@"score"]];
	}];
    
	return matching_items;
}

NSMutableArray* FilterHistory(NSArray* history, NSString* filterStr)
{
    NSMutableArray* result = [NSMutableArray array];
    for (NSString* item in history)
    {
        if ([item rangeOfString:filterStr].location != NSNotFound)
            [result addObject:item];
    }
    
    return result;
}

#pragma mark Version Info
NSString* TSXAppFullVersion()
{
	return [NSString stringWithUTF8String:GIT_REVISION];
}

#pragma mark iPad/iPhone detection

BOOL IsPad()
{
#ifdef UI_USER_INTERFACE_IDIOM
    return (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad);
#else
    return NO;
#endif
}

BOOL IsPhone()
{
#ifdef UI_USER_INTERFACE_IDIOM
    return (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPhone);
#else
    return NO;
#endif    
}

// set mouse buttons swapped flag
static BOOL g_swap_mouse_buttons = NO;
void SetSwapMouseButtonsFlag(BOOL swapped)
{
    g_swap_mouse_buttons = swapped;    
}

// set invert scrolling flag
static BOOL g_invert_scrolling = NO;
void SetInvertScrollingFlag(BOOL invert)
{
    g_invert_scrolling = invert;
}

// return event value for left mouse button 
int GetLeftMouseButtonClickEvent(BOOL down)
{
    if (g_swap_mouse_buttons)
        return (PTR_FLAGS_BUTTON2 | (down ? PTR_FLAGS_DOWN : 0));
    else
        return (PTR_FLAGS_BUTTON1 | (down ? PTR_FLAGS_DOWN : 0));
}

// return event value for right mouse button 
int GetRightMouseButtonClickEvent(BOOL down)
{
    if (g_swap_mouse_buttons)
        return (PTR_FLAGS_BUTTON1 | (down ? PTR_FLAGS_DOWN : 0));
    else
        return (PTR_FLAGS_BUTTON2 | (down ? PTR_FLAGS_DOWN : 0));
}

// get mouse move event
int GetMouseMoveEvent()
{
    return (PTR_FLAGS_MOVE);    
}

// return mouse wheel event
int GetMouseWheelEvent(BOOL down)
{
    if (g_invert_scrolling)
        down = !down;
    
    if(down)
        return (PTR_FLAGS_WHEEL | PTR_FLAGS_WHEEL_NEGATIVE | (0x0088));
    else
        return (PTR_FLAGS_WHEEL | (0x0078));    
}

// scrolling gesture detection delta
CGFloat GetScrollGestureDelta()
{
    return 10.0f;
}

// this hack activates the iphone's WWAN interface in case it is offline
void WakeUpWWAN()
{
    NSURL * url = [[[NSURL alloc] initWithString:@"http://www.nonexistingdummyurl.com"] autorelease];
    //NSData * data =
    [NSData dataWithContentsOfURL:url]; // we don't need data but assigning one causes a "data not used" compiler warning
}


#pragma mark System Info functions

NSString* TSXGetPrimaryMACAddress(NSString *sep)
{
    NSString* macaddress = @"";

    struct ifaddrs *addrs;

    if (getifaddrs(&addrs) < 0)
    {
        NSLog(@"getPrimaryMACAddress: getifaddrs failed.");
        return macaddress;
    }

    for (struct ifaddrs *cursor = addrs; cursor!=NULL; cursor = cursor->ifa_next) 
    {
        if(strcmp(cursor->ifa_name, "en0"))
            continue;
        if( (cursor->ifa_addr->sa_family == AF_LINK) 
           && (((struct sockaddr_dl *) cursor->ifa_addr)->sdl_type == 0x6 /*IFT_ETHER*/))  
        {
            struct sockaddr_dl *dlAddr = (struct sockaddr_dl *) cursor->ifa_addr;
            if(dlAddr->sdl_alen != 6)
                continue;
            unsigned char* base = (unsigned char *) &dlAddr->sdl_data[dlAddr->sdl_nlen];
            macaddress = [NSString hexStringFromData:base ofSize:6 withSeparator:sep afterNthChar:1];
            break;
        }
    }

    freeifaddrs(addrs);

    return macaddress;
}

BOOL TSXDeviceHasJailBreak()
{
    if ([[NSFileManager defaultManager] fileExistsAtPath:@"/Applications/Cydia.app/"])
        return YES;

    if ([[NSFileManager defaultManager] fileExistsAtPath:@"/etc/apt/"])
        return YES;

    return NO;	
}

NSString* TSXGetPlatform()
{
    size_t size;
    sysctlbyname("hw.machine", NULL, &size, NULL, 0);
    char *machine = malloc(size);
    sysctlbyname("hw.machine", machine, &size, NULL, 0);
    NSString *platform = [NSString stringWithCString:machine encoding:NSASCIIStringEncoding];
    free(machine);
    return platform;
}

