
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/windows.h>

/*
 * Conversion *to* Unicode
 * MultiByteToWideChar: http://msdn.microsoft.com/en-us/library/windows/desktop/dd319072/
 */

/* English */

static BYTE en_Hello_UTF8[] = "Hello\0";
static BYTE en_Hello_UTF16[] = "\x48\x00\x65\x00\x6C\x00\x6C\x00\x6F\x00\x00\x00";
static int en_Hello_UTF8_Length = sizeof(en_Hello_UTF8) / (sizeof(CHAR));
static int en_Hello_UTF16_Length = sizeof(en_Hello_UTF16) / (sizeof(WCHAR));

static BYTE en_HowAreYou_UTF8[] = "How are you?\0";
static BYTE en_HowAreYou_UTF16[] = "\x48\x00\x6F\x00\x77\x00\x20\x00\x61\x00\x72\x00\x65\x00\x20\x00"
		"\x79\x00\x6F\x00\x75\x00\x3F\x00\x00\x00";
static int en_HowAreYou_UTF8_Length = sizeof(en_HowAreYou_UTF8) / (sizeof(CHAR));
static int en_HowAreYou_UTF16_Length = sizeof(en_HowAreYou_UTF16) / (sizeof(WCHAR));

/* French */

static BYTE fr_Hello_UTF8[] = "Allo\0";
static BYTE fr_Hello_UTF16[] = "\x41\x00\x6C\x00\x6C\x00\x6F\x00\x00\x00";
static int fr_Hello_UTF8_Length = sizeof(fr_Hello_UTF8) / (sizeof(CHAR));
static int fr_Hello_UTF16_Length = sizeof(fr_Hello_UTF16) / (sizeof(WCHAR));

static BYTE fr_HowAreYou_UTF8[] = "\x43\x6F\x6D\x6D\x65\x6E\x74\x20\xC3\xA7\x61\x20\x76\x61\x3F\x00";
static BYTE fr_HowAreYou_UTF16[] = "\x43\x00\x6F\x00\x6D\x00\x6D\x00\x65\x00\x6E\x00\x74\x00\x20\x00"
		"\xE7\x00\x61\x00\x20\x00\x76\x00\x61\x00\x3F\x00\x00\x00";
static int fr_HowAreYou_UTF8_Length = sizeof(fr_HowAreYou_UTF8) / (sizeof(CHAR));
static int fr_HowAreYou_UTF16_Length = sizeof(fr_HowAreYou_UTF16) / (sizeof(WCHAR));

/* Russian */

static BYTE ru_Hello_UTF8[] = "\xD0\x97\xD0\xB4\xD0\xBE\xD1\x80\xD0\xBE\xD0\xB2\xD0\xBE\x00";
static BYTE ru_Hello_UTF16[] = "\x17\x04\x34\x04\x3E\x04\x40\x04\x3E\x04\x32\x04\x3E\x04\x00\x00";
static int ru_Hello_UTF8_Length = sizeof(ru_Hello_UTF8) / (sizeof(CHAR));
static int ru_Hello_UTF16_Length = sizeof(ru_Hello_UTF16) / (sizeof(WCHAR));

static BYTE ru_HowAreYou_UTF8[] = "\xD0\x9A\xD0\xB0\xD0\xBA\x20\xD0\xB4\xD0\xB5\xD0\xBB\xD0\xB0\x3F\x00";
static BYTE ru_HowAreYou_UTF16[] = "\x1A\x04\x30\x04\x3A\x04\x20\x00\x34\x04\x35\x04\x3B\x04\x30\x04"
		"\x3F\x00\x00\x00";
static int ru_HowAreYou_UTF8_Length = sizeof(ru_HowAreYou_UTF8) / (sizeof(CHAR));
static int ru_HowAreYou_UTF16_Length = sizeof(ru_HowAreYou_UTF16) / (sizeof(WCHAR));

/* Arabic */

static BYTE ar_Hello_UTF8[] = "\xD8\xA7\xD9\x84\xD8\xB3\xD9\x84\xD8\xA7\xD9\x85\x20\xD8\xB9\xD9"
		"\x84\xD9\x8A\xD9\x83\xD9\x85\x00";
static BYTE ar_Hello_UTF16[] = "\x27\x06\x44\x06\x33\x06\x44\x06\x27\x06\x45\x06\x20\x00\x39\x06"
		"\x44\x06\x4A\x06\x43\x06\x45\x06\x00\x00";
static int ar_Hello_UTF8_Length = sizeof(ar_Hello_UTF8) / (sizeof(CHAR));
static int ar_Hello_UTF16_Length = sizeof(ar_Hello_UTF16) / (sizeof(WCHAR));

static BYTE ar_HowAreYou_UTF8[] = "\xD9\x83\xD9\x8A\xD9\x81\x20\xD8\xAD\xD8\xA7\xD9\x84\xD9\x83\xD8"
		"\x9F\x00";
static BYTE ar_HowAreYou_UTF16[] = "\x43\x06\x4A\x06\x41\x06\x20\x00\x2D\x06\x27\x06\x44\x06\x43\x06"
		"\x1F\x06\x00\x00";
static int ar_HowAreYou_UTF8_Length = sizeof(ar_HowAreYou_UTF8) / (sizeof(CHAR));
static int ar_HowAreYou_UTF16_Length = sizeof(ar_HowAreYou_UTF16) / (sizeof(WCHAR));

/* Chinese */

static BYTE ch_Hello_UTF8[] = "\xE4\xBD\xA0\xE5\xA5\xBD";
static BYTE ch_Hello_UTF16[] = "\x60\x4F\x7D\x59";
static int ch_Hello_UTF8_Length = sizeof(ch_Hello_UTF8) / (sizeof(CHAR));
static int ch_Hello_UTF16_Length = sizeof(ch_Hello_UTF16) / (sizeof(WCHAR));

static BYTE ch_HowAreYou_UTF8[] = "\xE4\xBD\xA0\xE5\xA5\xBD\xE5\x90\x97";
static BYTE ch_HowAreYou_UTF16[] = "\x60\x4F\x7D\x59\x17\x54";
static int ch_HowAreYou_UTF8_Length = sizeof(ch_HowAreYou_UTF8) / (sizeof(CHAR));
static int ch_HowAreYou_UTF16_Length = sizeof(ch_HowAreYou_UTF16) / (sizeof(WCHAR));

void string_hexdump(BYTE* data, int length)
{
	BYTE* p = data;
	int i, line, offset = 0;

	while (offset < length)
	{
		printf("%04x ", offset);

		line = length - offset;

		if (line > 16)
			line = 16;

		for (i = 0; i < line; i++)
			printf("%02x ", p[i]);

		for (; i < 16; i++)
			printf("   ");

		for (i = 0; i < line; i++)
			printf("%c", (p[i] >= 0x20 && p[i] < 0x7F) ? p[i] : '.');

		printf("\n");

		offset += line;
		p += line;
	}
}

int convert_utf8_to_utf16(BYTE* utf8, int utf8_length, BYTE* utf16, int utf16_length)
{
	int length;
	LPWSTR lpWideCharStr;

	length = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR) utf8, -1, NULL, 0);

	if (length != utf16_length)
	{
		printf("MultiByteToWideChar: unexpected required length: actual: %d, expected: %d\n",
			length, utf16_length);

		printf("UTF8:\n");
		string_hexdump((BYTE*) utf8, utf8_length);

		return -1;
	}

	lpWideCharStr = (LPWSTR) malloc(length * sizeof(WCHAR));

	length = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR) utf8, length, lpWideCharStr, length);

	if (length != utf16_length)
	{
		printf("MultiByteToWideChar: unexpected conversion length: actual: %d, expected: %d\n",
			length, utf16_length);

		printf("UTF8:\n");
		string_hexdump((BYTE*) utf8, utf8_length);

		if (length > 0)
		{
			printf("UTF16 (actual):\n");
			string_hexdump((BYTE*) lpWideCharStr, length * sizeof(WCHAR));
		}

		printf("UTF16 (expecting):\n");
		string_hexdump((BYTE*) utf16, utf16_length * sizeof(WCHAR));

		return 1;
		//return -1;
	}

	if (memcmp(utf16, lpWideCharStr, length) != 0)
	{
		printf("MultiByteToWideChar: unexpected string\n");

		printf("actual:\n");
		string_hexdump((BYTE*) utf16, utf16_length);

		printf("expected:\n");
		string_hexdump((BYTE*) lpWideCharStr, utf16_length);

		return 1;
		//return -1;
	}

	printf("UTF8:\n");
	string_hexdump((BYTE*) utf8, utf8_length);

	printf("UTF16:\n");
	string_hexdump((BYTE*) utf16, utf16_length * sizeof(WCHAR));

	free(lpWideCharStr);

	return length;
}

int TestMultiByteToWideChar(int argc, char* argv[])
{
	int length;
	LPWSTR lpWideCharStr;

	/**
	 * int MultiByteToWideChar(
	 * _In_       UINT CodePage,
	 * _In_       DWORD dwFlags,
	 * _In_       LPCSTR lpMultiByteStr,
	 * _In_       int cbMultiByte,
	 * _Out_opt_  LPWSTR lpWideCharStr,
	 * _In_       int cchWideChar
	 * );
	 */

	/**
	 * The function returns 0 if it does not succeed.
	 */

	/**
	 * If the function succeeds and cchWideChar is 0, the return value is the
	 * required size, in characters, for the buffer indicated by lpWideCharStr.
	 */

	if (convert_utf8_to_utf16(en_Hello_UTF8, en_Hello_UTF8_Length, en_Hello_UTF16, en_Hello_UTF16_Length) < 1)
		return -1;
	if (convert_utf8_to_utf16(en_HowAreYou_UTF8, en_HowAreYou_UTF8_Length, en_HowAreYou_UTF16, en_HowAreYou_UTF16_Length) < 1)
		return -1;

	if (convert_utf8_to_utf16(fr_Hello_UTF8, fr_Hello_UTF8_Length, fr_Hello_UTF16, fr_Hello_UTF16_Length) < 1)
		return -1;
	if (convert_utf8_to_utf16(fr_HowAreYou_UTF8, fr_HowAreYou_UTF8_Length, fr_HowAreYou_UTF16, fr_HowAreYou_UTF16_Length) < 1)
		return -1;

	if (convert_utf8_to_utf16(ru_Hello_UTF8, ru_Hello_UTF8_Length, ru_Hello_UTF16, ru_Hello_UTF16_Length) < 1)
		return -1;
	if (convert_utf8_to_utf16(ru_HowAreYou_UTF8, ru_HowAreYou_UTF8_Length, ru_HowAreYou_UTF16, ru_HowAreYou_UTF16_Length) < 1)
		return -1;

	if (convert_utf8_to_utf16(ar_Hello_UTF8, ar_Hello_UTF8_Length, ar_Hello_UTF16, ar_Hello_UTF16_Length) < 1)
		return -1;
	if (convert_utf8_to_utf16(ar_HowAreYou_UTF8, ar_HowAreYou_UTF8_Length, ar_HowAreYou_UTF16, ar_HowAreYou_UTF16_Length) < 1)
		return -1;

	if (convert_utf8_to_utf16(ch_Hello_UTF8, ch_Hello_UTF8_Length, ch_Hello_UTF16, ch_Hello_UTF16_Length) < 1)
		return -1;
	if (convert_utf8_to_utf16(ch_HowAreYou_UTF8, ch_HowAreYou_UTF8_Length, ch_HowAreYou_UTF16, ch_HowAreYou_UTF16_Length) < 1)
		return -1;

	return 0;
}
