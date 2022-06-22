/*
 RDP Session object

 Copyright 2013 Thincast Technologies GmbH, Authors: Martin Fleisz, Dorian Johnson

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import "ios_freerdp.h"
#import "ios_freerdp_ui.h"
#import "ios_freerdp_events.h"

#import "RDPSession.h"
#import "TSXTypes.h"
#import "Bookmark.h"
#import "ConnectionParams.h"

NSString *TSXSessionDidDisconnectNotification = @"TSXSessionDidDisconnect";
NSString *TSXSessionDidFailToConnectNotification = @"TSXSessionDidFailToConnect";

@interface RDPSession (Private)
- (void)runSession;
- (void)runSessionFinished:(NSNumber *)result;
- (mfInfo *)mfi;

// The connection thread calls these on the main thread.
- (void)sessionWillConnect;
- (void)sessionDidConnect;
- (void)sessionDidDisconnect;
- (void)sessionDidFailToConnect:(int)reason;
- (void)sessionBitmapContextWillChange;
- (void)sessionBitmapContextDidChange;
@end

@implementation RDPSession

@synthesize delegate = _delegate, params = _params, toolbarVisible = _toolbar_visible,
            uiRequestCompleted = _ui_request_completed, bookmark = _bookmark;

+ (void)initialize
{
	ios_init_freerdp();
}

static BOOL addArgument(int *argc, char ***argv, const char *fmt, ...)
{
	va_list ap;
	char *arg = NULL;
	char **tmp = realloc(*argv, (*argc + 1) * sizeof(char *));

	if (!tmp)
		return FALSE;

	*argv = tmp;
	*argc = *argc + 1;
	va_start(ap, fmt);
	vasprintf(&arg, fmt, ap);
	va_end(ap);
	(*argv)[*argc - 1] = arg;
	return TRUE;
}

static BOOL addFlag(int *argc, char ***argv, const char *str, BOOL flag)
{
	return addArgument(argc, argv, "%s%s", flag ? "+" : "-", str);
}

static void freeArguments(int argc, char **argv)
{
	int i;

	for (i = 0; i < argc; i++)
		free(argv[i]);

	free(argv);
}

// Designated initializer.
- (id)initWithBookmark:(ComputerBookmark *)bookmark
{
	int status;
	char **argv = NULL;
	int argc = 0;

	if (!(self = [super init]))
		return nil;

	if (!bookmark)
		[NSException raise:NSInvalidArgumentException
		            format:@"%s: params may not be nil.", __func__];

	_bookmark = [bookmark retain];
	_params = [[bookmark params] copy];
	_name = [[bookmark label] retain];
	_delegate = nil;
	_toolbar_visible = YES;
	_freerdp = ios_freerdp_new();
	_ui_request_completed = [[NSCondition alloc] init];
	BOOL connected_via_3g = ![bookmark conntectedViaWLAN];

	if (!addArgument(&argc, &argv, "iFreeRDP"))
		goto out_free;

	if (!addArgument(&argc, &argv, "/gdi:sw"))
		goto out_free;

	// Screen Size is set on connect (we need a valid delegate in case the user choose an automatic
	// screen size)

	// Other simple numeric settings
	if ([_params hasValueForKey:@"colors"])
		if (!addArgument(&argc, &argv, "/bpp:%d",
		                 [_params intForKey:@"colors" with3GEnabled:connected_via_3g]))
			goto out_free;

	if ([_params hasValueForKey:@"port"])
		if (!addArgument(&argc, &argv, "/port:%d", [_params intForKey:@"port"]))
			goto out_free;

	if ([_params boolForKey:@"console"])
		if (!addArgument(&argc, &argv, "/admin"))
			goto out_free;

	if (!addArgument(&argc, &argv, "/v:%s", [_params UTF8StringForKey:@"hostname"]))
		goto out_free;

	// String settings
	if ([[_params StringForKey:@"username"] length])
	{
		if (!addArgument(&argc, &argv, "/u:%s", [_params UTF8StringForKey:@"username"]))
			goto out_free;
	}

	if ([[_params StringForKey:@"password"] length])
	{
		if (!addArgument(&argc, &argv, "/p:%s", [_params UTF8StringForKey:@"password"]))
			goto out_free;
	}

	if ([[_params StringForKey:@"domain"] length])
	{
		if (!addArgument(&argc, &argv, "/d:%s", [_params UTF8StringForKey:@"domain"]))
			goto out_free;
	}

	if ([[_params StringForKey:@"working_directory"] length])
	{
		if (!addArgument(&argc, &argv, "/shell-dir:%s",
		                 [_params UTF8StringForKey:@"working_directory"]))
			goto out_free;
	}

	if ([[_params StringForKey:@"remote_program"] length])
	{
		if (!addArgument(&argc, &argv, "/shell:%s", [_params UTF8StringForKey:@"remote_program"]))
			goto out_free;
	}

	// RemoteFX
	if ([_params boolForKey:@"perf_remotefx" with3GEnabled:connected_via_3g])
		if (!addArgument(&argc, &argv, "/rfx"))
			goto out_free;

	if ([_params boolForKey:@"perf_gfx" with3GEnabled:connected_via_3g])
		if (!addArgument(&argc, &argv, "/gfx"))
			goto out_free;

	if ([_params boolForKey:@"perf_h264" with3GEnabled:connected_via_3g])
		if (!addArgument(&argc, &argv, "/gfx-h264"))
			goto out_free;

	if (![_params boolForKey:@"perf_remotefx" with3GEnabled:connected_via_3g] &&
	    ![_params boolForKey:@"perf_gfx" with3GEnabled:connected_via_3g] &&
	    ![_params boolForKey:@"perf_h264" with3GEnabled:connected_via_3g])
		if (!addArgument(&argc, &argv, "/nsc"))
			goto out_free;

	if (!addFlag(&argc, &argv, "bitmap-cache", TRUE))
		goto out_free;

	if (!addFlag(&argc, &argv, "wallpaper",
	             [_params boolForKey:@"perf_show_desktop" with3GEnabled:connected_via_3g]))
		goto out_free;

	if (!addFlag(&argc, &argv, "window-drag",
	             [_params boolForKey:@"perf_window_dragging" with3GEnabled:connected_via_3g]))
		goto out_free;

	if (!addFlag(&argc, &argv, "menu-anims",
	             [_params boolForKey:@"perf_menu_animation" with3GEnabled:connected_via_3g]))
		goto out_free;

	if (!addFlag(&argc, &argv, "themes",
	             [_params boolForKey:@"perf_windows_themes" with3GEnabled:connected_via_3g]))
		goto out_free;

	if (!addFlag(&argc, &argv, "fonts",
	             [_params boolForKey:@"perf_font_smoothing" with3GEnabled:connected_via_3g]))
		goto out_free;

	if (!addFlag(&argc, &argv, "aero",
	             [_params boolForKey:@"perf_desktop_composition" with3GEnabled:connected_via_3g]))
		goto out_free;

	if ([_params hasValueForKey:@"width"])
		if (!addArgument(&argc, &argv, "/w:%d", [_params intForKey:@"width"]))
			goto out_free;

	if ([_params hasValueForKey:@"height"])
		if (!addArgument(&argc, &argv, "/h:%d", [_params intForKey:@"height"]))
			goto out_free;

	// security
	switch ([_params intForKey:@"security"])
	{
		case TSXProtocolSecurityNLA:
			if (!addArgument(&argc, &argv, "/sec:NLA"))
				goto out_free;

			break;

		case TSXProtocolSecurityTLS:
			if (!addArgument(&argc, &argv, "/sec:TLS"))
				goto out_free;

			break;

		case TSXProtocolSecurityRDP:
			if (!addArgument(&argc, &argv, "/sec:RDP"))
				goto out_free;

			break;

		default:
			break;
	}

	// ts gateway settings
	if ([_params boolForKey:@"enable_tsg_settings"])
	{
		if (!addArgument(&argc, &argv, "/g:%s", [_params UTF8StringForKey:@"tsg_hostname"]))
			goto out_free;

		if (!addArgument(&argc, &argv, "/gp:%d", [_params intForKey:@"tsg_port"]))
			goto out_free;

		if (!addArgument(&argc, &argv, "/gu:%s", [_params intForKey:@"tsg_username"]))
			goto out_free;

		if (!addArgument(&argc, &argv, "/gp:%s", [_params intForKey:@"tsg_password"]))
			goto out_free;

		if (!addArgument(&argc, &argv, "/gd:%s", [_params intForKey:@"tsg_domain"]))
			goto out_free;
	}

	// Remote keyboard layout
	if (!addArgument(&argc, &argv, "/kbd:%d", 0x409))
		goto out_free;

	status =
	    freerdp_client_settings_parse_command_line(_freerdp->context->settings, argc, argv, FALSE);

	if (0 != status)
		goto out_free;

	freeArguments(argc, argv);
	[self mfi]->session = self;
	return self;
out_free:
	freeArguments(argc, argv);
	[self release];
	return nil;
}

- (void)dealloc
{
	[self setDelegate:nil];
	[_bookmark release];
	[_name release];
	[_params release];
	[_ui_request_completed release];
	ios_freerdp_free(_freerdp);
	[super dealloc];
}

- (CGContextRef)bitmapContext
{
	return [self mfi]->bitmap_context;
}

#pragma mark -
#pragma mark Connecting and disconnecting

- (void)connect
{
	// Set Screen Size to automatic if widht or height are still 0
	rdpSettings *settings = _freerdp->context->settings;

	if (settings->DesktopWidth == 0 || settings->DesktopHeight == 0)
	{
		CGSize size = CGSizeZero;

		if ([[self delegate] respondsToSelector:@selector(sizeForFitScreenForSession:)])
			size = [[self delegate] sizeForFitScreenForSession:self];

		if (!CGSizeEqualToSize(CGSizeZero, size))
		{
			[_params setInt:size.width forKey:@"width"];
			[_params setInt:size.height forKey:@"height"];
			settings->DesktopWidth = size.width;
			settings->DesktopHeight = size.height;
		}
	}

	// TODO: This is a hack to ensure connections to RDVH with 16bpp don't have an odd screen
	// resolution width
	//       Otherwise this could result in screen corruption ..
	if (freerdp_settings_get_uint32(settings, FreeRDP_ColorDepth) <= 16)
		settings->DesktopWidth &= (~1);

	[self performSelectorInBackground:@selector(runSession) withObject:nil];
}

- (void)disconnect
{
	mfInfo *mfi = [self mfi];
	ios_events_send(mfi, [NSDictionary dictionaryWithObject:@"disconnect" forKey:@"type"]);

	if (mfi->connection_state == TSXConnectionConnecting)
	{
		mfi->unwanted = YES;
		[self sessionDidDisconnect];
		return;
	}
}

- (TSXConnectionState)connectionState
{
	return [self mfi]->connection_state;
}

// suspends the session
- (void)suspend
{
	if (!_suspended)
	{
		_suspended = YES;
		//        instance->update->SuppressOutput(instance->context, 0, NULL);
	}
}

// resumes a previously suspended session
- (void)resume
{
	if (_suspended)
	{
		/*        RECTANGLE_16 rec;
		        rec.left = 0;
		        rec.top = 0;
		        rec.right = instance->settings->width;
		        rec.bottom = instance->settings->height;
		*/
		_suspended = NO;
		//        instance->update->SuppressOutput(instance->context, 1, &rec);
		//        [delegate sessionScreenSettingsChanged:self];
	}
}

// returns YES if the session is started
- (BOOL)isSuspended
{
	return _suspended;
}

#pragma mark -
#pragma mark Input events

- (void)sendInputEvent:(NSDictionary *)eventDescriptor
{
	if ([self mfi]->connection_state == TSXConnectionConnected)
		ios_events_send([self mfi], eventDescriptor);
}

#pragma mark -
#pragma mark Server events (main thread)

- (void)setNeedsDisplayInRectAsValue:(NSValue *)rect_value
{
	if ([[self delegate] respondsToSelector:@selector(session:needsRedrawInRect:)])
		[[self delegate] session:self needsRedrawInRect:[rect_value CGRectValue]];
}

#pragma mark -
#pragma mark interface functions

- (UIImage *)getScreenshotWithSize:(CGSize)size
{
	NSAssert([self mfi]->bitmap_context != nil,
	         @"Screenshot requested while having no valid RDP drawing context");
	CGImageRef cgImage = CGBitmapContextCreateImage([self mfi]->bitmap_context);
	UIGraphicsBeginImageContext(size);
	CGContextTranslateCTM(UIGraphicsGetCurrentContext(), 0, size.height);
	CGContextScaleCTM(UIGraphicsGetCurrentContext(), 1.0, -1.0);
	CGContextDrawImage(UIGraphicsGetCurrentContext(), CGRectMake(0, 0, size.width, size.height),
	                   cgImage);
	UIImage *viewImage = UIGraphicsGetImageFromCurrentImageContext();
	UIGraphicsEndImageContext();
	CGImageRelease(cgImage);
	return viewImage;
}

- (rdpSettings *)getSessionParams
{
	return _freerdp->context->settings;
}

- (NSString *)sessionName
{
	return _name;
}

@end

#pragma mark -
@implementation RDPSession (Private)

- (mfInfo *)mfi
{
	return MFI_FROM_INSTANCE(_freerdp);
}

// Blocks until rdp session finishes.
- (void)runSession
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	// Run the session
	[self performSelectorOnMainThread:@selector(sessionWillConnect)
	                       withObject:nil
	                    waitUntilDone:YES];
	int result_code = ios_run_freerdp(_freerdp);
	[self mfi]->connection_state = TSXConnectionDisconnected;
	[self performSelectorOnMainThread:@selector(runSessionFinished:)
	                       withObject:[NSNumber numberWithInt:result_code]
	                    waitUntilDone:YES];
	[pool release];
}

// Main thread.
- (void)runSessionFinished:(NSNumber *)result
{
	int result_code = [result intValue];

	switch (result_code)
	{
		case MF_EXIT_CONN_CANCELED:
			[self sessionDidDisconnect];
			break;

		case MF_EXIT_LOGON_TIMEOUT:
		case MF_EXIT_CONN_FAILED:
			[self sessionDidFailToConnect:result_code];
			break;

		case MF_EXIT_SUCCESS:
		default:
			[self sessionDidDisconnect];
			break;
	}
}

#pragma mark -
#pragma mark Session management (main thread)

- (void)sessionWillConnect
{
	if ([[self delegate] respondsToSelector:@selector(sessionWillConnect:)])
		[[self delegate] sessionWillConnect:self];
}

- (void)sessionDidConnect
{
	if ([[self delegate] respondsToSelector:@selector(sessionDidConnect:)])
		[[self delegate] sessionDidConnect:self];
}

- (void)sessionDidFailToConnect:(int)reason
{
	[[NSNotificationCenter defaultCenter]
	    postNotificationName:TSXSessionDidFailToConnectNotification
	                  object:self];

	if ([[self delegate] respondsToSelector:@selector(session:didFailToConnect:)])
		[[self delegate] session:self didFailToConnect:reason];
}

- (void)sessionDidDisconnect
{
	[[NSNotificationCenter defaultCenter] postNotificationName:TSXSessionDidDisconnectNotification
	                                                    object:self];

	if ([[self delegate] respondsToSelector:@selector(sessionDidDisconnect:)])
		[[self delegate] sessionDidDisconnect:self];
}

- (void)sessionBitmapContextWillChange
{
	if ([[self delegate] respondsToSelector:@selector(sessionBitmapContextWillChange:)])
		[[self delegate] sessionBitmapContextWillChange:self];
}

- (void)sessionBitmapContextDidChange
{
	if ([[self delegate] respondsToSelector:@selector(sessionBitmapContextDidChange:)])
		[[self delegate] sessionBitmapContextDidChange:self];
}

- (void)sessionRequestsAuthenticationWithParams:(NSMutableDictionary *)params
{
	if ([[self delegate] respondsToSelector:@selector(session:requestsAuthenticationWithParams:)])
		[[self delegate] session:self requestsAuthenticationWithParams:params];
}

- (void)sessionVerifyCertificateWithParams:(NSMutableDictionary *)params
{
	if ([[self delegate] respondsToSelector:@selector(session:verifyCertificateWithParams:)])
		[[self delegate] session:self verifyCertificateWithParams:params];
}

@end
