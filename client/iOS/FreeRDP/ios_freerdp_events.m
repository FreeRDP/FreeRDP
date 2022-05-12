/*
 RDP event queuing

 Copyright 2013 Thincast Technologies GmbH, Author: Dorian Johnson

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#include <winpr/assert.h>

#include "ios_freerdp_events.h"

#pragma mark -
#pragma mark Sending compacted input events (from main thread)

// While this function may be called from any thread that has an autorelease pool allocated, it is
// not threadsafe: caller is responsible for synchronization
BOOL ios_events_send(mfInfo *mfi, NSDictionary *event_description)
{
	NSData *encoded_description = [NSKeyedArchiver archivedDataWithRootObject:event_description];

	WINPR_ASSERT(mfi);

	if ([encoded_description length] > 32000 || (mfi->event_pipe_producer == -1))
		return FALSE;

	uint32_t archived_data_len = (uint32_t)[encoded_description length];

	// NSLog(@"writing %d bytes to input event pipe", archived_data_len);

	if (write(mfi->event_pipe_producer, &archived_data_len, 4) == -1)
	{
		NSLog(@"%s: Failed to write length descriptor to pipe.", __func__);
		return FALSE;
	}

	if (write(mfi->event_pipe_producer, [encoded_description bytes], archived_data_len) == -1)
	{
		NSLog(@"%s: Failed to write %d bytes into the event queue (event type: %@).", __func__,
		      (int)[encoded_description length], [event_description objectForKey:@"type"]);
		return FALSE;
	}

	return TRUE;
}

#pragma mark -
#pragma mark Processing compacted input events (from connection thread runloop)

static BOOL ios_events_handle_event(mfInfo *mfi, NSDictionary *event_description)
{
	NSString *event_type = [event_description objectForKey:@"type"];
	BOOL should_continue = TRUE;
	rdpInput *input;

	WINPR_ASSERT(mfi);

	freerdp *instance = mfi->instance;
	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);

	input = instance->context->input;
	WINPR_ASSERT(input);

	if ([event_type isEqualToString:@"mouse"])
	{
		input->MouseEvent(input, [[event_description objectForKey:@"flags"] unsignedShortValue],
		                  [[event_description objectForKey:@"coord_x"] unsignedShortValue],
		                  [[event_description objectForKey:@"coord_y"] unsignedShortValue]);
	}
	else if ([event_type isEqualToString:@"keyboard"])
	{
		if ([[event_description objectForKey:@"subtype"] isEqualToString:@"scancode"])
			freerdp_input_send_keyboard_event(
			    input, [[event_description objectForKey:@"flags"] unsignedShortValue],
			    [[event_description objectForKey:@"scancode"] unsignedShortValue]);
		else if ([[event_description objectForKey:@"subtype"] isEqualToString:@"unicode"])
			freerdp_input_send_unicode_keyboard_event(
			    input, [[event_description objectForKey:@"flags"] unsignedShortValue],
			    [[event_description objectForKey:@"unicode_char"] unsignedShortValue]);
		else
			NSLog(@"%s: doesn't know how to send keyboard input with subtype %@", __func__,
			      [event_description objectForKey:@"subtype"]);
	}
	else if ([event_type isEqualToString:@"disconnect"])
		should_continue = FALSE;
	else
		NSLog(@"%s: unrecognized event type: %@", __func__, event_type);

	return should_continue;
}

BOOL ios_events_check_handle(mfInfo *mfi)
{
	WINPR_ASSERT(mfi);

	if (WaitForSingleObject(mfi->handle, 0) != WAIT_OBJECT_0)
		return TRUE;

	if (mfi->event_pipe_consumer == -1)
		return TRUE;

	uint32_t archived_data_length = 0;
	ssize_t bytes_read;

	// First, read the length of the blob
	bytes_read = read(mfi->event_pipe_consumer, &archived_data_length, 4);

	if (bytes_read == -1 || archived_data_length < 1 || archived_data_length > 32000)
	{
		NSLog(@"%s: just read length descriptor. bytes_read=%ld, archived_data_length=%u", __func__,
		      bytes_read, archived_data_length);
		return FALSE;
	}

	// NSLog(@"reading %d bytes from input event pipe", archived_data_length);

	NSMutableData *archived_object_data =
	    [[NSMutableData alloc] initWithLength:archived_data_length];
	bytes_read =
	    read(mfi->event_pipe_consumer, [archived_object_data mutableBytes], archived_data_length);

	if (bytes_read != archived_data_length)
	{
		NSLog(@"%s: attempted to read data; read %ld bytes but wanted %d bytes.", __func__,
		      bytes_read, archived_data_length);
		[archived_object_data release];
		return FALSE;
	}

	id unarchived_object_data = [NSKeyedUnarchiver unarchiveObjectWithData:archived_object_data];
	[archived_object_data release];

	return ios_events_handle_event(mfi, unarchived_object_data);
}

HANDLE ios_events_get_handle(mfInfo *mfi)
{
	WINPR_ASSERT(mfi);
	return mfi->handle;
}

// Sets up the event pipe
BOOL ios_events_create_pipe(mfInfo *mfi)
{
	int pipe_fds[2];

	WINPR_ASSERT(mfi);

	if (pipe(pipe_fds) == -1)
	{
		NSLog(@"%s: pipe failed.", __func__);
		return FALSE;
	}

	mfi->event_pipe_consumer = pipe_fds[0];
	mfi->event_pipe_producer = pipe_fds[1];
	mfi->handle = CreateFileDescriptorEvent(NULL, FALSE, FALSE, mfi->event_pipe_consumer,
	                                        WINPR_FD_READ | WINPR_FD_WRITE);
	return TRUE;
}

void ios_events_free_pipe(mfInfo *mfi)
{
	WINPR_ASSERT(mfi);
	int consumer_fd = mfi->event_pipe_consumer, producer_fd = mfi->event_pipe_producer;

	mfi->event_pipe_consumer = mfi->event_pipe_producer = -1;
	close(producer_fd);
	close(consumer_fd);
	CloseHandle(mfi->handle);
}
