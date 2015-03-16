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
#import <freerdp/client/cmdline.h>

static AppDelegate* _singleDelegate = nil;
void AppDelegate_EmbedWindowEventHandler(void* context, EmbedWindowEventArgs* e);
void AppDelegate_ConnectionResultEventHandler(void* context, ConnectionResultEventArgs* e);
void AppDelegate_ErrorInfoEventHandler(void* ctx, ErrorInfoEventArgs* e);
void mac_set_view_size(rdpContext* context, MRDPView* view);

@implementation AppDelegate

- (void)dealloc
{
	[super dealloc];
}

@synthesize window = window;


@synthesize context = context;

- (void) applicationDidFinishLaunching:(NSNotification*)aNotification
{
	int status;
	mfContext* mfc;

	_singleDelegate = self;
	[self CreateContext];

	status = [self ParseCommandLineArguments];

	mfc = (mfContext*) context;
	mfc->view = (void*) mrdpView;

	if (status < 0)
	{

	}
	else
	{
		PubSub_SubscribeConnectionResult(context->pubSub, AppDelegate_ConnectionResultEventHandler);
		PubSub_SubscribeErrorInfo(context->pubSub, AppDelegate_ErrorInfoEventHandler);
		PubSub_SubscribeEmbedWindow(context->pubSub, AppDelegate_EmbedWindowEventHandler);
		
		freerdp_client_start(context);
	}
}

- (void) applicationWillTerminate:(NSNotification*)notification
{
	NSLog(@"Stopping...\n");
	freerdp_client_stop(context);

	[mrdpView releaseResources];
	_singleDelegate = nil;

	NSLog(@"Stopped.\n");
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
	return YES;
}

- (int) ParseCommandLineArguments
{
	int i;
	int length;
	int status;
	char* cptr;

	NSArray* args = [[NSProcessInfo processInfo] arguments];

	context->argc = (int) [args count];
	context->argv = malloc(sizeof(char*) * context->argc);
	
	i = 0;
	
	for (NSString* str in args)
	{
		/* filter out some arguments added by XCode */
		
		if ([str isEqualToString:@"YES"])
			continue;
		
		if ([str isEqualToString:@"-NSDocumentRevisionsDebugMode"])
			continue;
		
		length = (int) ([str length] + 1);
		cptr = (char*) malloc(length);
		strcpy(cptr, [str UTF8String]);
		context->argv[i++] = cptr;
	}
	
	context->argc = i;
	
	status = freerdp_client_settings_parse_command_line(context->settings, context->argc, context->argv, FALSE);
	
	status = freerdp_client_settings_command_line_status_print(context->settings, status, context->argc, context->argv);

	return status;
}

- (void) CreateContext
{
	RDP_CLIENT_ENTRY_POINTS clientEntryPoints;

	ZeroMemory(&clientEntryPoints, sizeof(RDP_CLIENT_ENTRY_POINTS));
	clientEntryPoints.Size = sizeof(RDP_CLIENT_ENTRY_POINTS);
	clientEntryPoints.Version = RDP_CLIENT_INTERFACE_VERSION;

	RdpClientEntry(&clientEntryPoints);

	context = freerdp_client_context_new(&clientEntryPoints);
}

- (void) ReleaseContext
{
	mfContext* mfc;
	MRDPView* view;
	
	mfc = (mfContext*) context;
	view = (MRDPView*) mfc->view;
	
	[view releaseResources];
	[view release];
	 mfc->view = nil;
	
	freerdp_client_context_free(context);
	context = nil;
}


/** *********************************************************************
 * called when we fail to connect to a RDP server - Make sure this is called from the main thread.
 ***********************************************************************/

- (void) rdpConnectError : (NSString*) withMessage
{
	NSString* message = withMessage ? withMessage : @"Error connecting to server";

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

- (void) alertDidEnd:(NSAlert *)a returnCode:(NSInteger)rc contextInfo:(void *)ci
{
	[NSApp terminate:nil];
}


@end

void AppDelegate_EmbedWindowEventHandler(void* ctx, EmbedWindowEventArgs* e)
{
	rdpContext* context = (rdpContext*) ctx;
	
	if (_singleDelegate)
	{
		mfContext* mfc = (mfContext*) context;
		_singleDelegate->mrdpView = mfc->view;

		if (_singleDelegate->window)
		{
			[[_singleDelegate->window contentView] addSubview:mfc->view];
		}
		
		mac_set_view_size(context, mfc->view);
	}
}

/** *********************************************************************
 * On connection error, display message and quit application
 ***********************************************************************/

void AppDelegate_ConnectionResultEventHandler(void* ctx, ConnectionResultEventArgs* e)
{
	NSLog(@"ConnectionResult event result:%d\n", e->result);
	if (_singleDelegate)
	{
		if (e->result != 0)
		{
			NSString* message = nil;
			if (connectErrorCode == AUTHENTICATIONERROR)
			{
				message = [NSString stringWithFormat:@"%@", @"Authentication failure, check credentials."];
			}
			
			
			// Making sure this should be invoked on the main UI thread.
			[_singleDelegate performSelectorOnMainThread:@selector(rdpConnectError:) withObject:message waitUntilDone:FALSE];
		}
	}
}

void AppDelegate_ErrorInfoEventHandler(void* ctx, ErrorInfoEventArgs* e)
{
	NSLog(@"ErrorInfo event code:%d\n", e->code);
	if (_singleDelegate)
	{
		// Retrieve error message associated with error code
		NSString* message = nil;
		if (e->code != ERRINFO_NONE)
		{
			const char* errorMessage = freerdp_get_error_info_string(e->code);
			message = [[NSString alloc] initWithUTF8String:errorMessage];
		}
		
		// Making sure this should be invoked on the main UI thread.
		[_singleDelegate performSelectorOnMainThread:@selector(rdpConnectError:) withObject:message waitUntilDone:TRUE];
		[message release];
	}
}

void mac_set_view_size(rdpContext* context, MRDPView* view)
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
	
	if (context->settings->Fullscreen)
		[[view window] toggleFullScreen:nil];
}
