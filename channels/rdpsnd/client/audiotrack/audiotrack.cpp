#include <dlfcn.h>
#include <assert.h>
#include <malloc.h>

#include <android/log.h>
#define TAG "freerdp_android_audiotrack"
#define LOGI(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)

#include "audiotrack.h"

#define SIZE_OF_AUDIOTRACK 256

// _ZN7android11AudioSystem19getOutputFrameCountEPii
typedef int (*AudioSystem_getOutputFrameCount)(int *, int);
// _ZN7android11AudioSystem16getOutputLatencyEPji
typedef int (*AudioSystem_getOutputLatency)(unsigned int *, int);
// _ZN7android11AudioSystem21getOutputSamplingRateEPii
typedef int (*AudioSystem_getOutputSamplingRate)(int *, int);

// _ZN7android10AudioTrack16getMinFrameCountEPiij
typedef int (*AudioTrack_getMinFrameCount)(int *, int, unsigned int);

// _ZN7android10AudioTrackC1EijiiijPFviPvS1_ES1_ii
typedef void (*AudioTrack_ctor)(void *, int, unsigned int, int, int, int, unsigned int, void (*)(int, void *, void *), void *, int, int);
// _ZN7android10AudioTrackC1EijiiijPFviPvS1_ES1_i
typedef void (*AudioTrack_ctor_legacy)(void *, int, unsigned int, int, int, int, unsigned int, void (*)(int, void *, void *), void *, int);
// _ZN7android10AudioTrackD1Ev
typedef void (*AudioTrack_dtor)(void *);
// _ZNK7android10AudioTrack9initCheckEv
typedef int (*AudioTrack_initCheck)(void *);
typedef uint32_t (*AudioTrack_latency)(void *);
// _ZN7android10AudioTrack5startEv
typedef void (*AudioTrack_start)(void *);
// _ZN7android10AudioTrack4stopEv
typedef void (*AudioTrack_stop)(void *);
// _ZN7android10AudioTrack5writeEPKvj
typedef int (*AudioTrack_write)(void *, void  const*, unsigned int);
// _ZN7android10AudioTrack5flushEv
typedef void (*AudioTrack_flush)(void *);

static void *libmedia;
static AudioSystem_getOutputFrameCount as_getOutputFrameCount;
static AudioSystem_getOutputLatency as_getOutputLatency;
static AudioSystem_getOutputSamplingRate as_getOutputSamplingRate;

static AudioTrack_getMinFrameCount at_getMinFrameCount;
static AudioTrack_ctor at_ctor;
static AudioTrack_ctor_legacy at_ctor_legacy;
static AudioTrack_dtor at_dtor;
static AudioTrack_initCheck at_initCheck;
static AudioTrack_latency  at_latency;
static AudioTrack_start at_start;
static AudioTrack_stop at_stop;
static AudioTrack_write at_write;
static AudioTrack_flush at_flush;

class AndroidAudioTrack
{
private:
	void* mAudioTrack;

public:
	AndroidAudioTrack()
		: mAudioTrack(NULL)
	{		
	}
	
	virtual ~AndroidAudioTrack()
	{
		close();
	}
	
	void close()
	{
		if (mAudioTrack) {
			if (at_stop)
				at_stop(mAudioTrack);
			if (at_flush)
				at_flush(mAudioTrack);
			if (at_dtor)
				at_dtor(mAudioTrack);
			free(mAudioTrack);
			mAudioTrack = NULL;
		}
	}
	
	int set(int streamType, uint32_t sampleRate, int format, int channels)
	{
		int status;
		int minFrameCount = 0;
		int size = 0;
		
		LOGI("streamTyp = %d, sampleRate = %d, format = %d, channels = %d\n", streamType, sampleRate,  format, channels);
		close();

        if (at_getMinFrameCount) {
            status = at_getMinFrameCount(&minFrameCount, streamType, sampleRate);
            LOGI("at_getMinFrameCount %d, %d\n", minFrameCount, status);
        }
        //size = minFrameCount * (channels == CHANNEL_OUT_STEREO ? 2 : 1) * 4;
        
		mAudioTrack = malloc(SIZE_OF_AUDIOTRACK);
		*((uint32_t *) ((uint32_t)mAudioTrack + SIZE_OF_AUDIOTRACK - 4)) = 0xbaadbaad;
		if (at_ctor) {
			at_ctor(mAudioTrack, streamType, sampleRate, format, channels, size, 0, NULL, NULL, 0, 0);
		} else if (at_ctor_legacy) {
			at_ctor_legacy(mAudioTrack, streamType, sampleRate, format, channels, size, 0, NULL, NULL, 0);
		} else {
		    LOGI("Cannot create AudioTrack!");
			free(mAudioTrack);
			mAudioTrack = NULL;
			return -1;
		}
		assert( (*((uint32_t *) ((uint32_t)mAudioTrack + SIZE_OF_AUDIOTRACK - 4)) == 0xbaadbaad) );

		/* And Init */
		status = at_initCheck(mAudioTrack);
		LOGI("at_initCheck = %d\n", status);

		/* android 1.6 uses channel count instead of stream_type */
		if (status != 0 && at_ctor_legacy) {
			channels = (channels == CHANNEL_OUT_STEREO) ? 2 : 1;
			at_ctor_legacy(mAudioTrack, streamType, sampleRate, format, channels, size, 0, NULL, NULL, 0);
			status = at_initCheck(mAudioTrack);
			LOGI("at_initCheck2 = %d\n", status);
		}
		if (status != 0) {		
			LOGI("Cannot create AudioTrack!");
			free(mAudioTrack);
			mAudioTrack = NULL;
		}
		return status;
	}
	uint32_t latency()
	{
	   if (mAudioTrack && at_latency) {
			return at_latency(mAudioTrack);
		}
		return 0; 
	}	
	int start()
	{
		if (mAudioTrack && at_start) {
			at_start(mAudioTrack);
			return ANDROID_AUDIOTRACK_RESULT_SUCCESS;
		}
		return ANDROID_AUDIOTRACK_RESULT_ERRNO;
	}
	int write(void* buffer, int size)
	{
		if (mAudioTrack && at_write) {
			return at_write(mAudioTrack, buffer, size);
		}
		return ANDROID_AUDIOTRACK_RESULT_ERRNO;	
	}
	int flush()
	{
		if (mAudioTrack && at_flush) {
			at_flush(mAudioTrack);
			return ANDROID_AUDIOTRACK_RESULT_SUCCESS;
		}
		return ANDROID_AUDIOTRACK_RESULT_ERRNO;
	}	
	int stop()
	{
		if (mAudioTrack && at_stop) {
			at_stop(mAudioTrack);
			return ANDROID_AUDIOTRACK_RESULT_SUCCESS;
		}
		return ANDROID_AUDIOTRACK_RESULT_ERRNO;
	}
	int reload()
	{
		return ANDROID_AUDIOTRACK_RESULT_SUCCESS;
	}
};


static void* InitLibrary()
{
	/* DL Open libmedia */
    void *p_library;
    p_library = dlopen("libmedia.so", RTLD_NOW);
    if (!p_library)
        return NULL;

    /* Register symbols */
    as_getOutputFrameCount = (AudioSystem_getOutputFrameCount)(dlsym(p_library, "_ZN7android11AudioSystem19getOutputFrameCountEPii"));
    as_getOutputLatency = (AudioSystem_getOutputLatency)(dlsym(p_library, "_ZN7android11AudioSystem16getOutputLatencyEPji"));
    as_getOutputSamplingRate = (AudioSystem_getOutputSamplingRate)(dlsym(p_library, "_ZN7android11AudioSystem21getOutputSamplingRateEPii"));
    at_getMinFrameCount = (AudioTrack_getMinFrameCount)(dlsym(p_library, "_ZN7android10AudioTrack16getMinFrameCountEPiij"));
    at_ctor = (AudioTrack_ctor)(dlsym(p_library, "_ZN7android10AudioTrackC1EijiiijPFviPvS1_ES1_ii"));
    at_ctor_legacy = (AudioTrack_ctor_legacy)(dlsym(p_library, "_ZN7android10AudioTrackC1EijiiijPFviPvS1_ES1_i"));
    at_dtor = (AudioTrack_dtor)(dlsym(p_library, "_ZN7android10AudioTrackD1Ev"));
    at_initCheck = (AudioTrack_initCheck)(dlsym(p_library, "_ZNK7android10AudioTrack9initCheckEv"));
    at_latency = (AudioTrack_latency)(dlsym(p_library, "_ZNK7android10AudioTrack7latencyEv"));
    at_start = (AudioTrack_start)(dlsym(p_library, "_ZN7android10AudioTrack5startEv"));
    at_stop = (AudioTrack_stop)(dlsym(p_library, "_ZN7android10AudioTrack4stopEv"));
    at_write = (AudioTrack_write)(dlsym(p_library, "_ZN7android10AudioTrack5writeEPKvj"));
    at_flush = (AudioTrack_flush)(dlsym(p_library, "_ZN7android10AudioTrack5flushEv"));

    LOGI("p_library : %p\n", p_library);
    LOGI("as_getOutputFrameCount : %p\n", as_getOutputFrameCount);
    LOGI("as_getOutputLatency : %p\n", as_getOutputLatency);
    LOGI("as_getOutputSamplingRate : %p\n", as_getOutputSamplingRate);
    LOGI("at_getMinFrameCount : %p\n", at_getMinFrameCount);
    LOGI("at_ctor : %p\n", at_ctor);
    LOGI("at_ctor_legacy : %p\n", at_ctor_legacy);
    LOGI("at_dtor : %p\n", at_dtor);
    LOGI("at_initCheck : %p\n", at_initCheck);
    LOGI("at_latency : %p\n", at_latency);
    LOGI("at_start : %p\n", at_start);
    LOGI("at_stop : %p\n", at_stop);
    LOGI("at_write : %p\n", at_write);
    LOGI("at_flush : %p\n", at_flush);
    
    /* We need the first 3 or the last 1 */
#if 0    
    if (!((as_getOutputFrameCount && as_getOutputLatency && as_getOutputSamplingRate)
        || at_getMinFrameCount)) {
        dlclose(p_library);
        return NULL;
    }
#endif    

    // We need all the other Symbols
    if (!((at_ctor || at_ctor_legacy) && at_dtor && at_initCheck &&
           at_start && at_stop && at_write && at_flush)) {
        dlclose(p_library);
        return NULL;
    }
    return p_library;
}

int freerdp_android_at_open(AUDIO_DRIVER_HANDLE* outHandle)
{
    int ret = ANDROID_AUDIOTRACK_RESULT_SUCCESS;
    AndroidAudioTrack* audioTrack = new AndroidAudioTrack();
    
    *outHandle = audioTrack;
        
    return ret;
}

int freerdp_android_at_close(AUDIO_DRIVER_HANDLE handle)
{
    AndroidAudioTrack* audioTrack = (AndroidAudioTrack*)handle;
    if (!audioTrack)
        return ANDROID_AUDIOTRACK_RESULT_ERRNO;
    delete audioTrack;
    return ANDROID_AUDIOTRACK_RESULT_SUCCESS;
}

int freerdp_android_at_set(AUDIO_DRIVER_HANDLE handle, int streamType, uint32_t sampleRate, int format, int channels)
{
    AndroidAudioTrack* audioTrack = (AndroidAudioTrack*)handle;
    if (!audioTrack)
        return ANDROID_AUDIOTRACK_RESULT_ERRNO;
    return audioTrack->set(streamType, sampleRate, format, channels);
}

int freerdp_android_at_set_volume(AUDIO_DRIVER_HANDLE handle, float left, float right)
{
    AndroidAudioTrack* audioTrack = (AndroidAudioTrack*)handle;
    if (!audioTrack)
        return ANDROID_AUDIOTRACK_RESULT_ERRNO;
    return 0;    
}

uint32_t freerdp_android_at_latency(AUDIO_DRIVER_HANDLE handle)
{
    AndroidAudioTrack* audioTrack = (AndroidAudioTrack*)handle;
    if (!audioTrack)
        return ANDROID_AUDIOTRACK_RESULT_ERRNO;
    return audioTrack->latency();
}

int freerdp_android_at_start(AUDIO_DRIVER_HANDLE handle)
{
    AndroidAudioTrack* audioTrack = (AndroidAudioTrack*)handle;
    if (!audioTrack)
        return ANDROID_AUDIOTRACK_RESULT_ERRNO;
    return audioTrack->start();
}

int freerdp_android_at_write(AUDIO_DRIVER_HANDLE handle, void *buffer, int buffer_size)
{
    AndroidAudioTrack* audioTrack = (AndroidAudioTrack*)handle;
    if (!audioTrack)
        return ANDROID_AUDIOTRACK_RESULT_ERRNO;
    return audioTrack->write(buffer, buffer_size);
}

int freerdp_android_at_flush(AUDIO_DRIVER_HANDLE handle)
{
    AndroidAudioTrack* audioTrack = (AndroidAudioTrack*)handle;
    if (!audioTrack)
        return ANDROID_AUDIOTRACK_RESULT_ERRNO;
    return audioTrack->flush();
}

int freerdp_android_at_stop(AUDIO_DRIVER_HANDLE handle)
{
    AndroidAudioTrack* audioTrack = (AndroidAudioTrack*)handle;
    if (!audioTrack)
        return ANDROID_AUDIOTRACK_RESULT_ERRNO;
    return audioTrack->stop();    
}

int freerdp_android_at_reload(AUDIO_DRIVER_HANDLE handle)
{
    AndroidAudioTrack* audioTrack = (AndroidAudioTrack*)handle;
    if (!audioTrack)
        return ANDROID_AUDIOTRACK_RESULT_ERRNO;
    return audioTrack->reload();    
}

int freerdp_android_at_init_library()
{
    if (libmedia == NULL)
    {        
        libmedia = InitLibrary();
        LOGI("loaded");
    }

    return 0;
}
