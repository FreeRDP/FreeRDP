/** function for converting YUV420p data to the RGB format (but without any special upconverting)
 * It's completely written in nasm-x86-assembly for intel processors supporting SSSE3 and higher.
 * The target dstStep (6th parameter) must be a multiple of 16.
 * srcStep[0] must be (target dstStep) / 4 or bigger and srcStep[1] the next multiple of four
 * of the half of srcStep[0] or bigger
 */

#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/sysinfo.h>
#include <winpr/crt.h>
#include <freerdp/types.h>
#include <freerdp/primitives.h>


#ifdef WITH_SSE2

#include <emmintrin.h>
#include <tmmintrin.h>

pstatus_t ssse3_YUV420ToRGB_8u_P3AC4R(const BYTE **pSrc, int *srcStep,
		BYTE *pDst, int dstStep, const prim_size_t *roi)
{
	char last_line,last_column;
	int i,nWidth,nHeight,VaddDst,VaddY,VaddU,VaddV;
	
	BYTE *UData,*VData,*YData;
	
	__m128i r0,r1,r2,r3,r4,r5,r6,r7;
	__m128i *buffer;
	
	
	buffer=_aligned_malloc(4*16,16);
	
	
	YData=(BYTE *)pSrc[0];
	UData=(BYTE *)pSrc[1];
	VData=(BYTE *)pSrc[2];
	
	nWidth=roi->width;
	nHeight=roi->height;
	
	
	if((last_column=nWidth&3)){
		switch(last_column){
			case 1: r7=_mm_set_epi32(0,0,0,0xFFFFFFFF); break;
			case 2: r7=_mm_set_epi32(0,0,0xFFFFFFFF,0xFFFFFFFF); break;
			case 3: r7=_mm_set_epi32(0,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF); break;
		}
		_mm_store_si128(buffer+3,r7);
		last_column=1;
	}
	
	nWidth+=3;
	nWidth=nWidth>>2;
	
	
	last_line=nHeight&1;
	nHeight++;
	nHeight=nHeight>>1;
	
	
	VaddDst=(dstStep<<1)-(nWidth<<4);
	VaddY=(srcStep[0]<<1)-(nWidth<<2);
	VaddU=srcStep[1]-(((nWidth<<1)+2)&0xFFFC);
	VaddV=srcStep[2]-(((nWidth<<1)+2)&0xFFFC);
	
	
	while(nHeight-- >0){
		if(nHeight==0){
			last_line=last_line<<1;
		}
		
		i=0;
		do{
/*
 * Well, in the end it should look like this:
 *	C = Y;
 *	D = U - 128;
 *	E = V - 128;
 *
 *	R = clip(( 256 * C           + 403 * E + 128) >> 8);
 *	G = clip(( 256 * C -  48 * D - 120 * E + 128) >> 8);
 *	B = clip(( 256 * C + 475 * D           + 128) >> 8);
 */
			if(!(i&0x01)){
/* Y-, U- and V-data is stored in different arrays.
 * We start with processing U-data.
 *
 * at first we fetch four U-values from its array and shuffle them like this:
 *	0d0d 0c0c 0b0b 0a0a
 * we've done two things: converting the values to signed words and duplicating
 * each value, because always two pixel "share" the same U- (and V-) data
 */
				r0=_mm_cvtsi32_si128(*(UINT32 *)UData);
				r5=_mm_set_epi32(0x80038003,0x80028002,0x80018001,0x80008000);
				r0=_mm_shuffle_epi8(r0,r5);
				
				UData+=4;
				
				r3=_mm_set_epi16(128,128,128,128,128,128,128,128);
				r0=_mm_subs_epi16(r0,r3);
				
				r2=r0;
				
				r4=r0;
				r7=_mm_set_epi16(48,48,48,48,48,48,48,48);
				r0=_mm_mullo_epi16(r0,r7);
				r4=_mm_mulhi_epi16(r4,r7);
				r7=r0;
				r0=_mm_unpacklo_epi16(r0,r4);
				r4=_mm_unpackhi_epi16(r7,r4);
				
				
				r6=_mm_set_epi32(128,128,128,128);
				r0=_mm_sub_epi32(r0,r6);
				r4=_mm_sub_epi32(r4,r6);
				
				
				r1=r2;
				r7=_mm_set_epi16(475,475,475,475,475,475,475,475);
				r1=_mm_mullo_epi16(r1,r7);
				r2=_mm_mulhi_epi16(r2,r7);
				r7=r1;
				r1=_mm_unpacklo_epi16(r1,r2);
				r7=_mm_unpackhi_epi16(r7,r2);
				
				r1=_mm_add_epi32(r1,r6);
				r7=_mm_add_epi32(r7,r6);
				
				_mm_store_si128(buffer+1,r7);
				
/* Now we've prepared U-data. Preparing V-data is actually the same, just with other coefficients */
				r2=_mm_cvtsi32_si128(*(UINT32 *)VData);
				r2=_mm_shuffle_epi8(r2,r5);
				
				VData+=4;
				
				r2=_mm_subs_epi16(r2,r3);
				
				r5=r2;
				
				
				r3=r2;
				r7=_mm_set_epi16(403,403,403,403,403,403,403,403);
				r2=_mm_mullo_epi16(r2,r7);
				r3=_mm_mulhi_epi16(r3,r7);
				r7=r2;
				r2=_mm_unpacklo_epi16(r2,r3);
				r7=_mm_unpackhi_epi16(r7,r3);
				
				r2=_mm_add_epi32(r2,r6);
				r7=_mm_add_epi32(r7,r6);
				
				_mm_store_si128(buffer+2,r7);
				
				
				
				r3=r5;
				r7=_mm_set_epi16(120,120,120,120,120,120,120,120);
				r3=_mm_mullo_epi16(r3,r7);
				r5=_mm_mulhi_epi16(r5,r7);
				r7=r3;
				r3=_mm_unpacklo_epi16(r3,r5);
				r7=_mm_unpackhi_epi16(r7,r5);
				
				r0=_mm_add_epi32(r0,r3);
				r4=_mm_add_epi32(r4,r7);
				
				_mm_store_si128(buffer,r4);
			}else{
				r1=_mm_load_si128(buffer+1);
				r2=_mm_load_si128(buffer+2);
				r0=_mm_load_si128(buffer);
			}
			
			if(++i==nWidth)
				last_column=last_column<<1;
			
			//processing Y data
			r4=_mm_cvtsi32_si128(*(UINT32 *)YData);
			r7=_mm_set_epi32(0x80800380,0x80800280,0x80800180,0x80800080);
			r4=_mm_shuffle_epi8(r4,r7);
			
			r5=r4;
			r6=r4;
			
			r4=_mm_add_epi32(r4,r2);
			r5=_mm_sub_epi32(r5,r0);
			r6=_mm_add_epi32(r6,r1);
			
			
			r4=_mm_slli_epi32(r4,8);
			r5=_mm_slli_epi32(r5,8);
			r6=_mm_slli_epi32(r6,8);
			
			r7=_mm_set_epi32(0,0,0,0);
			r4=_mm_max_epi16(r4,r7);
			r5=_mm_max_epi16(r5,r7);
			r6=_mm_max_epi16(r6,r7);
			
			r7=_mm_set_epi32(0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000);
			r4=_mm_min_epi16(r4,r7);
			r5=_mm_min_epi16(r5,r7);
			r6=_mm_min_epi16(r6,r7);
			
			//r7=_mm_set_epi32(0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000);
			r4=_mm_and_si128(r4,r7);
			
			r7=_mm_set_epi32(0x80800E80,0x80800A80,0x80800680,0x80800280);
			r5=_mm_shuffle_epi8(r5,r7);
			
			r7=_mm_set_epi32(0x8080800E,0x8080800A,0x80808006,0x80808002);
			r6=_mm_shuffle_epi8(r6,r7);
			
			
			r4=_mm_or_si128(r4,r5);
			r4=_mm_or_si128(r4,r6);
			
			
			if(last_column&0x02){
				r6=_mm_load_si128(buffer+3);
				r4=_mm_and_si128(r4,r6);
				r5=_mm_lddqu_si128((__m128i *)pDst);
				r6=_mm_andnot_si128(r6,r5);
				r4=_mm_or_si128(r4,r6);
			}
			_mm_storeu_si128((__m128i *)pDst,r4);
			
			//Y data processing in secound line
			if(!(last_line&0x02)){
				r4=_mm_cvtsi32_si128(*(UINT32 *)(YData+srcStep[0]));
				r7=_mm_set_epi32(0x80800380,0x80800280,0x80800180,0x80800080);
				r4=_mm_shuffle_epi8(r4,r7);
				
				r5=r4;
				r6=r4;
				
				r4=_mm_add_epi32(r4,r2);
				r5=_mm_sub_epi32(r5,r0);
				r6=_mm_add_epi32(r6,r1);
				
				
				r4=_mm_slli_epi32(r4,8);
				r5=_mm_slli_epi32(r5,8);
				r6=_mm_slli_epi32(r6,8);
				
				r7=_mm_set_epi32(0,0,0,0);
				r4=_mm_max_epi16(r4,r7);
				r5=_mm_max_epi16(r5,r7);
				r6=_mm_max_epi16(r6,r7);
				
				r7=_mm_set_epi32(0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000);
				r4=_mm_min_epi16(r4,r7);
				r5=_mm_min_epi16(r5,r7);
				r6=_mm_min_epi16(r6,r7);
				
				r7=_mm_set_epi32(0x00FF0000,0x00FF0000,0x00FF0000,0x00FF0000);
				r4=_mm_and_si128(r4,r7);
				
				r7=_mm_set_epi32(0x80800E80,0x80800A80,0x80800680,0x80800280);
				r5=_mm_shuffle_epi8(r5,r7);
				
				r7=_mm_set_epi32(0x8080800E,0x8080800A,0x80808006,0x80808002);
				r6=_mm_shuffle_epi8(r6,r7);
				
				
				r4=_mm_or_si128(r4,r5);
				r4=_mm_or_si128(r4,r6);
				
				
				if(last_column&0x02){
					r6=_mm_load_si128(buffer+3);
					r4=_mm_and_si128(r4,r6);
					r5=_mm_lddqu_si128((__m128i *)(pDst+dstStep));
					r6=_mm_andnot_si128(r6,r5);
					r4=_mm_or_si128(r4,r6);
					
					last_column=last_column>>1;
				}
				_mm_storeu_si128((__m128i *)(pDst+dstStep),r4);
			}
			
			pDst+=16;
			YData+=4;
			
		}while(i<nWidth);
		
		pDst+=VaddDst;
		YData+=VaddY;
		UData+=VaddU;
		VData+=VaddV;
	}
		
	_aligned_free(buffer);
	
	return PRIMITIVES_SUCCESS;
}
#endif

void primitives_init_YUV_opt(primitives_t *prims)
{
#ifdef WITH_SSE2
	if(IsProcessorFeaturePresentEx(PF_EX_SSSE3)&&IsProcessorFeaturePresent(PF_SSE3_INSTRUCTIONS_AVAILABLE))
	{
		prims->YUV420ToRGB_8u_P3AC4R=ssse3_YUV420ToRGB_8u_P3AC4R;
	}
#endif
}
