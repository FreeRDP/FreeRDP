/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client: what the stats and info overlay shows
 *
 * Copyright 2026 Cong Zhang <congzhangzh@gmail.com>
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

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <sstream>

#include <freerdp/channels/rdpgfx.h>

#include "sdl_overlay_model.hpp"

namespace sdl::overlay
{
	namespace
	{
		struct Label
		{
			const char* id;
			const char* label;
		};

		/* by codec ID, as the web client's Info card names them (codec-stats.js) */
		constexpr std::array<Label, 13> codecs = { {
			{ "UNCOMPRESSED", "Uncompressed" },
			{ "AV1", "AV1" },
			{ "CAVIDEO", "RemoteFX" },
			{ "CLEARCODEC", "ClearCodec" },
			{ "CAPROGRESSIVE", "Progressive" },
			{ "PLANAR", "Planar" },
			{ "AVC420", "H.264 4:2:0 (AVC420)" },
			{ "ALPHA", "Alpha" },
			{ "AVC444", "H.264 4:4:4 (AVC444)" },
			{ "AVC444v2", "H.264 4:4:4 (AVC444v2)" },
			{ "CAPROGRESSIVE_V2", "Progressive v2" },
			{ "HEVC420", "HEVC 4:2:0" },
			{ "HEVC444", "HEVC 4:4:4" },
		} };

		/* paints that carry no codec */
		constexpr std::array<Label, 3> paints = { {
			{ "RDPGFX_CMDID_SOLIDFILL", "SolidFill" },
			{ "RDPGFX_CMDID_SURFACETOSURFACE", "SurfaceToSurface" },
			{ "RDPGFX_CMDID_CACHETOSURFACE", "CacheToSurface" },
		} };

		constexpr const char* codecPrefix = "RDPGFX_CODECID_";
	} // namespace

	static bool startsWith(const std::string& s, const char* prefix)
	{
		return s.compare(0, strlen(prefix), prefix) == 0;
	}

	static std::string fixed(double v, int precision)
	{
		std::array<char, 64> buffer{};
		(void)snprintf(buffer.data(), buffer.size(), "%.*f", precision, v);
		return buffer.data();
	}

	/* per second since prev; nothing without prev, or for a counter that went down (a new
	 * channel after a reconnect) */
	static std::optional<double> rate(const Names& names, const Reading& now, const Reading* prev,
	                                  const std::string& name)
	{
		if (!prev || (now.ns <= prev->ns))
			return std::nullopt;
		const auto a = names.get(now, name);
		const auto b = names.get(*prev, name);
		if (!a || !b || (*a < *b))
			return std::nullopt;
		const double dt = static_cast<double>(now.ns - prev->ns) / 1e9;
		return static_cast<double>(*a - *b) / dt;
	}

	static std::string per(const std::optional<double>& v, int precision = 1)
	{
		return v ? fixed(*v, precision) : "–";
	}

	/* AV1's counters count AV2 when the server confirmed RDPGFX_CAPVERSION_FRDP_2; without
	 * the caps version the label says both */
	static const char* av1Label(const Names& names, const Reading& now)
	{
#if defined(WITH_GFX_AV2)
		const auto caps = names.get(now, "GFX_CAPS_VERSION").value_or(0);
		if (caps == 0)
			return "AV1/AV2";
		return (caps == RDPGFX_CAPVERSION_FRDP_2) ? "AV2" : "AV1";
#else
		/* a build without AV2 never confirms it */
		WINPR_UNUSED(names);
		WINPR_UNUSED(now);
		return "AV1";
#endif
	}

	static void videoRows(std::vector<Row>& rows, const Names& names, const Reading& now,
	                      const Reading* prev)
	{
		/* without the graphics channel's statistics the row hides */
		if (!names.has(std::string(codecPrefix) + "AVC420"))
			return;

		std::vector<std::string> parts;
		for (const auto& c : codecs)
		{
			const std::string name = std::string(codecPrefix) + c.id;
			if (names.get(now, name).value_or(0) == 0)
				continue;
			const std::string label = (strcmp(c.id, "AV1") == 0) ? av1Label(names, now) : c.label;
			const auto bytes = rate(names, now, prev, name + "_BYTES");
			const auto kbit = bytes ? std::optional<double>(*bytes * 8.0 / 1000.0) : std::nullopt;
			parts.push_back(label + " " + per(rate(names, now, prev, name), 0) + "/s " +
			                per(kbit, 0) + " kbit/s");
		}
		for (const auto& p : paints)
		{
			if (names.get(now, p.id).value_or(0) == 0)
				continue;
			parts.push_back(std::string(p.label) + " " + per(rate(names, now, prev, p.id), 0) +
			                "/s");
		}

		if (parts.empty())
		{
			rows.push_back({ "Video codecs", "no updates yet" });
			return;
		}
		for (size_t x = 0; x < parts.size(); x++)
			rows.push_back({ (x == 0) ? "Video codecs" : "", parts.at(x) });
	}

	static void networkRows(std::vector<Row>& rows, const Names& names, const Reading& now,
	                        const Reading* prev)
	{
		const auto bytesRow = [&](const char* label, const char* name)
		{
			const auto total = names.get(now, name);
			if (!total)
				return;
			const auto bytes = rate(names, now, prev, name);
			const auto kbit = bytes ? std::optional<double>(*bytes * 8.0 / 1000.0) : std::nullopt;
			rows.push_back({ label, per(kbit, 0) + " kbit/s, " +
			                            fixed(static_cast<double>(*total) / 1e6, 1) +
			                            " MB this connection" });
		};
		bytesRow("Network in (RDP)", "NETWORK_BYTES_RECEIVED");
		bytesRow("Network out (RDP)", "NETWORK_BYTES_SENT");

		/* the server's own measurements, 0 until it makes them: not every server does */
		if (const auto rtt = names.get(now, "NETWORK_RTT_AVERAGE_MS"))
		{
			const auto base = names.get(now, "NETWORK_RTT_BASE_MS").value_or(0);
			rows.push_back({ "Round trip", (*rtt != 0) ? std::to_string(*rtt) + " ms (lowest " +
			                                                 std::to_string(base) + ")"
			                                           : "not measured" });
		}
		if (const auto kbps = names.get(now, "NETWORK_BANDWIDTH_KBPS"))
			rows.push_back(
			    { "Bandwidth", (*kbps != 0) ? std::to_string(*kbps) + " kbit/s" : "not measured" });
	}
	Names::Names(const std::vector<std::string>& names)
	{
		for (size_t x = 0; x < names.size(); x++)
		{
			const auto& name = names.at(x);
			if (name.empty() || (name == "FREERDP_CLIENT_STATS_UNUSED") ||
			    startsWith(name, "RDPGFX_CODECID_UNKNOWN") ||
			    startsWith(name, "RDPGFX_STATS_UNUSED"))
				continue;
			_index.emplace(name, x);
		}
	}

	std::optional<uint64_t> Names::get(const Reading& r, const std::string& name) const
	{
		const auto it = _index.find(name);
		if ((it == _index.end()) || (it->second >= r.values.size()))
			return std::nullopt;
		return r.values.at(it->second);
	}

	bool Names::has(const std::string& name) const
	{
		return _index.find(name) != _index.end();
	}

	std::vector<Row> realtimeRows(const Names& names, const Reading& now, const Reading* prev,
	                              const View& view)
	{
		std::vector<Row> rows;
		rows.push_back({ "Resolution", std::to_string(now.desktopWidth) + " × " +
		                                   std::to_string(now.desktopHeight) + ", window " +
		                                   std::to_string(view.windowWidth) + " × " +
		                                   std::to_string(view.windowHeight) });
		videoRows(rows, names, now, prev);
		networkRows(rows, names, now, prev);

		std::array<char, 32> session{};
		(void)snprintf(session.data(), session.size(), "%" PRIu64 ":%02" PRIu64,
		               view.sessionSeconds / 60, view.sessionSeconds % 60);
		rows.push_back({ "Session", session.data() });
		return rows;
	}

	Layout layout(float leftWidth, float leftHeight, float rightWidth, float rightHeight,
	              float width, float margin)
	{
		Layout l;
		l.left = { margin, margin, leftWidth, leftHeight };
		l.right = { std::max(0.0F, width - margin - rightWidth), margin, rightWidth, rightHeight };

		const bool hasLeft = (leftWidth > 0) && (leftHeight > 0);
		if (hasLeft && (margin + leftWidth + margin + rightWidth + margin > width))
			l.right.y = margin + leftHeight + margin;
		return l;
	}

	std::string buildSummary(const char* config)
	{
		if (!config)
			return "";

		const std::array<const char*, 3> keys = { "Build type:", "Compiler:",
			                                      "Target architecture:" };
		std::vector<std::string> parts;
		std::istringstream lines(config);
		std::string line;
		while (std::getline(lines, line))
		{
			for (const auto* key : keys)
			{
				if (!startsWith(line, key))
					continue;
				auto value = line.substr(strlen(key));
				const auto start = value.find_first_not_of(' ');
				if (start != std::string::npos)
					parts.push_back(value.substr(start));
			}
		}

		std::string summary;
		for (const auto& part : parts)
			summary += summary.empty() ? part : ", " + part;
		return summary;
	}
} // namespace sdl::overlay
