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

static AppDelegate* _singleDelegate = nil;
void AppDelegate_EmbedWindowEventHandler(void* context, EmbedWindowEventArgs* e);
void AppDelegate_ConnectionResultEventHandler(void* context, ConnectionResultEventArgs* e);

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
		PubSub_SubscribeEmbedWindow(context->pubSub, AppDelegate_EmbedWindowEventHandler);
		
		freerdp_client_start(context);
	}
}

- (void) applicationWillTerminate:(NSNotification*)notification
{
	[mrdpView releaseResources];
    _singleDelegate = nil;    
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
	return YES;
}

- (int) ParseCommandLineArguments
{
	int i;
	int len;
	int status;
	char* cptr;
	int argc;
	char** argv = nil;

	NSArray* args = [[NSProcessInfo processInfo] arguments];

	argc = (int) [args count];
	argv = malloc(sizeof(char*) * argc);
	
	i = 0;
	
	for (NSString* str in args)
	{
		len = (int) ([str length] + 1);
		cptr = (char*) malloc(len);
		strcpy(cptr, [str UTF8String]);
		argv[i++] = cptr;
	}
	
	status = freerdp_client_parse_command_line(context, argc, argv);

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
	freerdp_client_context_free(context);
	context = nil;
}


/** *********************************************************************
 * called when we fail to connect to a RDP server
 ***********************************************************************/

- (void) rdpConnectError
{
	// TODO: This should be called on the main thread
	
	NSString* message = @"Error connecting to server";
	if (connectErrorCode == AUTHENTICATIONERROR)
	{
		message = [NSString stringWithFormat:@"%@:\n%@", message, @"Authentication failure, check credentials."];
	}
	
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
    }
}

/** *********************************************************************
 * On connection error, display message and quit application
 ***********************************************************************/

void AppDelegate_ConnectionResultEventHandler(void* ctx, ConnectionResultEventArgs* e)
{
	if (_singleDelegate)
	{
		if (e->result != 0)
		{
			[_singleDelegate rdpConnectError];
		}
	}
}