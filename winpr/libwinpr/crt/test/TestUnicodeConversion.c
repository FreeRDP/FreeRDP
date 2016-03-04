
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/error.h>
#include <winpr/windows.h>

/* Letters */

static BYTE c_cedilla_UTF8[] = "\xC3\xA7\x00";
static BYTE c_cedilla_UTF16[] = "\xE7\x00\x00\x00";
static int c_cedilla_cchWideChar = 2;
static int c_cedilla_cbMultiByte = 3;

/* English */

static BYTE en_Hello_UTF8[] = "Hello\0";
static BYTE en_Hello_UTF16[] = "\x48\x00\x65\x00\x6C\x00\x6C\x00\x6F\x00\x00\x00";
static int en_Hello_cchWideChar = 6;
static int en_Hello_cbMultiByte = 6;

static BYTE en_HowAreYou_UTF8[] = "How are you?\0";
static BYTE en_HowAreYou_UTF16[] = "\x48\x00\x6F\x00\x77\x00\x20\x00\x61\x00\x72\x00\x65\x00\x20\x00"
		"\x79\x00\x6F\x00\x75\x00\x3F\x00\x00\x00";
static int en_HowAreYou_cchWideChar = 13;
static int en_HowAreYou_cbMultiByte = 13;

/* French */

static BYTE fr_Hello_UTF8[] = "Allo\0";
static BYTE fr_Hello_UTF16[] = "\x41\x00\x6C\x00\x6C\x00\x6F\x00\x00\x00";
static int fr_Hello_cchWideChar = 5;
static int fr_Hello_cbMultiByte = 5;

static BYTE fr_HowAreYou_UTF8[] = "\x43\x6F\x6D\x6D\x65\x6E\x74\x20\xC3\xA7\x61\x20\x76\x61\x3F\x00";
static BYTE fr_HowAreYou_UTF16[] = "\x43\x00\x6F\x00\x6D\x00\x6D\x00\x65\x00\x6E\x00\x74\x00\x20\x00"
		"\xE7\x00\x61\x00\x20\x00\x76\x00\x61\x00\x3F\x00\x00\x00";
static int fr_HowAreYou_cchWideChar = 15;
static int fr_HowAreYou_cbMultiByte = 16;

/* Russian */

static BYTE ru_Hello_UTF8[] = "\xD0\x97\xD0\xB4\xD0\xBE\xD1\x80\xD0\xBE\xD0\xB2\xD0\xBE\x00";
static BYTE ru_Hello_UTF16[] = "\x17\x04\x34\x04\x3E\x04\x40\x04\x3E\x04\x32\x04\x3E\x04\x00\x00";
static int ru_Hello_cchWideChar = 8;
static int ru_Hello_cbMultiByte = 15;

static BYTE ru_HowAreYou_UTF8[] = "\xD0\x9A\xD0\xB0\xD0\xBA\x20\xD0\xB4\xD0\xB5\xD0\xBB\xD0\xB0\x3F\x00";
static BYTE ru_HowAreYou_UTF16[] = "\x1A\x04\x30\x04\x3A\x04\x20\x00\x34\x04\x35\x04\x3B\x04\x30\x04"
		"\x3F\x00\x00\x00";
static int ru_HowAreYou_cchWideChar = 10;
static int ru_HowAreYou_cbMultiByte = 17;

/* Arabic */

static BYTE ar_Hello_UTF8[] = "\xD8\xA7\xD9\x84\xD8\xB3\xD9\x84\xD8\xA7\xD9\x85\x20\xD8\xB9\xD9"
		"\x84\xD9\x8A\xD9\x83\xD9\x85\x00";
static BYTE ar_Hello_UTF16[] = "\x27\x06\x44\x06\x33\x06\x44\x06\x27\x06\x45\x06\x20\x00\x39\x06"
		"\x44\x06\x4A\x06\x43\x06\x45\x06\x00\x00";
static int ar_Hello_cchWideChar = 13;
static int ar_Hello_cbMultiByte = 24;

static BYTE ar_HowAreYou_UTF8[] = "\xD9\x83\xD9\x8A\xD9\x81\x20\xD8\xAD\xD8\xA7\xD9\x84\xD9\x83\xD8"
		"\x9F\x00";
static BYTE ar_HowAreYou_UTF16[] = "\x43\x06\x4A\x06\x41\x06\x20\x00\x2D\x06\x27\x06\x44\x06\x43\x06"
		"\x1F\x06\x00\x00";
static int ar_HowAreYou_cchWideChar = 10;
static int ar_HowAreYou_cbMultiByte = 18;

/* Chinese */

static BYTE ch_Hello_UTF8[] = "\xE4\xBD\xA0\xE5\xA5\xBD\x00";
static BYTE ch_Hello_UTF16[] = "\x60\x4F\x7D\x59\x00\x00";
static int ch_Hello_cchWideChar = 3;
static int ch_Hello_cbMultiByte = 7;

static BYTE ch_HowAreYou_UTF8[] = "\xE4\xBD\xA0\xE5\xA5\xBD\xE5\x90\x97\x00";
static BYTE ch_HowAreYou_UTF16[] = "\x60\x4F\x7D\x59\x17\x54\x00\x00";
static int ch_HowAreYou_cchWideChar = 4;
static int ch_HowAreYou_cbMultiByte = 10;

/* Uppercasing */

static BYTE ru_Administrator_lower[] =
		"\xd0\x90\xd0\xb4\xd0\xbc\xd0\xb8\xd0\xbd\xd0\xb8\xd1\x81\xd1\x82\xd1\x80\xd0\xb0\xd1\x82\xd0\xbe\xd1\x80\x00";

static BYTE ru_Administrator_upper[] =
		"\xd0\x90\xd0\x94\xd0\x9c\xd0\x98\xd0\x9d\xd0\x98\xd0\xa1\xd0\xa2\xd0\xa0\xd0\x90\xd0\xa2\xd0\x9e\xd0\xa0\x00";

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

int convert_utf8_to_utf16(BYTE* lpMultiByteStr, BYTE* expected_lpWideCharStr, int expected_cchWideChar)
{
	int length;
	int cbMultiByte;
	int cchWideChar;
	LPWSTR lpWideCharStr;

	cbMultiByte = strlen((char*) lpMultiByteStr);
	cchWideChar = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR) lpMultiByteStr, -1, NULL, 0);

	printf("MultiByteToWideChar Input UTF8 String:\n");
	string_hexdump(lpMultiByteStr, cbMultiByte + 1);

	printf("MultiByteToWideChar required cchWideChar: %d\n", cchWideChar);

	if (cchWideChar != expected_cchWideChar)
	{
		printf("MultiByteToWideChar unexpected cchWideChar: actual: %d expected: %d\n",
			cchWideChar, expected_cchWideChar);
		return -1;
	}

	lpWideCharStr = (LPWSTR) malloc(cchWideChar * sizeof(WCHAR));
	if (!lpWideCharStr)
	{
		printf("MultiByteToWideChar: unable to allocate memory for test\n");
		return -1;
	}
	lpWideCharStr[cchWideChar - 1] = 0xFFFF; /* should be overwritten if null terminator is inserted properly */
	length = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR) lpMultiByteStr, cbMultiByte + 1, lpWideCharStr, cchWideChar);

	printf("MultiByteToWideChar converted length (WCHAR): %d\n", length);

	if (!length)
	{
		DWORD error = GetLastError();
		printf("MultiByteToWideChar error: 0x%08X\n", error);
		return -1;
	}

	if (length != expected_cchWideChar)
	{
		printf("MultiByteToWideChar unexpected converted length (WCHAR): actual: %d expected: %d\n",
			length, expected_cchWideChar);
		return -1;
	}

	if (_wcscmp(lpWideCharStr, (WCHAR*) expected_lpWideCharStr) != 0)
	{
		printf("MultiByteToWideChar unexpected string:\n");

		printf("UTF8 String:\n");
		string_hexdump(lpMultiByteStr, cbMultiByte + 1);

		printf("UTF16 String (actual):\n");
		string_hexdump((BYTE*) lpWideCharStr, length * sizeof(WCHAR));

		printf("UTF16 String (expected):\n");
		string_hexdump((BYTE*) expected_lpWideCharStr, expected_cchWideChar * sizeof(WCHAR));

		return -1;
	}

	printf("MultiByteToWideChar Output UTF16 String:\n");
	string_hexdump((BYTE*) lpWideCharStr, length * sizeof(WCHAR));
	printf("\n");

	free(lpWideCharStr);

	return length;
}

int convert_utf16_to_utf8(BYTE* lpWideCharStr, BYTE* expected_lpMultiByteStr, int expected_cbMultiByte)
{
	int length;
	int cchWideChar;
	int cbMultiByte;
	LPSTR lpMultiByteStr = NULL;

	cchWideChar = _wcslen((WCHAR*) lpWideCharStr);
	cbMultiByte = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR) lpWideCharStr, -1, NULL, 0, NULL, NULL);

	printf("WideCharToMultiByte Input UTF16 String:\n");
	string_hexdump(lpWideCharStr, (cchWideChar + 1) * sizeof(WCHAR));

	printf("WideCharToMultiByte required cbMultiByte: %d\n", cbMultiByte);

	if (cbMultiByte != expected_cbMultiByte)
	{
		printf("WideCharToMultiByte unexpected cbMultiByte: actual: %d expected: %d\n",
			cbMultiByte, expected_cbMultiByte);
		return -1;
	}

	lpMultiByteStr = (LPSTR) malloc(cbMultiByte);
	if (!lpMultiByteStr)
	{
		printf("WideCharToMultiByte: unable to allocate memory for test\n");
		return -1;
	}
	lpMultiByteStr[cbMultiByte - 1] = 0xFF; /* should be overwritten if null terminator is inserted properly */
	length = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR) lpWideCharStr, cchWideChar + 1, lpMultiByteStr, cbMultiByte, NULL, NULL);

	printf("WideCharToMultiByte converted length (BYTE): %d\n", length);

	if (!length)
	{
		DWORD error = GetLastError();
		printf("WideCharToMultiByte error: 0x%08X\n", error);
		return -1;
	}

	if (length != expected_cbMultiByte)
	{
		printf("WideCharToMultiByte unexpected converted length (BYTE): actual: %d expected: %d\n",
			length, expected_cbMultiByte);
		return -1;
	}

	if (strcmp(lpMultiByteStr, (char*) expected_lpMultiByteStr) != 0)
	{
		printf("WideCharToMultiByte unexpected string:\n");

		printf("UTF16 String:\n");
		string_hexdump((BYTE*) lpWideCharStr, (cchWideChar + 1) * sizeof(WCHAR));

		printf("UTF8 String (actual):\n");
		string_hexdump((BYTE*) lpMultiByteStr, cbMultiByte);

		printf("UTF8 String (expected):\n");
		string_hexdump((BYTE*) expected_lpMultiByteStr, expected_cbMultiByte);

		return -1;
	}

	printf("WideCharToMultiByte Output UTF8 String:\n");
	string_hexdump((BYTE*) lpMultiByteStr, cbMultiByte);
	printf("\n");

	free(lpMultiByteStr);

	return length;
}

BOOL test_unicode_uppercasing(BYTE* lower, BYTE* upper)
{
	WCHAR* lowerW = NULL;
	int lowerLength;
	WCHAR* upperW = NULL;
	int upperLength;

	lowerLength = ConvertToUnicode(CP_UTF8, 0, (LPSTR) lower, -1, &lowerW, 0);
	upperLength = ConvertToUnicode(CP_UTF8, 0, (LPSTR) upper, -1, &upperW, 0);

	CharUpperBuffW(lowerW, lowerLength);

	if (_wcscmp(lowerW, upperW) != 0)
	{
		printf("Lowercase String:\n");
		string_hexdump((BYTE*) lowerW, lowerLength * 2);

		printf("Uppercase String:\n");
		string_hexdump((BYTE*) upperW, upperLength * 2);

		return FALSE;
	}

	free(lowerW);
	free(upperW);

	printf("success\n\n");
	return TRUE;
}

BOOL test_ConvertFromUnicode_wrapper()
{
	/*               00  01  02  03  04  05  06  07  08  09  10  11  12  13  14  15  16  17  18 */
	WCHAR src1[] = { 'R','I','C','H',' ','T','E','X','T',' ','F','O','R','M','A','T','@','@','@' };
	WCHAR src2[] = { 'R','I','C','H',' ','T','E','X','T',' ','F','O','R','M','A','T', 0 };
	CHAR  cmp0[] = { 'R','I','C','H',' ','T','E','X','T',' ','F','O','R','M','A','T', 0 };
	CHAR* dst = NULL;
	int i;

	/* Test unterminated unicode string:
	 * ConvertFromUnicode must always null-terminate, even if the src string isn't
	 */

	printf("Input UTF16 String:\n");
	string_hexdump((BYTE*) src1, 19 * sizeof(WCHAR));

	i = ConvertFromUnicode(CP_UTF8, 0, src1, 16, &dst, 0, NULL, NULL);
	if (i != 16)
	{
		fprintf(stderr, "ConvertFromUnicode failure A1: unexpectedly returned %d instead of 16\n", i);
		goto fail;
	}
	if (dst == NULL)
	{
		fprintf(stderr, "ConvertFromUnicode failure A2: destination ist NULL\n");
		goto fail;
	}
	if ((i = strlen(dst)) != 16)
	{
		fprintf(stderr, "ConvertFromUnicode failure A3: dst length is %d instead of 16\n", i);
		goto fail;
	}
	if (strcmp(dst, cmp0))
	{
		fprintf(stderr, "ConvertFromUnicode failure A4: data mismatch\n");
		goto fail;
	}
	printf("Output UTF8 String:\n");
	string_hexdump((BYTE*) dst, i + 1);

	free(dst);
	dst = NULL;

	/* Test null-terminated string */

	printf("Input UTF16 String:\n");
	string_hexdump((BYTE*) src2, (_wcslen(src2) + 1 ) * sizeof(WCHAR));

	i = ConvertFromUnicode(CP_UTF8, 0, src2, -1, &dst, 0, NULL, NULL);
	if (i != 17)
	{
		fprintf(stderr, "ConvertFromUnicode failure B1: unexpectedly returned %d instead of 17\n", i);
		goto fail;
	}
	if (dst == NULL)
	{
		fprintf(stderr, "ConvertFromUnicode failure B2: destination ist NULL\n");
		goto fail;
	}
	if ((i = strlen(dst)) != 16)
	{
		fprintf(stderr, "ConvertFromUnicode failure B3: dst length is %d instead of 16\n", i);
		goto fail;
	}
	if (strcmp(dst, cmp0))
	{
		fprintf(stderr, "ConvertFromUnicode failure B: data mismatch\n");
		goto fail;
	}
	printf("Output UTF8 String:\n");
	string_hexdump((BYTE*) dst, i + 1);

	free(dst);
	dst = NULL;

	printf("success\n\n");

	return TRUE;

fail:
	free(dst);
	return FALSE;
}

BOOL test_ConvertToUnicode_wrapper()
{
	/*               00  01  02  03  04  05  06  07  08  09  10  11  12  13  14  15  16  17  18 */
	CHAR  src1[] = { 'R','I','C','H',' ','T','E','X','T',' ','F','O','R','M','A','T','@','@','@' };
	CHAR  src2[] = { 'R','I','C','H',' ','T','E','X','T',' ','F','O','R','M','A','T', 0 };
	WCHAR cmp0[] = { 'R','I','C','H',' ','T','E','X','T',' ','F','O','R','M','A','T', 0 };
	WCHAR* dst = NULL;
	int i;

	/* Test unterminated unicode string:
	 * ConvertToUnicode must always null-terminate, even if the src string isn't
	 */

	printf("Input UTF8 String:\n");
	string_hexdump((BYTE*) src1, 19);

	i = ConvertToUnicode(CP_UTF8, 0, src1, 16, &dst, 0);
	if (i != 16)
	{
		fprintf(stderr, "ConvertToUnicode failure A1: unexpectedly returned %d instead of 16\n", i);
		goto fail;
	}
	if (dst == NULL)
	{
		fprintf(stderr, "ConvertToUnicode failure A2: destination ist NULL\n");
		goto fail;
	}
	if ((i = _wcslen(dst)) != 16)
	{
		fprintf(stderr, "ConvertToUnicode failure A3: dst length is %d instead of 16\n", i);
		goto fail;
	}
	if (_wcscmp(dst, cmp0))
	{
		fprintf(stderr, "ConvertToUnicode failure A4: data mismatch\n");
		goto fail;
	}
	printf("Output UTF16 String:\n");
	string_hexdump((BYTE*) dst, (i + 1) * sizeof(WCHAR));

	free(dst);
	dst = NULL;

	/* Test null-terminated string */

	printf("Input UTF8 String:\n");
	string_hexdump((BYTE*) src2, strlen(src2) + 1);

	i = ConvertToUnicode(CP_UTF8, 0, src2, -1, &dst, 0);
	if (i != 17)
	{
		fprintf(stderr, "ConvertToUnicode failure B1: unexpectedly returned %d instead of 17\n", i);
		goto fail;
	}
	if (dst == NULL)
	{
		fprintf(stderr, "ConvertToUnicode failure B2: destination ist NULL\n");
		goto fail;
	}
	if ((i = _wcslen(dst)) != 16)
	{
		fprintf(stderr, "ConvertToUnicode failure B3: dst length is %d instead of 16\n", i);
		goto fail;
	}
	if (_wcscmp(dst, cmp0))
	{
		fprintf(stderr, "ConvertToUnicode failure B: data mismatch\n");
		goto fail;
	}
	printf("Output UTF16 String:\n");
	string_hexdump((BYTE*) dst, (i + 1) * 2);

	free(dst);
	dst = NULL;

	printf("success\n\n");

	return TRUE;

fail:
	free(dst);
	return FALSE;
}

int TestUnicodeConversion(int argc, char* argv[])
{
	/* Letters */

	printf("Letters\n");

	if (convert_utf8_to_utf16(c_cedilla_UTF8, c_cedilla_UTF16, c_cedilla_cchWideChar) < 1)
		return -1;

	if (convert_utf16_to_utf8(c_cedilla_UTF16, c_cedilla_UTF8, c_cedilla_cbMultiByte) < 1)
		return -1;
	
	/* English */

	printf("English\n");

	if (convert_utf8_to_utf16(en_Hello_UTF8, en_Hello_UTF16, en_Hello_cchWideChar) < 1)
		return -1;
	if (convert_utf8_to_utf16(en_HowAreYou_UTF8, en_HowAreYou_UTF16, en_HowAreYou_cchWideChar) < 1)
		return -1;

	if (convert_utf16_to_utf8(en_Hello_UTF16, en_Hello_UTF8, en_Hello_cbMultiByte) < 1)
		return -1;
	if (convert_utf16_to_utf8(en_HowAreYou_UTF16, en_HowAreYou_UTF8, en_HowAreYou_cbMultiByte) < 1)
		return -1;

	/* French */

	printf("French\n");

	if (convert_utf8_to_utf16(fr_Hello_UTF8, fr_Hello_UTF16, fr_Hello_cchWideChar) < 1)
		return -1;
	if (convert_utf8_to_utf16(fr_HowAreYou_UTF8, fr_HowAreYou_UTF16, fr_HowAreYou_cchWideChar) < 1)
		return -1;

	if (convert_utf16_to_utf8(fr_Hello_UTF16, fr_Hello_UTF8, fr_Hello_cbMultiByte) < 1)
		return -1;
	if (convert_utf16_to_utf8(fr_HowAreYou_UTF16, fr_HowAreYou_UTF8, fr_HowAreYou_cbMultiByte) < 1)
		return -1;

	/* Russian */

	printf("Russian\n");

	if (convert_utf8_to_utf16(ru_Hello_UTF8, ru_Hello_UTF16, ru_Hello_cchWideChar) < 1)
		return -1;
	if (convert_utf8_to_utf16(ru_HowAreYou_UTF8, ru_HowAreYou_UTF16, ru_HowAreYou_cchWideChar) < 1)
		return -1;

	if (convert_utf16_to_utf8(ru_Hello_UTF16, ru_Hello_UTF8, ru_Hello_cbMultiByte) < 1)
		return -1;
	if (convert_utf16_to_utf8(ru_HowAreYou_UTF16, ru_HowAreYou_UTF8, ru_HowAreYou_cbMultiByte) < 1)
		return -1;

	/* Arabic */

	printf("Arabic\n");

	if (convert_utf8_to_utf16(ar_Hello_UTF8, ar_Hello_UTF16, ar_Hello_cchWideChar) < 1)
		return -1;
	if (convert_utf8_to_utf16(ar_HowAreYou_UTF8, ar_HowAreYou_UTF16, ar_HowAreYou_cchWideChar) < 1)
		return -1;

	if (convert_utf16_to_utf8(ar_Hello_UTF16, ar_Hello_UTF8, ar_Hello_cbMultiByte) < 1)
		return -1;
	if (convert_utf16_to_utf8(ar_HowAreYou_UTF16, ar_HowAreYou_UTF8, ar_HowAreYou_cbMultiByte) < 1)
		return -1;

	/* Chinese */

	printf("Chinese\n");

	if (convert_utf8_to_utf16(ch_Hello_UTF8, ch_Hello_UTF16, ch_Hello_cchWideChar) < 1)
		return -1;
	if (convert_utf8_to_utf16(ch_HowAreYou_UTF8, ch_HowAreYou_UTF16, ch_HowAreYou_cchWideChar) < 1)
		return -1;

	if (convert_utf16_to_utf8(ch_Hello_UTF16, ch_Hello_UTF8, ch_Hello_cbMultiByte) < 1)
		return -1;
	if (convert_utf16_to_utf8(ch_HowAreYou_UTF16, ch_HowAreYou_UTF8, ch_HowAreYou_cbMultiByte) < 1)
		return -1;

	/* Uppercasing */

	printf("Uppercasing\n");

	if (!test_unicode_uppercasing(ru_Administrator_lower, ru_Administrator_upper))
		return -1;

	/* ConvertFromUnicode */

	printf("ConvertFromUnicode\n");

	if (!test_ConvertFromUnicode_wrapper())
		return -1;

	/* ConvertToUnicode */

	printf("ConvertToUnicode\n");

	if (!test_ConvertToUnicode_wrapper())
		return -1;
/*

	printf("----------------------------------------------------------\n\n");

	if (0)
	{
		BYTE src[] = { 'R',0,'I',0,'C',0,'H',0,' ',0, 'T',0,'E',0,'X',0,'T',0,' ',0,'F',0,'O',0,'R',0,'M',0,'A',0,'T',0,'@',0,'@',0 };
		//BYTE src[] = { 'R',0,'I',0,'C',0,'H',0,' ',0,  0,0,  'T',0,'E',0,'X',0,'T',0,' ',0,'F',0,'O',0,'R',0,'M',0,'A',0,'T',0,'@',0,'@',0 };
		//BYTE src[] = { 0,0,'R',0,'I',0,'C',0,'H',0,' ',0, 'T',0,'E',0,'X',0,'T',0,' ',0,'F',0,'O',0,'R',0,'M',0,'A',0,'T',0,'@',0,'@',0 };
		char* dst = NULL;
		int num;
		num = ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) src, 16, &dst, 0, NULL, NULL);
		printf("ConvertFromUnicode returned %d dst=[%s]\n", num, dst);
		string_hexdump((BYTE*)dst, num+1);
	}
	if (1)
	{
		char src[] = "RICH TEXT FORMAT@@@@@@";
		WCHAR *dst = NULL;
		int num;
		num = ConvertToUnicode(CP_UTF8, 0, src, 16, &dst, 0);
		printf("ConvertToUnicode returned %d dst=%p\n", num, dst);
		string_hexdump((BYTE*)dst, num * 2 + 2);

	}
*/
	return 0;
}
