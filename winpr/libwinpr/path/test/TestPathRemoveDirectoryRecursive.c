#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/file.h>

static BOOL make_file(const char* base, const char* name)
{
	char path[512] = WINPR_C_ARRAY_INIT;
	(void)_snprintf(path, sizeof(path), "%s/%s", base, name);
	FILE* file = fopen(path, "wb");
	if (!file)
		return FALSE;
	const BOOL ok = fputs("data", file) >= 0;
	return (fclose(file) == 0) && ok;
}

static BOOL make_dir(const char* base, const char* name)
{
	char path[512] = WINPR_C_ARRAY_INIT;
	(void)_snprintf(path, sizeof(path), "%s/%s", base, name);
	return mkdir(path, 0700) == 0;
}

static BOOL make_link(const char* base, const char* target, const char* name)
{
	char path[512] = WINPR_C_ARRAY_INIT;
	(void)_snprintf(path, sizeof(path), "%s/%s", base, name);
	return symlink(target, path) == 0;
}

static BOOL exists(const char* base, const char* name)
{
	struct stat st = WINPR_C_ARRAY_INIT;
	char path[512] = WINPR_C_ARRAY_INIT;
	(void)_snprintf(path, sizeof(path), "%s/%s", base, name);
	return lstat(path, &st) == 0;
}

static BOOL remove_below(const char* base, const char* name)
{
	char path[512] = WINPR_C_ARRAY_INIT;
	(void)_snprintf(path, sizeof(path), "%s/%s", base, name);
	return winpr_RemoveDirectory_RecursiveA(path);
}

/* The tree goes, with its directories and everything in them */
static BOOL test_tree(const char* base)
{
	if (!make_dir(base, "tree") || !make_dir(base, "tree/sub") || !make_dir(base, "tree/empty") ||
	    !make_file(base, "tree/a") || !make_file(base, "tree/sub/b"))
		return FALSE;
	return remove_below(base, "tree") && !exists(base, "tree");
}

/* A link goes, and what it points to stays */
static BOOL test_links_stay_out(const char* base)
{
	if (!make_dir(base, "kept") || !make_file(base, "kept/keep") || !make_file(base, "keep2") ||
	    !make_dir(base, "links") || !make_dir(base, "links/sub") ||
	    !make_link(base, "../kept", "links/to-dir") ||
	    !make_link(base, "../../kept", "links/sub/deep") ||
	    !make_link(base, "../keep2", "links/to-file"))
		return FALSE;

	return remove_below(base, "links") && !exists(base, "links") && exists(base, "kept/keep") &&
	       exists(base, "keep2");
}

/* The named path itself may be the link */
static BOOL test_named_link(const char* base)
{
	if (!make_link(base, "kept", "link"))
		return FALSE;
	return remove_below(base, "link") && !exists(base, "link") && exists(base, "kept/keep");
}

int TestPathRemoveDirectoryRecursive(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	int rc = -1;
	char base[] = "/tmp/TestPathRemoveDirectoryRecursive.XXXXXX";
	if (!mkdtemp(base))
		return -1;

	if (!test_tree(base))
		(void)fprintf(stderr, "a tree was not removed\n");
	else if (!test_links_stay_out(base))
		(void)fprintf(stderr, "a link inside was followed\n");
	else if (!test_named_link(base))
		(void)fprintf(stderr, "the named link was followed\n");
	else
		rc = 0;

	(void)winpr_RemoveDirectory_RecursiveA(base);
	return rc;
}
