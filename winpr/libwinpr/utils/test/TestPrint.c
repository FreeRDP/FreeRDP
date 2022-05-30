
#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/print.h>

/**
 * C Programming/C Reference/stdio.h/printf:
 * http://en.wikibooks.org/wiki/C_Programming/C_Reference/stdio.h/printf
 *
 * C Programming/Procedures and functions/printf:
 * http://en.wikibooks.org/wiki/C_Programming/Procedures_and_functions/printf
 *
 * C Tutorial – printf, Format Specifiers, Format Conversions and Formatted Output:
 * http://www.codingunit.com/printf-format-specifiers-format-conversions-and-formatted-output
 */

#define _printf printf

static BOOL test_bin_tohex_string(void)
{
	BOOL rc = FALSE;
	{
		const BYTE binbuffer[33] = { 0 };
		const char empty[33] = { 0 };
		char strbuffer[33] = { 0 };

		size_t len =
		    winpr_BinToHexStringBuffer(NULL, sizeof(binbuffer), strbuffer, sizeof(strbuffer), TRUE);
		if (len != 0)
			goto fail;
		if (memcmp(strbuffer, empty, sizeof(strbuffer)) != 0)
			goto fail;
		len = winpr_BinToHexStringBuffer(binbuffer, 0, strbuffer, sizeof(strbuffer), TRUE);
		if (len != 0)
			goto fail;
		if (memcmp(strbuffer, empty, sizeof(strbuffer)) != 0)
			goto fail;
		len =
		    winpr_BinToHexStringBuffer(binbuffer, sizeof(binbuffer), NULL, sizeof(strbuffer), TRUE);
		if (len != 0)
			goto fail;
		if (memcmp(strbuffer, empty, sizeof(strbuffer)) != 0)
			goto fail;
		len = winpr_BinToHexStringBuffer(binbuffer, sizeof(binbuffer), strbuffer, 0, TRUE);
		if (len != 0)
			goto fail;
		if (memcmp(strbuffer, empty, sizeof(strbuffer)) != 0)
			goto fail;
		len = winpr_BinToHexStringBuffer(binbuffer, 0, strbuffer, 0, TRUE);
		if (len != 0)
			goto fail;
		if (memcmp(strbuffer, empty, sizeof(strbuffer)) != 0)
			goto fail;
		len = winpr_BinToHexStringBuffer(binbuffer, sizeof(binbuffer), NULL, 0, TRUE);
		if (len != 0)
			goto fail;
		if (memcmp(strbuffer, empty, sizeof(strbuffer)) != 0)
			goto fail;
		len = winpr_BinToHexStringBuffer(NULL, sizeof(binbuffer), strbuffer, 0, TRUE);
		if (len != 0)
			goto fail;
		if (memcmp(strbuffer, empty, sizeof(strbuffer)) != 0)
			goto fail;

		len = winpr_BinToHexStringBuffer(binbuffer, 0, NULL, 0, TRUE);
		if (len != 0)
			goto fail;
		if (memcmp(strbuffer, empty, sizeof(strbuffer)) != 0)
			goto fail;
		len = winpr_BinToHexStringBuffer(NULL, 0, NULL, 0, TRUE);
		if (len != 0)
			goto fail;
		if (memcmp(strbuffer, empty, sizeof(strbuffer)) != 0)
			goto fail;
		len = winpr_BinToHexStringBuffer(binbuffer, 0, NULL, 0, FALSE);
		if (len != 0)
			goto fail;
		if (memcmp(strbuffer, empty, sizeof(strbuffer)) != 0)
			goto fail;
	}
	{
		const BYTE binbuffer1[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17 };
		const char strbuffer1[] = "0102030405060708090A0B0C0D0E0F1011";
		const char strbuffer1_space[] = "01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F 10 11";

		char buffer[1024] = { 0 };
		size_t len = winpr_BinToHexStringBuffer(binbuffer1, sizeof(binbuffer1), buffer,
		                                        sizeof(buffer), FALSE);
		if (len != strnlen(strbuffer1, sizeof(strbuffer1)))
			goto fail;
		if (memcmp(strbuffer1, buffer, sizeof(strbuffer1)) != 0)
			goto fail;
		len = winpr_BinToHexStringBuffer(binbuffer1, sizeof(binbuffer1), buffer, sizeof(buffer),
		                                 TRUE);
		if (len != strnlen(strbuffer1_space, sizeof(strbuffer1_space)))
			goto fail;
		if (memcmp(strbuffer1_space, buffer, sizeof(strbuffer1_space)) != 0)
			goto fail;
	}
	{
		const BYTE binbuffer1[] = { 0xF1, 0xe2, 0xD3, 0xc4, 0xB5, 0xA6, 0x97, 0x88, 0x79,
			                        0x6A, 0x5b, 0x4C, 0x3d, 0x2E, 0x1f, 0x00, 0xfF };
		const char strbuffer1[] = "F1E2D3C4B5A69788796A5B4C3D2E1F00FF";
		const char strbuffer1_space[] = "F1 E2 D3 C4 B5 A6 97 88 79 6A 5B 4C 3D 2E 1F 00 FF";
		char buffer[1024] = { 0 };
		size_t len = winpr_BinToHexStringBuffer(binbuffer1, sizeof(binbuffer1), buffer,
		                                        sizeof(buffer), FALSE);
		if (len != strnlen(strbuffer1, sizeof(strbuffer1)))
			goto fail;
		if (memcmp(strbuffer1, buffer, sizeof(strbuffer1)) != 0)
			goto fail;
		len = winpr_BinToHexStringBuffer(binbuffer1, sizeof(binbuffer1), buffer, sizeof(buffer),
		                                 TRUE);
		if (len != strnlen(strbuffer1_space, sizeof(strbuffer1_space)))
			goto fail;
		if (memcmp(strbuffer1_space, buffer, sizeof(strbuffer1_space)) != 0)
			goto fail;
	}
	{
	}
	rc = TRUE;
fail:
	return rc;
}

static BOOL test_bin_tohex_string_alloc(void)
{
	BOOL rc = FALSE;
	char* str = NULL;
	{
		const BYTE binbuffer[33] = { 0 };

		str = winpr_BinToHexString(NULL, sizeof(binbuffer), TRUE);
		if (str)
			goto fail;
		str = winpr_BinToHexString(binbuffer, 0, TRUE);
		if (str)
			goto fail;
		str = winpr_BinToHexString(binbuffer, 0, FALSE);
		if (str)
			goto fail;
		str = winpr_BinToHexString(NULL, sizeof(binbuffer), FALSE);
		if (str)
			goto fail;
	}
	{
		const BYTE binbuffer1[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17 };
		const char strbuffer1[] = "0102030405060708090A0B0C0D0E0F1011";
		const char strbuffer1_space[] = "01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F 10 11";

		str = winpr_BinToHexString(binbuffer1, sizeof(binbuffer1), FALSE);
		if (!str)
			goto fail;
		if (memcmp(strbuffer1, str, sizeof(strbuffer1)) != 0)
			goto fail;
		free(str);
		str = winpr_BinToHexString(binbuffer1, sizeof(binbuffer1), TRUE);
		if (!str)
			goto fail;
		if (memcmp(strbuffer1_space, str, sizeof(strbuffer1_space)) != 0)
			goto fail;
		free(str);
		str = NULL;
	}
	{
		const BYTE binbuffer1[] = { 0xF1, 0xe2, 0xD3, 0xc4, 0xB5, 0xA6, 0x97, 0x88, 0x79,
			                        0x6A, 0x5b, 0x4C, 0x3d, 0x2E, 0x1f, 0x00, 0xfF };
		const char strbuffer1[] = "F1E2D3C4B5A69788796A5B4C3D2E1F00FF";
		const char strbuffer1_space[] = "F1 E2 D3 C4 B5 A6 97 88 79 6A 5B 4C 3D 2E 1F 00 FF";
		str = winpr_BinToHexString(binbuffer1, sizeof(binbuffer1), FALSE);
		if (!str)
			goto fail;
		if (memcmp(strbuffer1, str, sizeof(strbuffer1)) != 0)
			goto fail;

		free(str);
		str = winpr_BinToHexString(binbuffer1, sizeof(binbuffer1), TRUE);
		if (!str)
			goto fail;
		if (memcmp(strbuffer1_space, str, sizeof(strbuffer1_space)) != 0)
			goto fail;
		free(str);
		str = NULL;
	}
	rc = TRUE;
fail:
	free(str);
	return rc;
}

static BOOL test_hex_string_to_bin(void)
{
	BOOL rc = FALSE;
	{
		const char stringbuffer[] = "123456789ABCDEFabcdef";
		const BYTE empty[1024] = { 0 };
		BYTE buffer[1024] = { 0 };
		size_t len = winpr_HexStringToBinBuffer(NULL, 0, NULL, 0);
		if (len != 0)
			goto fail;
		if (memcmp(buffer, empty, sizeof(buffer)) != 0)
			goto fail;
		len = winpr_HexStringToBinBuffer(NULL, sizeof(stringbuffer), buffer, sizeof(buffer));
		if (len != 0)
			goto fail;
		if (memcmp(buffer, empty, sizeof(buffer)) != 0)
			goto fail;
		len = winpr_HexStringToBinBuffer(stringbuffer, 0, buffer, sizeof(buffer));
		if (len != 0)
			goto fail;
		if (memcmp(buffer, empty, sizeof(buffer)) != 0)
			goto fail;
		len = winpr_HexStringToBinBuffer(stringbuffer, sizeof(stringbuffer), NULL, sizeof(buffer));
		if (len != 0)
			goto fail;
		if (memcmp(buffer, empty, sizeof(buffer)) != 0)
			goto fail;
		len = winpr_HexStringToBinBuffer(stringbuffer, sizeof(stringbuffer), buffer, 0);
		if (len != 0)
			goto fail;
		if (memcmp(buffer, empty, sizeof(buffer)) != 0)
			goto fail;
	}
	{
		const char stringbuffer[] = "123456789ABCDEF1abcdef";
		const BYTE expected[] = {
			0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf1, 0xab, 0xcd, 0xef
		};
		BYTE buffer[32] = { 0 };
		size_t len =
		    winpr_HexStringToBinBuffer(stringbuffer, sizeof(stringbuffer), buffer, sizeof(buffer));
		if (len != sizeof(expected))
			goto fail;
		if (memcmp(buffer, expected, sizeof(expected)) != 0)
			goto fail;
		len = winpr_HexStringToBinBuffer(stringbuffer, sizeof(stringbuffer), buffer,
		                                 sizeof(expected) / 2);
		if (len != sizeof(expected) / 2)
			goto fail;
		if (memcmp(buffer, expected, sizeof(expected) / 2) != 0)
			goto fail;
	}
	{
		const char stringbuffer[] = "12 34 56 78 9A BC DE F1 ab cd ef";
		const BYTE expected[] = {
			0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf1, 0xab, 0xcd, 0xef
		};
		BYTE buffer[1024] = { 0 };
		size_t len =
		    winpr_HexStringToBinBuffer(stringbuffer, sizeof(stringbuffer), buffer, sizeof(buffer));
		if (len != sizeof(expected))
			goto fail;
		if (memcmp(buffer, expected, sizeof(expected)) != 0)
			goto fail;
		len = winpr_HexStringToBinBuffer(stringbuffer, sizeof(stringbuffer), buffer,
		                                 sizeof(expected) / 2);
		if (len != sizeof(expected) / 2)
			goto fail;
		if (memcmp(buffer, expected, sizeof(expected) / 2) != 0)
			goto fail;
	}
	{
		const char stringbuffer[] = "123456789ABCDEF1abcdef9";
		const BYTE expected[] = { 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc,
			                      0xde, 0xf1, 0xab, 0xcd, 0xef, 0x09 };
		BYTE buffer[1024] = { 0 };
		size_t len =
		    winpr_HexStringToBinBuffer(stringbuffer, sizeof(stringbuffer), buffer, sizeof(buffer));
		if (len != sizeof(expected))
			goto fail;
		if (memcmp(buffer, expected, sizeof(expected)) != 0)
			goto fail;
		len = winpr_HexStringToBinBuffer(stringbuffer, sizeof(stringbuffer), buffer,
		                                 sizeof(expected) / 2);
		if (len != sizeof(expected) / 2)
			goto fail;
		if (memcmp(buffer, expected, sizeof(expected) / 2) != 0)
			goto fail;
	}
	{
		const char stringbuffer[] = "12 34 56 78 9A BC DE F1 ab cd ef 9";
		const BYTE expected[] = { 0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc,
			                      0xde, 0xf1, 0xab, 0xcd, 0xef, 0x09 };
		BYTE buffer[1024] = { 0 };
		size_t len =
		    winpr_HexStringToBinBuffer(stringbuffer, sizeof(stringbuffer), buffer, sizeof(buffer));
		if (len != sizeof(expected))
			goto fail;
		if (memcmp(buffer, expected, sizeof(expected)) != 0)
			goto fail;
		len = winpr_HexStringToBinBuffer(stringbuffer, sizeof(stringbuffer), buffer,
		                                 sizeof(expected) / 2);
		if (len != sizeof(expected) / 2)
			goto fail;
		if (memcmp(buffer, expected, sizeof(expected) / 2) != 0)
			goto fail;
	}
	rc = TRUE;
fail:
	return rc;
}

int TestPrint(int argc, char* argv[])
{
	int a, b;
	float c, d;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	/**
	 * 7
	 *   7
	 * 007
	 * 5.10
	 */

	a = 15;
	b = a / 2;
	_printf("%d\n", b);
	_printf("%3d\n", b);
	_printf("%03d\n", b);
	c = 15.3f;
	d = c / 3;
	_printf("%3.2f\n", d);

	/**
	 *   0 -17.778
	 *  20 -6.667
	 *  40 04.444
	 *  60 15.556
	 *  80 26.667
	 * 100 37.778
	 * 120 48.889
	 * 140 60.000
	 * 160 71.111
	 * 180 82.222
	 * 200 93.333
	 * 220 104.444
	 * 240 115.556
	 * 260 126.667
	 * 280 137.778
	 * 300 148.889
	 */

	for (a = 0; a <= 300; a = a + 20)
		_printf("%3d %06.3f\n", a, (5.0 / 9.0) * (a - 32));

	/**
	 * The color: blue
	 * First number: 12345
	 * Second number: 0025
	 * Third number: 1234
	 * Float number: 3.14
	 * Hexadecimal: ff
	 * Octal: 377
	 * Unsigned value: 150
	 * Just print the percentage sign %
	 */

	_printf("The color: %s\n", "blue");
	_printf("First number: %d\n", 12345);
	_printf("Second number: %04d\n", 25);
	_printf("Third number: %i\n", 1234);
	_printf("Float number: %3.2f\n", 3.14159);
	_printf("Hexadecimal: %x/%X\n", 255, 255);
	_printf("Octal: %o\n", 255);
	_printf("Unsigned value: %u\n", 150);
	_printf("Just print the percentage sign %%\n");

	/**
	 * :Hello, world!:
	 * :  Hello, world!:
	 * :Hello, wor:
	 * :Hello, world!:
	 * :Hello, world!  :
	 * :Hello, world!:
	 * :     Hello, wor:
	 * :Hello, wor     :
	 */

	_printf(":%s:\n", "Hello, world!");
	_printf(":%15s:\n", "Hello, world!");
	_printf(":%.10s:\n", "Hello, world!");
	_printf(":%-10s:\n", "Hello, world!");
	_printf(":%-15s:\n", "Hello, world!");
	_printf(":%.15s:\n", "Hello, world!");
	_printf(":%15.10s:\n", "Hello, world!");
	_printf(":%-15.10s:\n", "Hello, world!");

	if (!test_bin_tohex_string())
		return -1;
	if (!test_bin_tohex_string_alloc())
		return -1;
	if (!test_hex_string_to_bin())
		return -1;
	return 0;
}
