/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client tests
 *
 * Copyright 2026 FreeRDP contributors
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

/* Pixel origins of local displays with mixed scales. */
#include "../sdl_display_offsets.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <numeric>
#include <vector>

#include <freerdp/settings_types.h>

static bool spansRange(int64_t a, int64_t alen, int64_t b, int64_t blen)
{
	return (a < b + blen) && (b < a + alen);
}

static bool intersect(const rdpMonitor& a, const rdpMonitor& b)
{
	return spansRange(a.x, a.width, b.x, b.width) && spansRange(a.y, a.height, b.y, b.height);
}

/* Two monitors are adjacent when they share an edge segment of positive length. */
static bool touch(const rdpMonitor& a, const rdpMonitor& b)
{
	const int64_t ax = a.x;
	const int64_t ay = a.y;
	const int64_t bx = b.x;
	const int64_t by = b.y;
	if (((ax + a.width == bx) || (bx + b.width == ax)) && spansRange(ay, a.height, by, b.height))
		return true;
	return ((ay + a.height == by) || (by + b.height == ay)) && spansRange(ax, a.width, bx, b.width);
}

/* MS-RDPEDISP 3.1.5.2: monitors must not overlap and must form one area, connected through
 * shared edges. */
static bool valid(const std::vector<rdpMonitor>& list)
{
	for (size_t i = 0; i < list.size(); i++)
		for (size_t j = i + 1; j < list.size(); j++)
			if (intersect(list[i], list[j]))
				return false;
	if (list.size() < 2)
		return true;

	std::vector<bool> reached(list.size(), false);
	std::vector<size_t> next{ 0 };
	reached[0] = true;
	while (!next.empty())
	{
		const auto i = next.back();
		next.pop_back();
		for (size_t j = 0; j < list.size(); j++)
		{
			if (!reached[j] && touch(list[i], list[j]))
			{
				reached[j] = true;
				next.push_back(j);
			}
		}
	}
	return std::all_of(reached.begin(), reached.end(), [](bool r) { return r; });
}

static bool expect(bool value, const char* message)
{
	if (!value)
		(void)fprintf(stderr, "FAIL: %s\n", message);
	return value;
}

struct Display
{
	const char* name;
	SDL_Rect points;
	int width;
	int height;
};

static sdl_display_offsets::Offsets offsets(const std::vector<Display>& displays,
                                            const std::vector<SDL_DisplayID>& ids, size_t primary,
                                            const std::vector<SDL_DisplayID>& selection = {})
{
	/* SDL reports bounds relative to the primary display, which starts at (0, 0). */
	const auto origin = displays[primary].points;
	sdl_display_offsets::Offsets result;
	for (size_t i = 0; i < displays.size(); i++)
	{
		auto bounds = displays[i].points;
		bounds.x -= origin.x;
		bounds.y -= origin.y;
		SDL_Rect pixel{ 0, 0, displays[i].width, displays[i].height };
		result.emplace(ids[i], std::pair{ bounds, pixel });
	}
	sdl_display_offsets::update(result, ids[primary], selection);
	return result;
}

static rdpMonitor monitor(const sdl_display_offsets::Offsets& o, SDL_DisplayID id)
{
	const auto& pixel = o.at(id).second;
	rdpMonitor m = {};
	m.x = pixel.x;
	m.y = pixel.y;
	m.width = pixel.w;
	m.height = pixel.h;
	return m;
}

static void dump(const std::vector<Display>& displays, const std::vector<SDL_DisplayID>& ids,
                 const sdl_display_offsets::Offsets& o)
{
	for (size_t i = 0; i < displays.size(); i++)
	{
		const auto m = monitor(o, ids[i]);
		(void)fprintf(stderr, "  %-8s id %u at %d,%d %dx%d\n", displays[i].name, ids[i], m.x, m.y,
		              m.width, m.height);
	}
}

/* Calls check with the pixel layout for every primary (or those listed) and every assignment of
 * display IDs (the displays are kept in a map ordered by ID). */
template <typename Check>
static bool forEachOrder(const char* name, const std::vector<Display>& displays,
                         const std::vector<size_t>& selection, Check check,
                         const std::vector<size_t>& primaries = {})
{
	std::vector<SDL_DisplayID> ids(displays.size());
	std::iota(ids.begin(), ids.end(), 1);
	do
	{
		std::vector<SDL_DisplayID> sel;
		sel.reserve(selection.size());
		for (const auto i : selection)
			sel.push_back(ids[i]);
		for (size_t primary = 0; primary < displays.size(); primary++)
		{
			if (!primaries.empty() &&
			    (std::find(primaries.begin(), primaries.end(), primary) == primaries.end()))
				continue;
			const auto o = offsets(displays, ids, primary, sel);
			std::vector<rdpMonitor> list;
			list.reserve(displays.size());
			for (size_t i = 0; i < displays.size(); i++)
				list.push_back(monitor(o, ids[i]));
			if (!check(list))
			{
				(void)fprintf(stderr, "FAIL: %s (primary %s)\n", name, displays[primary].name);
				dump(displays, ids, o);
				return false;
			}
		}
	} while (std::next_permutation(ids.begin(), ids.end()));
	return true;
}

/* The selected displays (empty: all) form a valid layout. */
static bool checkValid(const char* name, const std::vector<Display>& displays,
                       const std::vector<size_t>& selection = {})
{
	return forEachOrder(
	    name, displays, selection,
	    [&](const std::vector<rdpMonitor>& list)
	    {
		    std::vector<rdpMonitor> sent;
		    for (size_t i = 0; i < list.size(); i++)
		    {
			    if (selection.empty() ||
			        (std::find(selection.begin(), selection.end(), i) != selection.end()))
				    sent.push_back(list[i]);
		    }
		    return valid(sent);
	    });
}

/* Whenever the walk gives a valid layout, the result is valid too: every primary and every
 * assignment of display IDs. */
static bool checkNotWorse(const char* name, const std::vector<Display>& displays,
                          const std::vector<size_t>& selection = {})
{
	std::vector<SDL_DisplayID> ids(displays.size());
	std::iota(ids.begin(), ids.end(), 1);
	do
	{
		std::vector<SDL_DisplayID> sel;
		sel.reserve(selection.size());
		for (const auto i : selection)
			sel.push_back(ids[i]);
		for (size_t primary = 0; primary < displays.size(); primary++)
		{
			const auto o = offsets(displays, ids, primary, sel);
			auto before = o;
			sdl_display_offsets::walk(before, ids[primary]);
			std::vector<rdpMonitor> ours;
			std::vector<rdpMonitor> walked;
			for (size_t i = 0; i < displays.size(); i++)
			{
				if (selection.empty() ||
				    (std::find(selection.begin(), selection.end(), i) != selection.end()))
				{
					ours.push_back(monitor(o, ids[i]));
					walked.push_back(monitor(before, ids[i]));
				}
			}
			if (valid(walked) && !valid(ours))
			{
				(void)fprintf(stderr, "FAIL: %s is worse than the walk (primary %s)\n", name,
				              displays[primary].name);
				dump(displays, ids, o);
				return false;
			}
		}
	} while (std::next_permutation(ids.begin(), ids.end()));
	return true;
}

static bool checkDesk()
{
	/* A macOS arrangement in points, with the IDs and the fullscreen built-in size the client
	 * reports on a MacBook Air: a 1x FHD display above the 2x built-in, left aligned, and a 2x 4K
	 * display right of the FHD one, top aligned. The 4K and the built-in do not touch. */
	const std::vector<Display> desk{
		{ "built-in", { 0, 0, 1470, 956 }, 2940, 1846 },
		{ "4k", { 1920, -1080, 1920, 1080 }, 3840, 2160 },
		{ "fhd", { 0, -1080, 1920, 1080 }, 1920, 1080 },
	};

	/* Every selection with every primary and every assignment of display IDs. The built-in and
	 * the 4K do not touch: from the 4K, the built-in is placed through the FHD, which does not
	 * bring them together, so that selection is only not worse than the walk. */
	if (!checkValid("all three displays", desk) ||
	    !checkValid("the built-in and the FHD", desk, { 0, 2 }) ||
	    !checkNotWorse("the built-in and the 4K", desk, { 0, 1 }) ||
	    !checkValid("the FHD and the 4K", desk, { 1, 2 }))
		return false;

	/* With the built-in as primary, the layout the walk got wrong: the FHD on the built-in, left
	 * aligned, and the 4K right of the FHD, on the built-in's top edge. */
	const std::vector<SDL_DisplayID> ids{ 1, 2, 3 };
	for (const auto& selection : std::vector<std::vector<SDL_DisplayID>>{ {}, { 1, 3 }, { 1, 2 } })
	{
		const auto o = offsets(desk, ids, 0, selection);
		const auto uhd = monitor(o, 2);
		const auto fhd = monitor(o, 3);
		const bool withFhd = selection.empty() || (selection[1] == 3);
		const bool withUhd = selection.empty() || (selection[1] == 2);
		if (!expect((!withFhd || ((fhd.x == 0) && (fhd.y == -1080))) &&
		                (!withUhd || ((uhd.x == 1920) && (uhd.y == -2160))),
		            "the desk layout keeps the FHD above the built-in and the 4K next to it"))
		{
			dump(desk, ids, o);
			return false;
		}
	}
	return true;
}

static bool checkMixed()
{
	/* Ordinary MacBook layouts: valid with every primary and ID assignment. */
	const std::vector<Display> under{ { "built-in", { 0, 0, 1470, 956 }, 2940, 1912 },
		                              { "external", { 320, 956, 1920, 1080 }, 1920, 1080 } };
	const std::vector<Display> beside{ { "built-in", { 0, 0, 1470, 956 }, 2940, 1912 },
		                               { "fhd", { 1470, -539, 1920, 1080 }, 1920, 1080 } };
	const std::vector<Display> siblings{ { "1x", { 0, 0, 1920, 1080 }, 1920, 1080 },
		                                 { "upper", { 1920, -540, 1920, 1080 }, 3840, 2160 },
		                                 { "lower", { 1920, 540, 1920, 1080 }, 3840, 2160 } };
	const std::vector<Display> stacked{ { "built-in", { 0, 0, 1728, 1117 }, 3456, 2234 },
		                                { "fhd", { 1728, 576, 1920, 1080 }, 1920, 1080 },
		                                { "qhd", { 1408, 1656, 2560, 1440 }, 2560, 1440 } };
	if (!checkValid("a display below a 2x MacBook", under) ||
	    !checkValid("a display beside a 2x MacBook", beside) ||
	    !checkValid("two 2x displays right of a 1x one", siblings))
		return false;

	/* Layouts that the placement does not always solve: never worse than the walk. */
	const std::vector<Display> below{ { "2x", { 0, 0, 1920, 1080 }, 3840, 2160 },
		                              { "a", { -960, 1080, 1920, 1080 }, 1920, 1080 },
		                              { "b", { 960, 1080, 1920, 1080 }, 1920, 1080 } };
	const std::vector<Display> corner{ { "1x", { 0, 0, 1920, 1080 }, 1920, 1080 },
		                               { "right", { 1920, 0, 1920, 1080 }, 1920, 1080 },
		                               { "2x", { -1920, -1080, 1920, 1080 }, 3840, 2160 } };
	const std::vector<Display> blocked{ { "1x", { 0, 0, 1920, 1080 }, 1920, 1080 },
		                                { "portrait", { 1920, -840, 1080, 1920 }, 1080, 1920 },
		                                { "right", { 3000, 540, 1920, 1080 }, 1920, 1080 },
		                                { "2x", { 0, 1080, 1920, 1080 }, 3840, 2160 } };
	const std::vector<Display> stack{ { "built-in", { 0, 0, 1728, 1117 }, 3456, 2234 },
		                              { "portrait", { 0, 1117, 1080, 1920 }, 1080, 1920 },
		                              { "left", { -1260, 3037, 1920, 1080 }, 1920, 1080 },
		                              { "farleft", { -3180, 3037, 1920, 1080 }, 1920, 1080 } };
	const std::vector<Display> sides{ { "built-in", { 0, 0, 1728, 1117 }, 3456, 2234 },
		                              { "left", { -1920, 0, 1920, 1080 }, 1920, 1080 },
		                              { "upper left", { -1920, -1080, 1920, 1080 }, 1920, 1080 },
		                              { "right", { 1728, -720, 2560, 1440 }, 2560, 1440 } };
	return checkNotWorse("two displays stacked right of a 2x MacBook", stacked) &&
	       checkNotWorse("two displays below an unselected 2x one", below, { 1, 2 }) &&
	       checkNotWorse("a 2x display at a corner", corner) &&
	       checkNotWorse("a 2x display between others", blocked) &&
	       checkNotWorse("a MacBook with displays below it", stack) &&
	       checkNotWorse("a MacBook with displays on both sides", sides, { 0, 2, 3 });
}

static bool checkKept(const char* name, const std::vector<Display>& displays,
                      const std::vector<size_t>& selection = {})
{
	/* Relative to the first selected display, as the primary changes; all displays have the
	 * scale of the first one. */
	const size_t ref = selection.empty() ? 0 : selection.front();
	const int scale = displays[ref].width / displays[ref].points.w;
	return forEachOrder(
	    name, displays, selection,
	    [&](const std::vector<rdpMonitor>& list)
	    {
		    for (size_t i = 0; i < list.size(); i++)
		    {
			    if (!selection.empty() &&
			        (std::find(selection.begin(), selection.end(), i) == selection.end()))
				    continue;
			    if ((list[i].x - list[ref].x !=
			         scale * (displays[i].points.x - displays[ref].points.x)) ||
			        (list[i].y - list[ref].y !=
			         scale * (displays[i].points.y - displays[ref].points.y)))
				    return false;
		    }
		    return true;
	    });
}

static bool checkSameScale()
{
	const std::vector<Display> plain{ { "a", { 0, 0, 1920, 1080 }, 1920, 1080 },
		                              { "b", { 1920, 0, 1920, 1080 }, 1920, 1080 },
		                              { "c", { 0, 1080, 1920, 1080 }, 1920, 1080 } };
	/* The layout from #13507 (wlroots). */
	const std::vector<Display> wide{ { "ultrawide", { 0, 0, 3440, 1440 }, 3440, 1440 },
		                             { "above", { -400, -1080, 3840, 1080 }, 3840, 1080 },
		                             { "portrait", { 3440, -821, 1440, 2560 }, 1440, 2560 } };
	/* Displays touching only at a corner, without the primary they touch. */
	const std::vector<Display> corner{ { "primary", { 0, 0, 1920, 1080 }, 1920, 1080 },
		                               { "a", { 1920, 1080, 1920, 1080 }, 1920, 1080 },
		                               { "b", { 3840, 1080, 1920, 1080 }, 1920, 1080 } };
	/* Closing the gap between "left" and "above" would break the edge "left" shares with
	 * "farleft". */
	const std::vector<Display> row{ { "primary", { 0, 0, 1080, 1920 }, 1080, 1920 },
		                            { "left", { -1920, 1080, 1920, 1080 }, 1920, 1080 },
		                            { "farleft", { -3840, 1080, 1920, 1080 }, 1920, 1080 },
		                            { "above", { -400, -1080, 1920, 1080 }, 1920, 1080 } };
	/* The same at 2x. */
	std::vector<Display> retina = row;
	for (auto& d : retina)
	{
		d.width *= 2;
		d.height *= 2;
	}
	return checkKept("same-scale displays keep their positions", plain) &&
	       checkKept("the wlroots layout is kept", wide) &&
	       checkKept("selected displays away from the primary keep their positions", corner,
	                 { 1, 2 }) &&
	       checkKept("a row of displays below another keeps its positions", row) &&
	       checkKept("2x displays keep their point layout", retina);
}

static bool checkEdgeCases()
{
	sdl_display_offsets::Offsets none;
	sdl_display_offsets::update(none, 1);

	/* A primary that is not in the list: the first display takes its place. */
	const std::vector<Display> row{ { "a", { 0, 0, 1920, 1080 }, 1920, 1080 },
		                            { "b", { 1920, 0, 1920, 1080 }, 1920, 1080 },
		                            { "c", { 3840, 0, 1920, 1080 }, 3840, 2160 } };
	sdl_display_offsets::Offsets missing;
	for (size_t i = 0; i < row.size(); i++)
		missing.emplace(static_cast<SDL_DisplayID>(i + 1),
		                std::pair{ row[i].points, SDL_Rect{ 0, 0, row[i].width, row[i].height } });
	sdl_display_offsets::update(missing, 42);
	return expect((monitor(missing, 1).x == 0) && (monitor(missing, 2).x == 1920) &&
	                  (monitor(missing, 3).x == 3840),
	              "a missing primary is replaced by the first display");
}

int main()
{
	if (!checkDesk() || !checkMixed() || !checkSameScale() || !checkEdgeCases())
		return 1;
	return 0;
}
