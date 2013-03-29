
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

#if 0
#define _printf printf
#else
#define _printf wprintfx
#endif

int TestPrint(int argc, char* argv[])
{
	int a, b;
	float c, d;

	/**
	 * 7
	 *   7
	 * 007
	 * 5.10
	 */

	a = 15;
	b = a / 2;
	_printf("%d\n",b);
	_printf("%3d\n",b);
	_printf("%03d\n",b);
	c = 15.3f; d = c / 3;
	_printf("%3.2f\n",d);

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
	_printf("Just print the percentage sign %%\n", 10);

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

	return 0;
}
