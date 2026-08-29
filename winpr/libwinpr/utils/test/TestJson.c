#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/file.h>
#include <winpr/json.h>
#include <winpr/path.h>

int TestJson(int argc, char* argv[])
{
	static const char jsonData[] = "{\r\n  \"value\": \"test\"\r\n}\r\n";
	int rc = -1;
	FILE* fp = nullptr;
	WINPR_JSON* json = nullptr;
	char* path = nullptr;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	path = GetCombinedPath(TEST_BINARY_PATH, "TestJson-crlf.json");
	if (!path)
		goto fail;

	fp = winpr_fopen(path, "wb");
	if (!fp)
		goto fail;

	if (fwrite(jsonData, sizeof(char), sizeof(jsonData) - 1, fp) != sizeof(jsonData) - 1)
		goto fail;

	if (fclose(fp) != 0)
		goto fail;
	fp = nullptr;

	json = WINPR_JSON_ParseFromFile(path);
	if (!json)
		goto fail;

	const WINPR_JSON* value = WINPR_JSON_GetObjectItem(json, "value");
	if (!WINPR_JSON_IsString(value))
		goto fail;

	const char* str = WINPR_JSON_GetStringValue(value);
	if (!str || (strcmp(str, "test") != 0))
		goto fail;

	rc = 0;

fail:
	if (fp)
		(void)fclose(fp);
	WINPR_JSON_Delete(json);
	if (path)
		(void)winpr_DeleteFile(path);
	free(path);
	return rc;
}
