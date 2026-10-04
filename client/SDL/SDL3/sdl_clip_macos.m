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
