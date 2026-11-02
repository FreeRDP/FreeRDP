/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2026 Cong Zhang
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

#import <Foundation/Foundation.h>
#import <CoreMedia/CoreMedia.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include <stdatomic.h>

#include <winpr/assert.h>

#include <freerdp/log.h>

#include "mac_capture.h"

#define TAG SERVER_TAG("shadow.mac")

/* ScreenCaptureKit answers through completion handlers; the shadow server
 * runs no run loop, so wait for them, but never forever. */
static const int64_t mac_capture_timeout_ns = 10LL * NSEC_PER_SEC;

@interface MacCaptureOutput : NSObject <SCStreamOutput, SCStreamDelegate>
{
  @public
	macCaptureConfig config;
	float *audioBuffer;
	size_t audioBufferFrames;
	/* the stream ended by itself: it must not be stopped again */
	atomic_bool stopped;
}
@end

struct mac_capture
{
	macCaptureConfig config;
	MacCaptureOutput *output;
	SCStream *stream;
	dispatch_queue_t videoQueue;
	dispatch_queue_t audioQueue;
	BOOL running;
};

static void mac_capture_log_error(const char *what, NSError *error)
{
	if (error)
		WLog_ERR(TAG, "%s: %s (%s %ld)", what, error.localizedDescription.UTF8String,
		         error.domain.UTF8String, (long)error.code);
	else
		WLog_ERR(TAG, "%s: timed out", what);
}

@implementation MacCaptureOutput

- (void)dealloc
{
	free(audioBuffer);
}

- (void)onScreen:(CMSampleBufferRef)sampleBuffer
{
	CFArrayRef attachments = CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, false);
	if (!attachments || (CFArrayGetCount(attachments) < 1))
		return;

	NSDictionary *info = (__bridge NSDictionary *)CFArrayGetValueAtIndex(attachments, 0);
	NSNumber *status = info[SCStreamFrameInfoStatus];

	/* Idle frames (nothing changed) carry no picture */
	if (!status || (status.integerValue != SCFrameStatusComplete))
		return;

	CVPixelBufferRef frame = CMSampleBufferGetImageBuffer(sampleBuffer);
	if (!frame)
		return;

	NSArray *dirty = info[SCStreamFrameInfoDirtyRects];
	const size_t count = dirty.count;
	CGRect stackRects[32];
	CGRect *rects = stackRects;

	if (count > ARRAYSIZE(stackRects))
	{
		rects = calloc(count, sizeof(CGRect));
		if (!rects)
			return;
	}

	size_t used = 0;
	for (NSDictionary *value in dirty)
	{
		CGRect rect = CGRectNull;
		if (CGRectMakeWithDictionaryRepresentation((__bridge CFDictionaryRef)value, &rect))
			rects[used++] = rect;
	}

	/* No rectangles at all: take the whole frame */
	if (used == 0)
	{
		rects[0] = CGRectMake(0, 0, (CGFloat)CVPixelBufferGetWidth(frame),
		                      (CGFloat)CVPixelBufferGetHeight(frame));
		used = 1;
	}

	config.onFrame(config.context, frame, rects, used);

	if (rects != stackRects)
		free(rects);
}

- (void)onAudio:(CMSampleBufferRef)sampleBuffer
{
	if (!config.onAudio)
		return;

	CMFormatDescriptionRef format = CMSampleBufferGetFormatDescription(sampleBuffer);
	const AudioStreamBasicDescription *asbd =
	    format ? CMAudioFormatDescriptionGetStreamBasicDescription(format) : NULL;
	if (!asbd || (asbd->mFormatID != kAudioFormatLinearPCM) ||
	    !(asbd->mFormatFlags & kAudioFormatFlagIsFloat) || (asbd->mBitsPerChannel != 32))
		return;

	const UINT32 channels = asbd->mChannelsPerFrame;
	const size_t frames = (size_t)CMSampleBufferGetNumSamples(sampleBuffer);
	if ((channels == 0) || (frames == 0))
		return;

	/* ScreenCaptureKit hands out one buffer per channel (non-interleaved) */
	size_t listSize = 0;
	if (CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(sampleBuffer, &listSize, NULL, 0,
	                                                            NULL, NULL, 0, NULL) != noErr)
		return;

	AudioBufferList *list = calloc(1, listSize);
	if (!list)
		return;

	CMBlockBufferRef block = NULL;
	if (CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(
	        sampleBuffer, NULL, list, listSize, NULL, NULL,
	        kCMSampleBufferFlag_AudioBufferList_Assure16ByteAlignment, &block) != noErr)
	{
		free(list);
		return;
	}

	if (audioBufferFrames < frames)
	{
		float *tmp = realloc(audioBuffer, frames * channels * sizeof(float));
		if (!tmp)
			goto out;
		audioBuffer = tmp;
		audioBufferFrames = frames;
	}

	const BOOL interleaved = !(asbd->mFormatFlags & kAudioFormatFlagIsNonInterleaved);
	if (interleaved)
	{
		if ((list->mNumberBuffers < 1) ||
		    (list->mBuffers[0].mDataByteSize < frames * channels * sizeof(float)))
			goto out;
		memcpy(audioBuffer, list->mBuffers[0].mData, frames * channels * sizeof(float));
	}
	else
	{
		if (list->mNumberBuffers < channels)
			goto out;
		for (UINT32 c = 0; c < channels; c++)
		{
			const float *src = list->mBuffers[c].mData;
			if (list->mBuffers[c].mDataByteSize < frames * sizeof(float))
				goto out;
			for (size_t f = 0; f < frames; f++)
				audioBuffer[f * channels + c] = src[f];
		}
	}

	config.onAudio(config.context, audioBuffer, frames, channels, (UINT32)asbd->mSampleRate);

out:
	if (block)
		CFRelease(block);
	free(list);
}

- (void)stream:(SCStream *)stream
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
                   ofType:(SCStreamOutputType)type
{
	WINPR_UNUSED(stream);

	if (!CMSampleBufferIsValid(sampleBuffer))
		return;

	switch (type)
	{
		case SCStreamOutputTypeScreen:
			[self onScreen:sampleBuffer];
			break;
		case SCStreamOutputTypeAudio:
			[self onAudio:sampleBuffer];
			break;
		default:
			break;
	}
}

- (void)stream:(SCStream *)stream didStopWithError:(NSError *)error
{
	WINPR_UNUSED(stream);
	atomic_store(&stopped, true);
	WLog_WARN(TAG, "screen capture stopped: %s (%s %ld)",
	          error ? error.localizedDescription.UTF8String : "no reason given",
	          error ? error.domain.UTF8String : "-", error ? (long)error.code : 0L);
	if (config.onStopped)
		config.onStopped(config.context);
}

@end

BOOL mac_capture_check_access(void)
{
	if (CGPreflightScreenCaptureAccess())
		return TRUE;

	WLog_ERR(TAG, "no permission to record the screen: allow this program in System Settings > "
	              "Privacy & Security > Screen & System Audio Recording, then start it again");
	(void)CGRequestScreenCaptureAccess();
	return FALSE;
}

static SCDisplay *mac_capture_find_display(CGDirectDisplayID displayID)
{
	__block SCShareableContent *content = nil;
	__block NSError *error = nil;
	dispatch_semaphore_t done = dispatch_semaphore_create(0);

	[SCShareableContent
	    getShareableContentExcludingDesktopWindows:NO
	                           onScreenWindowsOnly:YES
	                             completionHandler:^(SCShareableContent *c, NSError *e) {
		                             content = c;
		                             error = e;
		                             dispatch_semaphore_signal(done);
	                             }];

	if (dispatch_semaphore_wait(done, dispatch_time(DISPATCH_TIME_NOW, mac_capture_timeout_ns)) !=
	    0)
		error = nil;

	if (!content)
	{
		mac_capture_log_error("the list of displays", error);
		return nil;
	}

	for (SCDisplay *display in content.displays)
	{
		if (display.displayID == displayID)
			return display;
	}

	WLog_ERR(TAG, "display %" PRIu32 " is not among the ones that can be captured", displayID);
	return nil;
}

macCapture *mac_capture_new(const macCaptureConfig *config)
{
	WINPR_ASSERT(config);
	WINPR_ASSERT(config->onFrame);

	macCapture *capture = calloc(1, sizeof(macCapture));
	if (!capture)
		return NULL;

	capture->config = *config;

	@autoreleasepool
	{
		SCDisplay *display = mac_capture_find_display(config->display);
		if (!display)
			goto fail;

		SCContentFilter *filter = [[SCContentFilter alloc] initWithDisplay:display
		                                                  excludingWindows:@[]];

		SCStreamConfiguration *streamConfig = [[SCStreamConfiguration alloc] init];
		streamConfig.width = config->width;
		streamConfig.height = config->height;
		streamConfig.pixelFormat = kCVPixelFormatType_32BGRA;
		streamConfig.colorSpaceName = kCGColorSpaceSRGB;
		streamConfig.minimumFrameInterval = CMTimeMake(1, (int32_t)MAX(config->fps, 1));
		streamConfig.showsCursor = config->showCursor;
		/* the subsystem keeps the newest frame until it was sent: leave
		 * ScreenCaptureKit enough buffers to go on */
		streamConfig.queueDepth = 5;

		if (config->audio)
		{
			if (@available(macOS 13.0, *))
			{
				streamConfig.capturesAudio = YES;
				streamConfig.excludesCurrentProcessAudio = YES;
				streamConfig.sampleRate = (NSInteger)config->audioRate;
				streamConfig.channelCount = (NSInteger)config->audioChannels;
			}
			else
			{
				WLog_WARN(TAG, "sound needs macOS 13 or later: no sound");
				capture->config.audio = FALSE;
			}
		}

		capture->output = [[MacCaptureOutput alloc] init];
		capture->output->config = capture->config;
		capture->stream = [[SCStream alloc] initWithFilter:filter
		                                     configuration:streamConfig
		                                          delegate:capture->output];
		if (!capture->stream)
			goto fail;

		capture->videoQueue = dispatch_queue_create("freerdp.shadow.mac.video", NULL);
		NSError *error = nil;
		if (![capture->stream addStreamOutput:capture->output
		                                 type:SCStreamOutputTypeScreen
		                   sampleHandlerQueue:capture->videoQueue
		                                error:&error])
		{
			mac_capture_log_error("screen output", error);
			goto fail;
		}

		if (capture->config.audio)
		{
			if (@available(macOS 13.0, *))
			{
				capture->audioQueue = dispatch_queue_create("freerdp.shadow.mac.audio", NULL);
				if (![capture->stream addStreamOutput:capture->output
				                                 type:SCStreamOutputTypeAudio
				                   sampleHandlerQueue:capture->audioQueue
				                                error:&error])
				{
					mac_capture_log_error("sound output: no sound", error);
					capture->output->config.onAudio = NULL;
				}
			}
		}
	}

	return capture;

fail:
	mac_capture_free(capture);
	return NULL;
}

static BOOL mac_capture_wait(const char *what, void (^call)(void (^)(NSError *)))
{
	__block NSError *error = nil;
	__block BOOL answered = NO;
	dispatch_semaphore_t done = dispatch_semaphore_create(0);

	call(^(NSError *e) {
		error = e;
		answered = YES;
		dispatch_semaphore_signal(done);
	});

	if (dispatch_semaphore_wait(done, dispatch_time(DISPATCH_TIME_NOW, mac_capture_timeout_ns)) !=
	        0 ||
	    !answered || error)
	{
		mac_capture_log_error(what, answered ? error : nil);
		return FALSE;
	}
	return TRUE;
}

BOOL mac_capture_start(macCapture *capture)
{
	if (!capture || !capture->stream)
		return FALSE;
	if (capture->running)
		return TRUE;

	SCStream *stream = capture->stream;
	capture->running = mac_capture_wait("starting the screen capture", ^(void (^done)(NSError *)) {
		[stream startCaptureWithCompletionHandler:done];
	});
	return capture->running;
}

void mac_capture_stop(macCapture *capture)
{
	if (!capture || !capture->stream || !capture->running)
		return;
	if (atomic_load(&capture->output->stopped))
	{
		capture->running = FALSE;
		return;
	}

	SCStream *stream = capture->stream;
	(void)mac_capture_wait("stopping the screen capture", ^(void (^done)(NSError *)) {
		[stream stopCaptureWithCompletionHandler:done];
	});
	capture->running = FALSE;
}

void mac_capture_free(macCapture *capture)
{
	if (!capture)
		return;

	mac_capture_stop(capture);

	@autoreleasepool
	{
		capture->stream = nil;
		capture->output = nil;
		capture->videoQueue = nil;
		capture->audioQueue = nil;
	}

	free(capture);
}
