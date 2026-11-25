/* JavaScript side of TestTimeZoneBrowser.c, linked with --js-library. */

/* node re-reads its time zone when process.env.TZ changes, so set it on the main thread */
function test_timezone_js_set(tz)
{
	if (typeof process !== "undefined")
		process.env.TZ = UTF8ToString(tz);
}

function test_timezone_js_is(tz)
{
	return Intl.DateTimeFormat().resolvedOptions().timeZone === UTF8ToString(tz) ? 1 : 0;
}

addToLibrary({
	test_timezone_js_set,
	test_timezone_js_set__sig: "vp",
	test_timezone_js_set__proxy: "sync",
	test_timezone_js_set__deps: ["$UTF8ToString"],
	test_timezone_js_is,
	test_timezone_js_is__sig: "ip",
	test_timezone_js_is__deps: ["$UTF8ToString"],
});
