#include <stdio.h>
typedef unsigned short UINT16;
typedef unsigned char BYTE;
static UINT16 HuffCodeLOM[] = { 0x0001, 0x0000, 0x0002, 0x0009, 0x0006, 0x0005, 0x000d, 0x000b,
	                            0x0003, 0x001b, 0x0007, 0x0017, 0x0037, 0x000f, 0x004f, 0x006f,
	                            0x002f, 0x00ef, 0x001f, 0x005f, 0x015f, 0x009f, 0x00df, 0x01df,
	                            0x003f, 0x013f, 0x00bf, 0x01bf, 0x007f, 0x017f, 0x00ff, 0x01ff };

UINT16 HashTable[32] = { [0 ... 31] = 0xffff };

BYTE tab[4] = { 0, 4, 10, 19 };

UINT16 hash(UINT16 key)
{
	return ((key & 0x1f) ^ (key >> 5) ^ (key >> 9));
}

BYTE minihash(UINT16 key)
{
	BYTE h;
	h = (key >> 4) & 0xf;
	return ((h ^ (h >> 2) ^ (h >> 3)) & 0x3);
}

void buildhashtable(void)
{
	int i, j;
	UINT16 h;

	for (i = 0; i < 32; i++)
	{
		h = hash(HuffCodeLOM[i]);

		if (HashTable[h] != 0xffff)
		{
			HashTable[h] ^= (HuffCodeLOM[i] & 0xfe0) ^ 0xfe0;
			HashTable[tab[minihash(HuffCodeLOM[i])]] = i;
		}
		else
		{
			HashTable[h] = i;
			HashTable[h] ^= 0xfe0;
		}

		printf("at %d %" PRIu16 "=0x%" PRIx16 "\n", i, h, HashTable[h]);
	}
}

BYTE getvalue(UINT16 huff)
{
	UINT16 h = HashTable[hash(huff)];

	if ((h ^ huff) >> 5)
	{
		return h & 0x1f;
	}
	else
		return HashTable[tab[minihash(huff)]];
}

main()
{
	int i;
	buildhashtable();
	printf("static UINT16 HuffIndexLOM[32] = {\n");

	for (i = 0; i < 32; i++)
	{
		if (i == 31)
			printf("0x%" PRIx16 " };\n", HashTable[i]);
		else
			printf("0x%" PRIx16 ", ", HashTable[i]);
	}

	for (i = 0; i < 32; i++)
		if (i != getvalue(HuffCodeLOM[i]))
			printf("Fail :( at %d : 0x%04" PRIx16 " got %" PRIu8 "\n", i, HuffCodeLOM[i],
			       getvalue(HuffCodeLOM[i]));

	return 0;
}
