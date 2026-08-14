
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/tchar.h>
#include <winpr/winpr.h>

typedef struct
{
	const char* what;
	HRESULT result;
	const char* expect;
} test_case_t;

WINPR_ATTR_NODISCARD
static BOOL test(size_t count, const test_case_t* cur)
{
	WINPR_ASSERT(cur);
	WINPR_ASSERT(cur->what);

	const size_t len = strlen(cur->what);
	char* out = calloc(len + 1, sizeof(char));
	if (!out)
		return FALSE;

	BOOL rc = TRUE;
	HRESULT hr = PathCchCanonicalize(out, len, cur->what);
	if (hr != cur->result)
	{
		rc = FALSE;
		(void)fprintf(stderr,
		              "[%s: %" PRIuz ": got result 0x%08" PRIx32 ", expected 0x%08" PRIx32 "\n",
		              __FILE__, count, hr, cur->result);
	}
	else if (hr == S_OK)
	{
		rc = strcmp(out, cur->expect) == 0;
		if (!rc)
			(void)fprintf(stderr, "[%s: %" PRIuz ": got result '%s', expected '%s'\n", __FILE__,
			              count, out, cur->expect);
	}
	free(out);
	return rc;
}

int TestPathCchCanonicalize(int argc, char* argv[])
{
	const test_case_t tests[] = {
		{ "////", S_OK, "/" },
		{ "///foo///bar//..///lala/././///.///.", S_OK, "/foo/lala" },
		{ "////..////.", S_OK, "/" },
		{ "///./gaga////foo/..///bar/..", S_OK, "/gaga" },
	};

	int rc = 0;
	for (size_t x = 0; x < ARRAYSIZE(tests); x++)
	{
		const test_case_t* cur = &tests[x];
		if (!test(x, cur))
			rc = -1;
	}
	return rc;
}
