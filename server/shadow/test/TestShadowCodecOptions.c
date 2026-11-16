#include <stdio.h>
#include <string.h>

#include <winpr/wtypes.h>
#include <winpr/cmdline.h>
#include <freerdp/server/shadow.h>

/* The option as freerdp-shadow-cli has it (server/shadow/cli/shadow.c): the table belongs to the
 * program, so this tests the parsing of the option, not the program's table. */
static COMMAND_LINE_ARGUMENT_A options[] = {
	{ "codec", COMMAND_LINE_VALUE_REQUIRED, "<codec>:<key>:<value>[,<codec>:<key>:<value>...]",
	  nullptr, nullptr, -1, nullptr, "Encoder settings" },
	{ nullptr, 0, nullptr, nullptr, nullptr, -1, nullptr, nullptr }
};

/* The encoder fields of rdpShadowServer, to compare */
typedef struct
{
	H264_RATECONTROL_MODE h264Mode;
	UINT32 h264BitRate;
	UINT32 h264QP;
#if defined(WITH_GFX_AV1)
	FREERDP_AV1_RATECONTROL av1Mode;
	UINT32 av1BitRate;
#endif
#if defined(WITH_GFX_AV2)
	FREERDP_AV2_RATECONTROL av2Mode;
	UINT32 av2BitRate;
#endif
#if defined(WITH_GFX_AZURE)
	FREERDP_HEVC_RATECONTROL hevcMode;
	UINT32 hevcBitRate;
#endif
} FIELDS;

static void get_fields(const rdpShadowServer* server, FIELDS* fields)
{
	memset(fields, 0, sizeof(*fields));
	fields->h264Mode = server->h264RateControlMode;
	fields->h264BitRate = server->h264BitRate;
	fields->h264QP = server->h264QP;
#if defined(WITH_GFX_AV1)
	fields->av1Mode = server->AV1RateControlMode;
	fields->av1BitRate = server->AV1BitRate;
#endif
#if defined(WITH_GFX_AV2)
	fields->av2Mode = server->AV2RateControlMode;
	fields->av2BitRate = server->AV2BitRate;
#endif
#if defined(WITH_GFX_AZURE)
	fields->hevcMode = server->HEVCRateControlMode;
	fields->hevcBitRate = server->HEVCBitRate;
#endif
}

/* Parses option on a new server. Accepted: the fields must be the defaults changed by change().
 * Refused: the command line must fail and every field keep its default. */
WINPR_ATTR_NODISCARD
static BOOL expect(const char* what, const char* option, BOOL accepted, void (*change)(FIELDS*))
{
	char* argv[] = { "freerdp-shadow-cli", (char*)option, nullptr };
	const int argc = option ? 2 : 1;

	rdpShadowServer* server = shadow_server_new();
	if (!server)
	{
		(void)fprintf(stderr, "%s: shadow_server_new failed\n", what);
		return FALSE;
	}

	FIELDS expected = { 0 };
	get_fields(server, &expected);
	if (accepted && change)
		change(&expected);

	const int status = shadow_server_parse_command_line(server, argc, argv, options);
	FIELDS got = { 0 };
	get_fields(server, &got);
	shadow_server_free(server);

	if ((status >= 0) != accepted)
	{
		(void)fprintf(stderr, "%s: status %d, expected it %s\n", what, status,
		              accepted ? "accepted" : "refused");
		return FALSE;
	}
	if (memcmp(&got, &expected, sizeof(got)) != 0)
	{
		(void)fprintf(stderr,
		              "%s: the fields are not as expected (h264 mode %d bit rate %" PRIu32
		              " qp %" PRIu32 ")\n",
		              what, (int)got.h264Mode, got.h264BitRate, got.h264QP);
		return FALSE;
	}
	return TRUE;
}

static void h264_vbr_4000(FIELDS* f)
{
	f->h264Mode = H264_RATECONTROL_VBR;
	f->h264BitRate = 4000000;
}

static void h264_cqp_26(FIELDS* f)
{
	f->h264Mode = H264_RATECONTROL_CQP;
	f->h264QP = 26;
}

static void h264_limits(FIELDS* f)
{
	f->h264BitRate = 1000;
	f->h264QP = 51;
}

#if defined(WITH_GFX_AV1)
static void av1_cq_5000(FIELDS* f)
{
	f->av1Mode = FREERDP_AV1_CQ;
	f->av1BitRate = 5000;
}
#endif

#if defined(WITH_GFX_AV2)
static void av2_q_800(FIELDS* f)
{
	f->av2Mode = FREERDP_AV2_Q;
	f->av2BitRate = 800;
}
#endif

#if defined(WITH_GFX_AZURE)
static void hevc_vbr_2000(FIELDS* f)
{
	f->hevcMode = FREERDP_HEVC_VBR;
	f->hevcBitRate = 2000000;
}
#endif

static void mixed(FIELDS* f)
{
	h264_cqp_26(f);
#if defined(WITH_GFX_AV1)
	f->av1BitRate = 3500;
#endif
}

int TestShadowCodecOptions(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	BOOL ok = TRUE;

	/* none: every default stays */
	ok &= expect("none", nullptr, TRUE, nullptr);

	/* h264: bit rates in kbit/s, h264BitRate in bit/s */
	ok &= expect("h264 vbr 4000", "/codec:h264:rate-control:vbr,h264:bitrate:4000", TRUE,
	             h264_vbr_4000);
	ok &= expect("h264 cqp 26", "/codec:h264:rate-control:cqp,h264:qp:26", TRUE, h264_cqp_26);
	ok &= expect("h264 limits", "/codec:h264:bitrate:1,h264:qp:51", TRUE, h264_limits);
	ok &= expect("h264 qp 52", "/codec:h264:qp:52", FALSE, nullptr);
	ok &= expect("h264 qp -1", "/codec:h264:qp:-1", FALSE, nullptr);
	ok &= expect("h264 cbr", "/codec:h264:rate-control:cbr", FALSE, nullptr);
	ok &= expect("h264 bit rate 4M", "/codec:h264:bitrate:4M", FALSE, nullptr);
	ok &= expect("h264 bit rate 0", "/codec:h264:bitrate:0", FALSE, nullptr);
	ok &= expect("h264 bit rate too high", "/codec:h264:bitrate:1000001", FALSE, nullptr);
	ok &= expect("h264 unknown key", "/codec:h264:fps:60", FALSE, nullptr);

	/* the whole value or nothing: a wrong part keeps the right ones from being set */
	ok &= expect("one wrong part", "/codec:h264:rate-control:cqp,h264:qp:99", FALSE, nullptr);
	ok &= expect("unknown codec", "/codec:vp9:bitrate:1000", FALSE, nullptr);
	ok &= expect("no key", "/codec:h264", FALSE, nullptr);
	ok &= expect("no value", "/codec", FALSE, nullptr);

#if defined(WITH_GFX_AV1)
	ok &= expect("av1 cq 5000", "/codec:av1:rate-control:cq,av1:bitrate:5000", TRUE, av1_cq_5000);
	ok &= expect("av1 bad mode", "/codec:av1:rate-control:cqp", FALSE, nullptr);
	ok &= expect("av1 qp", "/codec:av1:qp:20", FALSE, nullptr);
	ok &= expect("mixed", "/codec:h264:rate-control:cqp,h264:qp:26,av1:bitrate:3500", TRUE, mixed);
#else
	ok &= expect("av1 not built in", "/codec:av1:bitrate:3000", FALSE, nullptr);
	ok &= expect("mixed", "/codec:h264:rate-control:cqp,h264:qp:26", TRUE, mixed);
#endif

#if defined(WITH_GFX_AV2)
	ok &= expect("av2 q 800", "/codec:av2:rate-control:q,av2:bitrate:800", TRUE, av2_q_800);
	ok &= expect("av2 bad mode", "/codec:av2:rate-control:fast", FALSE, nullptr);
#else
	ok &= expect("av2 not built in", "/codec:av2:bitrate:500", FALSE, nullptr);
#endif

#if defined(WITH_GFX_AZURE)
	ok &= expect("hevc vbr 2000", "/codec:hevc:rate-control:vbr,hevc:bitrate:2000", TRUE,
	             hevc_vbr_2000);
	ok &= expect("hevc cbr", "/codec:hevc:rate-control:cbr", FALSE, nullptr);
#else
	ok &= expect("hevc not built in", "/codec:hevc:bitrate:500", FALSE, nullptr);
#endif

	return ok ? 0 : -1;
}
