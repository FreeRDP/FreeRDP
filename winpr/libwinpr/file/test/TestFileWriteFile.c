
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/file.h>
#include <winpr/windows.h>

static const char* get_dir(char* filename, size_t len)
{
#if defined(WIN32)
	if ((len == 0) || (strnlen_s(filename, len) == len))
		return NULL;
	char* ptr = strrchr(filename, '\\');
#else
	if ((len == 0) || (strnlen(filename, len) == len))
		return NULL;
	char* ptr = strrchr(filename, '/');
#endif
	if (!ptr)
		return NULL;
	*ptr = '\0';
	return filename;
}

static BOOL get_tmp(char* path, size_t len)
{
#if defined(WIN32)
	const char template[] = "tmpdir.XXXXXX";
	strncmp(path, template, strnlen_s(template, len) + 1);
	if (!mktemp_s(path))
		return FALSE;
	return winpr_str_append("testfile", path, len, "\\");
#else
	const char template[] = "/tmp/tmpdir.XXXXXX";
	if (!strncpy(path, template, strnlen(template, len) + 1))
		return FALSE;
	if (!mkdtemp(path))
		return FALSE;
	return winpr_str_append("testfile", path, len, "/");
#endif
}

static BOOL test_write(const char* filename, const char* data, size_t datalen)
{
	BOOL rc = FALSE;

	WINPR_ASSERT(filename);
	WINPR_ASSERT(data);
	WINPR_ASSERT(datalen > 0);

	HANDLE hdl =
	    CreateFileA(filename, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
	if (!hdl || (hdl == INVALID_HANDLE_VALUE))
		goto fail;

	DWORD written = 0;
	if (!WriteFile(hdl, data, datalen, &written, NULL))
		goto fail;
	if (written != datalen)
		goto fail;

	if (!FlushFileBuffers(hdl))
		goto fail;

	rc = TRUE;
fail:
	CloseHandle(hdl);
	return rc;
}

static BOOL test_read(const char* filename, const char* data, size_t datalen)
{
	BOOL rc = FALSE;

	WINPR_ASSERT(filename);
	WINPR_ASSERT(data);
	WINPR_ASSERT(datalen > 0);

	char* cmp = calloc(datalen + 1, sizeof(char));
	HANDLE hdl =
	    CreateFileA(filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (!hdl || (hdl == INVALID_HANDLE_VALUE) || !cmp)
		goto fail;

	DWORD read = 0;
	if (!ReadFile(hdl, cmp, datalen, &read, NULL))
		goto fail;
	if (read != datalen)
		goto fail;
	if (memcmp(data, cmp, datalen) != 0)
		goto fail;
	if (FlushFileBuffers(hdl))
		goto fail;

	rc = TRUE;
fail:
	free(cmp);
	CloseHandle(hdl);
	return rc;
}

int TestFileWriteFile(int argc, char* argv[])
{
	const char data[] = "sometesttext\nanother line\r\ngogogo\r\tfoo\t\r\n\r";
	char filename[MAX_PATH] = { 0 };

	int rc = -1;
	if (!get_tmp(filename, sizeof(filename)))
		goto fail;

	if (!test_write(filename, data, sizeof(data)))
		goto fail;

	if (!test_read(filename, data, sizeof(data)))
		goto fail;

	rc = 0;
fail:
	if (!DeleteFile(filename))
		rc = -2;

	const char* d = get_dir(filename, sizeof(filename));
	if (d)
	{
		if (!RemoveDirectory(d))
			rc = -3;
	}
	return rc;
}
