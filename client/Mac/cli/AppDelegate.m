//
//  AppDelegate.m
//  MacClient2
//
//  Created by Benoît et Kathy on 2013-05-08.
//
//

#import "AppDelegate.h"
#import "MacFreeRDP-library/mfreerdp.h"
#import "MacFreeRDP-library/mf_client.h"

@implementation AppDelegate

- (void)dealloc
{
	[super dealloc];
}

@synthesize window = window;

@synthesize mrdpView = mrdpView;

@synthesize context = context;

- (void) applicationDidFinishLaunching:(NSNotification*)aNotification
{
	int status;
	mfContext* mfc;

	[self CreateContext];

	status = [self ParseCommandLineArguments];

	mfc = (mfContext*) context;
	mfc->view = (void*) mrdpView;

	if (status < 0)
	{

	}
	else
	{
		freerdp_client_start(context);
	}
}

- (void) applicationWillTerminate:(NSNotification*)notification
{
	[mrdpView releaseResources];
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

@end
