/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server Demo C++ Module
 *
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
 * Copyright 2021 Armin Novak <anovak@thincast.com>
 * Copyright 2021 Thincast Technologies GmbH
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

#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <winpr/crt.h>   /* _winpr_Crc32 */

#include <freerdp/api.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/scancode.h>
#include <freerdp/server/proxy/proxy_modules_api.h>
#include <freerdp/server/proxy/proxy_types.h>
#include <freerdp/server/proxy/proxy_context.h>

extern "C" {
#include "qrcodegen.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}


#define TAG MODULE_TAG("demo")

static constexpr int TARGET_FPS = 30;

struct Recorder {
    bool opened = false;
    int width = 0, height = 0;
    AVRational time_base{1, TARGET_FPS};
    int64_t pts = 0;

    AVFormatContext* fmt = nullptr;
    AVCodecContext*  enc = nullptr;
    AVStream*        st  = nullptr;
    SwsContext*      sws = nullptr;

    AVFrame* frame_yuv = nullptr;  // encoder input (YUV420P)
    AVFrame* frame_bgra = nullptr; // staging (for sws input)

    bool open(const char* path, int w, int h) {
        width = w; height = h;

        // Output format/IO
        if (avformat_alloc_output_context2(&fmt, nullptr, nullptr, path) < 0 || !fmt) {
            WLog_ERR(TAG, "avformat_alloc_output_context2 failed");
            return false;
        }

        // Choose H.264 encoder (libx264 if available, otherwise default h264)
        const AVCodec* codec = avcodec_find_encoder_by_name("libx264");
        if (!codec) codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!codec) {
            WLog_ERR(TAG, "no H.264 encoder found");
            return false;
        }

        st = avformat_new_stream(fmt, codec);
        if (!st) {
            WLog_ERR(TAG, "avformat_new_stream failed");
            return false;
        }
        st->id = fmt->nb_streams - 1;
        st->time_base = time_base;

        enc = avcodec_alloc_context3(codec);
        if (!enc) return false;

        enc->codec_id = codec->id;
        enc->width    = width;
        enc->height   = height;
        enc->pix_fmt  = AV_PIX_FMT_YUV420P;
        enc->time_base = time_base;
        enc->framerate = av_inv_q(time_base);
        enc->gop_size = 60;
        enc->max_b_frames = 2;
        av_opt_set(enc->priv_data, "preset", "veryfast", 0);
        av_opt_set(enc->priv_data, "tune", "zerolatency", 0);

        if (fmt->oformat->flags & AVFMT_GLOBALHEADER)
            enc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        if (avcodec_open2(enc, codec, nullptr) < 0) {
            WLog_ERR(TAG, "avcodec_open2 failed");
            return false;
        }

        if (avcodec_parameters_from_context(st->codecpar, enc) < 0) {
            WLog_ERR(TAG, "avcodec_parameters_from_context failed");
            return false;
        }

        // frames
        frame_yuv = av_frame_alloc();
        frame_bgra = av_frame_alloc();
        if (!frame_yuv || !frame_bgra) return false;

        frame_yuv->format = enc->pix_fmt;
        frame_yuv->width  = width;
        frame_yuv->height = height;
        if (av_frame_get_buffer(frame_yuv, 32) < 0) return false;

        // Keep source format BGRA to match gdi_init_ex(PIXEL_FORMAT_BGRA32).
        frame_bgra->format = AV_PIX_FMT_BGRA;
        frame_bgra->width  = width;
        frame_bgra->height = height;
        if (av_frame_get_buffer(frame_bgra, 32) < 0) return false;

        // Scaler: source BGRA -> target YUV420P
        sws = sws_getContext(width, height, AV_PIX_FMT_BGRA,
                             width, height, AV_PIX_FMT_YUV420P,
                             SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (!sws) {
            WLog_ERR(TAG, "sws_getContext failed");
            return false;
        }

        // open IO
        if (!(fmt->oformat->flags & AVFMT_NOFILE)) {
            if (avio_open(&fmt->pb, path, AVIO_FLAG_WRITE) < 0) {
                WLog_ERR(TAG, "avio_open failed");
                return false; 
            }
        }

        if (avformat_write_header(fmt, nullptr) < 0) {
            WLog_ERR(TAG, "avformat_write_header failed");
            return false;
        }

        opened = true;
        WLog_INFO(TAG, "Recorder opened %s (%dx%d @ %d fps)", path, width, height, TARGET_FPS);
        return true;
    }

    void close() {
        if (!opened) return;
        // flush
        avcodec_send_frame(enc, nullptr);
        for (;;) {
            AVPacket pkt; av_init_packet(&pkt);
            pkt.data = nullptr; pkt.size = 0;
            int r = avcodec_receive_packet(enc, &pkt);
            if (r == AVERROR(EAGAIN) || r == AVERROR_EOF) break;
            pkt.stream_index = st->index;
            av_packet_rescale_ts(&pkt, enc->time_base, st->time_base);
            av_interleaved_write_frame(fmt, &pkt);
            av_packet_unref(&pkt);
        }

        av_write_trailer(fmt);
        if (!(fmt->oformat->flags & AVFMT_NOFILE) && fmt->pb)
            avio_closep(&fmt->pb);

        sws_freeContext(sws);
        av_frame_free(&frame_yuv);
        av_frame_free(&frame_bgra);
        avcodec_free_context(&enc);
        avformat_free_context(fmt);

        opened = false;
        WLog_INFO(TAG, "Recorder closed");
    }

    bool encodeBGRA(const uint8_t* src, int src_stride) {
        if (!opened) {
            WLog_ERR(TAG, "not opened");
            return false;
        }

        // copy BGRA into frame_bgra
        // Make the destination (staging) BGRA/BGR0 frame writable first,
        // then the YUV frame. This ordering ensures we fill the staging
        // buffer before conversion.
        if (av_frame_make_writable(frame_bgra) < 0) {
            WLog_ERR(TAG, "av_frame_make_writable(frame_bgra) failed");
            return false;
        }
        if (av_frame_make_writable(frame_yuv) < 0) {
            WLog_ERR(TAG, "av_frame_make_writable(frame_yuv) failed");
            return false;
        }
 
        // Fill frame_bgra with src
        // libavutil provides a helper, but memcpy line by line is fine:
        const size_t expected_line_bytes = (size_t)width * 4;
        for (int y = 0; y < height; ++y) {
            const uint8_t* s = src + (size_t)y * src_stride;
            uint8_t* d = frame_bgra->data[0] + (size_t)y * frame_bgra->linesize[0];
            // copy only the minimum of the provided stride and expected bytes
            size_t copy_bytes = expected_line_bytes;
            if ((size_t)src_stride < copy_bytes)
                copy_bytes = (size_t)src_stride;
            memcpy(d, s, copy_bytes);
            // If frame linesize is larger than copy_bytes, zero the remainder
            if ((size_t)frame_bgra->linesize[0] > copy_bytes)
                memset(d + copy_bytes, 0, frame_bgra->linesize[0] - copy_bytes);
        } 
 
        // Debug: log first pixel to help verify color channel order (B,G,R,?).
        if (height > 0 && width > 0 && frame_bgra->data[0]) {
            uint8_t* px = frame_bgra->data[0];
            WLog_INFO(TAG, "first pixel (B,G,R,?): %u,%u,%u,%u", px[0], px[1], px[2], px[3]);
        }
 
        // BGRA -> YUV420P
        sws_scale(sws,
                  frame_bgra->data, frame_bgra->linesize,
                  0, height,
                  frame_yuv->data, frame_yuv->linesize);

        frame_yuv->pts = pts++;

        // send to encoder
        if (avcodec_send_frame(enc, frame_yuv) < 0) {
            WLog_ERR(TAG, "avcodec_send_frame failed");
            return false;
        }

        // receive packets
        for (;;) {
            AVPacket pkt; av_init_packet(&pkt);
            pkt.data = nullptr; pkt.size = 0;
            int r = avcodec_receive_packet(enc, &pkt);
            if (r == AVERROR(EAGAIN) || r == AVERROR_EOF) {
                av_packet_unref(&pkt);
                break;
            }

            pkt.stream_index = st->index;
            av_packet_rescale_ts(&pkt, enc->time_base, st->time_base);
            av_interleaved_write_frame(fmt, &pkt);
            av_packet_unref(&pkt);
        }
        return true;
    }
};

struct demo_custom_data
{
	proxyPluginsManager* mgr;
	int somesetting;
    Recorder rec;
    bool initialized = false;
};

static constexpr char plugin_name[] = "demo";
static constexpr char plugin_desc[] = "this is a test plugin";

/* 5x7 glyphs (LSB at top row bit 0). Only the ones we need */
static const uint8_t* get_glyph(char c)
{
    /* columns, 7 rows per column, LSB is row 0 */
    static const uint8_t glyph_H[5]     = { 0x7F, 0x08, 0x08, 0x08, 0x7F }; /* H */
    static const uint8_t glyph_e[5]     = { 0x3C, 0x4A, 0x4A, 0x4A, 0x30 };
    static const uint8_t glyph_l[5]     = { 0x00, 0x41, 0x7F, 0x40, 0x00 };
    static const uint8_t glyph_o[5]     = { 0x3E, 0x41, 0x41, 0x41, 0x3E };
    static const uint8_t glyph_w[5]     = { 0x7E, 0x02, 0x0C, 0x02, 0x7E };
    static const uint8_t glyph_r[5]     = { 0x7E, 0x08, 0x04, 0x04, 0x08 };
    static const uint8_t glyph_d[5]     = { 0x38, 0x44, 0x44, 0x44, 0x7F };
    static const uint8_t glyph_space[5] = { 0x00, 0x00, 0x00, 0x00, 0x00 };
    static const uint8_t glyph_comma[5] = { 0x00, 0x00, 0x60, 0x00, 0x00 };

    switch (c)
    {
        case 'H': return glyph_H;
        case 'e': return glyph_e;
        case 'l': return glyph_l;
        case 'o': return glyph_o;
        case 'w': return glyph_w;
        case 'r': return glyph_r;
        case 'd': return glyph_d;
        case ' ': return glyph_space;
        case ',': return glyph_comma;
        case 'W': return glyph_w;
        case 'h': return glyph_H;       /* crude fallback */
        case '!': return glyph_comma;   /* crude fallback */
        default:  return glyph_space;
    }
}

/* Send a simple ‘H’ in the middle of a 320x64 BGRX surface via SurfaceBits */
static void proxy_send_hello_surface_bits(proxyData* pdata)
{
    if (!pdata || !pdata->ps) {
		WLog_ERR(TAG, "invalid pdata or pdata->ps");
		return;
	} else {
		WLog_INFO(TAG, "pdata %p, pdata->ps %p", pdata, pdata->ps);
	}
    rdpContext* context = &pdata->ps->context;
    if (!context || !context->update) {
		WLog_ERR(TAG, "invalid context or context->update");
		return;
	} else {
		WLog_INFO(TAG, "context %p, context->update %p", context, context->update);
	}
    rdpUpdate* update = context->update;

    const UINT32 W = 320, H = 64;
    const UINT32 Bpp = 4;                 /* 32bpp BGRX */
    const UINT32 stride = W * Bpp;

    BYTE* pixels = (BYTE*)calloc((size_t)W * H, Bpp);
    if (!pixels) {
        WLog_ERR(TAG, "failed to allocate pixel buffer");
        return;
    }

    /* background */
    for (UINT32 y=0; y<H; y++) {
        BYTE* row = pixels + y * stride;
        for (UINT32 x=0; x<W; x++) {
            BYTE* p = row + x * Bpp;
            p[0]=0xD0; p[1]=0xE6; p[2]=0xFF; p[3]=0xFF;   /* B,G,R,ALPHA/UNUSED (set opaque) */
        }
    }
    /* draw a big 'H' (same as you had)… */
    {
        const char* text = "H";
        const int scale = 6;
        int gx = (int)(W - (5*scale))/2, gy = (int)(H - (7*scale))/2;

        for (const char* pc=text; *pc; ++pc) {
            const uint8_t* g = get_glyph(*pc);
            for (int col=0; col<5; col++) {
                uint8_t colbits = g[col];
                for (int row=0; row<7; row++) {
                    if (!((colbits >> row) & 1)) continue;
                    for (int sy=0; sy<scale; sy++) {
                        int yy = gy + row*scale + sy; if ((UINT32)yy>=H) continue;
                        BYTE* drow = pixels + (size_t)yy * stride;
                        for (int sx=0; sx<scale; sx++) {
                            int xx = gx + col*scale + sx; if ((UINT32)xx>=W) continue;
                            BYTE* px = drow + (size_t)xx * Bpp;
                            px[0]=0xFF; px[1]=0xFF; px[2]=0xFF; px[3]=0xFF;  /* white (opaque) */
                        }
                    } 
                }
            }
            gx += (5*scale) + (1*scale);
        }
    }

    /* debug: report available update callbacks */
    WLog_INFO(TAG, "update callbacks: SurfaceBits=%p SurfaceFrameMarker=%p BitmapUpdate=%p",
              (void*)update->SurfaceBits, (void*)update->SurfaceFrameMarker, (void*)update->BitmapUpdate);

    /* frame markers (3.x struct) */
    static UINT32 s_frameId = 1;
    if (update->SurfaceFrameMarker) {
        SURFACE_FRAME_MARKER fm = {0};
        fm.frameId = s_frameId;
        fm.frameAction = SURFACECMD_FRAMEACTION_BEGIN;
        update->SurfaceFrameMarker(context, &fm);
    }

    /* populate SURFACE_BITS_COMMAND with TS_BITMAP_DATA_EX inside */
    SURFACE_BITS_COMMAND cmd;
    memset(&cmd, 0, sizeof(cmd));
    /* Use SET surface bits (inclusive coordinates). Many implementations expect destRight/destBottom
       to be inclusive (W-1, H-1). CMDTYPE_SET_SURFACE_BITS is often the reliable choice. */
    cmd.cmdType    = CMDTYPE_SET_SURFACE_BITS;
    cmd.destLeft   = 0;
    cmd.destTop    = 0;
    cmd.destRight  = (INT32)W - 1;
    cmd.destBottom = (INT32)H - 1;
    cmd.skipCompression = TRUE;

    TS_BITMAP_DATA_EX* b = &cmd.bmp;
    b->bpp               = 32;                       /* 32-bpp */
    b->codecID           = RDP_CODEC_ID_NONE;        /* raw */
    b->width             = (UINT16)W;
    b->height            = (UINT16)H; 
    b->flags             = 0;                        /* no compression flags */
    b->bitmapDataLength  = (UINT32)(stride * H);
    b->bitmapData        = pixels;                   /* pointer to BGRX buffer */

    /* call SurfaceBits if available */
    if (update->SurfaceBits) {
        WLog_INFO(TAG, "calling SurfaceBits cmdType=%u dest=[%d..%d]x[%d..%d] dataLen=%u",
                  (unsigned)cmd.cmdType, cmd.destLeft, cmd.destRight, cmd.destTop, cmd.destBottom,
                  b->bitmapDataLength);
        update->SurfaceBits(context, &cmd);
        WLog_INFO(TAG, "SurfaceBits invoked");
    } else {
        WLog_WARN(TAG, "SurfaceBits callback not available on this context");
    }

    if (update->SurfaceFrameMarker) {
        SURFACE_FRAME_MARKER fm = {0};
        fm.frameId = s_frameId++;
        fm.frameAction = SURFACECMD_FRAMEACTION_END;
        update->SurfaceFrameMarker(context, &fm);
    }

    /* If SurfaceBits didn't work, log presence of BitmapUpdate for later troubleshooting */
    if (!update->SurfaceBits && update->BitmapUpdate) {
        WLog_INFO(TAG, "BitmapUpdate available as alternative (not invoked automatically)");
    }

    free(pixels);
}


/* Render 'payload' centered on an WxH canvas and send via SurfaceBits */
static void proxy_send_qr_center_qrcodegen(proxyData* pdata,
                                           const char* payload)
{
    if (!pdata || !pdata->ps) { WLog_ERR(TAG, "no pdata/ps"); return; }
    rdpContext* context = &pdata->ps->context;
    if (!context || !context->update) { WLog_ERR(TAG, "no context/update"); return; }
    rdpUpdate* update = context->update;
    rdpSettings* s = context->settings;
    if (!s) { WLog_ERR(TAG, "no settings"); return; }

    /* Use the negotiated desktop size */
    UINT32 W = s->DesktopWidth ? s->DesktopWidth : 800;
    UINT32 H = s->DesktopHeight ? s->DesktopHeight : 600;

    const UINT32 Bpp = 4;                 /* BGRX */
    const UINT32 stride = W * Bpp;

    BYTE* pixels = (BYTE*)calloc((size_t)W * H, Bpp);
    if (!pixels) { WLog_ERR(TAG, "OOM"); return; }

    /* White background, X/alpha = 0 for BGRX */
    for (UINT32 y = 0; y < H; y++) {
        BYTE* row = pixels + (size_t)y * stride;
        for (UINT32 x = 0; x < W; x++) {
            BYTE* p = row + (size_t)x * Bpp;
            p[0] = 0xFF; p[1] = 0xFF; p[2] = 0xFF; p[3] = 0x00;
        }
    }

    /* --- Generate QR with qrcodegen --- */
	uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(40)];
	uint8_t tmp[qrcodegen_BUFFER_LEN_FOR_VERSION(40)];
    bool ok = qrcodegen_encodeText(payload, tmp, qr,
                                   qrcodegen_Ecc_MEDIUM,
                                   1, 40,                    /* auto version */
                                   qrcodegen_Mask_AUTO,
                                   true);                   /* boostEcl */
	if (!ok) {
		WLog_WARN(TAG, "ECC_MEDIUM failed, retrying with ECC_LOW");
		ok = qrcodegen_encodeText(
			payload,
			tmp, qr,
			qrcodegen_Ecc_LOW,
			1, 40,
			qrcodegen_Mask_AUTO,
			true
		);
	}
	if (!ok) {
		WLog_ERR(TAG, "qrcodegen_encodeText failed (payload len=%zu)", strlen(payload));
		free(pixels);
		return;
	}

    const int qrSize  = qrcodegen_getSize(qr);  /* modules per side */
    const int border  = 4;                      /* quiet zone */
    const int totalM  = qrSize + 2 * border;

    /* Fit exactly: choose module size so (totalM * module_px) ≤ min(W,H) */
    int module_px = (int)((W < H ? W : H) / totalM);
	module_px /= 2;
    if (module_px < 2) module_px = 2; /* minimum visible size */

    const int image_px = totalM * module_px;
    const int offx = (int)W / 2 - image_px / 2;
    const int offy = (int)H / 2 - image_px / 2;

    WLog_INFO(TAG, "QR: size=%d modules, total=%d, module_px=%d, img=%dx%d, canvas=%ux%u, off=%d,%d",
              qrSize, totalM, module_px, image_px, image_px, W, H, offx, offy);

    /* Paint black modules (BGRX black, X=0) */
    for (int my = 0; my < qrSize; my++) {
        for (int mx = 0; mx < qrSize; mx++) {
            if (!qrcodegen_getModule(qr, mx, my)) continue;

            const int x0 = offx + (mx + border) * module_px;
            const int y0 = offy + (my + border) * module_px;

            for (int yy = 0; yy < module_px; yy++) {
                int py = y0 + yy;
                if ((unsigned)py >= H) continue;
                BYTE* row = pixels + (size_t)py * stride;
                for (int xx = 0; xx < module_px; xx++) {
                    int px = x0 + xx;
                    if ((unsigned)px >= W) continue;
                    BYTE* p = row + (size_t)px * Bpp;
                    p[0] = 0x00; p[1] = 0x00; p[2] = 0x00; p[3] = 0x00;
                }
            }
        }
    }

    /* Optional: frame markers */
    static UINT32 s_frameId = 1;
    if (update->SurfaceFrameMarker) {
        SURFACE_FRAME_MARKER fm = { 0 };
        fm.frameId = s_frameId;
        fm.frameAction = SURFACECMD_FRAMEACTION_BEGIN;
        update->SurfaceFrameMarker(context, &fm);
    }

    /* Send as raw SurfaceBits (32bpp BGRX) */
    SURFACE_BITS_COMMAND cmd; memset(&cmd, 0, sizeof(cmd));
    cmd.cmdType    = CMDTYPE_SET_SURFACE_BITS;
    cmd.destLeft   = 0;
    cmd.destTop    = 0;
    cmd.destRight  = (INT32)W - 1;
    cmd.destBottom = (INT32)H - 1;
    cmd.skipCompression = TRUE;

    TS_BITMAP_DATA_EX* b = &cmd.bmp;
    b->bpp              = 32;
    b->codecID          = RDP_CODEC_ID_NONE;
    b->width            = (UINT16)W;
    b->height           = (UINT16)H;
    b->flags            = 0;
    b->bitmapDataLength = (UINT32)(stride * H);
    b->bitmapData       = pixels;

    if (update->SurfaceBits) {
        update->SurfaceBits(context, &cmd);
    } else {
        WLog_WARN(TAG, "SurfaceBits not available");
    }

    if (update->SurfaceFrameMarker) {
        SURFACE_FRAME_MARKER fm = { 0 };
        fm.frameId = s_frameId++;
        fm.frameAction = SURFACECMD_FRAMEACTION_END;
        update->SurfaceFrameMarker(context, &fm);
    }

    free(pixels);
}


// static BOOL demo_plugin_unload([[maybe_unused]] proxyPlugin* plugin)
// {
// 	WINPR_ASSERT(plugin);

// 	std::cout << "C++ demo plugin: unloading..." << std::endl; 

// 	/* Here we have to free up our custom data storage. */
// 	if (plugin)
// 		delete static_cast<struct demo_custom_data*>(plugin->custom);

// 	return TRUE;
// }

static BOOL demo_plugin_unload(proxyPlugin* plugin) {
    if (!plugin) return FALSE;
    auto* st = static_cast<demo_custom_data*>(plugin->custom);
    if (st) {
        st->rec.close();
        delete st;
    }
    return TRUE;
}

static BOOL demo_client_init_connect([[maybe_unused]] proxyPlugin* plugin,
                                     [[maybe_unused]] proxyData* pdata,
                                     [[maybe_unused]] void* custom)
{
	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(custom);

	WLog_INFO(TAG, "called");
	return TRUE;
}

static BOOL demo_client_uninit_connect([[maybe_unused]] proxyPlugin* plugin,
                                       [[maybe_unused]] proxyData* pdata,
                                       [[maybe_unused]] void* custom)
{
	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(custom);

	WLog_INFO(TAG, "called");
	return TRUE;
}

static BOOL demo_client_pre_connect([[maybe_unused]] proxyPlugin* plugin,
                                    [[maybe_unused]] proxyData* pdata,
                                    [[maybe_unused]] void* custom)
{
	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(custom);

    return TRUE;
}

static BOOL demo_client_post_connect([[maybe_unused]] proxyPlugin* plugin,
                                     [[maybe_unused]] proxyData* pdata,
                                     [[maybe_unused]] void* custom)
{
	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(custom);
    rdpContext* ctx = pdata->pc ? &pdata->pc->context : NULL;
    if (!ctx) return TRUE;

    ctx->settings->SoftwareGdi = TRUE;
    ctx->settings->DeactivateClientDecoding = FALSE;
    
    return TRUE;
}
 
static BOOL demo_client_post_disconnect([[maybe_unused]] proxyPlugin* plugin,
                                        [[maybe_unused]] proxyData* pdata,
                                        [[maybe_unused]] void* custom)
{
	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(custom);

	WLog_INFO(TAG, "called"); 
    if (!plugin) return FALSE;
    auto* st = static_cast<demo_custom_data*>(plugin->custom);
    if (st) {
        st->rec.close();
        WLog_INFO(TAG, "demo_client_post_disconnect: recorder closed");
        delete st;
    }

	return TRUE;
}

static BOOL demo_client_x509_certificate([[maybe_unused]] proxyPlugin* plugin,
                                         [[maybe_unused]] proxyData* pdata,
                                         [[maybe_unused]] void* custom)
{
	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(custom);

	WLog_INFO(TAG, "called");
	return TRUE;
}

static BOOL demo_client_login_failure([[maybe_unused]] proxyPlugin* plugin,
                                      [[maybe_unused]] proxyData* pdata,
                                      [[maybe_unused]] void* custom)
{
	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(custom);

	WLog_INFO(TAG, "called");
	return TRUE;
}

static void dump_ppm_once(const char* path, const uint8_t* bgra, int W, int H, int stride) {
    static bool dumped=false; if (dumped) return; dumped=true;
    FILE* f=fopen(path,"wb"); if(!f){ WLog_ERR(TAG,"PPM fopen failed"); return; }
    fprintf(f,"P6\n%d %d\n255\n", W,H);
    for (int y=0;y<H;y++) {
        const uint8_t* s = bgra + (size_t)y*stride;
        for (int x=0;x<W;x++) {
            fputc(s[x*4+2], f); // R
            fputc(s[x*4+1], f); // G
            fputc(s[x*4+0], f); // B
        }
    }
    fclose(f);
    WLog_INFO(TAG,"Wrote %s", path);
}

// static BOOL demo_client_end_paint([[maybe_unused]] proxyPlugin* plugin,
//                                   [[maybe_unused]] proxyData* pdata, [[maybe_unused]] void* custom)
// {
// 	WINPR_ASSERT(plugin);
// 	WINPR_ASSERT(pdata);
// 	WINPR_ASSERT(custom);

// 	WLog_INFO(TAG, "called");
// 	return TRUE;
// }
static BOOL demo_client_end_paint([[maybe_unused]] proxyPlugin* plugin,
                                   [[maybe_unused]] proxyData* pdata, [[maybe_unused]] void* custom)
{
    WINPR_ASSERT(plugin);
    WINPR_ASSERT(pdata);
    WINPR_ASSERT(custom);

    // if (!plugin || !pdata || !pdata->pc) {
    //     WLog_ERR(TAG, "invalid parameters");
    //     return FALSE;
    // }
    // rdpContext* context = &pdata->pc->context;
    // if (!context) {
    //     WLog_ERR(TAG, "no context");
    //     return FALSE;
    // }

    // auto* st = static_cast<demo_custom_data*>(plugin->custom);
    // if (!st) {
    //     WLog_ERR(TAG, "no custom data");
    //     return FALSE;
    // }
 
    // rdpSettings* s = context->settings;
    // rdpGdi*   g = context->gdi;

    // if (!s) {
    //     WLog_ERR(TAG, "no settings");
    //     return FALSE;
    // }

    // if (!g) {
    //     WLog_ERR(TAG, "no gdi");
    //     return TRUE; // continue session anyway
    // }

    // if (!s || !g || !g->primary || !g->primary->bitmap || !g->primary->bitmap->data) {
    //     WLog_ERR(TAG, "GDI not ready");
    //     return TRUE;
    // }

    // /* Use the actual GDI bitmap dimensions as the source size.
    //    The negotiated DesktopWidth/Height may differ from g->width/g->height
    //    and using settings alone to size the recorder can cause stride/width
    //    mismatches and mis-copies. */
    // const int src_w = (int)g->width;
    // const int src_h = (int)g->height;
    // const int negotiated_w = (int)(s->DesktopWidth ? s->DesktopWidth : src_w);
    // const int negotiated_h = (int)(s->DesktopHeight ? s->DesktopHeight : src_h);

    // const uint8_t* bgra = g->drawing->bitmap->data;
    // const int stride = g->stride; // bytes per line (from GDI)
 
    // /* Diagnostic: log first source pixel and a small CRC of the first few lines */
    // if (bgra && (size_t)stride * (size_t)src_h >= 4) {
    //     size_t sample_lines = (size_t)src_h < 4 ? (size_t)src_h : 4;
    //     size_t sample_len = (size_t)stride * sample_lines;
    //     WLog_INFO(TAG, "src first pixel (B,G,R,A) = %u,%u,%u,%u stride=%d src=%dx%d negotiated=%dx%d",
    //               bgra[0], bgra[1], bgra[2], bgra[3], stride, src_w, src_h, negotiated_w, negotiated_h);
    // } else {
    //     WLog_WARN(TAG, "src buffer too small to sample"); 
    // }
 
    // if (!st->initialized) {
    //     // open recorder using the actual source (GDI) dimensions
    //     if (!st->rec.open("/tmp/output.mp4", src_w, src_h)) {
    //         WLog_ERR(TAG, "Recorder open failed"); 
    //         return FALSE; // continue session anyway
    //     }
    //     st->initialized = true;
    //     WLog_INFO(TAG, "demo_client_end_paint: opened recorder for src %dx%d (negotiated %dx%d) stride %d",
    //               src_w, src_h, negotiated_w, negotiated_h, stride);
    // }

    // // Sanity: ensure stride can hold at least src_w * 4 bytes; otherwise adjust a safe copy width
    // const size_t expected_bytes_per_line = (size_t)src_w * 4;
    // size_t safe_copy_width = src_w;
    // if ((size_t)stride < expected_bytes_per_line) {
    //     safe_copy_width = (size_t)stride / 4;
    //     WLog_WARN(TAG, "stride (%d) smaller than expected (%zu). using safe width=%zu", stride, expected_bytes_per_line, safe_copy_width);
    // }
    
    // dump_ppm_once("/tmp/frame.ppm", bgra, g->width, g->height, g->stride);

    // // The recorder was opened with src_w/src_h; encode using the BGRA buffer and stride.
    // // Recorder::encodeBGRA already copies min(width*4, src_stride) per line, so it will honor `stride`.
    // // If you changed Recorder to rely on the opened width, ensure open() used src_w/src_h as above.
    // st->rec.encodeBGRA(bgra, stride); 
    return TRUE;
}

// The ClientRedirect callback is invoked when the proxy detects a client-directed
// redirection event. Typical scenarios include server-initiated redirection (load
// balancing), RDP file-based redirection, or protocol-driven session transfer.
// A plugin can inspect the provided context/custom pointer (if populated by the
// proxy) to read or modify the redirection target, credentials or handling policy,
// or block/allow the redirect. Here we only log that a redirect hook was called.
static BOOL demo_client_redirect([[maybe_unused]] proxyPlugin* plugin,
                                 [[maybe_unused]] proxyData* pdata, [[maybe_unused]] void* custom)
{
	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	// 'custom' may contain redirect-specific info depending on proxy implementation.
	// Cast and inspect it here if you need to alter the target or credentials.

	WLog_INFO(TAG, "called: ClientRedirect invoked (no-op demo handler)");
	return TRUE;
} 

static BOOL demo_server_post_connect([[maybe_unused]] proxyPlugin* plugin,
                                     [[maybe_unused]] proxyData* pdata,
                                     [[maybe_unused]] void* custom)
{
	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(custom);

	WLog_INFO(TAG, "called");
	return TRUE;
}

static BOOL demo_server_peer_activate([[maybe_unused]] proxyPlugin* plugin,
                                      [[maybe_unused]] proxyData* pdata,
                                      [[maybe_unused]] void* custom)
{
    WINPR_ASSERT(plugin);
    WINPR_ASSERT(pdata);
    WINPR_ASSERT(custom);

    return TRUE;
}

static BOOL demo_server_channels_init([[maybe_unused]] proxyPlugin* plugin,
                                      [[maybe_unused]] proxyData* pdata,
                                      [[maybe_unused]] void* custom)
{
	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(custom);

	WLog_INFO(TAG, "called"); 
	return TRUE;
}

static BOOL demo_server_channels_free([[maybe_unused]] proxyPlugin* plugin,
                                      [[maybe_unused]] proxyData* pdata,
                                      [[maybe_unused]] void* custom)
{
	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(custom);

	WLog_INFO(TAG, "called");
	return TRUE;
}

static BOOL demo_server_session_end([[maybe_unused]] proxyPlugin* plugin,
                                    [[maybe_unused]] proxyData* pdata,
                                    [[maybe_unused]] void* custom)
{
	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(custom);

	WLog_INFO(TAG, "called");
	return TRUE;
}

static BOOL demo_filter_keyboard_event([[maybe_unused]] proxyPlugin* plugin,
                                       [[maybe_unused]] proxyData* pdata,
                                       [[maybe_unused]] void* param)
{
	proxyPluginsManager* mgr = nullptr;
	auto event_data = static_cast<const proxyKeyboardEventInfo*>(param);

	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(event_data);

	mgr = plugin->mgr;
	WINPR_ASSERT(mgr);

	if (event_data == nullptr)
		return FALSE;

	if (event_data->rdp_scan_code == RDP_SCANCODE_KEY_B)
	{
		/* user typed 'B', that means bye :) */
		std::cout << "C++ demo plugin: aborting connection" << std::endl;
		mgr->AbortConnect(mgr, pdata);
	}

	return TRUE;
}

static BOOL demo_filter_unicode_event([[maybe_unused]] proxyPlugin* plugin,
                                      [[maybe_unused]] proxyData* pdata,
                                      [[maybe_unused]] void* param)
{
	proxyPluginsManager* mgr = nullptr;
	auto event_data = static_cast<const proxyUnicodeEventInfo*>(param);

	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(event_data);

	mgr = plugin->mgr;
	WINPR_ASSERT(mgr);

	if (event_data == nullptr)
		return FALSE;

	if (event_data->code == 'b')
	{
		/* user typed 'B', that means bye :) */
		std::cout << "C++ demo plugin: aborting connection" << std::endl;
		mgr->AbortConnect(mgr, pdata);
	}

	return TRUE;
}

static BOOL demo_mouse_event([[maybe_unused]] proxyPlugin* plugin,
                             [[maybe_unused]] proxyData* pdata, [[maybe_unused]] void* param)
{
	auto event_data = static_cast<const proxyMouseEventInfo*>(param);

	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(event_data);

	// WLog_INFO(TAG, "called %p", event_data);
	return TRUE;
}

static BOOL demo_mouse_ex_event([[maybe_unused]] proxyPlugin* plugin,
                                [[maybe_unused]] proxyData* pdata, [[maybe_unused]] void* param)
{
	auto event_data = static_cast<const proxyMouseExEventInfo*>(param);

	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(event_data);

	WLog_INFO(TAG, "called %p", event_data);
	return TRUE;
}

static BOOL demo_client_channel_data([[maybe_unused]] proxyPlugin* plugin,
                                     [[maybe_unused]] proxyData* pdata,
                                     [[maybe_unused]] void* param)
{
	const auto* channel = static_cast<const proxyChannelDataEventInfo*>(param);

	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(channel);

	// WLog_INFO(TAG, "%s [0x%04" PRIx16 "] got %" PRIuz, channel->channel_name, channel->channel_id,
	//           channel->data_len);
	return TRUE;
}

static BOOL demo_server_channel_data([[maybe_unused]] proxyPlugin* plugin,
                                     [[maybe_unused]] proxyData* pdata,
                                     [[maybe_unused]] void* param)
{
	const auto* channel = static_cast<const proxyChannelDataEventInfo*>(param);

	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(channel);

	WLog_WARN(TAG, "%s [0x%04" PRIx16 "] got %" PRIuz, channel->channel_name, channel->channel_id,
	          channel->data_len);
	return TRUE;
}

static BOOL demo_dynamic_channel_create([[maybe_unused]] proxyPlugin* plugin,
                                        [[maybe_unused]] proxyData* pdata,
                                        [[maybe_unused]] void* param)
{
	const auto* channel = static_cast<const proxyChannelDataEventInfo*>(param);

	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(channel);

	WLog_WARN(TAG, "%s [0x%04" PRIx16 "]", channel->channel_name, channel->channel_id);
	return TRUE;
}

static BOOL demo_server_fetch_target_addr([[maybe_unused]] proxyPlugin* plugin,
                                          [[maybe_unused]] proxyData* pdata,
                                          [[maybe_unused]] void* param)
{
    auto event_data = static_cast<const proxyFetchTargetEventInfo*>(param);

    WINPR_ASSERT(plugin);
    WINPR_ASSERT(pdata);  
    WINPR_ASSERT(event_data);

    WLog_INFO(TAG, "target info called %p", event_data);

    /* cast away const to modify the target for this session */
    auto mod_event = const_cast<proxyFetchTargetEventInfo*>(event_data);
    if (!mod_event)
        return FALSE; 
 
    /* free any existing address string and set new address/port
       Try reading /rdp-proxy/credentials.txt containing "host:port".
       Fallback to hardcoded "172.18.0.5:37715" on error. */
    char addrbuf[256] = {0};
    if (FILE* f = fopen("/rdp-proxy/credentials.txt", "r")) {
        if (fgets(addrbuf, sizeof(addrbuf), f)) {
            /* strip trailing newline/cr/space/tab */
            size_t len = strlen(addrbuf);
            while (len > 0 && (addrbuf[len-1] == '\n' || addrbuf[len-1] == '\r' ||
                               addrbuf[len-1] == ' '  || addrbuf[len-1] == '\t')) {
                addrbuf[--len] = '\0';
            }
        }
        fclose(f);
    }

    const char* use_host = "172.18.0.5";
    int use_port = 37715;
    if (addrbuf[0]) {
        /* allow leading spaces, use pointer into buffer */
        char* p = addrbuf;
        while (*p == ' ' || *p == '\t') ++p;
        char* colon = strchr(p, ':');
        if (colon) {
            *colon = '\0';
            use_host = p;
            use_port = atoi(colon + 1);
            if (use_port == 0)
                use_port = 37715;
        } else if (p[0] != '\0') {
            use_host = p;
        }
    }

    if (mod_event->target_address)
        free(mod_event->target_address);
    mod_event->target_address = strdup(use_host);
    mod_event->target_port = (unsigned)use_port;
 
     mod_event->fetch_method = ProxyFetchTargetMethod::PROXY_FETCH_TARGET_METHOD_CONFIG;
     /* make config writable and set credentials (free existing strings first) */
     auto cfg = const_cast<proxy_config*>(pdata->config);
     if (cfg)
     {
         if (cfg->TargetPassword) 
             free(cfg->TargetPassword);
         if (cfg->TargetUser)
             free(cfg->TargetUser);
 		if (cfg->TargetDomain)
 			free(cfg->TargetDomain);
		if (cfg->TargetHost)
			free(cfg->TargetHost);
		/* Set config TargetPort and TargetHost from parsed values */
		cfg->TargetPort = (uint16_t)use_port;
		cfg->TargetDomain = strdup("");
		cfg->TargetHost = strdup(use_host);
        cfg->TargetPassword = strdup("XXXXX"); 
        cfg->TargetUser = strdup("XXXXXXX"); 
     } 

	WLog_INFO(TAG, "Overriding username and password -> %s:%s", cfg->TargetUser, cfg->TargetPassword);
	
    WLog_INFO(TAG, "Overriding target -> %s:%u", mod_event->target_address, mod_event->target_port);

	proxy_send_qr_center_qrcodegen(
    pdata,
    "dummypayload-for-testing-only-primary"
	);

	std::this_thread::sleep_for(std::chrono::seconds(2));

	proxy_send_qr_center_qrcodegen(
    pdata,
    "dummypayload-for-testing-only-second"
	);

	std::this_thread::sleep_for(std::chrono::seconds(2));

    return TRUE; 
}

static BOOL demo_server_peer_logon([[maybe_unused]] proxyPlugin* plugin,
                                   [[maybe_unused]] proxyData* pdata, [[maybe_unused]] void* param)
{
	auto info = static_cast<const proxyServerPeerLogon*>(param);
	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(info);
	WINPR_ASSERT(info->identity);

	WLog_INFO(TAG, "%d", info->automatic);
	return TRUE;
}

static BOOL demo_dyn_channel_intercept_list([[maybe_unused]] proxyPlugin* plugin,
                                            [[maybe_unused]] proxyData* pdata,
                                            [[maybe_unused]] void* arg)
{
	auto data = static_cast<proxyChannelToInterceptData*>(arg);

	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(data);

	WLog_INFO(TAG, "%s: %p", __func__, data);
	return TRUE;
}

static BOOL demo_static_channel_intercept_list([[maybe_unused]] proxyPlugin* plugin,
                                               [[maybe_unused]] proxyData* pdata,
                                               [[maybe_unused]] void* arg)
{
	auto data = static_cast<proxyChannelToInterceptData*>(arg);

	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(data);

	WLog_INFO(TAG, "%s: %p", __func__, data);
	return TRUE;
}

static BOOL demo_dyn_channel_intercept([[maybe_unused]] proxyPlugin* plugin,
                                       [[maybe_unused]] proxyData* pdata,
                                       [[maybe_unused]] void* arg)
{
	auto data = static_cast<proxyDynChannelInterceptData*>(arg);

	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(data);

	WLog_INFO(TAG, "%s: %p", __func__, data);
	return TRUE;
}

static BOOL int_proxy_module_entry_point(proxyPluginsManager* plugins_manager, void* userdata)
{
	struct demo_custom_data* custom = nullptr;
	proxyPlugin plugin = {};

	plugin.name = plugin_name;
	plugin.description = plugin_desc;
	plugin.PluginUnload = demo_plugin_unload;
	plugin.ClientInitConnect = demo_client_init_connect;
	plugin.ClientUninitConnect = demo_client_uninit_connect;
	plugin.ClientPreConnect = demo_client_pre_connect;
	plugin.ClientPostConnect = demo_client_post_connect;
	plugin.ClientPostDisconnect = demo_client_post_disconnect;
	plugin.ClientX509Certificate = demo_client_x509_certificate;
	plugin.ClientLoginFailure = demo_client_login_failure;
	plugin.ClientEndPaint = demo_client_end_paint;
	plugin.ClientRedirect = demo_client_redirect;
	plugin.ServerPostConnect = demo_server_post_connect;
	plugin.ServerPeerActivate = demo_server_peer_activate;
	plugin.ServerChannelsInit = demo_server_channels_init;
	plugin.ServerChannelsFree = demo_server_channels_free;
	plugin.ServerSessionEnd = demo_server_session_end;
	plugin.KeyboardEvent = demo_filter_keyboard_event;
	plugin.UnicodeEvent = demo_filter_unicode_event;
	plugin.MouseEvent = demo_mouse_event;
	plugin.MouseExEvent = demo_mouse_ex_event;
	plugin.ClientChannelData = demo_client_channel_data;
	plugin.ServerChannelData = demo_server_channel_data;
	plugin.DynamicChannelCreate = demo_dynamic_channel_create;
	plugin.ServerFetchTargetAddr = demo_server_fetch_target_addr;
	plugin.ServerPeerLogon = demo_server_peer_logon;

	plugin.StaticChannelToIntercept = demo_static_channel_intercept_list;
	plugin.DynChannelToIntercept = demo_dyn_channel_intercept_list;
	plugin.DynChannelIntercept = demo_dyn_channel_intercept;

	plugin.userdata = userdata;

	custom = new (struct demo_custom_data);
	if (!custom)
		return FALSE;

	custom->mgr = plugins_manager;
	custom->somesetting = 42;

	plugin.custom = custom;
	plugin.userdata = userdata;

	return plugins_manager->RegisterPlugin(plugins_manager, &plugin);
}

#ifdef __cplusplus
extern "C"
{
#endif
#if defined(BUILD_SHARED_LIBS)
	FREERDP_API BOOL proxy_module_entry_point(proxyPluginsManager* plugins_manager, void* userdata);

	BOOL proxy_module_entry_point(proxyPluginsManager* plugins_manager, void* userdata)
	{
		return int_proxy_module_entry_point(plugins_manager, userdata);
	}  
#else
FREERDP_API BOOL demo_proxy_module_entry_point(proxyPluginsManager* plugins_manager,
                                               void* userdata);
BOOL demo_proxy_module_entry_point(proxyPluginsManager* plugins_manager, void* userdata)
{
	return int_proxy_module_entry_point(plugins_manager, userdata);
}
#endif 
#ifdef __cplusplus
}
#endif
