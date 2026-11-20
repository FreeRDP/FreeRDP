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
#include <os/lock.h>

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

/* The offers are replaced on the main thread and detached on the channel thread */
static os_unfair_lock s_lock = OS_UNFAIR_LOCK_INIT;
static cliprdr_file_context_data_fkt s_data_fkt = NULL;
static void *s_data_userdata = NULL;
static NSUInteger s_generation = 0;
static unsigned s_download_serial = 0;

/* All file presenters share this serial queue: one download at a time, off the main thread */
static NSOperationQueue *download_queue(void)
{
	static NSOperationQueue *queue = nil;
	static dispatch_once_t once;
	dispatch_once(&once, ^{
		queue = [NSOperationQueue new];
		queue.maxConcurrentOperationCount = 1;
		queue.name = @"com.freerdp.client.cliprdr.file";
	});
	return queue;
}

/* Remove downloaded files as a coordinated writer: it waits for an application that still
 * reads them, e.g. the Finder copying a file that was just downloaded. */
static void remove_path(NSString *path)
{
	NSURL *url = [NSURL fileURLWithPath:path isDirectory:YES];
	NSFileCoordinator *coordinator = [[NSFileCoordinator alloc] initWithFilePresenter:nil];
	__block BOOL removed = NO;
	[coordinator coordinateWritingItemAtURL:url
	                                options:NSFileCoordinatorWritingForDeleting
	                                  error:nil
	                             byAccessor:^(NSURL *newURL) {
		                             [[NSFileManager defaultManager] removeItemAtURL:newURL
		                                                                       error:nil];
		                             removed = YES;
	                             }];
	if (!removed)
		[[NSFileManager defaultManager] removeItemAtURL:url error:nil];
}

/* The placeholders of one server copy and what downloads them */
@interface CliprdrFileOffer : NSObject
@property(nonatomic) cliprdr_file_apple_download_fkt downloadFkt;
@property(nonatomic) cliprdr_file_apple_release_fkt releaseFkt;
@property(nonatomic) void *userdata;
@property(nonatomic, copy) NSString *directory;
@property(nonatomic, strong) NSMutableArray *presenters;
- (void)invalidate;
@end

static CliprdrFileOffer *s_offer = nil;
static CliprdrFileOffer *s_previous_offer = nil;

@implementation CliprdrFileOffer

/* Stop watching the placeholders. Not on the download queue: it delivers their messages. */
- (void)invalidate
{
	for (id<NSFilePresenter> presenter in self.presenters)
		[NSFileCoordinator removeFilePresenter:presenter];
	[self.presenters removeAllObjects];
}

- (void)dealloc
{
	/* The last reference might be the one of a download, on the download queue */
	NSArray *presenters = [_presenters copy];
	NSString *directory = _directory;
	cliprdr_file_apple_release_fkt releaseFkt = _releaseFkt;
	void *userdata = _userdata;

	dispatch_async(dispatch_get_main_queue(), ^{
		for (id<NSFilePresenter> presenter in presenters)
			[NSFileCoordinator removeFilePresenter:presenter];
		if (releaseFkt)
			releaseFkt(userdata);
		if (directory)
		{
			dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
				remove_path(directory);
			});
		}
	});
}

@end

/* Watches one placeholder. An application that reads a file with file coordination, like
 * the Finder does to copy it, has to wait until its presenter lets it: this is where the
 * contents are downloaded. The application waits in its own copy operation only, and the
 * message arrives on the download queue, so neither it nor this process is blocked. */
@interface CliprdrFilePresenter : NSObject <NSFilePresenter>
@property(nonatomic, weak) CliprdrFileOffer *offer;
@property(nonatomic) size_t index;
@property(nonatomic, strong) NSURL *url;
@end

@implementation CliprdrFilePresenter

- (NSURL *)presentedItemURL
{
	return self.url;
}

- (NSOperationQueue *)presentedItemOperationQueue
{
	return download_queue();
}

- (void)relinquishPresentedItemToReader:(void (^)(void (^_Nullable reacquirer)(void)))reader
{
	/* This reference keeps the offer and its files until the download returned */
	CliprdrFileOffer *offer = self.offer;

	/* Better no file than an empty one: the download removes what it could not complete,
	 * and without an offer there is nothing to download from any more */
	if (offer && offer.downloadFkt)
		(void)offer.downloadFkt(offer.userdata, self.index);
	else
		[[NSFileManager defaultManager] removeItemAtURL:self.url error:nil];
	reader(nil);
}

@end

/* Provides the other formats of a server copy (e.g. its text) when they are pasted */
@interface CliprdrDataProvider : NSObject <NSPasteboardItemDataProvider>
@property(nonatomic) NSUInteger generation;
/* pasteboard type -> MIME type the data is requested with */
@property(nonatomic, strong) NSDictionary<NSPasteboardType, NSString *> *mimeTypes;
@end

static CliprdrDataProvider *s_provider = nil;

@implementation CliprdrDataProvider

- (void)pasteboard:(NSPasteboard *)pasteboard
                  item:(NSPasteboardItem *)item
    provideDataForType:(NSPasteboardType)type
{
	if (self.generation != s_generation)
		return;

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

static NSURL *path_url(const char *path)
{
	NSString *str =
	    [[NSFileManager defaultManager] stringWithFileSystemRepresentation:path
	                                                                length:strlen(path)];
	/* Looks at the placeholder to tell a directory from a file */
	return str ? [NSURL fileURLWithPath:str] : nil;
}

BOOL cliprdr_file_apple_offer(const char *directory, const char **paths, const BOOL *present,
                              size_t count, const size_t *top_level, size_t top_level_count,
                              cliprdr_file_apple_download_fkt download_fkt,
                              cliprdr_file_apple_release_fkt release_fkt, void *offer_userdata,
                              const char **mime_types, size_t mime_count,
                              cliprdr_file_context_data_fkt data_fkt, void *data_userdata)
{
	if (@available(macOS 11.0, *))
	{
		@autoreleasepool
		{
			/* From here on the offer frees offer_userdata and the directory, also when
			 * this function fails */
			CliprdrFileOffer *offer = [CliprdrFileOffer new];
			offer.downloadFkt = download_fkt;
			offer.releaseFkt = release_fkt;
			offer.userdata = offer_userdata;
			offer.directory = [[NSFileManager defaultManager]
			    stringWithFileSystemRepresentation:directory
			                                length:strlen(directory)];
			offer.presenters = [NSMutableArray array];

			for (size_t i = 0; i < count; i++)
			{
				if (!paths[i] || !present[i])
					continue;

				CliprdrFilePresenter *presenter = [CliprdrFilePresenter new];
				presenter.offer = offer;
				presenter.index = i;
				presenter.url = path_url(paths[i]);
				if (!presenter.url)
					return FALSE;

				[offer.presenters addObject:presenter];
				[NSFileCoordinator addFilePresenter:presenter];
			}

			CliprdrDataProvider *provider = nil;
			NSDictionary<NSPasteboardType, NSString *> *mimeTypes =
			    get_item_types(mime_types, mime_count);

			NSMutableArray *items = [NSMutableArray array];
			for (size_t i = 0; i < top_level_count; i++)
			{
				/* A real file URL: an application gets it right away, and waits for the
				 * contents only when it reads the file */
				NSURL *url = path_url(paths[top_level[i]]);
				NSPasteboardItem *item = [NSPasteboardItem new];
				if (!url || ![item setString:[url absoluteString] forType:NSPasteboardTypeFileURL])
					return FALSE;

				if ((i == 0) && ([mimeTypes count] > 0))
				{
					/* NSPasteboardItem does not keep its data provider alive */
					provider = [CliprdrDataProvider new];
					provider.mimeTypes = mimeTypes;
					if (![item setDataProvider:provider forTypes:[mimeTypes allKeys]])
						return FALSE;
				}
				[items addObject:item];
			}

			NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
			/* Universal Clipboard reads new contents right away, which would download
			 * every copied file. Keep the files on this Mac until they are pasted. */
			[pasteboard prepareForNewContentsWithOptions:NSPasteboardContentsCurrentHostOnly];
			if (![pasteboard writeObjects:items])
				return FALSE;

			/* Keep the previous offer, Finder might still be copying from it */
			os_unfair_lock_lock(&s_lock);
			CliprdrFileOffer *oldest = s_previous_offer;
			s_previous_offer = s_offer;
			s_offer = offer;
			s_data_fkt = data_fkt;
			s_data_userdata = data_userdata;
			provider.generation = ++s_generation;
			s_provider = provider;
			os_unfair_lock_unlock(&s_lock);
			oldest = nil; /* released without the lock */
			return TRUE;
		}
	}

	if (release_fkt)
		release_fkt(offer_userdata);
	[[NSFileManager defaultManager] removeItemAtPath:@(directory) error:nil];
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
		os_unfair_lock_lock(&s_lock);
		CliprdrFileOffer *offer = s_offer;
		CliprdrFileOffer *previous = s_previous_offer;
		s_offer = nil;
		s_previous_offer = nil;
		s_data_fkt = NULL;
		s_data_userdata = NULL;
		s_generation++;
		s_provider = nil;
		os_unfair_lock_unlock(&s_lock);

		[offer invalidate];
		[previous invalidate];
		offer = nil;
		previous = nil;

		/* The caller cancelled its downloads: wait until the last one returned, it uses
		 * the caller's context. Later messages find no offer any more. */
		[download_queue() waitUntilAllOperationsAreFinished];

		/* Not as a coordinated writer: that would wait for an application that still
		 * copies a downloaded file, for as long as its copy takes */
		[[NSFileManager defaultManager] removeItemAtPath:download_root() error:nil];
	}
}
