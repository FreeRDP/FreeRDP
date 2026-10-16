/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Clipboard file transfer, macOS pasteboard part
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
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "client_cliprdr_file_apple.h"

static const char s_mime_uri_list[] = "text/uri-list";
static const char s_mime_text_utf8[] = "text/plain;charset=utf-8";

static NSArray<NSURL *> *get_file_urls(void)
{
	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
	NSDictionary *options = @{NSPasteboardURLReadingFileURLsOnlyKey: @YES};
	return [pasteboard readObjectsForClasses:@[[NSURL class]] options:options];
}

char *cliprdr_file_apple_get_uri_list(size_t *size)
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

		char *data = strdup([list UTF8String]);
		if (data && size)
			*size = strlen(data);
		return data;
	}
}

static cliprdr_file_apple_provide_file_fkt s_file_fkt = NULL;
static void *s_file_userdata = NULL;
static cliprdr_file_context_data_fkt s_data_fkt = NULL;
static void *s_data_userdata = NULL;
static NSUInteger s_generation = 0;
static NSMutableArray *s_providers = nil;
static unsigned s_download_serial = 0;

@interface CliprdrFileProvider : NSObject <NSPasteboardItemDataProvider>
@property(nonatomic) size_t index;
@property(nonatomic) NSUInteger generation;
/* pasteboard type -> MIME type the data is requested with */
@property(nonatomic, strong) NSDictionary<NSPasteboardType, NSString *> *mimeTypes;
@end

@implementation CliprdrFileProvider

- (void)pasteboard:(NSPasteboard *)pasteboard
                  item:(NSPasteboardItem *)item
    provideDataForType:(NSPasteboardType)type
{
	if (self.generation != s_generation)
		return;

	if ([type isEqualToString:NSPasteboardTypeFileURL])
	{
		if (!s_file_fkt)
			return;

		char *path = s_file_fkt(s_file_userdata, self.index);
		if (!path)
			return;

		NSURL *url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path]];
		free(path);
		[item setString:[url absoluteString] forType:NSPasteboardTypeFileURL];
		return;
	}

	NSString *mime = self.mimeTypes[type];
	if (!mime || !s_data_fkt)
		return;

	size_t size = 0;
	const void *data = s_data_fkt(s_data_userdata, [mime UTF8String], &size);
	if (!data)
		return;

	if ([type isEqualToString:NSPasteboardTypeString])
	{
		/* Applications that paste text get the text of the copy, e.g. the paths */
		NSString *text = [[NSString alloc] initWithBytes:data
		                                          length:size
		                                        encoding:NSUTF8StringEncoding];
		if (text)
			[item setString:text forType:type];
	}
	else
		[item setData:[NSData dataWithBytes:data length:size] forType:type];
}

@end

/* Text becomes the native string, text/uri-list the file items. Everything else keeps its
 * own type, so the caller still recognises the copy as its own (e.g. by a private type)
 * when the pasteboard change comes back to it. */
API_AVAILABLE(macos(11.0))
static NSDictionary<NSPasteboardType, NSString *> *get_item_types(const char **mime_types,
                                                                  size_t mime_count)
{
	NSMutableDictionary<NSPasteboardType, NSString *> *types = [NSMutableDictionary dictionary];

	for (size_t i = 0; i < mime_count; i++)
	{
		const char *mime = mime_types[i];
		if (!mime || (strcmp(mime, s_mime_uri_list) == 0))
			continue;

		if (strncmp(mime, "text/", 5) == 0)
		{
			types[NSPasteboardTypeString] = @(s_mime_text_utf8);
			continue;
		}

		NSString *mime_type = [NSString stringWithUTF8String:mime];
		UTType *uttype = mime_type ? [UTType typeWithMIMEType:mime_type] : nil;
		if (uttype && !types[uttype.identifier])
			types[uttype.identifier] = mime_type;
	}
	return types;
}

BOOL cliprdr_file_apple_offer(size_t count, cliprdr_file_apple_provide_file_fkt file_fkt,
                              void *file_userdata, const char **mime_types, size_t mime_count,
                              cliprdr_file_context_data_fkt data_fkt, void *data_userdata)
{
	if (@available(macOS 11.0, *))
	{
		@autoreleasepool
		{
			s_file_fkt = file_fkt;
			s_file_userdata = file_userdata;
			s_data_fkt = data_fkt;
			s_data_userdata = data_userdata;
			s_generation++;

			NSDictionary<NSPasteboardType, NSString *> *mimeTypes =
			    get_item_types(mime_types, mime_count);

			/* NSPasteboardItem does not keep its data provider alive */
			s_providers = [NSMutableArray array];
			NSMutableArray *items = [NSMutableArray array];
			for (size_t i = 0; i < count; i++)
			{
				CliprdrFileProvider *provider = [CliprdrFileProvider new];
				provider.index = i;
				provider.generation = s_generation;
				[s_providers addObject:provider];

				NSMutableArray<NSPasteboardType> *types =
				    [NSMutableArray arrayWithObject:NSPasteboardTypeFileURL];
				if (i == 0)
				{
					provider.mimeTypes = mimeTypes;
					[types addObjectsFromArray:[mimeTypes allKeys]];
				}

				NSPasteboardItem *item = [NSPasteboardItem new];
				if (![item setDataProvider:provider forTypes:types])
					return FALSE;
				[items addObject:item];
			}

			NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
			/* Universal Clipboard reads new contents right away, which would download
			 * every copied file. Keep the files on this Mac until they are pasted. */
			[pasteboard prepareForNewContentsWithOptions:NSPasteboardContentsCurrentHostOnly];
			return [pasteboard writeObjects:items];
		}
	}
	return FALSE;
}

static NSString *download_root(void)
{
	NSString *name = [NSString stringWithFormat:@"freerdp-clipboard-%d", getpid()];
	return [NSTemporaryDirectory() stringByAppendingPathComponent:name];
}

char *cliprdr_file_apple_new_download_dir(void)
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

void cliprdr_file_apple_detach(void)
{
	@autoreleasepool
	{
		s_file_fkt = NULL;
		s_file_userdata = NULL;
		s_data_fkt = NULL;
		s_data_userdata = NULL;
		s_generation++;
		s_providers = nil;
		[[NSFileManager defaultManager] removeItemAtPath:download_root() error:nil];
	}
}
