#include <stdio.h>
#include <winpr/wtypes.h>
#include <winpr/strlst.h>
#include "../str.h"

#define printref()  printf("%s:%d: Test %s:",__FILE__, __LINE__,__FUNCTION__)

#define ERROR(format, ...)						\
	do{								\
		fprintf(stderr, format, ##__VA_ARGS__);			\
		printref();						\
		printf(format, ##__VA_ARGS__);				\
		fflush(stdout);						\
	}while(0)

#define FAILURE(format, ...)						\
	do{								\
		printref();						\
		printf(" FAILURE ");					\
		printf(format, __VA_ARGS__);				\
		fflush(stdout);						\
	}while(0)


#define check(expression, compare, expected, format)				\
	check_internal(expression, compare, expected, format, result##__LINE__)
#define check_internal(expression, compare, expected, format, result)		\
	{									\
		typeof(expression) result = expression;				\
		if (!(result compare expected))					\
		{								\
			FAILURE("  %s %s %s\n %s resulted in "format"\n",	\
			        #expression, #compare, #expected,		\
			        #expression,  result);				\
			return FALSE;						\
		}								\
	}




#define countof(a)  (sizeof (a) / sizeof (a[0]))
#define no_convert(src, dst)	  dst = (BYTE *)src
#define no_free(p)		  (void)p
#define convert_to_utf8(src, dst) ConvertFromUnicode(CP_UTF8, 0, (WCHAR*)src, -1,(CHAR * *) &dst, 0, NULL, NULL)

static struct
{
	BYTE  astring[32];
	WCHAR wstring[32];
	struct
	{
		int expected;
		BYTE target[32];
	} tests[16];
} compares[] =
{
	{
		"hello world",
		{'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', 0},
		{
			{1, ""},
			{1, "h"},
			{1, "hello worl"},
			{0, "hello world"},
			{ -1, "hello world how do you do"},
			{1, "hello aaaaa"},
			{ -1, "hello zzzzz"},
			{ -2, "done"}
		}
	},
	{
		"m",
		{'m', 0},
		{
			{1, ""},
			{1, "h"},
			{1, "hh"},
			{0, "m"},
			{ -1, "mm"},
			{ -1, "z"},
			{ -2, "done"}
		}
	},
	{
		"",
		{0},
		{
			{0, ""},
			{ -1, "h"},
			{ -1, "hh"},
			{ -1, "m"},
			{ -1, "mm"},
			{ -1, "z"},
			{ -2, "done"}
		}
	}
};

BOOL test_compare()
{
	BOOL success = TRUE;
	int i;
	int j;
	BOOL widechar = FALSE;
#define test_compare_loop(string, convert, free)                                                                        \
	do                                                                                                              \
	{                                                                                                               \
		for (i = 0;i < countof(compares);i ++ )                                                                 \
		{                                                                                                       \
			BYTE * string = (BYTE *)compares[i].string;                                                     \
			for(j = 0;compares[i].tests[j].expected != -2;j ++ )                                            \
			{                                                                                               \
				int expected = compares[i].tests[j].expected;                                           \
				BYTE * target =(BYTE *)compares[i].tests[j].target;                                     \
				int result = compare(widechar, string, target);                                             \
				if (! (result == expected))                                                             \
				{                                                                                       \
					BYTE *  cstr = 0;                                                               \
					BYTE *  ctgt = 0;                                                               \
					convert(string, cstr);                                                          \
					convert(target, ctgt);                                                          \
					FAILURE("compare(char, %s, %s) failed!\n it resulted in %d,  expected % d\n",   \
					        cstr, ctgt, result, expected);                                          \
					free(cstr);                                                                     \
					free(ctgt);                                                                     \
					success = FALSE;                                                                \
				}                                                                                       \
			}                                                                                               \
		}                                                                                                       \
	}while(0)
	widechar = 0;
	test_compare_loop(astring, no_convert,	    no_free);
	widechar = 1;
	test_compare_loop(wstring, convert_to_utf8, free);
	return success;
}



static struct
{
	BYTE  astring[32];
	WCHAR wstring[32];
	struct
	{
		int expected;
		BYTE target[32];
		int max;
	} tests[64];
} ncompares[] =
{
	{
		"hello world",
		{'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', 0},
		{
			{1, "", 1000},
			{1, "h", 1000},
			{1, "hello worl", 1000},
			{0, "hello world", 1000},
			{ -1, "hello world how do you do", 1000},
			{1, "hello aaaaa", 1000},
			{ -1, "hello zzzzz", 1000},

			{0, "", 0},
			{0, "h", 0},
			{0, "hello worl", 0},
			{0, "hello world", 0},
			{0, "hello world how do you do", 0},
			{0, "hello aaaaa", 0},
			{0, "hello zzzzz", 0},

			{1, "", 5},
			{1, "h", 5},
			{0, "hello worl", 5},
			{0, "hello world", 5},
			{0, "hello world how do you do", 5},
			{0, "hello aaaaa", 5},
			{0, "hello zzzzz", 5},

			{1, "", 8},
			{1, "h", 8},
			{0, "hello worl", 8},
			{0, "hello world", 8},
			{0, "hello world how do you do", 8},
			{1, "hello aaaaa", 8},
			{ -1, "hello zzzzz", 8},

			{ -2, "done", 0}
		}
	},
	{
		"m",
		{'m', 0},
		{
			{1, "", 1000},
			{1, "h", 1000},
			{1, "hh", 1000},
			{0, "m", 1000},
			{ -1, "mm", 1000},
			{ -1, "z", 1000},

			{1, "", 1},
			{1, "h", 1},
			{1, "hh", 1},
			{0, "m", 1},
			{0, "mm", 1},
			{ -1, "z", 1},

			{1, "", 2},
			{1, "h", 2},
			{1, "hh", 2},
			{0, "m", 2},
			{ -1, "mm", 2},
			{ -1, "z", 2},

			{ -2, "done", 0},
		}
	},
	{
		"",
		{0},
		{
			{0, "", 1000},
			{ -1, "h", 1000},
			{ -1, "hh", 1000},
			{ -1, "m", 1000},
			{ -1, "mm", 1000},
			{ -1, "z", 1000},

			{0, "", 0},
			{0, "h", 0},
			{0, "hh", 0},
			{0, "m", 0},
			{0, "mm", 0},
			{0, "z", 0},

			{0, "", 1},
			{ -1, "h", 1},
			{ -1, "hh", 1},
			{ -1, "m", 1},
			{ -1, "mm", 1},
			{ -1, "z", 1},

			{ -2, "done", 0}
		}
	}
};


BOOL test_ncompare()
{
	BOOL success = TRUE;
	int i;
	int j;
	BOOL widechar = FALSE;
#define test_ncompare_loop(string, convert, free)                                                                               \
	do                                                                                                                      \
	{                                                                                                                       \
		for (i = 0;i < countof(ncompares);i ++ )                                                                        \
		{                                                                                                               \
			BYTE * string = (BYTE *)ncompares[i].string;                                                            \
			for(j = 0;ncompares[i].tests[j].expected != -2;j ++ )                                                   \
			{                                                                                                       \
				int expected = ncompares[i].tests[j].expected;                                                  \
				BYTE * target =(BYTE *)ncompares[i].tests[j].target;                                            \
				int max = ncompares[i].tests[j].max;                                                            \
				int result = ncompare(widechar, string, target, max);						\
				if (! (result == expected))                                                                     \
				{                                                                                               \
					BYTE *  cstr = 0;                                                                       \
					convert(string, cstr);                                                                  \
					FAILURE("ncompare(char, %s, %s, %d) failed!\n it resulted in %d,  expected % d\n",      \
					        cstr, target, max, result, expected );						\
					result = ncompare(widechar, string, target, max);					\
					free(cstr);                                                                             \
					success = FALSE;                                                                        \
				}                                                                                               \
			}                                                                                                       \
		}                                                                                                               \
	} while(0)
	widechar = 0;
	test_ncompare_loop(astring, no_convert,	     no_free);
	widechar = 1;
	test_ncompare_loop(wstring, convert_to_utf8, free);
	return success;
}



static struct
{
	int widechar;
	int srclen;
	int dstlen;
	union
	{
		struct
		{
			BYTE source[32];
			BYTE destination[32];
			BYTE expected[32];
		} bytes;
		struct
		{
			WCHAR source[32];
			WCHAR destination[32];
			WCHAR expected[32];
		} wchars;
	} strings;
} ncopies[] =
{
	{ 0, 0, 4,  { .bytes = {"", "foo", "foo"}}},
	{ 0, 1, 4,  { .bytes = {"", "foo", "\0oo"}}},
	{ 0, 1, 7,  { .bytes = {"\0foo", "barbaz", "\0arbaz"}}},
	{ 0, 4, 7,  { .bytes = {"\0foo", "barbaz", "\0fooaz"}}},
	{ 0, 8, 12, { .bytes = {"foo\0bar", "foo-bar-baz", "foo\0bar\0baz"}}},
	{ 0, 4, 4,  { .bytes = {"foo", "bar", "foo"}}},
	{ 1, 0, 4,  { .wchars = {{0}, {'f', 'o', 'o', 0}, {'f', 'o', 'o', 0}}}},
	{ 1, 1, 4,  { .wchars = {{0}, {'f', 'o', 'o', 0}, {0, 'o', 'o', 0}}}},
	{ 1, 1, 7,  { .wchars = {{0, 'f', 'o', 'o', 0}, {'b', 'a', 'r', 'b', 'a', 'z', 0}, {0, 'a', 'r', 'b', 'a', 'z', 0}}}},
	{ 1, 4, 7,  { .wchars = {{0, 'f', 'o', 'o', 0}, {'b', 'a', 'r', 'b', 'a', 'z', 0}, {0, 'f', 'o', 'o', 'a', 'z', 0}}}},
	{
		1, 8, 12, {
			.wchars = {{'f', 'o', 'o', 0, 'b', 'a', 'r', 0},
				{'f', 'o', 'o', '-', 'b', 'a', 'r', '-', 'b', 'a', 'z', 0},
				{'f', 'o', 'o', 0, 'b', 'a', 'r', 0, 'b', 'a', 'z', 0}
			}
		}
	},
	{
		1, 4, 4,  {
			.wchars = {{'f', 'o', 'o', 0},
				{'b', 'a', 'r', 0},
				{'f', 'o', 'o', 0}
			}
		}
	},
};

void memdump(void* pointer, int size)
{
	unsigned char* address = pointer;
	const char*   sep = "";
	int i;

	for (i = 0; i < size; i ++)
	{
		printf("%s%02X", sep, address[i]);
		sep = " ";
	}
}

void ncopy(BOOL widechar, void* destination, void* source, int count);

BOOL test_ncopy()
{
	BOOL success = TRUE;
	int i;

	for (i = 0; i < countof(ncopies); i ++)
	{
		int widechar = ncopies[i].widechar;
		int width = (widechar ? 2 : 1);
		int srclen = ncopies[i].srclen;
		int dstlen = ncopies[i].dstlen;
		BYTE* source = (widechar
		                ? (BYTE*)ncopies[i].strings.wchars.source
		                : ncopies[i].strings.bytes.source);
		BYTE* destination = (widechar
		                     ? (BYTE*)ncopies[i].strings.wchars.destination
		                     : ncopies[i].strings.bytes.destination);
		BYTE* mestination = (BYTE*)calloc(dstlen, (widechar
		                                  ? sizeof(WCHAR)
		                                  : sizeof(BYTE)));
		BYTE* expected = (widechar
		                  ? (BYTE*)ncopies[i].strings.wchars.expected
		                  : ncopies[i].strings.bytes.expected);
		memcpy(mestination, destination, width * dstlen);
		ncopy(widechar, mestination, source, srclen);

		if (0 != memcmp(mestination, expected, width * dstlen))
		{
			FAILURE("[%d]  ncopy(%s, source = {", i, widechar ? "WCHAR" : "BYTE");
			memdump(source, width * srclen);
			printf("}, destination = {");
			memdump(destination, width * dstlen);
			printf("}, len = %d) \n", srclen);
			printf("  resulted in {");
			memdump(mestination, width * dstlen);
			printf("}\n	expected {");
			memdump(expected, width * dstlen);
			printf("}\n");
			fflush(stdout);
			success = FALSE;
		}
	}

	return success;
}


static struct
{
	int widechar;
	BOOL expected;
	union
	{
		struct
		{
			BYTE string[32];
			BYTE substring[32];
		} bytes;
		struct
		{
			WCHAR string[32];
			BYTE substring[32];
		} wchars;
	} strings;
} contains_tests[] =
{
	{ 0, 1, { .bytes = {"",	  ""}} },
	{ 0, 1, { .bytes = {"foo", ""}} },
	{ 0, 0, { .bytes = {"",	  "foo"}} },
	{ 0, 1, { .bytes = {"foo", "foo"}} },
	{ 0, 1, { .bytes = {"fooprefix", "foo"}} },
	{ 0, 1, { .bytes = {"suffixfoo", "foo"}} },
	{ 0, 1, { .bytes = {"inthefoomiddle", "foo"}} },
	{ 0, 0, { .bytes = {"foo", "bar"}} },
	{ 0, 0, { .bytes = {"fooprefix", "bar"}} },
	{ 0, 0, { .bytes = {"suffixfoo", "bar"}} },
	{ 0, 0, { .bytes = {"inthefoomiddle", "bar"}} },
	{ 1, 1, { .wchars = {{0},  ""}}},
	{ 1, 1, { .wchars = {{'f', 'o', 'o', 0}, "" }}},
	{ 1, 0, { .wchars = {{0},   "foo" }}},
	{ 1, 1, { .wchars = {{'f', 'o', 'o', 0}, "foo"}}},
	{ 1, 1, { .wchars = {{'f', 'o', 'o', 'p', 'r', 'e', 'f', 'i', 'x', 0}, "foo" }}},
	{ 1, 1, { .wchars = {{'s', 'u', 'f', 'f', 'i', 'x', 'f', 'o', 'o', 0}, "foo" }}},
	{ 1, 1, { .wchars = {{'i', 'n', 't', 'h', 'e', 'f', 'o', 'o', 'm', 'i', 'd', 'd', 'l', 'e', 0}, "foo" }}},
	{ 1, 0, { .wchars = {{'f', 'o', 'o', 0},  "bar" }}},
	{ 1, 0, { .wchars = {{'f', 'o', 'o', 'p', 'r', 'e', 'f', 'i', 'x', 0},	"bar" }}},
	{ 1, 0, { .wchars = {{'s', 'u', 'f', 'f', 'i', 'x', 'f', 'o', 'o', 0}, "bar" }}},
	{ 1, 0, { .wchars = {{'i', 'n', 't', 'h', 'e', 'f', 'o', 'o', 'm', 'i', 'd', 'd', 'l', 'e', 0}, "bar" }}},
};


BOOL test_contains()
{
	BOOL success = TRUE;
	int i;

	for (i = 0; i < countof(contains_tests); i ++)
	{
		int widechar = contains_tests[i].widechar;
		BOOL expected = contains_tests[i].expected;
		BYTE* string = (widechar
		                ? (BYTE*)contains_tests[i].strings.wchars.string
		                : contains_tests[i].strings.bytes.string);
		BYTE* substring = (widechar
		                   ? (BYTE*)contains_tests[i].strings.wchars.substring
		                   : contains_tests[i].strings.bytes.substring);
		BOOL result = contains(widechar, string, (char *) substring);

		if ((result &&	!expected) || (!result && expected))
		{
			BYTE* out_of_memory = (BYTE*)"OUT OF MEMORY";
			BYTE* cstring;

			if (widechar)
			{
				convert_to_utf8(string, cstring);
			}
			else
			{
				cstring = (BYTE*)strdup((char*)string);
			}

			FAILURE("[%d]  contains(%s, string = \"%s\", substring = \"%s\") -> %s,	 expected %s\n",
			        i,
			        (widechar ? "WCHAR" : "BYTE"),
			        cstring ? cstring : out_of_memory,
			        substring,
			        (result ? "TRUE" : "FALSE"),
			        (expected ? "TRUE" : "FALSE"));

			if (cstring != out_of_memory)
			{
				free(cstring);
			}

			success = FALSE;
		}
	}

	return success;
}



static struct
{
	int widechar;
	BOOL expected;
	union
	{
		BYTE bytes[32];
		WCHAR wchars[32];
	} string;
	const char* substrings[32];
} stringhassubstrings[] =
{
	{ 0, 0, { .bytes = ""}, { "foo", "bar", "baz", 0} },
	{ 0, 1, { .bytes = ""}, { "foo", "bar", "", "baz", 0} },
	{ 0, 0, { .bytes = "foo"}, { "food", "foot", "bar", "aa", 0}},
	{ 0, 1, { .bytes = "foo"}, { "food", "foot", "foo", "bar", "aa", 0}},
	{ 0, 1, { .bytes = "foo"}, { "food", "foot", "bar", "oo", "quux", 0}},
	{ 0, 1, { .bytes = "foo"}, { "food", "foot", "bar", "fo", "quux", 0}},
	{ 0, 1, { .bytes = "foo"}, { "food", "foot", "bar", "o", "quux", 0}},
	{ 0, 0, { .bytes = ""}, {0} },
	{ 0, 0, { .bytes = "foo"}, {0} },
	{ 1, 0, { .wchars = {0}}, { "foo", "bar", "baz", 0} },
	{ 1, 1, { .wchars = {0}}, { "foo", "bar", "", "baz", 0} },
	{ 1, 0, { .wchars = {'f', 'o', 'o', 0}}, { "food", "foot", "bar", "aa", 0}},
	{ 1, 1, { .wchars = {'f', 'o', 'o', 0}}, { "food", "foot", "foo", "bar", "aa", 0}},
	{ 1, 1, { .wchars = {'f', 'o', 'o', 0}}, { "food", "foot", "bar", "oo", "quux", 0}},
	{ 1, 1, { .wchars = {'f', 'o', 'o', 0}}, { "food", "foot", "bar", "fo", "quux", 0}},
	{ 1, 1, { .wchars = {'f', 'o', 'o', 0}}, { "food", "foot", "bar", "o", "quux", 0}},
	{ 1, 0, { .wchars = {0}}, {0} },
	{ 1, 0, { .wchars = {'f', 'o', 'o', 0}}, {0} },
};


void LinkedList_PrintStrings(wLinkedList* list);

wLinkedList* LinkedList_FromStringList(char** string_list)
{
	int j;
	wLinkedList* list = LinkedList_New();

	if (!list)
	{
		ERROR("Cannot allocate new linked list.\n");
		return 0;;
	}

	for (j = 0; string_list[j]; j ++)
	{
		LinkedList_AddLast(list, (void*)(string_list[j]));
	}

	return list;
}



BOOL test_stringhassubstrings()
{
	BOOL success = TRUE;
	int i;

	for (i = 0; i < countof(stringhassubstrings); i ++)
	{
		int widechar = stringhassubstrings[i].widechar;
		BOOL expected = stringhassubstrings[i].expected;
		BYTE* string = (widechar
		                ? (BYTE*)stringhassubstrings[i].string.wchars
		                : stringhassubstrings[i].string.bytes);
		wLinkedList* list = LinkedList_FromStringList((char**)stringhassubstrings[i].substrings);
		BOOL result;

		if (!list)
		{
			return FALSE;
		}

		result = LinkedList_StringHasSubstring(widechar, string, list);

		if ((result &&	!expected) || (!result && expected))
		{
			BYTE* out_of_memory = (BYTE*)"OUT OF MEMORY";
			BYTE* cstring;

			if (widechar)
			{
				convert_to_utf8(string, cstring);
			}
			else
			{
				cstring = (BYTE*)strdup((char*)string);
			}

			FAILURE("[%d]  LinkedList_StringHasSubstring(%s, string = \"%s\", substrings = ",
			        i,
			        (widechar ? "WCHAR" : "BYTE"),
			        cstring ? cstring : out_of_memory);
			LinkedList_PrintStrings(list);
			printf(") -> %s,  expected %s\n",
			       (result ? "TRUE" : "FALSE"),
			       (expected ? "TRUE" : "FALSE"));
			fflush(stdout);

			if (cstring != out_of_memory)
			{
				free(cstring);
			}

			success = FALSE;
		}
	}

	return success;
}





/*
string_array are 2D arrays of either BYTE or WCHAR,
considered as a vector of strings.
The vector can be considered shorter than its size, using the empty string as terminating entry.
Strings can be considered shorter than their size,  using null-termination.

string_array_count_strings returns the position of the first empty string, or the size of the vector if none found.

*/

int string_array_count_strings(BOOL widechar, BYTE* data, int string_count, int string_size)
{
	int count = 0;

	while (count < string_count)
	{
		if (widechar)
		{
			if (*(WCHAR *)data != 0)
			{
				count ++;
				data = (BYTE *)((WCHAR *)data + string_size);
			}
			else
			{
				return count;
			}
		}
		else
		{
			if (*data != 0)
			{
				count ++;
				data += string_size;
			}
			else
			{
				return count;
			}
		}
	}

	return count;
}


BYTE** string_array_to_string_list(BOOL widechar, BYTE* data, int string_count,
                                   int string_size)
{
	int i;
	int count = string_array_count_strings(widechar, data, string_count, string_size);
	BYTE** list = calloc(count + 1, sizeof(list[0]));

	if (!list)
	{
		ERROR("Cannot allocate %d bytes", (count + 1) *	 sizeof(list[0]));
		return 0;
	}

	for (i = 0; i < count; i ++)
	{
		list[i] = data;
		if (widechar)
		{
			data = (BYTE *)((WCHAR *)data + string_size);
		}
		else
		{
			data += string_size;
		}
	}

	list[i] = 0;
	return list;
}

/*

string_list_to_msz converts a string list  (a null - terminated vector
of pointers  to strings) to a  Microsoft Double-Null-Terminated string
list.

The strings can be BYTE or WCHAR strings.

string_list_to_msz can convert the strings byte-> byte,	 byte->wchar,
wchar->wchar and wchar->byte.

*/

/*
string_list_size returns the total number of bytes required to store
the string list as a Microsoft double-null-terminated string list.
It's the sum of the size of all the strings (included the terminating null)
in the list (plus the final terminating null).
*/
int string_list_size(BOOL widechar, BYTE** list)
{
	int size = 0;
	int i;

	if (widechar)
	{
		for (i = 0; list[i]; i ++)
		{
			size += 1 + lstrlenW((WCHAR *)list[i]);
		}

		size ++ ;
		size *= sizeof(WCHAR);
	}
	else
	{
		for (i = 0; list[i]; i ++)
		{
			size += 1 + strlen((char *)list[i]);
		}

		size ++ ;
	}

	return size;
}

int convert_to_unicode(void* source, void** destination)
{
	return ConvertToUnicode(CP_UTF8, 0, (LPCSTR)source, -1, (LPWSTR*)destination, 0);
}

int convert_from_unicode(void* source, void** destination)
{
	return ConvertFromUnicode(CP_UTF8, 0, (LPCWSTR)source, -1, (LPSTR*)destination, 0, "?", 0);
}

typedef int (* convert_pr)(void* source, void** destination);
typedef int (*length_pr)(void*);

int no_conversion(void * source, void ** destination)
{
	*destination = strdup(source);
	return strlen(source);
}

BOOL string_list_to_msz(BOOL input_widechar, BYTE** list, BOOL output_widechar, LPSTR*   mszString,
                        DWORD* cchStrings)
{
	int count = string_list_length((const char * const*)list);
	int total_size;
	int input_width = input_widechar ? sizeof(WCHAR) : sizeof(BYTE);
	int output_width = output_widechar ? sizeof(WCHAR) : sizeof(BYTE);
	convert_pr convert = output_widechar ? convert_to_unicode : no_conversion;
	BOOL templist_allocated = FALSE;
	void** templist = 0;
	length_pr templen = output_widechar ? (length_pr)lstrlenW : (length_pr)lstrlenA;
	LPSTR mszDest;
	LPSTR current;
	int i;

	if (output_width == input_width)
	{
		total_size = string_list_size(input_widechar, list);
		templist = (void**)list;
	}
	else
	{
		templist = calloc(count, sizeof(templist[0]));
		templist_allocated = TRUE;

		if (!templist)
		{
			ERROR("Cannot allocate temporary memory (%d bytes)\n", (count * sizeof(templist[0])));
			return FALSE;
		}

		total_size = 0;

		for (i = 0; i < count; i ++)
		{
			int size = convert(list[i], & templist[i]);
			total_size += size;
		}

		total_size += output_width;
	}

	mszDest = malloc(total_size);

	if (!mszDest)
	{
		ERROR("Cannot allocate result memory (%d bytes)\n", total_size);
		return FALSE;
	}

	current = mszDest;

	for (i = 0; i < count; i ++)
	{
		int len = templen(templist[i]) + 1;
		memcpy(current, templist[i], output_width * len);
		current += output_width * len;
	}

	memcpy(current, current - output_width, output_width);

	if (templist_allocated)
	{
		free(templist);
	}

	(*mszString) = mszDest;
	(*cchStrings) = 1 + (current - mszDest) / output_width;
	return TRUE;
}


#define mszfilterstrings_string_count	(8)
#define mszfilterstrings_string_size   (16)
static struct
{
	int widechar;
	const char*   filter_list[mszfilterstrings_string_size];
	union
	{
		BYTE bytes[mszfilterstrings_string_count][mszfilterstrings_string_size];
		WCHAR wchars[mszfilterstrings_string_count][mszfilterstrings_string_size];
	} input, output;
} mszfilterstrings[] =
{
	{
		0,
		{ "82", "42", 0},
		.input =  { .bytes = { "Xiring 8282", "Xiring 5555", "NeoWave 4242", ""}},
		.output = { .bytes = { "Xiring 8282", "NeoWave 4242", ""}}
	},
	{
		0,
		{ "Xiring", 0},
		{ .bytes = { "Xiring 8282", "Xiring 5555", "NeoWave 4242", ""}},
		{ .bytes = { "Xiring 8282", "Xiring 5555", ""}}
	},
	{
		0,
		{ "NeoWave", 0},
		{ .bytes = { "Xiring 8282", "Xiring 5555", "NeoWave 4242", ""}},
		{ .bytes = { "NeoWave 4242", ""}}
	},
	{
		0,
		{ "5555", 0},
		{ .bytes = { "Xiring 8282", "Xiring 5555", "NeoWave 4242", ""}},
		{ .bytes = { "Xiring 5555", ""}}
	},
	{
		0,
		{ "", 0},
		{ .bytes = { "Xiring 8282", "Xiring 5555", "NeoWave 4242", ""}},
		{ .bytes = { "Xiring 8282", "Xiring 5555", "NeoWave 4242", ""}}
	},
	{
		1,
		{ "82", "42", 0},
		{
			.wchars = { {'X', 'i', 'r', 'i', 'n', 'g', ' ', '8', '2', '8', '2', 0},
				{'X', 'i', 'r', 'i', 'n', 'g', ' ', '5', '5', '5', '5', 0},
				{'N', 'e', 'o', 'W', 'a', 'v', 'e', ' ', '4', '2', '4', '2', 0},
				{0}
			}
		},
		{
			.wchars = { {'X', 'i', 'r', 'i', 'n', 'g', ' ', '8', '2', '8', '2', 0},
				{'N', 'e', 'o', 'W', 'a', 'v', 'e', ' ', '4', '2', '4', '2', 0},
				{0}
			}
		}
	},
	{
		1,
		{ "Xiring", 0},
		{
			.wchars = { {'X', 'i', 'r', 'i', 'n', 'g', ' ', '8', '2', '8', '2', 0},
				{'X', 'i', 'r', 'i', 'n', 'g', ' ', '5', '5', '5', '5', 0},
				{'N', 'e', 'o', 'W', 'a', 'v', 'e', ' ', '4', '2', '4', '2', 0},
				{0}
			}
		},
		{
			.wchars = { {'X', 'i', 'r', 'i', 'n', 'g', ' ', '8', '2', '8', '2', 0},
				{'X', 'i', 'r', 'i', 'n', 'g', ' ', '5', '5', '5', '5', 0},
				{0}
			}
		}
	},
	{
		1,
		{ "NeoWave", 0},
		{
			.wchars = { {'X', 'i', 'r', 'i', 'n', 'g', ' ', '8', '2', '8', '2', 0},
				{'X', 'i', 'r', 'i', 'n', 'g', ' ', '5', '5', '5', '5', 0},
				{'N', 'e', 'o', 'W', 'a', 'v', 'e', ' ', '4', '2', '4', '2', 0},
				{0}
			}
		},
		{
			.wchars = { {'N', 'e', 'o', 'W', 'a', 'v', 'e', ' ', '4', '2', '4', '2', 0},
				{0}
			}
		}
	},
	{
		1,
		{ "5555", 0},
		{
			.wchars = { {'X', 'i', 'r', 'i', 'n', 'g', ' ', '8', '2', '8', '2', 0},
				{'X', 'i', 'r', 'i', 'n', 'g', ' ', '5', '5', '5', '5', 0},
				{'N', 'e', 'o', 'W', 'a', 'v', 'e', ' ', '4', '2', '4', '2', 0},
				{0}
			}
		},
		{
			.wchars = { {'X', 'i', 'r', 'i', 'n', 'g', ' ', '5', '5', '5', '5', 0},
				{0}
			}
		}
	},
	{
		1,
		{ "", 0},
		{
			.wchars = { {'X', 'i', 'r', 'i', 'n', 'g', ' ', '8', '2', '8', '2', 0},
				{'X', 'i', 'r', 'i', 'n', 'g', ' ', '5', '5', '5', '5', 0},
				{'N', 'e', 'o', 'W', 'a', 'v', 'e', ' ', '4', '2', '4', '2', 0},
				{0}
			}
		},
		{
			.wchars = { {'X', 'i', 'r', 'i', 'n', 'g', ' ', '8', '2', '8', '2', 0},
				{'X', 'i', 'r', 'i', 'n', 'g', ' ', '5', '5', '5', '5', 0},
				{'N', 'e', 'o', 'W', 'a', 'v', 'e', ' ', '4', '2', '4', '2', 0},
				{0}
			}
		}
	}
};


inline int min(int a, int b)
{
	return a < b ? a : b;
}

BOOL test_mszfilterstrings()
{
	BOOL success = TRUE;
	int i;

	for (i = 0; i < countof(mszfilterstrings); i ++)
	{
		int widechar = mszfilterstrings[i].widechar;
		char** filter_list = (char**)mszfilterstrings[i].filter_list;
		DWORD cchInput = 0;
		BYTE* input;
		DWORD cchOutput = 0;
		BYTE* output;
		wLinkedList* list = LinkedList_FromStringList((char**)filter_list);
		int size;

		if (!list)
		{
			return FALSE;
		}

		if (widechar)
		{
			BYTE** list;
			list = string_array_to_string_list(widechar, (BYTE*)&mszfilterstrings[i].input.wchars,
			                                   mszfilterstrings_string_count, mszfilterstrings_string_size);

			if (!list)
			{
				return FALSE;
			}

			string_list_to_msz(widechar, list, widechar, (LPSTR*)& input, & cchInput);
			free(list);
			list = string_array_to_string_list(widechar, (BYTE*)&mszfilterstrings[i].output.wchars,
			                                   mszfilterstrings_string_count, mszfilterstrings_string_size);

			if (!list)
			{
				return FALSE;
			}

			string_list_to_msz(widechar, list, widechar, (LPSTR*)& output, & cchOutput);
			free(list);
		}
		else
		{
			BYTE** list;
			list = string_array_to_string_list(widechar, (BYTE*)&mszfilterstrings[i].input.bytes,
			                                   mszfilterstrings_string_count, mszfilterstrings_string_size);

			if (!list)
			{
				return FALSE;
			}

			string_list_to_msz(widechar, list, widechar, (LPSTR*)& input, & cchInput);
			free(list);
			list = string_array_to_string_list(widechar, (BYTE*)&mszfilterstrings[i].output.bytes,
			                                   mszfilterstrings_string_count, mszfilterstrings_string_size);

			if (!list)
			{
				return FALSE;
			}

			string_list_to_msz(widechar, list, widechar, (LPSTR*)& output, & cchOutput);
			free(list);
		}

		mszFilterStrings(widechar, (LPSTR)input, & cchInput, list);
		size = mszSize(widechar, input);

		if (cchInput != size / (widechar?sizeof (WCHAR):sizeof (char)))
		{
			FAILURE("[%d] after mszFilterStrings,  cchInput = %d should be	equal to mszSize(widechar, input) / (widechar?sizeof(WCHAR):sizeof(char)) = %d\n",
			        i, cchInput, size / (widechar?sizeof (WCHAR):sizeof (char)));
		}

		if (cchInput != cchOutput)
		{
			FAILURE("[%d] after mszFilterStrings,  cchInput = %d should be	equal to cchOutput = %d\n",
			        i, cchInput, cchOutput);
			printf("result:	  ");
			memdump(input, cchInput);
			printf("\nexpected: ");
			memdump(output, cchOutput);
			printf("\n");
			fflush(stdout);
			success = FALSE;
		}

		if (0 != memcmp(input, output, min(cchInput, cchOutput)))
		{
			FAILURE("[%d] after mszFilterStrings the mszString is different from expected:", i);
			printf("\nresult:   ");
			memdump(input, cchInput);
			printf("\nexpected: ");
			memdump(output, cchOutput);
			printf("\n");
			fflush(stdout);
			success = FALSE;
		}
	}

	return success;
}



#define mszstring_enumerator_string_count   (8)
#define mszstring_enumerator_string_size   (16)
static struct
{
	int widechar;
	int count;
	union
	{
		BYTE bytes[mszstring_enumerator_string_count][mszstring_enumerator_string_size];
		WCHAR wchars[mszstring_enumerator_string_count][mszstring_enumerator_string_size];
	} string;
} mszstring_enumerator_tests[] =
{
	{
		.widechar = 0,
		.count = 3,
		.string =  { .bytes = { "Xiring 8282", "Xiring 5555", "NeoWave 4242", ""}}
	},
	{
		.widechar = 0,
		.count = 2,
		.string =  { .bytes = { "Xiring 5555", "NeoWave 4242", ""}}
	},
	{
		.widechar = 0,
		.count = 1,
		.string =  { .bytes = { "Xiring 8282", ""}}
	},
	{
		.widechar = 0,
		.count = 0,
		.string =  { .bytes = {""}}
	},
	{
		.widechar = 1,
		.count = 3,
		.string =  {
			.wchars = { {'X', 'i', 'r', 'i', 'n', 'g', ' ', '8', '2', '8', '2', 0},
				{'X', 'i', 'r', 'i', 'n', 'g', ' ', '5', '5', '5', '5', 0},
				{'N', 'e', 'o', 'W', 'a', 'v', 'e', ' ', '4', '2', '4', '2', 0},
				{0}
			}
		},
	},
	{
		.widechar = 1,
		.count = 2,
		.string =  {
			.wchars = { {'X', 'i', 'r', 'i', 'n', 'g', ' ', '5', '5', '5', '5', 0},
				{'N', 'e', 'o', 'W', 'a', 'v', 'e', ' ', '4', '2', '4', '2', 0},
				{0}
			}
		},
	},
	{
		.widechar = 1,
		.count = 1,
		.string =  {
			.wchars = { {'N', 'e', 'o', 'W', 'a', 'v', 'e', ' ', '4', '2', '4', '2', 0},
				{0}
			}
		},
	},
	{
		.widechar = 1,
		.count = 0,
		.string =  {
			.wchars = { {0}
			}
		},
	}
};


BOOL test_mszStrings_Enumerator()
{
	BOOL success = TRUE;
	int i;

	for (i = 0; i < countof(mszstring_enumerator_tests); i ++)
	{
		int widechar = mszstring_enumerator_tests[i].widechar;
		int count = mszstring_enumerator_tests[i].count;
		DWORD cchString = 0;
		BYTE* mszString;
		mszStrings_Enumerator enumerator;
		int k;
		int size;
		BYTE** list;

		if (widechar)
		{
			list = string_array_to_string_list(widechar, (BYTE*)&mszstring_enumerator_tests[i].string.wchars,
			                                   mszstring_enumerator_string_count,
			                                   mszstring_enumerator_string_size);
		}
		else
		{
			list = string_array_to_string_list(widechar, (BYTE*)&mszstring_enumerator_tests[i].string.bytes,
			                                   mszstring_enumerator_string_count,
			                                   mszstring_enumerator_string_size);
		}

		if (!list)
		{
			return FALSE;
		}

		string_list_to_msz(widechar, list, widechar, (LPSTR*)& mszString, & cchString);
		size = mszSize(widechar, mszString);

		if (cchString != size / (widechar?sizeof (WCHAR):sizeof (char)))
		{
			FAILURE("[%d] after string_list_to_msz,	 cchString = %d should be  equal to mszSize(widechar, mszString) / (widechar?sizeof(WCHAR):sizeof(char)) = %d\n",
			        i, cchString, size / (widechar?sizeof (WCHAR):sizeof (char)));
		}

		mszStrings_Enumerator_Reset(&enumerator, widechar, mszString);
		k = 0;

		while (mszStrings_Enumerator_MoveNext(& enumerator))
		{
			if (list[k] != mszStrings_Enumerator_Current(& enumerator))
			{
				FAILURE("[%d] mszString[ %d ] != list[ %d ]\n", i, k, k);
			}

			k ++ ;
		}

		if (k != count)
		{
			FAILURE("[%d] strings count with enumerator = %d,  should be %d\n",
			        i, k, count);
			printf("result:	  ");
			memdump(mszString, cchString);
		}

		free(list);
	}

	return success;
}





int TestStr(int argc, char* argv[])
{
	BOOL success = TRUE;
	success &= test_compare();
	success &= test_ncompare();
	success &= test_ncopy();
	success &= test_contains();
	success &= test_stringhassubstrings();
	success &= test_mszfilterstrings();
	success &= test_mszStrings_Enumerator();
	return success ? 0 : -1;
}
