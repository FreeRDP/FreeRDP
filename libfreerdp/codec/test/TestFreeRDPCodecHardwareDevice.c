/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * H.264 hardware device selection tests
 *
 * Copyright 2026 Daniel Nylander
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
#include <freerdp/config.h>
#include <glob.h>
#include <libavutil/hwcontext.h>

static int test_glob(const char*, int, int (*)(const char*, int), glob_t*);
static void test_globfree(glob_t*);
static int test_device_create(AVBufferRef**, enum AVHWDeviceType, const char*, AVDictionary*, int);
#define glob test_glob
#define globfree test_globfree
#define av_hwdevice_ctx_create test_device_create
#include "../h264_ffmpeg.c"
#undef glob
#undef globfree
#undef av_hwdevice_ctx_create

static size_t attempts;
static size_t success_at;
static int scan_status;
static BOOL scanned;
static const char* expected_device;
static BOOL unexpected_device;
static char* nodes[] = { "/dev/dri/renderD128", "/dev/dri/renderD130", "/dev/dri/renderD135" };

static int test_glob(const char* pattern, int flags, int (*errfunc)(const char*, int),
                     glob_t* paths)
{
	WINPR_UNUSED(pattern);
	WINPR_UNUSED(flags);
	WINPR_UNUSED(errfunc);
	scanned = TRUE;
	if (scan_status == 0)
	{
		paths->gl_pathc = ARRAYSIZE(nodes);
		paths->gl_pathv = nodes;
	}
	return scan_status;
}
static void test_globfree(glob_t* paths)
{
	WINPR_UNUSED(paths);
}
static int test_device_create(AVBufferRef** ctx, enum AVHWDeviceType type, const char* device,
                              AVDictionary* opts, int flags)
{
	WINPR_UNUSED(type);
	WINPR_UNUSED(opts);
	WINPR_UNUSED(flags);
	const char* expected = expected_device;
	if (scanned && attempts < ARRAYSIZE(nodes))
		expected = nodes[attempts];
	if ((!device != !expected) || (device && expected && strcmp(device, expected) != 0))
		unexpected_device = TRUE;
	attempts++;
	if (attempts != success_at)
		return AVERROR(ENODEV);
	*ctx = av_buffer_alloc(1);
	return *ctx ? 0 : AVERROR(ENOMEM);
}
static BOOL check_device(const char* device, enum AVHWDeviceType type, size_t success,
                         int scan_result, size_t expected_attempts, BOOL expected_scan)
{
	H264_CONTEXT_LIBAVCODEC sys = { 0 };
	sys.type = type;
	sys.device = device ? _strdup(device) : nullptr;
	attempts = 0;
	success_at = success;
	scan_status = scan_result;
	scanned = FALSE;
	unexpected_device = FALSE;
	expected_device = (type == AV_HWDEVICE_TYPE_VAAPI) ? device : nullptr;
	const int rc = create_hwctx(&sys);
	BOOL ok = (attempts == expected_attempts) && (scanned == expected_scan) && !unexpected_device;
	ok = ok && ((rc == 0) == (success > 0));
	if (ok && success && expected_scan)
		ok = sys.device && strcmp(sys.device, nodes[success - 1]) == 0;
	if (!ok)
		fprintf(stderr, "device selection failed: attempts=%zu scan=%d rc=%d\n", attempts, scanned,
		        rc);
	free(sys.device);
	av_buffer_unref(&sys.hwctx);
	return ok;
}
int main(void)
{
	if (!check_device(nullptr, AV_HWDEVICE_TYPE_VAAPI, 2, 0, 2, TRUE) ||
	    !check_device(nullptr, AV_HWDEVICE_TYPE_VAAPI, 0, 0, 3, TRUE) ||
	    !check_device(nullptr, AV_HWDEVICE_TYPE_VAAPI, 0, GLOB_NOMATCH, 0, TRUE) ||
	    !check_device(nullptr, AV_HWDEVICE_TYPE_VAAPI, 0, GLOB_NOSPACE, 0, TRUE) ||
	    !check_device("/dev/dri/renderD130", AV_HWDEVICE_TYPE_VAAPI, 1, 0, 1, FALSE) ||
	    !check_device("/dev/dri/renderD130", AV_HWDEVICE_TYPE_VAAPI, 0, 0, 1, FALSE) ||
	    !check_device(nullptr, AV_HWDEVICE_TYPE_VULKAN, 1, 0, 1, FALSE))
		return 1;
	return 0;
}
