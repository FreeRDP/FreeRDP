/*
 RDP ui callbacks

 Copyright 2013 Thincast Technologies GmbH, Authors: Martin Fleisz, Dorian Johnson

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import <Foundation/Foundation.h>

#import <freerdp/gdi/gdi.h>
#import "ios_freerdp_ui.h"

#import "RDPSession.h"

#pragma mark -
#pragma mark Certificate authentication

static void ios_resize_display_buffer(mfInfo *mfi);
static BOOL ios_ui_authenticate_raw(freerdp *instance, char **username, char **password,
                                    char **domain, const char *title)
{
	mfInfo *mfi = MFI_FROM_INSTANCE(instance);
	NSMutableDictionary *params = [NSMutableDictionary
	    dictionaryWithObjectsAndKeys:(*username) ? [NSString stringWithUTF8String:*username] : @"",
	                                 @"username",
	                                 (*password) ? [NSString stringWithUTF8String:*password] : @"",
	                                 @"password",
	                                 (*domain) ? [NSString stringWithUTF8String:*domain] : @"",
	                                 @"domain",
	                                 [NSString stringWithUTF8String:instance->context->settings->
	                                                                ServerHostname],
	                                 @"hostname", // used for the auth prompt message; not changed
	                                 nil];
	// request auth UI
	[mfi->session performSelectorOnMainThread:@selector(sessionRequestsAuthenticationWithParams:)
	                               withObject:params
	                            waitUntilDone:YES];
	// wait for UI request to be completed
	[[mfi->session uiRequestCompleted] lock];
	[[mfi->session uiRequestCompleted] wait];
	[[mfi->session uiRequestCompleted] unlock];

	if (![[params valueForKey:@"result"] boolValue])
	{
		mfi->unwanted = YES;
		return FALSE;
	}

	// Free old values
	free(*username);
	free(*password);
	free(*domain);
	// set values back
	*username = _strdup([[params objectForKey:@"username"] UTF8String]);
	*password = _strdup([[params objectForKey:@"password"] UTF8String]);
	*domain = _strdup([[params objectForKey:@"domain"] UTF8String]);

	if (!(*username) || !(*password) || !(*domain))
	{
		free(*username);
		free(*password);
		free(*domain);
		return FALSE;
	}

	return TRUE;
}

BOOL ios_ui_authenticate(freerdp *instance, char **username, char **password, char **domain)
{
	return ios_ui_authenticate_raw(instance, username, password, domain, "");
}

BOOL ios_ui_gw_authenticate(freerdp *instance, char **username, char **password, char **domain)
{
	return ios_ui_authenticate_raw(instance, username, password, domain, "gateway");
}

DWORD ios_ui_verify_certificate_ex(freerdp *instance, const char *host, UINT16 port,
                                   const char *common_name, const char *subject, const char *issuer,
                                   const char *fingerprint, DWORD flags)
{
	// check whether we accept all certificates
	if ([[NSUserDefaults standardUserDefaults] boolForKey:@"security.accept_certificates"] == YES)
		return 2;

	mfInfo *mfi = MFI_FROM_INSTANCE(instance);
	NSMutableDictionary *params = [NSMutableDictionary
	    dictionaryWithObjectsAndKeys:(subject) ? [NSString stringWithUTF8String:subject] : @"",
	                                 @"subject",
	                                 (issuer) ? [NSString stringWithUTF8String:issuer] : @"",
	                                 @"issuer",
	                                 (fingerprint) ? [NSString stringWithUTF8String:subject] : @"",
	                                 @"fingerprint", nil];
	// request certificate verification UI
	[mfi->session performSelectorOnMainThread:@selector(sessionVerifyCertificateWithParams:)
	                               withObject:params
	                            waitUntilDone:YES];
	// wait for UI request to be completed
	[[mfi->session uiRequestCompleted] lock];
	[[mfi->session uiRequestCompleted] wait];
	[[mfi->session uiRequestCompleted] unlock];

	if (![[params valueForKey:@"result"] boolValue])
	{
		mfi->unwanted = YES;
		return 0;
	}

	return 1;
}

DWORD ios_ui_verify_changed_certificate_ex(freerdp *instance, const char *host, UINT16 port,
                                           const char *common_name, const char *subject,
                                           const char *issuer, const char *fingerprint,
                                           const char *old_subject, const char *old_issuer,
                                           const char *old_fingerprint, DWORD flags)
{
	return ios_ui_verify_certificate_ex(instance, host, port, common_name, subject, issuer,
	                                    fingerprint, flags);
}

#pragma mark -
#pragma mark Graphics updates

BOOL ios_ui_begin_paint(rdpContext *context)
{
	rdpGdi *gdi = context->gdi;
	gdi->primary->hdc->hwnd->invalid->null = TRUE;
	return TRUE;
}

BOOL ios_ui_end_paint(rdpContext *context)
{
	mfInfo *mfi = MFI_FROM_INSTANCE(context->instance);
	rdpGdi *gdi = context->gdi;
	CGRect dirty_rect =
	    CGRectMake(gdi->primary->hdc->hwnd->invalid->x, gdi->primary->hdc->hwnd->invalid->y,
	               gdi->primary->hdc->hwnd->invalid->w, gdi->primary->hdc->hwnd->invalid->h);

	if (!gdi->primary->hdc->hwnd->invalid->null)
		[mfi->session performSelectorOnMainThread:@selector(setNeedsDisplayInRectAsValue:)
		                               withObject:[NSValue valueWithCGRect:dirty_rect]
		                            waitUntilDone:NO];

	return TRUE;
}

BOOL ios_ui_resize_window(rdpContext *context)
{
	rdpSettings *settings;
	rdpGdi *gdi;

	if (!context || !context->settings)
		return FALSE;

	settings = context->settings;
	gdi = context->gdi;

	if (!gdi_resize(gdi, settings->DesktopWidth, settings->DesktopHeight))
		return FALSE;

	ios_resize_display_buffer(MFI_FROM_INSTANCE(context->instance));
	return TRUE;
}

#pragma mark -
#pragma mark Exported

static void ios_create_bitmap_context(mfInfo *mfi)
{
	[mfi->session performSelectorOnMainThread:@selector(sessionBitmapContextWillChange)
	                               withObject:nil
	                            waitUntilDone:YES];
	rdpGdi *gdi = mfi->instance->context->gdi;
	CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();

	if (FreeRDPGetBytesPerPixel(gdi->dstFormat) == 2)
		mfi->bitmap_context = CGBitmapContextCreate(
		    gdi->primary_buffer, gdi->width, gdi->height, 5, gdi->stride, colorSpace,
		    kCGBitmapByteOrder16Little | kCGImageAlphaNoneSkipFirst);
	else
		mfi->bitmap_context = CGBitmapContextCreate(
		    gdi->primary_buffer, gdi->width, gdi->height, 8, gdi->stride, colorSpace,
		    kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst);

	CGColorSpaceRelease(colorSpace);
	[mfi->session performSelectorOnMainThread:@selector(sessionBitmapContextDidChange)
	                               withObject:nil
	                            waitUntilDone:YES];
}

void ios_allocate_display_buffer(mfInfo *mfi)
{
	ios_create_bitmap_context(mfi);
}

void ios_resize_display_buffer(mfInfo *mfi)
{
	// Release the old context in a thread-safe manner
	CGContextRef old_context = mfi->bitmap_context;
	mfi->bitmap_context = NULL;
	CGContextRelease(old_context);
	// Create the new context
	ios_create_bitmap_context(mfi);
}
