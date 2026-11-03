/**
 * WinPR: Windows Portable Runtime
 * Browser time zone detection (Emscripten builds)
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

/* JavaScript side of timezone.c, linked with --js-library.
 * A browser has no /etc/timezone or /etc/localtime, so ask it for its IANA time zone. Without
 * this the server gets "Client Local Time", cannot match it and falls back to its first time
 * zone (UTC-12). The Windows zone mapping only knows the Etc/UTC spelling of UTC. */
function winpr_timezone_js_get(buffer, size)
{
	let tz = "";
	try
	{
		tz = Intl.DateTimeFormat().resolvedOptions().timeZone ?? "";
	}
	catch (e)
	{
	}
	stringToUTF8(tz === "UTC" ? "Etc/UTC" : tz, buffer, size);
}

addToLibrary({
	winpr_timezone_js_get,
	winpr_timezone_js_get__sig: "vpp",
	winpr_timezone_js_get__deps: ["$stringToUTF8"],
});
