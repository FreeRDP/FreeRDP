
#include <winpr/crt.h>
#include <winpr/winpr.h>
#include <winpr/collections.h>

/**
 * Ternary Raster Operations:
 * See "Windows Graphics Programming: Win32 GDI and DirectDraw", chapter 11. Advanced Bitmap Graphics
 *
 * Operators:
 *
 * 	AND		&		a
 * 	OR		|		o
 * 	NOT		~		n
 * 	XOR		^		x
 *
 * Operands:
 *
 * 	Pen/Brush			P
 * 	Destination			D
 * 	Source				S
 *
 * Example:
 *
 * Raster operation which returns P if S is 1 or D otherwise:
 * (rop_S & rop_P) | (~rop_S & rop_D); -> 0xE2 (0x00E20746)
 *
 * Postfix notation: DSPDxax
 * Infix notation: D^(S&(P^D))), (S&P)|(~S&D)
 *
 * DSPDxax using D^(S&(P^D)):
 *
 * 	mov eax, P	// P
 * 	xor eax, D	// P^D
 * 	and eax, S	// S&(P^D)
 * 	xor eax, D	// D^(S&(P^D))
 * 	mov D, eax	// write result
 *
 * DSPDxax using (S&P)|(~S&D):
 *
 * 	mov eax, S	// S
 * 	and eax, P	// S&P
 * 	mov ebx, S	// S
 * 	not ebx		// ~S
 * 	and ebx, D	// ~D&D
 * 	or eax, ebx	// (S&P)|(~S&D)
 * 	mov D, eax	// write result
 *
 * Raster operation lower word encoding:
 *
 *  _______________________________________________________________________________
 * |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |
 * |   Op5   |   Op4   |   Op3   |   Op2   |   Op1   | Not| Parse String |  Offset |
 * |____|____|____|____|____|____|____|____|____|____|____|____|____|____|____|____|
 *   15   14   13   12   11   10    9    8    7    6    5    4    3   2     1    0
 *
 * Operator:
 * 	0: NOT
 * 	1: XOR
 * 	2: OR
 * 	3: AND
 *
 * Parse String:
 * 	0: SPDDDDDD
 * 	1: SPDSPDSP
 * 	2: SDPSDPSD
 * 	3: DDDDDDDD
 * 	4: DDDDDDDD
 * 	5: S+SP-DSS
 * 	6: S+SP-PDS
 * 	7: S+SD-PDS
 *
 * The lower word for 0x00E20746 is 0x0746 (00000111 01000110)
 *
 * 00		Op5 (NOT, n)
 * 00		Op4 (NOT, n)
 * 01		Op3 (XOR, x)
 * 11		Op2 (AND, a)
 * 01		Op1 (XOR, x)
 * 0		Not (unused)
 * 001		String (SPDSPDSP)
 * 10		Offset (2)
 *
 * We shift SPDSPDSP to the left by 2: DSPDSPSP
 *
 * We have 5 operators: 3 binary operators and the last two are unary operators,
 * so only four operands are needed. The parse string is truncated to reflect
 * the number of operands we need: DSPD
 *
 * The operator string (from Op1 to Op5) is xaxnn, which can be simplified to xax
 *
 * The complete string representing the operation is DSPDxax
 *
 */

static char* gdi_convert_postfix_to_infix(const char* postfix)
{
	int i;
	int length;
	BOOL unary;
	wStack* stack;
	int al, bl, cl, dl;
	char* a, *b, *c, *d;
	bl = cl = dl = 0;
	stack = Stack_New(FALSE);
	length = strlen(postfix);

	for (i = 0; i < length; i++)
	{
		if ((postfix[i] == 'P') || (postfix[i] == 'D') || (postfix[i] == 'S'))
		{
			/* token is an operand, push on the stack */
			a = malloc(2);
			a[0] = postfix[i];
			a[1] = '\0';
			//printf("Operand: %s\n", a);
			Stack_Push(stack, a);
		}
		else
		{
			/* token is an operator */
			unary = FALSE;
			c = malloc(2);
			c[0] = postfix[i];
			c[1] = '\0';

			if (c[0] == 'a')
			{
				c[0] = '&';
			}
			else if (c[0] == 'o')
			{
				c[0] = '|';
			}
			else if (c[0] == 'n')
			{
				c[0] = '~';
				unary = TRUE;
			}
			else if (c[0] == 'x')
			{
				c[0] = '^';
			}
			else
			{
				printf("invalid operator: %c\n", c[0]);
			}

			//printf("Operator: %s\n", c);
			a = (char*) Stack_Pop(stack);

			if (unary)
				b = NULL;
			else
				b = (char*) Stack_Pop(stack);

			al = strlen(a);

			if (b)
				bl = strlen(b);

			cl = 1;
			dl = al + bl + cl + 3;
			d = malloc(dl + 1);
			sprintf_s(d, dl, "(%s%s%s)", b ? b : "", c, a);
			Stack_Push(stack, d);
			free(a);
			free(b);
			free(c);
		}
	}

	d = (char*) Stack_Pop(stack);
	Stack_Free(stack);
	return d;
}

static const char* test_ROP3[] =
{
	"DSPDxax",
	"PSDPxax",
	"SPna",
	"DSna",
	"DPa",
	"PDxn",
	"DSxn",
	"PSDnox",
	"PDSona",
	"DSPDxox",
	"DPSDonox",
	"SPDSxax",
	"DPon",
	"DPna",
	"Pn",
	"PDna",
	"DPan",
	"DSan",
	"DSxn",
	"DPa",
	"D",
	"DPno",
	"SDno",
	"PDno",
	"DPo"
};

int TestGdiRop3(int argc, char* argv[])
{
	int index;

	for (index = 0; index < sizeof(test_ROP3) / sizeof(test_ROP3[0]); index++)
	{
		const char* postfix = test_ROP3[index];
		char* infix = gdi_convert_postfix_to_infix(postfix);

		if (!infix)
			return -1;

		printf("%s\t\t%s\n", postfix, infix);
		free(infix);
	}

	return 0;
}
