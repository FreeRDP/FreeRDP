#ifndef _TSMF_PLATFORM_H_
#define _TSMF_PLATFORM_H_

#include <gst/gst.h>
#include <tsmf_decoder.h>

typedef struct _TSMFGstreamerDecoder
{
	ITSMFDecoder iface;

	int media_type; /* TSMF_MAJOR_TYPE_AUDIO or TSMF_MAJOR_TYPE_VIDEO */

	gint64 duration;

	GstState state;
	GstCaps *gst_caps;

	GstElement *pipe;
	GstElement *src;
	GstElement *outsink;
	GstElement *volume;

	BOOL ready;
	BOOL paused;
	UINT64 last_sample_end_time;

	double gstVolume;
	BOOL gstMuted;

	int pipeline_start_time_valid; /* We've set the start time and have not reset the pipeline */
	int shutdown; /* The decoder stream is shutting down */

	void *platform;

	BOOL (*ack_cb)(void *,BOOL);
	void (*sync_cb)(void *);
	void *stream;

} TSMFGstreamerDecoder;

const char *get_type(TSMFGstreamerDecoder *mdecoder);

const char *tsmf_platform_get_video_sink(void);
const char *tsmf_platform_get_audio_sink(void);

int tsmf_platform_create(TSMFGstreamerDecoder *decoder);
int tsmf_platform_set_format(TSMFGstreamerDecoder *decoder);
int tsmf_platform_register_handler(TSMFGstreamerDecoder *decoder);
int tsmf_platform_free(TSMFGstreamerDecoder *decoder);

int tsmf_window_create(TSMFGstreamerDecoder *decoder);
int tsmf_window_resize(TSMFGstreamerDecoder *decoder, int x, int y,
					   int width, int height, int nr_rect, RDP_RECT *visible);
int tsmf_window_destroy(TSMFGstreamerDecoder *decoder);

int tsmf_window_pause(TSMFGstreamerDecoder *decoder);
int tsmf_window_resume(TSMFGstreamerDecoder *decoder);

BOOL tsmf_gstreamer_add_pad(TSMFGstreamerDecoder *mdecoder);
void tsmf_gstreamer_remove_pad(TSMFGstreamerDecoder *mdecoder);

#endif
