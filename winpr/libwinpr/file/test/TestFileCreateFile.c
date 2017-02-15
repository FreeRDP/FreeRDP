
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/file.h>
#include <winpr/path.h>
#include <winpr/handle.h>
#include <winpr/windows.h>

int TestFileCreateFile(int argc, char* argv[])
{
	HANDLE handle;
	HRESULT hr;
	DWORD written;
	const char buffer[] = "Some random text\r\njust want it done.";
	char cmp[sizeof(buffer)];
	LPSTR name = GetKnownSubPath(KNOWN_PATH_TEMP, "CreateFile.testfile");

	int rc = 0;

	if (!name)
		return -1;

	/* On windows we would need '\\' or '/' as seperator.
	 * Single '\' do not work. */
	hr = PathCchConvertStyleA(name, strlen(name), PATH_STYLE_UNIX);
	if (FAILED(hr))
		rc = -1;

	handle = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, NULL,
			CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
	if (!handle)
	{
		free(name);
		return -1;
	}

	if (!PathFileExistsA(name))
		rc = -1;

	if (!WriteFile(handle, buffer, sizeof(buffer), &written, NULL))
		rc = -1;

	if (written != sizeof(buffer))
		rc = -1;

	written = SetFilePointer(handle, 5, NULL, FILE_BEGIN);

	if (written != 5)
		rc = -1;

	written = SetFilePointer(handle, 0, NULL, FILE_CURRENT);

	if (written != 5)
		rc = -1;

	written = SetFilePointer(handle, -5, NULL, FILE_CURRENT);

	if (written != 0)
		rc = -1;

	if (!ReadFile(handle, cmp, sizeof(cmp), &written, NULL))
		rc = -1;

	if (written != sizeof(cmp))
		rc = -1;

	if (memcmp(buffer, cmp, sizeof(buffer)))
		rc = -1;

	if (!CloseHandle(handle))
		rc = -1;

	if (!DeleteFileA(name))
		rc = -1;

	if (PathFileExistsA(name))
		rc = -1;

	free(name);

	return rc;
}
