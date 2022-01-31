/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Static Virtual Channel Interface
 *
 * Copyright 2021 Armin Novak
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

#include <winpr/sysinfo.h>
#include <winpr/path.h>
#include <winpr/string.h>

#include <freerdp/streamdump.h>
#include <freerdp/transport_io.h>

struct stream_dump_context
{
	rdpTransportIo io;
	size_t writeDumpOffset;
	size_t readDumpOffset;
	size_t replayOffset;
	UINT64 replayTime;
	CONNECTION_STATE state;
};

BOOL stream_dump_read_line(FILE* fp, wStream* s, UINT64* pts, size_t* pOffset)
{
	BOOL rc = FALSE;
	UINT64 ts;
	UINT64 size = 0;
	size_t r;

	if (!fp || !s)
		return FALSE;

	if (pOffset)
		_fseeki64(fp, *pOffset, SEEK_SET);

	r = fread(&ts, 1, sizeof(ts), fp);
	if (r != sizeof(ts))
		goto fail;
	r = fread(&size, 1, sizeof(size), fp);
	if (r != sizeof(size))
		goto fail;
	if (!Stream_EnsureRemainingCapacity(s, size))
		goto fail;
	r = fread(Stream_Pointer(s), 1, size, fp);
	if (r != size)
		goto fail;
	Stream_Seek(s, size);

	if (pOffset)
	{
		INT64 tmp = _ftelli64(fp);
		if (tmp < 0)
			goto fail;
		*pOffset = (size_t)tmp;
	}

	if (pts)
		*pts = ts;
	rc = TRUE;

fail:
	Stream_SealLength(s);
	return rc;
}

BOOL stream_dump_write_line(FILE* fp, wStream* s)
{
	BOOL rc = FALSE;
	const UINT64 t = GetTickCount64();
	const BYTE* data = Stream_Buffer(s);
	const UINT64 size = Stream_Length(s);

	if (!fp || !s)
		return FALSE;

	{
		size_t r = fwrite(&t, 1, sizeof(t), fp);
		if (r != sizeof(t))
			goto fail;
		r = fwrite(&size, 1, sizeof(size), fp);
		if (r != sizeof(size))
			goto fail;
		r = fwrite(data, 1, size, fp);
		if (r != size)
			goto fail;
	}

	rc = TRUE;
fail:
	return rc;
}

static FILE* stream_dump_get_file(const rdpSettings* settings, const char* name, const char* mode)
{
	const char* cfolder;
	char* folder = NULL;
	char* file = NULL;
	FILE* fp = NULL;

	if (!settings || !mode)
		return NULL;

	cfolder = freerdp_settings_get_string(settings, FreeRDP_TransportDumpFile);
	if (!cfolder)
		folder = GetKnownSubPath(KNOWN_PATH_TEMP, "freerdp-transport-dump");
	else
		folder = _strdup(cfolder);

	if (!folder)
		goto fail;

	if (name)
	{
		char buffer[8192] = { 0 };
		int rc = _snprintf(buffer, sizeof(buffer), "%s.dump", name);
		if ((rc <= 0) || ((size_t)rc >= sizeof(buffer)))
			goto fail;

		if (!winpr_PathFileExists(folder))
		{
			if (!winpr_PathMakePath(folder, NULL))
				goto fail;
		}
		file = GetCombinedPath(folder, buffer);
	}
	else
	{
		if (!winpr_PathFileExists(folder))
			goto fail;
		file = _strdup(folder);
	}

	if (!file)
		goto fail;

	fp = winpr_fopen(file, mode);
fail:
	free(folder);
	free(file);
	return fp;
}

SSIZE_T stream_dump_append(const rdpContext* context, const char* name, wStream* s, size_t* offset)
{
	SSIZE_T rc = -1;
	FILE* fp;
	CONNECTION_STATE state = freerdp_get_state(context);
	int r;

	if (!context || !s || !offset)
		return -1;

	if (state < context->dump->state)
		return 0;

	fp = stream_dump_get_file(context->settings, name, "ab");
	if (!fp)
		return -1;

	r = _fseeki64(fp, *offset, SEEK_SET);
	if (r < 0)
		goto fail;

	if (!stream_dump_write_line(fp, s))
		goto fail;
	rc = _ftelli64(fp);
	if (rc < 0)
		goto fail;
	*offset = (size_t)rc;
fail:
	fclose(fp);
	return rc;
}

SSIZE_T stream_dump_get(const rdpContext* context, const char* name, wStream* s, size_t* offset,
                        UINT64* pts)
{
	SSIZE_T rc = -1;
	FILE* fp;
	int r;

	if (!context || !s || !offset)
		return -1;
	fp = stream_dump_get_file(context->settings, name, "rb");
	if (!fp)
		return -1;
	r = _fseeki64(fp, *offset, SEEK_SET);
	if (r < 0)
		goto fail;

	if (!stream_dump_read_line(fp, s, pts, offset))
		goto fail;

	rc = _ftelli64(fp);
fail:
	fclose(fp);
	return rc;
}

static int stream_dump_transport_write(rdpTransport* transport, wStream* s)
{
	SSIZE_T r;
	rdpContext* ctx = transport_get_context(transport);

	WINPR_ASSERT(ctx);
	WINPR_ASSERT(ctx->dump);
	WINPR_ASSERT(s);

	r = stream_dump_append(ctx, "write", s, &ctx->dump->writeDumpOffset);
	if (r < 0)
		return -1;

	WINPR_ASSERT(ctx->dump->io.WritePdu);
	return ctx->dump->io.WritePdu(transport, s);
}

static int stream_dump_transport_read(rdpTransport* transport, wStream* s)
{
	int rc;
	rdpContext* ctx = transport_get_context(transport);

	WINPR_ASSERT(ctx);
	WINPR_ASSERT(ctx->dump);
	WINPR_ASSERT(s);

	WINPR_ASSERT(ctx->dump->io.ReadPdu);
	rc = ctx->dump->io.ReadPdu(transport, s);
	if (rc > 0)
	{
		SSIZE_T r = stream_dump_append(ctx, "read", s, &ctx->dump->readDumpOffset);
		if (r < 0)
			return -1;
	}
	return rc;
}

static BOOL stream_dump_register_write_handlers(rdpContext* context)
{
	rdpTransportIo dump;
	const rdpTransportIo* dfl = freerdp_get_io_callbacks(context);

	if (!freerdp_settings_get_bool(context->settings, FreeRDP_TransportDump))
		return TRUE;

	WINPR_ASSERT(dfl);
	dump = *dfl;

	/* Remember original callbacks for later */
	WINPR_ASSERT(context->dump);
	context->dump->io.ReadPdu = dfl->ReadPdu;
	context->dump->io.WritePdu = dfl->WritePdu;

	/* Set our dump wrappers */
	dump.WritePdu = stream_dump_transport_write;
	dump.ReadPdu = stream_dump_transport_read;
	return freerdp_set_io_callbacks(context, &dump);
}

static int stream_dump_replay_transport_write(rdpTransport* transport, wStream* s)
{
	rdpContext* ctx = transport_get_context(transport);
	size_t size;

	WINPR_ASSERT(ctx);
	WINPR_ASSERT(s);

	size = Stream_Length(s);
	WLog_ERR("abc", "replay write %" PRIuz, size);
	// TODO: Compare with write file

	return 1;
}

static int stream_dump_replay_transport_read(rdpTransport* transport, wStream* s)
{
	rdpContext* ctx = transport_get_context(transport);

	size_t size = 0;
	time_t slp = 0;
	UINT64 ts = 0;

	WINPR_ASSERT(ctx);
	WINPR_ASSERT(ctx->dump);
	WINPR_ASSERT(s);

	if (stream_dump_get(ctx, NULL, s, &ctx->dump->replayOffset, &ts) < 0)
		return -1;

	if ((ctx->dump->replayTime > 0) && (ts > ctx->dump->replayTime))
		slp = ts - ctx->dump->replayTime;
	ctx->dump->replayTime = ts;

	size = Stream_Length(s);
	Stream_SetPosition(s, 0);
	WLog_ERR("abc", "replay read %" PRIuz, size);

	if (slp > 0)
		Sleep(slp);

	return 1;
}

static int stream_dump_replay_transport_tcp_connect(rdpContext* context, rdpSettings* settings,
                                                    const char* hostname, int port, DWORD timeout)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(settings);
	WINPR_ASSERT(hostname);

	return 42;
}

static BOOL stream_dump_replay_transport_tls_connect(rdpTransport* transport)
{
	WINPR_ASSERT(transport);
	return TRUE;
}

static BOOL stream_dump_replay_transport_accept(rdpTransport* transport)
{
	WINPR_ASSERT(transport);
	return TRUE;
}

static BOOL stream_dump_register_read_handlers(rdpContext* context)
{
	rdpTransportIo dump;
	const rdpTransportIo* dfl = freerdp_get_io_callbacks(context);

	if (!freerdp_settings_get_bool(context->settings, FreeRDP_TransportDumpReplay))
		return TRUE;

	WINPR_ASSERT(dfl);
	dump = *dfl;

	/* Remember original callbacks for later */
	WINPR_ASSERT(context->dump);
	context->dump->io.ReadPdu = dfl->ReadPdu;
	context->dump->io.WritePdu = dfl->WritePdu;

	/* Set our dump wrappers */
	dump.WritePdu = stream_dump_transport_write;
	dump.ReadPdu = stream_dump_transport_read;

	/* Set our dump wrappers */
	dump.WritePdu = stream_dump_replay_transport_write;
	dump.ReadPdu = stream_dump_replay_transport_read;
	dump.TCPConnect = stream_dump_replay_transport_tcp_connect;
	dump.TLSAccept = stream_dump_replay_transport_accept;
	dump.TLSConnect = stream_dump_replay_transport_tls_connect;
	return freerdp_set_io_callbacks(context, &dump);
}

BOOL stream_dump_register_handlers(rdpContext* context, CONNECTION_STATE state)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->dump);
	context->dump->state = state;
	if (!stream_dump_register_write_handlers(context))
		return FALSE;
	return stream_dump_register_read_handlers(context);
}

void stream_dump_free(rdpStreamDumpContext* dump)
{
	free(dump);
}

rdpStreamDumpContext* stream_dump_new(void)
{
	rdpStreamDumpContext* dump = calloc(1, sizeof(rdpStreamDumpContext));
	if (!dump)
		return NULL;

	return dump;
}
