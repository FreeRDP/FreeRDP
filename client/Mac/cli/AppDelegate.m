//
//  AppDelegate.m
//  MacClient2
//
//  Created by Benoît et Kathy on 2013-05-08.
//
//

#import "AppDelegate.h"
#import "MacFreeRDP/mfreerdp.h"
#import "MacFreeRDP/mf_client.h"
#import "MacFreeRDP/MRDPView.h"

#import <winpr/assert.h>
#import <freerdp/client/cmdline.h>

static AppDelegate *_singleDelegate = nil;
void AppDelegate_ConnectionResultEventHandler(void *context, const ConnectionResultEventArgs *e);
void AppDelegate_ErrorInfoEventHandler(void *ctx, const ErrorInfoEventArgs *e);
void AppDelegate_EmbedWindowEventHandler(void *context, const EmbedWindowEventArgs *e);
void AppDelegate_ResizeWindowEventHandler(void *context, const ResizeWindowEventArgs *e);
void mac_set_view_size(rdpContext *context, MRDPView *view);

@implementation AppDelegate

- (void)dealloc
{
	[super dealloc];
}

@synthesize window = window;

@synthesize context = context;

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
	int status;
	mfContext *mfc;
	_singleDelegate = self;
	[self CreateContext];
	status = [self ParseCommandLineArguments];
	mfc = (mfContext *)context;
	WINPR_ASSERT(mfc);

	mfc->view = (void *)mrdpView;

	if (status == 0)
	{
		NSScreen *screen = [[NSScreen screens] objectAtIndex:0];
		NSRect screenFrame = [screen frame];
		rdpSettings *settings = context->settings;

		WINPR_ASSERT(settings);

		if (settings->Fullscreen)
		{
			settings->DesktopWidth = screenFrame.size.width;
			settings->DesktopHeight = screenFrame.size.height;
		}

		PubSub_SubscribeConnectionResult(context->pubSub, AppDelegate_ConnectionResultEventHandler);
		PubSub_SubscribeErrorInfo(context->pubSub, AppDelegate_ErrorInfoEventHandler);
		PubSub_SubscribeEmbedWindow(context->pubSub, AppDelegate_EmbedWindowEventHandler);
		PubSub_SubscribeResizeWindow(context->pubSub, AppDelegate_ResizeWindowEventHandler);
		freerdp_client_start(context);
		NSString *winTitle;

		if (settings->WindowTitle && settings->WindowTitle[0])
		{
			winTitle = [[NSString alloc] initWithCString:settings->WindowTitle
			                                    encoding:NSUTF8StringEncoding];
		}
		else
		{
			winTitle = [[NSString alloc]
			    initWithFormat:@"%@:%u",
			                   [NSString stringWithCString:settings->ServerHostname
			                                      encoding:NSUTF8StringEncoding],
			                   settings -> ServerPort];
		}

		[window setTitle:winTitle];
	}
	else
	{
		[NSApp terminate:self];
	}
}

- (void)applicationWillBecomeActive:(NSNotification *)notification
{
	[mrdpView resume];
}

- (void)applicationWillResignActive:(NSNotification *)notification
{
	[mrdpView pause];
}

- (void)applicationWillTerminate:(NSNotification *)notification
{
	NSLog(@"Stopping...\n");
	freerdp_client_stop(context);
	[mrdpView releaseResources];
	_singleDelegate = nil;
	NSLog(@"Stopped.\n");
	[NSApp terminate:self];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
	return YES;
}

- (int)ParseCommandLineArguments
{
	int i;
	int length;
	int status;
	char *cptr;
	NSArray *args = [[NSProcessInfo processInfo] arguments];
	context->argc = (int)[args count];
	context->argv = malloc(sizeof(char *) * context->argc);
	i = 0;

	for (NSString *str in args)
	{
		/* filter out some arguments added by XCode */
		if ([str isEqualToString:@"YES"])
			continue;

		if ([str isEqualToString:@"-NSDocumentRevisionsDebugMode"])
			continue;

		length = (int)([str length] + 1);
		cptr = (char *)malloc(length);
		sprintf_s(cptr, length, "%s", [str UTF8String]);
		context->argv[i++] = cptr;
	}

	context->argc = i;
	status = freerdp_client_settings_parse_command_line(context->settings, context->argc,
	                                                    context->argv, FALSE);
	freerdp_client_settings_command_line_status_print(context->settings, status, context->argc,
	                                                  context->argv);
	return status;
}

- (void)CreateContext
{
	RDP_CLIENT_ENTRY_POINTS clientEntryPoints = { 0 };

	clientEntryPoints.Size = sizeof(RDP_CLIENT_ENTRY_POINTS);
	clientEntryPoints.Version = RDP_CLIENT_INTERFACE_VERSION;
	RdpClientEntry(&clientEntryPoints);
	context = freerdp_client_context_new(&clientEntryPoints);
}

- (void)ReleaseContext
{
	mfContext *mfc;
	MRDPView *view;
	mfc = (mfContext *)context;
	view = (MRDPView *)mfc->view;
	[view exitFullScreenModeWithOptions:nil];
	[view releaseResources];
	[view release];
	mfc->view = nil;
	freerdp_client_context_free(context);
	context = nil;
}

/** *********************************************************************
 * called when we fail to connect to a RDP server - Make sure this is called from the main thread.
 ***********************************************************************/

- (void)rdpConnectError:(NSString *)withMessage
{
	mfContext *mfc;
	MRDPView *view;
	mfc = (mfContext *)context;
	view = (MRDPView *)mfc->view;
	[view exitFullScreenModeWithOptions:nil];
	NSString *message = withMessage ? withMessage : @"Error connecting to server";
	NSAlert *alert = [[NSAlert alloc] init];
	[alert setMessageText:message];
	[alert beginSheetModalForWindow:[self window]
	                  modalDelegate:self
	                 didEndSelector:@selector(alertDidEnd:returnCode:contextInfo:)
	                    contextInfo:nil];
}

/** *********************************************************************
 * just a terminate selector for above call
 ***********************************************************************/

- (void)alertDidEnd:(NSAlert *)a returnCode:(NSInteger)rc contextInfo:(void *)ci
{
	[NSApp terminate:nil];
}

@end

/** *********************************************************************
 * On connection error, display message and quit application
 ***********************************************************************/

void AppDelegate_ConnectionResultEventHandler(void *ctx, const ConnectionResultEventArgs *e)
{
	rdpContext *context = (rdpContext *)ctx;
	NSLog(@"ConnectionResult event result:%d\n", e->result);

	if (_singleDelegate)
	{
		if (e->result != 0)
		{
			NSString *message = nil;
			DWORD code = freerdp_get_last_error(context);
			switch (code)
			{
				case FREERDP_ERROR_AUTHENTICATION_FAILED:
					message = [NSString
					    stringWithFormat:@"%@", @"Authentication failure, check credentials."];
					break;
				default:
					break;
			}

			// Making sure this should be invoked on the main UI thread.
			[_singleDelegate performSelectorOnMainThread:@selector(rdpConnectError:)
			                                  withObject:message
			                               waitUntilDone:FALSE];
		}
	}
}

void AppDelegate_ErrorInfoEventHandler(void *ctx, const ErrorInfoEventArgs *e)
{
	NSLog(@"ErrorInfo event code:%d\n", e->code);

	if (_singleDelegate)
	{
		// Retrieve error message associated with error code
		NSString *message = nil;

		if (e->code != ERRINFO_NONE)
		{
			const char *errorMessage = freerdp_get_error_info_string(e->code);
			message = [[NSString alloc] initWithUTF8String:errorMessage];
		}

		// Making sure this should be invoked on the main UI thread.
		[_singleDelegate performSelectorOnMainThread:@selector(rdpConnectError:)
		                                  withObject:message
		                               waitUntilDone:TRUE];
		[message release];
	}
}

void AppDelegate_EmbedWindowEventHandler(void *ctx, const EmbedWindowEventArgs *e)
{
	rdpContext *context = (rdpContext *)ctx;

	if (_singleDelegate)
	{
		mfContext *mfc = (mfContext *)context;
		_singleDelegate->mrdpView = mfc->view;

		if (_singleDelegate->window)
		{
			[[_singleDelegate->window contentView] addSubview:mfc->view];
		}

		dispatch_async(dispatch_get_main_queue(), ^{
			mac_set_view_size(context, mfc->view);
		});
	}
}

void AppDelegate_ResizeWindowEventHandler(void *ctx, const ResizeWindowEventArgs *e)
{
	rdpContext *context = (rdpContext *)ctx;
	fprintf(stderr, "ResizeWindowEventHandler: %d %d\n", e->width, e->height);

	if (_singleDelegate)
	{
		mfContext *mfc = (mfContext *)context;
		dispatch_async(dispatch_get_main_queue(), ^{
			mac_set_view_size(context, mfc->view);
		});
	}
}

void mac_set_view_size(rdpContext *context, MRDPView *view)
{
	// set client area to specified dimensions
	NSRect innerRect;
	innerRect.origin.x = 0;
	innerRect.origin.y = 0;
	innerRect.size.width = context->settings->DesktopWidth;
	innerRect.size.height = context->settings->DesktopHeight;
	[view setFrame:innerRect];
	// calculate window of same size, but keep position
	NSRect outerRect = [[view window] frame];
	outerRect.size = [[view window] frameRectForContentRect:innerRect].size;
	// we are not in RemoteApp mode, disable larger than resolution
	[[view window] setContentMaxSize:innerRect.size];
	// set window to given area
	[[view window] setFrame:outerRect display:YES];
	// set window to front
	[NSApp activateIgnoringOtherApps:YES];

	if (context->settings->Fullscreen)
		[[view window] toggleFullScreen:nil];
}
