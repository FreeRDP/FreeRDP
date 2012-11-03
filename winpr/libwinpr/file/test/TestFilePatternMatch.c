
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/file.h>
#include <winpr/windows.h>

int TestFilePatternMatch(int argc, char* argv[])
{
	/* '*' expression */

	if (!FilePatternMatchA("document.txt", "*"))
	{
		printf("FilePatternMatchA error: FileName: %s Pattern: %s\n", "document.txt", "*");
		return -1;
	}

	/* '*X' expression */

	if (!FilePatternMatchA("document.txt", "*.txt"))
	{
		printf("FilePatternMatchA error: FileName: %s Pattern: %s\n", "document.txt", "*.txt");
		return -1;
	}

	if (FilePatternMatchA("document.docx", "*.txt"))
	{
		printf("FilePatternMatchA error: FileName: %s Pattern: %s\n", "document.docx", "*.txt");
		return -1;
	}

	if (FilePatternMatchA("document.txt.bak", "*.txt"))
	{
		printf("FilePatternMatchA error: FileName: %s Pattern: %s\n", "document.txt.bak", "*.txt");
		return -1;
	}

	if (FilePatternMatchA("bak", "*.txt"))
	{
		printf("FilePatternMatchA error: FileName: %s Pattern: %s\n", "bak", "*.txt");
		return -1;
	}

	/* 'X*' expression */

	if (!FilePatternMatchA("document.txt", "document.*"))
	{
		printf("FilePatternMatchA error: FileName: %s Pattern: %s\n", "document.txt", "document.*");
		return -1;
	}

	/* 'X?' expression */

	if (!FilePatternMatchA("document.docx", "document.doc?"))
	{
		printf("FilePatternMatchA error: FileName: %s Pattern: %s\n", "document.docx", "document.doc?");
		return -1;
	}

	if (FilePatternMatchA("document.doc", "document.doc?"))
	{
		printf("FilePatternMatchA error: FileName: %s Pattern: %s\n", "document.doc", "document.doc?");
		return -1;
	}

	/* no wildcards expression */

	if (!FilePatternMatchA("document.txt", "document.txt"))
	{
		printf("FilePatternMatchA error: FileName: %s Pattern: %s\n", "document.txt", "document.txt");
		return -1;
	}

	/* 'X * Y' expression */

	if (!FilePatternMatchA("X123Y.txt", "X*Y.txt"))
	{
		printf("FilePatternMatchA error: FileName: %s Pattern: %s\n", "X123Y.txt", "X*Y.txt");
		return -1;
	}

	if (!FilePatternMatchA("XY.txt", "X*Y.txt"))
	{
		printf("FilePatternMatchA error: FileName: %s Pattern: %s\n", "XY.txt", "X*Y.txt");
		return -1;
	}

	if (FilePatternMatchA("XZ.txt", "X*Y.txt"))
	{
		printf("FilePatternMatchA error: FileName: %s Pattern: %s\n", "XZ.txt", "X*Y.txt");
		return -1;
	}

	if (FilePatternMatchA("X123Z.txt", "X*Y.txt"))
	{
		printf("FilePatternMatchA error: FileName: %s Pattern: %s\n", "X123Z.txt", "X*Y.txt");
		return -1;
	}

	/* 'X * Y * Z' expression */

	if (!FilePatternMatchA("X123Y456Z.txt", "X*Y*Z.txt"))
	{
		printf("FilePatternMatchA error: FileName: %s Pattern: %s\n", "X123Y456Z.txt", "X*Y*Z.txt");
		return -1;
	}

	if (!FilePatternMatchA("XYZ.txt", "X*Y*Z.txt"))
	{
		printf("FilePatternMatchA error: FileName: %s Pattern: %s\n", "XYZ.txt", "X*Y*Z.txt");
		return -1;
	}

	if (!FilePatternMatchA("X123Y456W.txt", "X*Y*Z.txt"))
	{
		printf("FilePatternMatchA error: FileName: %s Pattern: %s\n", "X123Y456W.txt", "X*Y*Z.txt");
		return -1;
	}

	if (!FilePatternMatchA("XYW.txt", "X*Y*Z.txt"))
	{
		printf("FilePatternMatchA error: FileName: %s Pattern: %s\n", "XYW.txt", "X*Y*Z.txt");
		return -1;
	}

	/* 'X ? Y' expression */

	if (!FilePatternMatchA("X1Y.txt", "X?Y.txt"))
	{
		printf("FilePatternMatchA error: FileName: %s Pattern: %s\n", "X1Y.txt", "X?Y.txt");
		return -1;
	}

	if (FilePatternMatchA("XY.txt", "X?Y.txt"))
	{
		printf("FilePatternMatchA error: FileName: %s Pattern: %s\n", "XY.txt", "X?Y.txt");
		return -1;
	}

	if (FilePatternMatchA("XZ.txt", "X?Y.txt"))
	{
		printf("FilePatternMatchA error: FileName: %s Pattern: %s\n", "XZ.txt", "X?Y.txt");
		return -1;
	}

	if (FilePatternMatchA("X123Z.txt", "X?Y.txt"))
	{
		printf("FilePatternMatchA error: FileName: %s Pattern: %s\n", "X123Z.txt", "X?Y.txt");
		return -1;
	}

	/* 'X ? Y ? Z' expression */

	if (!FilePatternMatchA("X123Y456Z.txt", "X?Y?Z.txt"))
	{
		printf("FilePatternMatchA error: FileName: %s Pattern: %s\n", "X123Y456Z.txt", "X?Y?Z.txt");
		return -1;
	}

	if (FilePatternMatchA("XYZ.txt", "X?Y?Z.txt"))
	{
		printf("FilePatternMatchA error: FileName: %s Pattern: %s\n", "XYZ.txt", "X?Y?Z.txt");
		return -1;
	}

	if (!FilePatternMatchA("X123Y456W.txt", "X?Y?Z.txt"))
	{
		printf("FilePatternMatchA error: FileName: %s Pattern: %s\n", "X123Y456W.txt", "X?Y?Z.txt");
		return -1;
	}

	if (FilePatternMatchA("XYW.txt", "X?Y?Z.txt"))
	{
		printf("FilePatternMatchA error: FileName: %s Pattern: %s\n", "XYW.txt", "X?Y?Z.txt");
		return -1;
	}


	return 0;
}

