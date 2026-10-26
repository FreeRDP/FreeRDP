/**
 * WinPR: Windows Portable Runtime
 * Yielding to the browser's event loop (Emscripten builds)
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

/* JavaScript side of pollset.c, linked with --js-library. Each thread (the main browser thread
 * or a worker) has its own copy. */

/* Resolves once the event loop has run the tasks queued before it. A message posted on a
 * MessageChannel is such a task, and it runs as soon as the loop gets to it: setTimeout(0),
 * which emscripten_sleep(0) waits on, is clamped to 4 ms once timers nest more than five
 * deep, as they do in a client that keeps yielding. */
const winpr_pollset_channel = {
	port: null,
	waiting: []
};

function winpr_pollset_js_yield()
{
	const c = winpr_pollset_channel;
	if (!c.port)
	{
		const channel = new MessageChannel();
		channel.port1.onmessage = () => c.waiting.shift()();
		c.port = channel.port2;
	}
	return new Promise((resolve) => {
		c.waiting.push(resolve);
		c.port.postMessage(0);
	});
}

addToLibrary({
	$winpr_pollset_channel: winpr_pollset_channel,
	winpr_pollset_js_yield,
	winpr_pollset_js_yield__sig: "v",
	winpr_pollset_js_yield__async: "auto",
	winpr_pollset_js_yield__deps: ["$winpr_pollset_channel"],
});
