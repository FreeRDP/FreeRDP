#include <stdio.h>
#include <string.h>

#include <winpr/strlst.h>

#define printref() printf("%s:%d: in function %-40s:", __FILE__, __LINE__, __FUNCTION__)

#define ERROR(format, ...)                                  \
	do{                                                 \
		fprintf(stderr, format, ##__VA_ARGS__);     \
		printref();                                 \
		printf(format "\n", ##__VA_ARGS__);	    \
		fflush(stdout);                             \
	}while(0)

#define FAILURE(format, ...)                                \
	do{                                                 \
		printref();                                 \
		printf(" FAILURE ");                        \
		printf(format "\n", ##__VA_ARGS__);	    \
		fflush(stdout);                             \
	}while(0)


#define TEST(condition, format, ...)			    \
	if (!(condition))				    \
	{						    \
		FAILURE("test %s " format "\n", #condition, \
			##__VA_ARGS__);			    \
	}

static void print_test_title(int argc, char** argv)
{
	int i;
	printf("Running test:");

	for (i = 0; i < argc; i ++)
	{
		printf(" %s", argv[i]);
	}

	printf("\n");
}



static char * sl0[] = {NULL};
static char * sl1[] = {"Niflheim", NULL};
static char * sl40[] = {"Hello", "", "World", "!", NULL};
static char * sl40n[] = {"Hello", "World", "!", NULL};
static char * sl44[] = {"Good", "Bye", "Cruel", "World", NULL};
static char * sl44e[] = {"Good", "Bye", "Cruel", "World!", NULL};
static char * separator = "<space>";
static char * sl40string = "Hello<space><space>World<space>!";
static char * sl44string = "Good<space>Bye<space>Cruel<space>World";

#define TEST_COPY(sl, length)							\
	do									\
	{									\
		copy = string_list_copy(sl);					\
		TEST(copy != NULL, "string_list_copy(" #sl ") returned NULL");	\
		TEST(string_list_length(copy) == length,			\
			"got %d instead of expected %d",			\
			string_list_length(copy),				\
			length);						\
		TEST(string_list_equal(copy, sl),				\
			"copy is not equal to original!");			\
		if (!string_list_equal(copy, sl))				\
		{								\
			printf("original = \n");				\
			string_list_print(stdout, sl);				\
			printf("copy = \n");					\
			string_list_print(stdout, copy);			\
		}								\
		string_list_free(copy);						\
	}while (0)

int TestStringList(int argc, char* argv[])
{
	char * string;
	char ** copy;
	print_test_title(argc, argv);

	TEST(string_list_length(sl0) == 0, "got %d instead", string_list_length(sl0));
	TEST(string_list_length(sl1) == 1, "got %d instead", string_list_length(sl1));
	TEST(string_list_length(sl40) == 4, "got %d instead", string_list_length(sl40));
	TEST(string_list_length(sl44) == 4, "got %d instead", string_list_length(sl44));

	TEST(string_list_equal(sl0, sl0), "sl0 should be equal to itself!");
	TEST(string_list_equal(sl1, sl1), "sl1 should be equal to itself!");
	TEST(string_list_equal(sl40, sl40), "sl40 should be equal to itself!")
	TEST(string_list_equal(sl44, sl44), "sl44 should be equal to itself!");

	TEST(string_list_mismatch(sl0, sl0) == string_list_length(sl0),
		"sl0 should mismatch itself at its length,  not at %d",
		string_list_mismatch(sl0, sl0));
	TEST(string_list_mismatch(sl1, sl1) == string_list_length(sl1),
		"sl1 should mismatch itself at its length,  not at %d",
		string_list_mismatch(sl1, sl1));
	TEST(string_list_mismatch(sl40, sl40) == string_list_length(sl40),
		"sl40 should mismatch itself at its length,  not at %d",
		string_list_mismatch(sl40, sl40));
	TEST(string_list_mismatch(sl44, sl44) == string_list_length(sl44),
		"sl44 should mismatch itself at its length,  not at %d",
		string_list_mismatch(sl44, sl44));

	TEST(string_list_mismatch(sl0, sl1) == 0, "sl0 and sl1 should mismatch at 0!");
	TEST(string_list_mismatch(sl1, sl1) == 1, "sl1 mismatch at 1!");
	TEST(string_list_mismatch(sl44, sl44e) == 3, "sl44 and sl44e should mismatch at 3!");

	TEST(string_list_equal(sl40, sl40), "sl40 should be equal to itself!");
	TEST(string_list_equal(sl44, sl44), "sl44 should be equal to itself!");

	TEST_COPY(sl0, 0);
	TEST_COPY(sl1, 1);
	TEST_COPY(sl40, 4);
	TEST_COPY(sl44, 4);


	{
		string = string_list_join(sl44, separator);
		TEST(strcmp(string,  sl44string) == 0,
			"string_list_join of sl44 should be \"%s\",  not \"%s\".",
			sl44string, string);
		{
			copy = string_list_split_string(string, separator, 0);
			TEST(string_list_equal(copy, sl44),
				"splitting \"%s\" returns a mismatch at %d",
				string, string_list_mismatch(copy, sl44));
			string_list_free(copy);
		}
		free(string);
	}


	{
		string = string_list_join(sl40, separator);
		TEST(strcmp(string,  sl40string) == 0,
			"string_list_join of sl40 should be \"%s\",  not \"%s\".",
			sl40string, string);
		{
			copy = string_list_split_string(string, separator, 0);
			TEST(string_list_equal(copy, sl40),
				"splitting \"%s\" returns a mismatch at %d",
				string, string_list_mismatch(copy, sl40));
			string_list_free(copy);
		}
		{
			copy = string_list_split_string(string, separator, 1);
			TEST(!string_list_equal(copy, sl40n),
				"splitting \"%s\" returns a mismatch at %d",
				string, string_list_mismatch(copy, sl40n));
			string_list_free(copy);
		}
		free(string);
	}

	return 0;
}

