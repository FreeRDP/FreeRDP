/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Input Redirection Virtual Channel - Mac OS X implementation
 *
 * Copyright (c) 2015 Armin Novak <armin.novak@thincast.com>
 * Copyright 2015 Thincast Technologies GmbH
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/string.h>
#include <winpr/thread.h>
#include <winpr/debug.h>
#include <winpr/cmdline.h>

#define __COREFOUNDATION_CFPLUGINCOM__ 1
#define IUNKNOWN_C_GUTS void *_reserved; void* QueryInterface; void* AddRef; void* Release

#include <CoreAudio/CoreAudioTypes.h>
#include <CoreAudio/CoreAudio.h>
#include <AudioToolbox/AudioToolbox.h>
#include <AudioToolbox/AudioQueue.h>

#include <freerdp/addin.h>
#include <freerdp/codec/dsp.h>
#include <freerdp/channels/rdpsnd.h>

#include "audin_main.h"

#define MAC_AUDIO_QUEUE_NUM_BUFFERS     100
#define MAC_AUDIO_QUEUE_BUFFER_SIZE     32768

typedef struct _AudinMacDevice
{
    IAudinDevice iface;

    FREERDP_DSP_CONTEXT* dsp_context;

    audinFormat format;
    UINT32 FramesPerPacket;
    int dev_unit;

    AudinReceive receive;
    void* user_data;

    rdpContext* rdpcontext;

    bool isOpen;
    AudioQueueRef audioQueue;
    AudioStreamBasicDescription audioFormat;
    AudioQueueBufferRef audioBuffers[MAC_AUDIO_QUEUE_NUM_BUFFERS];
} AudinMacDevice;

static AudioFormatID audin_mac_get_format(const audinFormat* format)
{
    switch (format->wFormatTag)
    {
    case WAVE_FORMAT_PCM:
        return kAudioFormatLinearPCM;
    /*
    case WAVE_FORMAT_GSM610:
        return kAudioFormatMicrosoftGSM;
    case WAVE_FORMAT_ALAW:
        return kAudioFormatALaw;
    case WAVE_FORMAT_MULAW:
        return kAudioFormatULaw;
    case WAVE_FORMAT_AAC_MS:
        return kAudioFormatMPEG4AAC;
    case WAVE_FORMAT_ADPCM:
    case WAVE_FORMAT_DVI_ADPCM:
        return kAudioFormatLinearPCM;
        */
    }

    return 0;
}

static AudioFormatFlags audin_mac_get_flags_for_format(const audinFormat* format)
{
    switch(format->wFormatTag)
    {
    case WAVE_FORMAT_DVI_ADPCM:
    case WAVE_FORMAT_ADPCM:
    case WAVE_FORMAT_PCM:
        return kAudioFormatFlagIsSignedInteger;
    default:
        return 0;
    }
}

static BOOL audin_mac_format_supported(IAudinDevice* device, audinFormat* format)
{
    AudioFormatID req_fmt = 0;

    if (device == NULL || format == NULL)
        return FALSE;

    req_fmt = audin_mac_get_format(format);

    if (req_fmt == 0)
        return FALSE;

    return TRUE;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT audin_mac_set_format(IAudinDevice* device, audinFormat* format, UINT32 FramesPerPacket)
{
    AudinMacDevice* mac = (AudinMacDevice*)device;

    if (device == NULL || format == NULL)
        return ERROR_INVALID_PARAMETER;

    mac->FramesPerPacket = FramesPerPacket;
    CopyMemory(&(mac->format), format, sizeof(audinFormat));

    WLog_INFO(TAG, "Audio Format %s [channels=%d, samples=%d, bits=%d]",
              rdpsnd_get_audio_tag_string(format->wFormatTag),
              format->nChannels, format->nSamplesPerSec, format->wBitsPerSample);

    switch (format->wFormatTag)
    {
    case WAVE_FORMAT_ADPCM:
    case WAVE_FORMAT_DVI_ADPCM:
        mac->FramesPerPacket *= 4; /* Compression ratio. */
        mac->format.wBitsPerSample *= 4;
        break;
    }

    mac->audioFormat.mBitsPerChannel = mac->format.wBitsPerSample;
    mac->audioFormat.mChannelsPerFrame = mac->format.nChannels;
    mac->audioFormat.mFormatFlags = audin_mac_get_flags_for_format(format);
    mac->audioFormat.mFormatID = audin_mac_get_format(format);
    mac->audioFormat.mFramesPerPacket = 1;
    mac->audioFormat.mSampleRate = mac->format.nSamplesPerSec;
    mac->audioFormat.mBytesPerFrame =
            mac->audioFormat.mChannelsPerFrame *  mac->audioFormat.mBitsPerChannel / 8;
    mac->audioFormat.mBytesPerPacket =
            mac->audioFormat.mBytesPerFrame * mac->audioFormat.mFramesPerPacket;

    return CHANNEL_RC_OK;
}

static void mac_audio_queue_input_cb(void *aqData,
                                     AudioQueueRef inAQ,
                                     AudioQueueBufferRef inBuffer,
                                     const AudioTimeStamp *inStartTime,
                                     UInt32 inNumPackets,
                                     const AudioStreamPacketDescription *inPacketDesc)
{
    AudinMacDevice* mac = (AudinMacDevice*)aqData;
    UINT error;
    int encoded_size;
    const BYTE *encoded_data;
    BYTE *buffer = inBuffer->mAudioData;
    int buffer_size = inBuffer->mAudioDataByteSize;

    (void)inAQ;
    (void)inStartTime;
    (void)inNumPackets;
    (void)inPacketDesc;


 	/* Process. */
    switch (mac->format.wFormatTag) {
    case WAVE_FORMAT_ADPCM:
        if (!mac->dsp_context->encode_ms_adpcm(mac->dsp_context,
                                               buffer, buffer_size, mac->format.nChannels, mac->format.nBlockAlign))
        {
            SetLastError(ERROR_INTERNAL_ERROR);
            return;
        }
        encoded_data = mac->dsp_context->adpcm_buffer;
        encoded_size = mac->dsp_context->adpcm_size;
        break;
    case WAVE_FORMAT_DVI_ADPCM:
        if (!mac->dsp_context->encode_ima_adpcm(mac->dsp_context,
                                                buffer, buffer_size, mac->format.nChannels, mac->format.nBlockAlign))
        {
            SetLastError(ERROR_INTERNAL_ERROR);
            break;
        }
        encoded_data = mac->dsp_context->adpcm_buffer;
        encoded_size = mac->dsp_context->adpcm_size;
        break;
    default:
        encoded_data = buffer;
        encoded_size = buffer_size;
        break;
    }

    if ((error = mac->receive(encoded_data, encoded_size, mac->user_data)))
    {
        WLog_ERR(TAG, "mac->receive failed with error %lu", error);
        SetLastError(ERROR_INTERNAL_ERROR);
        return;
    }

}

static UINT audin_mac_close(IAudinDevice *device)
{
    UINT errCode = CHANNEL_RC_OK;
    char errString[1024];
    OSStatus devStat;
    AudinMacDevice *mac = (AudinMacDevice*)device;

    if (device == NULL)
        return ERROR_INVALID_PARAMETER;

    if (mac->isOpen)
    {
        devStat = AudioQueueStop(mac->audioQueue, true);
        if (devStat != 0)
        {
            errCode = GetLastError();
            WLog_ERR(TAG, "AudioQueueStop failed with %s [%d]",
                     winpr_strerror(errCode, errString, sizeof(errString)), errCode);
        }
        mac->isOpen = false;
    }

    if (mac->audioQueue)
    {
        devStat = AudioQueueDispose(mac->audioQueue, true);
        if (devStat != 0)
        {
            errCode = GetLastError();
            WLog_ERR(TAG, "AudioQueueDispose failed with %s [%d]",
                     winpr_strerror(errCode, errString, sizeof(errString)), errCode);
        }

        mac->audioQueue = NULL;
    }

    mac->receive = NULL;
    mac->user_data = NULL;

    return errCode;
}

static UINT audin_mac_open(IAudinDevice *device, AudinReceive receive, void *user_data) {
    AudinMacDevice *mac = (AudinMacDevice*)device;
    DWORD errCode;
    char errString[1024];
    OSStatus devStat;
    size_t index;

    mac->receive = receive;
    mac->user_data = user_data;

    devStat = AudioQueueNewInput(&(mac->audioFormat), mac_audio_queue_input_cb,
                                 mac, NULL, kCFRunLoopCommonModes, 0, &(mac->audioQueue));
    if (devStat != 0)
    {
        errCode = GetLastError();
        WLog_ERR(TAG, "AudioQueueNewInput failed with %s [%d]",
                 winpr_strerror(errCode, errString, sizeof(errString)), errCode);
        goto err_out;
    }

    for (index = 0; index < MAC_AUDIO_QUEUE_NUM_BUFFERS; index++)
    {
        devStat = AudioQueueAllocateBuffer(mac->audioQueue, MAC_AUDIO_QUEUE_BUFFER_SIZE,
                                           &mac->audioBuffers[index]);
        if (devStat != 0)
        {
            errCode = GetLastError();
            WLog_ERR(TAG, "AudioQueueAllocateBuffer failed with %s [%d]",
                     winpr_strerror(errCode, errString, sizeof(errString)), errCode);
            goto err_out;
        }

        devStat = AudioQueueEnqueueBuffer(mac->audioQueue,
                                          mac->audioBuffers[index],
                                          0,
                                          NULL);
        if (devStat != 0)
        {
            errCode = GetLastError();
            WLog_ERR(TAG, "AudioQueueEnqueueBuffer failed with %s [%d]",
                     winpr_strerror(errCode, errString, sizeof(errString)), errCode);
            goto err_out;
        }
    }

    freerdp_dsp_context_reset_adpcm(mac->dsp_context);

    devStat = AudioQueueStart(mac->audioQueue, NULL);
    if (devStat != 0)
    {
        errCode = GetLastError();
        WLog_ERR(TAG, "AudioQueueStart failed with %s [%d]",
                 winpr_strerror(errCode, errString, sizeof(errString)), errCode);
        goto err_out;
    }
    mac->isOpen = true;

    return CHANNEL_RC_OK;

err_out:
    audin_mac_close(device);
    return CHANNEL_RC_INITIALIZATION_ERROR;
}

static UINT audin_mac_free(IAudinDevice* device)
{
    AudinMacDevice *mac = (AudinMacDevice*)device;

    int error;

    if (device == NULL)
        return ERROR_INVALID_PARAMETER;

    if ((error = audin_mac_close(device)))
    {
        WLog_ERR(TAG, "audin_oss_close failed with error code %d!", error);
    }
    freerdp_dsp_context_free(mac->dsp_context);
    free(mac);

    return CHANNEL_RC_OK;
}

static COMMAND_LINE_ARGUMENT_A audin_mac_args[] =
{
    { "dev", COMMAND_LINE_VALUE_REQUIRED, "<device>", NULL, NULL, -1, NULL, "audio device name" },
    { NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
};

static UINT audin_mac_parse_addin_args(AudinMacDevice *device, ADDIN_ARGV *args)
{
    DWORD errCode;
    char errString[1024];
    int status;
    char* str_num, *eptr;
    DWORD flags;
    COMMAND_LINE_ARGUMENT_A* arg;
    AudinMacDevice* mac = (AudinMacDevice*)device;

    if (args->argc == 1)
        return CHANNEL_RC_OK;

    flags = COMMAND_LINE_SIGIL_NONE | COMMAND_LINE_SEPARATOR_COLON | COMMAND_LINE_IGN_UNKNOWN_KEYWORD;
    status = CommandLineParseArgumentsA(args->argc, (const char**)args->argv, audin_mac_args, flags, mac, NULL, NULL);

    if (status < 0)
        return ERROR_INVALID_PARAMETER;

    arg = audin_mac_args;

    do
    {
        if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
            continue;

        CommandLineSwitchStart(arg)
                CommandLineSwitchCase(arg, "dev")
        {
            str_num = _strdup(arg->Value);
            if (!str_num)
            {
                errCode = GetLastError();
                WLog_ERR(TAG, "_strdup failed with %s [%d]",
                         winpr_strerror(errCode, errString, sizeof(errString)), errCode);
                return CHANNEL_RC_NO_MEMORY;
            }
            mac->dev_unit = strtol(str_num, &eptr, 10);

            if (mac->dev_unit < 0 || *eptr != '\0')
                mac->dev_unit = -1;

            free(str_num);
        }
        CommandLineSwitchEnd(arg)
    }
    while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);

    return CHANNEL_RC_OK;
}

#ifdef BUILTIN_CHANNELS
#define freerdp_audin_client_subsystem_entry	mac_freerdp_audin_client_subsystem_entry
#else
#define freerdp_audin_client_subsystem_entry	FREERDP_API freerdp_audin_client_subsystem_entry
#endif

UINT freerdp_audin_client_subsystem_entry(PFREERDP_AUDIN_DEVICE_ENTRY_POINTS pEntryPoints)
{
    DWORD errCode;
    char errString[1024];
    ADDIN_ARGV *args;
    AudinMacDevice *mac;
    UINT error;

    mac = (AudinMacDevice*)calloc(1, sizeof(AudinMacDevice));
    if (!mac)
    {
        errCode = GetLastError();
        WLog_ERR(TAG, "calloc failed with %s [%d]",
                 winpr_strerror(errCode, errString, sizeof(errString)), errCode);
        return CHANNEL_RC_NO_MEMORY;
    }

    mac->iface.Open = audin_mac_open;
    mac->iface.FormatSupported = audin_mac_format_supported;
    mac->iface.SetFormat = audin_mac_set_format;
    mac->iface.Close = audin_mac_close;
    mac->iface.Free = audin_mac_free;
    mac->rdpcontext = pEntryPoints->rdpcontext;

    mac->dev_unit = -1;
    args = pEntryPoints->args;

    if ((error = audin_mac_parse_addin_args(mac, args)))
    {
        WLog_ERR(TAG, "audin_mac_parse_addin_args failed with %lu!", error);
        goto error_out;
    }

    mac->dsp_context = freerdp_dsp_context_new();
    if (!mac->dsp_context)
    {
        WLog_ERR(TAG, "freerdp_dsp_context_new failed!");
        error = CHANNEL_RC_NO_MEMORY;
        goto error_out;
    }

    if ((error = pEntryPoints->pRegisterAudinDevice(pEntryPoints->plugin, (IAudinDevice*) mac)))
    {
        WLog_ERR(TAG, "RegisterAudinDevice failed with error %lu!", error);
        goto error_out;
    }

    return CHANNEL_RC_OK;

error_out:
    freerdp_dsp_context_free(mac->dsp_context);
    free(mac);
    return error;

}
