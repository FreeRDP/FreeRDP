/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client clipboard helper, macOS file support
 *
 * Copyright 2026 Cong Zhang <13283869+congzhangzh@users.noreply.github.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#import <AppKit/AppKit.h>

#include "sdl_clip_macos.h"

static NSArray<NSURL *> *get_file_urls(void)
{
	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
	NSDictionary *options = @{NSPasteboardURLReadingFileURLsOnlyKey: @YES};
	return [pasteboard readObjectsForClasses:@[[NSURL class]] options:options];
}

bool sdl_clip_macos_has_files(void)
{
	@autoreleasepool
	{
		/* Only look at the types: reading the URLs would make lazy providers
		 * (e.g. other remote desktop clients) produce their files right away */
		NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
		return [pasteboard availableTypeFromArray:@[NSPasteboardTypeFileURL]] != nil;
	}
}

char *sdl_clip_macos_get_uri_list(size_t *size)
{
	@autoreleasepool
	{
		NSMutableString *list = [NSMutableString string];

		for (NSURL *url in get_file_urls())
		{
			/* Finder puts file reference URLs (file:///.file/id=...) on the pasteboard */
			NSURL *path_url = [url filePathURL];
			if (!path_url)
				continue;
			[list appendFormat:@"%@\r\n", [path_url absoluteString]];
		}

		if ([list length] == 0)
			return NULL;

		const char *utf8 = [list UTF8String];
		char *data = strdup(utf8);
		if (data && size)
			*size = strlen(data);
		return data;
	}
}

static sdl_clip_macos_provide_cb s_provide_cb = NULL;
static void *s_provide_userdata = NULL;
static NSUInteger s_generation = 0;
static NSInteger s_change_count = -1;
static NSMutableArray *s_providers = nil;
static unsigned s_download_serial = 0;

@interface SdlClipFileProvider : NSObject <NSPasteboardItemDataProvider>
@property(nonatomic) size_t index;
@property(nonatomic) NSUInteger generation;
@end

@implementation SdlClipFileProvider

- (void)pasteboard:(NSPasteboard *)pasteboard
                  item:(NSPasteboardItem *)item
    provideDataForType:(NSPasteboardType)type
{
	if (![type isEqualToString:NSPasteboardTypeFileURL])
		return;
	if (!s_provide_cb || (self.generation != s_generation))
		return;

	char *path = s_provide_cb(s_provide_userdata, self.index);
	if (!path)
		return;

	NSURL *url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path]];
	free(path);
	[item setString:[url absoluteString] forType:NSPasteboardTypeFileURL];
}

@end

bool sdl_clip_macos_offer_files(size_t count, sdl_clip_macos_provide_cb cb, void *userdata)
{
	@autoreleasepool
	{
		s_provide_cb = cb;
		s_provide_userdata = userdata;
		s_generation++;

		/* NSPasteboardItem does not keep its data provider alive */
		s_providers = [NSMutableArray array];
		NSMutableArray *items = [NSMutableArray array];
		for (size_t i = 0; i < count; i++)
		{
			SdlClipFileProvider *provider = [SdlClipFileProvider new];
			provider.index = i;
			provider.generation = s_generation;
			[s_providers addObject:provider];

			NSPasteboardItem *item = [NSPasteboardItem new];
			if (![item setDataProvider:provider forTypes:@[NSPasteboardTypeFileURL]])
				return false;
			[items addObject:item];
		}

		NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
		/* Universal Clipboard reads new contents right away, which would download
		 * every copied file. Keep the files on this Mac until they are pasted. */
		[pasteboard prepareForNewContentsWithOptions:NSPasteboardContentsCurrentHostOnly];
		const BOOL rc = [pasteboard writeObjects:items];
		s_change_count = [pasteboard changeCount];
		return rc;
	}
}

bool sdl_clip_macos_is_own_change(void)
{
	@autoreleasepool
	{
		return (s_change_count >= 0) &&
		       ([[NSPasteboard generalPasteboard] changeCount] == s_change_count);
	}
}

static NSString *download_root(void)
{
	NSString *name = [NSString stringWithFormat:@"freerdp-clipboard-%d", getpid()];
	return [NSTemporaryDirectory() stringByAppendingPathComponent:name];
}

char *sdl_clip_macos_new_download_dir(void)
{
	@autoreleasepool
	{
		NSFileManager *fm = [NSFileManager defaultManager];
		NSString *root = download_root();

		/* Keep the previous offer, Finder might still be copying from it */
		if (s_download_serial >= 2)
		{
			NSString *old = [NSString stringWithFormat:@"%u", s_download_serial - 2];
			[fm removeItemAtPath:[root stringByAppendingPathComponent:old] error:nil];
		}

		NSString *name = [NSString stringWithFormat:@"%u", s_download_serial++];
		NSString *dir = [root stringByAppendingPathComponent:name];
		NSDictionary *attributes = @{NSFilePosixPermissions: @0700};
		if (![fm createDirectoryAtPath:dir
		        withIntermediateDirectories:YES
		                         attributes:attributes
		                              error:nil])
			return NULL;
		return strdup([dir fileSystemRepresentation]);
	}
}

void sdl_clip_macos_detach(void)
{
	@autoreleasepool
	{
		s_provide_cb = NULL;
		s_provide_userdata = NULL;
		s_generation++;
		s_providers = nil;
		[[NSFileManager defaultManager] removeItemAtPath:download_root() error:nil];
	}
}
