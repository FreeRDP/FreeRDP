/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client: the monitors a web page shows the canvas on (Emscripten builds)
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

/* JavaScript side of SdlContext::updateWindowList() in sdl_context.cpp, linked with
 * --js-library. A browser gives the client one canvas, so one window. A page that shows that
 * canvas across several screens can set Module.freerdpMonitors to a function returning them:
 * [{ left, top, right, bottom, primary }], the edges as fractions (0 to 1) of the canvas, and
 * the server then gets one monitor each. Writes up to max of them to edges as five int32 each
 * (left, top, right, bottom in 1/10000 of the canvas, primary 0 or 1); returns their number,
 * 0 for none. */
function freerdp_web_monitors_js(edges, max)
{
	const list = typeof Module.freerdpMonitors === "function" ? Module.freerdpMonitors() : null;
	if (!Array.isArray(list))
		return 0;
	const n = Math.min(list.length, max);
	const at = (v) => Math.round(Math.min(Math.max(Number(v) || 0, 0), 1) * 10000);
	for (let i = 0; i < n; i++)
	{
		const m = list[i];
		const p = (edges >>> 2) + 5 * i;
		HEAP32[p] = at(m.left);
		HEAP32[p + 1] = at(m.top);
		HEAP32[p + 2] = at(m.right);
		HEAP32[p + 3] = at(m.bottom);
		HEAP32[p + 4] = m.primary ? 1 : 0;
	}
	return n;
}

addToLibrary({
	freerdp_web_monitors_js,
	freerdp_web_monitors_js__sig: "ipi",
	freerdp_web_monitors_js__proxy: "sync",
});
