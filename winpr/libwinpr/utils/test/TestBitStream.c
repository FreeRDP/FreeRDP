
#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/stream.h>
#include <winpr/bitstream.h>

void BitStrGen()
{
	DWORD i, j;
	char str[64];

	for (i = 0; i < 256;)
	{
		printf("\t");

		for (j = 0; j < 4; j++)
		{
			if (0)
			{
				/* Least Significant Bit First */
				str[0] = (i & (1 << 7)) ? '1' : '0';
				str[1] = (i & (1 << 6)) ? '1' : '0';
				str[2] = (i & (1 << 5)) ? '1' : '0';
				str[3] = (i & (1 << 4)) ? '1' : '0';
				str[4] = (i & (1 << 3)) ? '1' : '0';
				str[5] = (i & (1 << 2)) ? '1' : '0';
				str[6] = (i & (1 << 1)) ? '1' : '0';
				str[7] = (i & (1 << 0)) ? '1' : '0';
				str[8] = '\0';
			}
			else
			{
				/* Most Significant Bit First */
				str[7] = (i & (1 << 7)) ? '1' : '0';
				str[6] = (i & (1 << 6)) ? '1' : '0';
				str[5] = (i & (1 << 5)) ? '1' : '0';
				str[4] = (i & (1 << 4)) ? '1' : '0';
				str[3] = (i & (1 << 3)) ? '1' : '0';
				str[2] = (i & (1 << 2)) ? '1' : '0';
				str[1] = (i & (1 << 1)) ? '1' : '0';
				str[0] = (i & (1 << 0)) ? '1' : '0';
				str[8] = '\0';
			}

			printf("\"%s\",%s", str, j == 3 ? "" : " ");
			i++;
		}

		printf("\n");
	}
}

int TestBitStream(int argc, char* argv[])
{
	wBitStream* bs;
	BYTE buffer[1024];
	ZeroMemory(buffer, sizeof(buffer));
	bs = BitStream_New();
	if (!bs)
		return 1;
	BitStream_Attach(bs, buffer, sizeof(buffer));
	BitStream_Write_Bits(bs, 0xAF, 8); /* 11110101 */
	BitStream_Write_Bits(bs, 0xF, 4);  /* 1111 */
	BitStream_Write_Bits(bs, 0xA, 4);  /* 0101 */
	BitStream_Flush(bs);
	BitDump(__FUNCTION__, WLOG_INFO, buffer, bs->position, BITDUMP_MSB_FIRST);
	BitStream_Write_Bits(bs, 3, 2);    /* 11 */
	BitStream_Write_Bits(bs, 0, 3);    /* 000 */
	BitStream_Write_Bits(bs, 0x2D, 6); /* 101101 */
	BitStream_Write_Bits(bs, 0x19, 5); /* 11001 */
	// BitStream_Flush(bs); /* flush should be done automatically here (32 bits written) */
	BitDump(__FUNCTION__, WLOG_INFO, buffer, bs->position, BITDUMP_MSB_FIRST);
	BitStream_Write_Bits(bs, 3, 2); /* 11 */
	BitStream_Flush(bs);
	BitDump(__FUNCTION__, WLOG_INFO, buffer, bs->position, BITDUMP_MSB_FIRST);
	BitStream_Write_Bits(bs, 00, 2);  /* 00 */
	BitStream_Write_Bits(bs, 0xF, 4); /* 1111 */
	BitStream_Write_Bits(bs, 0, 20);
	BitStream_Write_Bits(bs, 0xAFF, 12); /* 111111110101 */
	BitStream_Flush(bs);
	BitDump(__FUNCTION__, WLOG_INFO, buffer, bs->position, BITDUMP_MSB_FIRST);
	BitStream_Free(bs);
	return 0;
}
