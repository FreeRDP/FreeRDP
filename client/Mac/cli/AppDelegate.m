//
//  AppDelegate.m
//  MacClient2
//
//  Created by Benoît et Kathy on 2013-05-08.
//
//

#import "AppDelegate.h"
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
    
    [self CreateContext];
    
    status = [self ParseCommandLineArguments];
    
    if (status < 0)
    {
        
    }
    else
    {
        [mrdpView rdpStart:context];
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
    char* cptr;
    int argc;
    char** argv = nil;
    int status;
    
	NSArray *args = [[NSProcessInfo processInfo] arguments];

    argc = (int) [args count];
	argv = malloc(sizeof(char *) * argc);
	
	i = 0;
	
	for (NSString * str in args)
    {
        len = (int) ([str length] + 1);
        cptr = (char *) malloc(len);
        strcpy(cptr, [str UTF8String]);
        argv[i++] = cptr;
    }
	
	status = freerdp_client_parse_command_line((rdpContext*) context, argc, argv);
    
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
    freerdp_client_context_free((rdpContext*) context);
    context = nil;
}

@end
