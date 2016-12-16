# Print Format Specifiers

## Lookup Table

We use the following format specifiers for all \*printf\* and WLog_* functions:

| Type               | signed    | unsigned  | octal     | hex       | HEX       |
| ------------------ | --------- | --------- | --------- | --------- | --------- |
| signed char        | %hhd      |           |           |           |           |
| unsigned char      |           | %hhu      | %hho      | %hhx      | %hhX      |
| short              | %hd       |           |           |           |           |
| unsigned short     |           | %hu       | %ho       | %hx       | %hX       |
| int                | %d        |           |           |           |           |
| unsigned int       |           | %u        | %o        | %x        | %X        |
| long               | %ld       |           |           |           |           |
| unsigned long      |           | %lu       | %lo       | %lx       | %lX       |
| long long          | %lld      |           |           |           |           |
| unsigned long long |           | %llu      | %llo      | %llx      | %llX      |
| size_t             |           | %"PRIuz"  | %"PRIoz"  | %"PRIxz"  | %"PRIXz"  |
| INT8               | %"PRId8"  |           |           |           |           |
| UINT8              |           | %"PRIu8"  | %"PRIo8"  | %"PRIx8"  | %"PRIX8"  |
| BOOLEAN            |           | %"PRIu8"  | %"PRIo8"  | %"PRIx8"  | %"PRIX8"  |
| BYTE               |           | %"PRIu8"  | %"PRIo8"  | %"PRIx8"  | %"PRIX8"  |
| CHAR               | %"PRId8"  |           |           |           |           |
| UCHAR              |           | %"PRIu8"  | %"PRIo8"  | %"PRIx8"  | %"PRIX8"  |
| INT16              | %"PRId16" |           |           |           |           |
| UINT16             |           | %"PRIu16" | %"PRIo16" | %"PRIx16" | %"PRIX16" |
| WORD               |           | %"PRIu16" | %"PRIo16" | %"PRIx16" | %"PRIX16" |
| WCHAR              |           | %"PRIu16" | %"PRIo16" | %"PRIx16" | %"PRIX16" |
| SHORT              | %"PRId16" |           |           |           |           |
| USHORT             |           | %"PRIu16" | %"PRIo16" | %"PRIx16" | %"PRIX16" |
| INT32              | %"PRId32" |           |           |           |           |
| UINT32             |           | %"PRIu32" | %"PRIo32" | %"PRIx32" | %"PRIX32" |
| INT                | %"PRId32" |           |           |           |           |
| UINT               |           | %"PRIu32" | %"PRIo32" | %"PRIx32" | %"PRIX32" |
| LONG               | %"PRId32" |           |           |           |           |
| HRESULT            | %"PRId32" |           |           | %"PRIx32" | %"PRIX32" |
| NTSTATUS           | %"PRId32" |           |           | %"PRIx32" | %"PRIX32" |
| ULONG              |           | %"PRIu32" | %"PRIo32" | %"PRIx32" | %"PRIX32" |
| DWORD              |           | %"PRIu32" | %"PRIo32" | %"PRIx32" | %"PRIX32" |
| DWORD32            |           | %"PRIu32" | %"PRIo32" | %"PRIx32" | %"PRIX32" |
| BOOL               | %"PRId32" |           |           |           |           |
| INT64              | %"PRId64" |           |           |           |           |
| LONG64             | %"PRId64" |           |           |           |           |
| LONGLONG           | %"PRId64" |           |           |           |           |
| UINT64             |           | %"PRIu64" | %"PRIo64" | %"PRIx64" | %"PRIX64" |
| ULONG64            |           | %"PRIu64" | %"PRIo64" | %"PRIx64" | %"PRIX64" |
| ULONGLONG          |           | %"PRIu64" | %"PRIo64" | %"PRIx64" | %"PRIX64" |
| DWORDLONG          |           | %"PRIu64" | %"PRIo64" | %"PRIx64" | %"PRIX64" |
| QWORD              |           | %"PRIu64" | %"PRIo64" | %"PRIx64" | %"PRIX64" |
| ULONG64            |           | %"PRIu64" | %"PRIo64" | %"PRIx64" | %"PRIX64" |


## Pointers

When printing pointers you should cast the argument to ``(void*)``:

```c
rdpContext *pContext;
fprintf(stderr, "rdp context is %p\n", (void*) pContext);
```

If you need more formatting options cast the pointer argument to `size_t` and use
any %"PRI*z" format specifier:

```c
rdpContext *pContext;
fprintf(stderr, "rdp context is %" PRIuz " (0x%" PRIXz ")\n", (size_t) pContext, (size_t) pContext);
```


## Integer Promotion

Remember that integer types smaller than int are promoted when an operation is
performed on them.

Wrong:

```c
UINT8 a, b;
fprintf(stderr, "a - b is %" PRIu8 "\n", a - b);
// depending on the system's PRIu8 definition you might get:
// warning: format specifies type 'unsigned char' but the argument has type 'int'
```

Correct:

```c
UINT8 a, b;
fprintf(stderr, "a - b is %d\n", a - b);
// or ...
fprintf(stderr, "a - b is %" PRIu8 "\n", (UINT8) (a - b));
```

## TCHAR

When using `_tprintf` or similar TCHAR formatting functions or macros you
need to enclose the PRI format defines:

```c
LPCTSTR lpFileName1;
UINT64 fileSize1;

_tprintf(_T("The size of %s is %") _T(PRIu64) _T("\n"), lpFileName1, fileSize1);
```

Since this makes the strings a lot harder to read try to avoid _tprintf if the
arguments don't contain TCHAR types.

Note: If all compilers were C99 compliant we could simply write ...

```c
_tprintf(_T("The size of %s is %") PRIu64 "\n"), lpFileName1, fileSize1);
```

... since the standard says that only one of the character sequences must be
prefixed by an encoding prefix and the rest of them are treated to have the
same. However, Microsoft Visual Studio versions older than VS 2015 are not C99
compliant in this regard.

See [How to use stdint types with _tprintf in Visual Studio 2013](http://stackoverflow.com/questions/41126081/how-to-use-stdint-types-with-tprintf-in-visual-studio-2013)
for more information.



## Links

- [[MS-DTYP] 2.2 Common Data Types](https://msdn.microsoft.com/en-us/library/cc230309.aspx)
- [Understand integer conversion rules](https://www.securecoding.cert.org/confluence/display/c/INT02-C.+Understand+integer+conversion+rules)
- [Printf format strings](https://en.wikipedia.org/wiki/Printf_format_string)
- [C data types - Basic Types](https://en.wikipedia.org/wiki/C_data_types#Basic_types)
