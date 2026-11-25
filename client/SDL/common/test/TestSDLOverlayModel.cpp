#include <array>
#include <cstdio>
#include <string>
#include <vector>

#include <freerdp/channels/rdpgfx.h>

#include "../../SDL3/sdl_overlay_model.hpp"

using sdl::overlay::Names;
using sdl::overlay::Reading;
using sdl::overlay::Row;

/* what AV1's counters are called before the caps version is known */
#if defined(WITH_GFX_AV2)
constexpr const char* av1Unknown = "AV1/AV2";
#else
constexpr const char* av1Unknown = "AV1";
#endif

/* the last name repeats the first: the first one counts */
static std::vector<std::string> statNames()
{
	return {
		"NETWORK_BYTES_RECEIVED",      "NETWORK_BYTES_SENT",     "NETWORK_BANDWIDTH_KBPS",
		"NETWORK_RTT_BASE_MS",         "NETWORK_RTT_AVERAGE_MS", "GFX_CAPS_VERSION",
		"RDPGFX_CODECID_UNKNOWN",      "RDPGFX_CODECID_AVC420",  "RDPGFX_CODECID_AVC420_BYTES",
		"RDPGFX_CMDID_CACHETOSURFACE", "RDPGFX_CODECID_AV1",     "RDPGFX_CODECID_UNKNOWN_BYTES",
		"FREERDP_CLIENT_STATS_UNUSED", "NETWORK_BYTES_RECEIVED",
	};
}

enum
{
	RX,
	TX,
	KBPS,
	RTT_BASE,
	RTT,
	CAPS,
	UNKNOWN,
	AVC420,
	AVC420_BYTES,
	CACHE,
	AV1,
	UNKNOWN_BYTES,
	UNUSED,
	RX_AGAIN,
	COUNT
};

static Reading reading(uint64_t seconds)
{
	Reading r;
	r.ns = seconds * 1000000000ull;
	r.values.resize(COUNT);
	r.desktopWidth = 1920;
	r.desktopHeight = 1080;
	return r;
}

static const Row* find(const std::vector<Row>& rows, const std::string& label)
{
	for (const auto& row : rows)
	{
		if (row.label == label)
			return &row;
	}
	return nullptr;
}

/* the row after the one labelled label */
static const Row* next(const std::vector<Row>& rows, const std::string& label)
{
	for (size_t x = 0; x + 1 < rows.size(); x++)
	{
		if (rows.at(x).label == label)
			return &rows.at(x + 1);
	}
	return nullptr;
}

static bool expect(const Row* row, const std::string& value, int line)
{
	if (!row)
	{
		(void)fprintf(stderr, "line %d: no row, expected '%s'\n", line, value.c_str());
		return false;
	}
	if (row->value != value)
	{
		(void)fprintf(stderr, "line %d: '%s' is '%s', expected '%s'\n", line, row->label.c_str(),
		              row->value.c_str(), value.c_str());
		return false;
	}
	return true;
}

#define EXPECT(row, value)                 \
	if (!expect((row), (value), __LINE__)) \
	return __LINE__
#define CHECK(cond)                                              \
	if (!(cond))                                                 \
	{                                                            \
		(void)fprintf(stderr, "line %d: %s\n", __LINE__, #cond); \
		return __LINE__;                                         \
	}

static int checkNames()
{
	const Names n(statNames());
	auto r = reading(1);
	r.values.at(RX) = 7;
	r.values.at(RX_AGAIN) = 99;
	r.values.at(UNKNOWN) = 5;
	CHECK(n.get(r, "NETWORK_BYTES_RECEIVED").value_or(0) == 7);
	CHECK(!n.has("RDPGFX_CODECID_UNKNOWN"));
	CHECK(!n.has("RDPGFX_CODECID_UNKNOWN_BYTES"));
	CHECK(!n.has("FREERDP_CLIENT_STATS_UNUSED"));
	CHECK(!n.get(r, "NO_SUCH_NAME"));

	/* a reading shorter than the names, as from another client */
	Reading shorter;
	shorter.values.resize(2);
	CHECK(!n.get(shorter, "NETWORK_RTT_AVERAGE_MS"));
	return 0;
}

static int checkRates()
{
	const Names n(statNames());
	auto prev = reading(10);
	auto now = reading(12);
	prev.values.at(RX) = 1000000;
	now.values.at(RX) = 1500000; /* 250000 bytes a second: 2000 kbit/s */
	now.values.at(TX) = 2500000;
	prev.values.at(AVC420) = 100;
	now.values.at(AVC420) = 160;
	prev.values.at(AVC420_BYTES) = 0;
	now.values.at(AVC420_BYTES) = 200000;
	now.values.at(CACHE) = 8;
	prev.values.at(CACHE) = 0;

	const sdl::overlay::View view{ 1280, 720, 125 };
	const auto rows = sdl::overlay::realtimeRows(n, now, &prev, view);
	EXPECT(find(rows, "Resolution"), "1920 × 1080, window 1280 × 720");
	EXPECT(find(rows, "Network in (RDP)"), "2000 kbit/s, 1.5 MB this connection");
	/* without TX in prev: a counter that went up from 0 */
	EXPECT(find(rows, "Network out (RDP)"), "10000 kbit/s, 2.5 MB this connection");
	EXPECT(find(rows, "Video codecs"), "H.264 4:2:0 (AVC420) 30/s 800 kbit/s");
	EXPECT(next(rows, "Video codecs"), "CacheToSurface 4/s");
	EXPECT(find(rows, "Session"), "2:05");

	/* no earlier reading: no rate */
	const auto first = sdl::overlay::realtimeRows(n, now, nullptr, view);
	EXPECT(find(first, "Network in (RDP)"), "– kbit/s, 1.5 MB this connection");
	EXPECT(find(first, "Video codecs"), "H.264 4:2:0 (AVC420) –/s – kbit/s");

	/* a counter that went down is a new channel, never a negative rate */
	prev.values.at(AVC420) = 500;
	const auto reset = sdl::overlay::realtimeRows(n, now, &prev, view);
	EXPECT(find(reset, "Video codecs"), "H.264 4:2:0 (AVC420) –/s 800 kbit/s");
	return 0;
}

static int checkLabels()
{
	const Names n(statNames());
	auto now = reading(1);
	now.values.at(AV1) = 1;

	/* no caps version yet: AV1's counters can be AV2's */
	auto rows = sdl::overlay::realtimeRows(n, now, nullptr, {});
	EXPECT(find(rows, "Video codecs"), std::string(av1Unknown) + " –/s – kbit/s");

#if defined(WITH_GFX_AV2)
	now.values.at(CAPS) = RDPGFX_CAPVERSION_FRDP_2;
	rows = sdl::overlay::realtimeRows(n, now, nullptr, {});
	EXPECT(find(rows, "Video codecs"), "AV2 –/s – kbit/s");
#endif

	/* any other caps version: AV1 */
	now.values.at(CAPS) = RDPGFX_CAPVERSION_107;
	rows = sdl::overlay::realtimeRows(n, now, nullptr, {});
	EXPECT(find(rows, "Video codecs"), "AV1 –/s – kbit/s");

	/* nothing decoded: no codec rows */
	const auto empty = sdl::overlay::realtimeRows(n, reading(1), nullptr, {});
	EXPECT(find(empty, "Video codecs"), "no updates yet");
	return 0;
}

static int checkNetwork()
{
	const Names n(statNames());
	auto now = reading(1);
	auto rows = sdl::overlay::realtimeRows(n, now, nullptr, {});
	EXPECT(find(rows, "Round trip"), "not measured");
	EXPECT(find(rows, "Bandwidth"), "not measured");

	now.values.at(RTT) = 23;
	now.values.at(RTT_BASE) = 11;
	now.values.at(KBPS) = 54000;
	rows = sdl::overlay::realtimeRows(n, now, nullptr, {});
	EXPECT(find(rows, "Round trip"), "23 ms (lowest 11)");
	EXPECT(find(rows, "Bandwidth"), "54000 kbit/s");

	/* a client without the graphics channel's statistics: no codec row at all */
	const Names few({ "NETWORK_BYTES_RECEIVED" });
	rows = sdl::overlay::realtimeRows(few, reading(1), nullptr, {});
	CHECK(!find(rows, "Video codecs"));
	CHECK(!find(rows, "Round trip"));
	return 0;
}

static int checkClient()
{
	const char* config = "Build configuration: WITH_A=ON WITH_B=OFF\n"
	                     "Build type:          Release\n"
	                     "CFLAGS:              -O2\n"
	                     "Compiler:            GNU, 16.2.0\n"
	                     "Target architecture: x86_64\n";
	CHECK(sdl::overlay::buildSummary(config) == "Release, GNU, 16.2.0, x86_64");
	CHECK(sdl::overlay::buildSummary(nullptr).empty());
	return 0;
}
static bool overlap(const sdl::overlay::Box& a, const sdl::overlay::Box& b)
{
	return (a.x < b.x + b.w) && (b.x < a.x + a.w) && (a.y < b.y + b.h) && (b.y < a.y + a.h);
}

static int checkLayout()
{
	/* both fit side by side: the right part at the top right */
	auto l = sdl::overlay::layout(400, 100, 300, 200, 1280, 12);
	CHECK(l.left.x == 12);
	CHECK(l.left.y == 12);
	CHECK(l.right.x == 1280 - 12 - 300);
	CHECK(l.right.y == 12);

	/* the 1000 px window of the rig: the right part goes under the left one */
	l = sdl::overlay::layout(590, 110, 480, 200, 1000, 12);
	CHECK(l.right.y == 12 + 110 + 12);
	CHECK(l.right.x == 1000 - 12 - 480);

	/* narrower than the right part: it starts at the left edge, still under the left one */
	l = sdl::overlay::layout(590, 110, 480, 200, 400, 12);
	CHECK(l.right.x == 0);
	CHECK(l.right.y == 134);

	/* no client part: the right part stays at the top */
	l = sdl::overlay::layout(0, 0, 480, 200, 400, 12);
	CHECK(l.right.y == 12);

	/* at any width the two never cover each other */
	for (int w = 50; w <= 3000; w += 7)
	{
		const auto width = static_cast<float>(w);
		for (const auto& sizes : { std::array<float, 4>{ 590, 110, 480, 200 },
		                           std::array<float, 4>{ 100, 300, 900, 50 },
		                           std::array<float, 4>{ 700, 90, 700, 400 } })
		{
			l = sdl::overlay::layout(sizes.at(0), sizes.at(1), sizes.at(2), sizes.at(3), width, 12);
			if (overlap(l.left, l.right))
			{
				(void)fprintf(stderr, "overlap at width %.0f\n", static_cast<double>(width));
				return __LINE__;
			}
		}
	}
	return 0;
}

int TestSDLOverlayModel([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
	for (auto* check :
	     { checkNames, checkRates, checkLabels, checkNetwork, checkClient, checkLayout })
	{
		const int rc = check();
		if (rc != 0)
			return rc;
	}
	return 0;
}
