
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include <winpr/cmdline.h>

/* We need to include the command line c file to get access to
 * the argument struct. */
#include "../common/cmdline.c"

LPSTR tmp = NULL;

LPCSTR tr_esc_str(LPCSTR arg)
{
	size_t cs = 0, x, ds;
	size_t s;
	
	if( NULL == arg )
		return NULL;

	s = strlen(arg);

	/* Find trailing whitespaces */
	while( (s > 0) && isspace(arg[s-1]))
		s--;

	/* Prepare a initial buffer with the size of the result string. */
	tmp = malloc(s * sizeof(LPCSTR));
	if( NULL == tmp )
	{
		fprintf(stderr, "Could not allocate string buffer.");
		exit(-2);
	}

	/* Copy character for character and check, if it is necessary to escape. */
	ds = s + 1;
	for(x=0; x<s; x++)
	{
		switch(arg[x])
		{
			case '<':
				ds += 3;
				tmp = realloc(tmp, ds * sizeof(LPCSTR));
				if( NULL == tmp )
				{
					fprintf(stderr, "Could not reallocate string buffer.");
					exit(-3);
				}
				tmp[cs++] = '&';
				tmp[cs++] = 'l';
				tmp[cs++] = 't';
				tmp[cs++] = ';';
				break;
			case '>':
				ds += 3;
				tmp = realloc(tmp, ds * sizeof(LPCSTR));
				if( NULL == tmp )
				{
					fprintf(stderr, "Could not reallocate string buffer.");
					exit(-4);
				}
				tmp[cs++] = '&';
				tmp[cs++] = 'g';
				tmp[cs++] = 't';
				tmp[cs++] = ';';
				break;
			case '\'':
				ds += 5;
				tmp = realloc(tmp, ds * sizeof(LPCSTR));
				if( NULL == tmp )
				{
					fprintf(stderr, "Could not reallocate string buffer.");
					exit(-5);
				}
				tmp[cs++] = '&';
				tmp[cs++] = 'a';
				tmp[cs++] = 'p';
				tmp[cs++] = 'o';
				tmp[cs++] = 's';
				tmp[cs++] = ';';
				break;
			case '"':
				ds += 5;
				tmp = realloc(tmp, ds * sizeof(LPCSTR));
				if( NULL == tmp )
				{
					fprintf(stderr, "Could not reallocate string buffer.");
					exit(-6);
				}
				tmp[cs++] = '&';
				tmp[cs++] = 'q';
				tmp[cs++] = 'u';
				tmp[cs++] = 'o';
				tmp[cs++] = 't';
				tmp[cs++] = ';';
				break;
			case '&':
				ds += 4;
				tmp = realloc(tmp, ds * sizeof(LPCSTR));
				if( NULL == tmp )
				{
					fprintf(stderr, "Could not reallocate string buffer.");
					exit(-7);
				}
				tmp[cs++] = '&';
				tmp[cs++] = 'a';
				tmp[cs++] = 'm';
				tmp[cs++] = 'p';
				tmp[cs++] = ';';
				break;
			default:
				tmp[cs++] = arg[x];
				break;
		}

		/* Assure, the string is '\0' terminated. */
		tmp[ds-1] = '\0';
	}

	return tmp;
}

int main(int argc, char *argv[])
{
	size_t elements = sizeof(args)/sizeof(args[0]);
	size_t x;
	const char *fname = "xfreerdp-argument.1.xml";
	FILE *fp = NULL;

	/* Open output file for writing, truncate if existing. */
	fp = fopen(fname, "w");
	if( NULL == fp )
	{
		fprintf(stderr, "Could not open '%s' for writing.", fname);
		return -1;
	}

	/* The tag used as header in the manpage */
	fprintf(fp, "<refsect1>\n");
	fprintf(fp, "\t<title>Options</title>\n");
	fprintf(fp, "\t\t<variablelist>\n");

	/* Iterate over argument struct and write data to docbook 4.5 
	 * compatible XML */
	if( elements < 2 )
	{
		fprintf(stderr, "The argument array 'args' is empty, writing an empty file.");
		elements = 1;
	}

	for(x=0; x<elements - 1; x++)
	{
		const COMMAND_LINE_ARGUMENT_A *arg = &args[x];

		fprintf(fp, "\t\t\t<varlistentry>\n");
		if( COMMAND_LINE_VALUE_REQUIRED == arg->Flags )
			fprintf(fp, "\t\t\t\t<term><option>/%s</option> <replaceable>%s</replaceable></term>\n", tr_esc_str(arg->Name), tr_esc_str(arg->Format) );
		else
			fprintf(fp, "\t\t\t\t<term><option>/%s</option></term>\n", tr_esc_str(arg->Name) );
		fprintf(fp, "\t\t\t\t<listitem>\n");
		fprintf(fp, "\t\t\t\t\t<para>%s</para>\n", tr_esc_str(arg->Text));
		
		fprintf(fp, "\t\t\t\t</listitem>\n");
		fprintf(fp, "\t\t\t</varlistentry>\n");
	}

	fprintf(fp, "\t\t</variablelist>\n");
	fprintf(fp, "\t</refsect1>\n");
	fclose(fp);

	if(NULL != tmp)
		free(tmp);

	return 0;
}

