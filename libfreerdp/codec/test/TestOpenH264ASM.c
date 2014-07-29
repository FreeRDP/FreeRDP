#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "TestOpenH264ASM.h"

int main(void){
	int ret,i;
	unsigned char *pDstData_c,*pDstData_asm,*pSrcData[3];
	int nSrcStep[2];
	
	struct timeval t1,t2,t3;
	
	pSrcData[0]=malloc(1920*1080*sizeof(char));
	pSrcData[1]=malloc(1920*1080/4*sizeof(char));
	pSrcData[2]=malloc(1920*1080/4*sizeof(char));
	pDstData_asm=malloc(1920*1080*4*sizeof(char));
	pDstData_c=malloc(1920*1080*4*sizeof(char));
	
	for(i=0;i<1920*1080;i++){
		pSrcData[0][i]=i%255;
		pSrcData[1][i/4]=pSrcData[0][i];
		pSrcData[2][i/4]=255-pSrcData[0][i];
	}
	
	printf("%X\n",pSrcData[0][0]);
	
	nSrcStep[0]=1088;
	nSrcStep[1]=544;
	
	gettimeofday(&t1,NULL);
		ret=freerdp_image_yuv_to_xrgb_asm(pDstData_asm,pSrcData,1024,768,1088,544);
	gettimeofday(&t2,NULL);
		freerdp_image_copy_yuv420p_to_xrgb(pDstData_c,1024*4,0,0,1024,768,pSrcData,nSrcStep,0,0);
	gettimeofday(&t3,NULL);
	
	printf("in asm (%d) it took %u sec %u usec,\nin c %u sec %u usec.\n",ret,(int)(t2.tv_sec-t1.tv_sec),(int)(t2.tv_usec-t1.tv_usec),
		(int)(t3.tv_sec-t2.tv_sec),(int)(t3.tv_usec-t2.tv_usec));
	
	printf("in asm the result was %X %X %X\n in c %X %X %X.\n",(unsigned char *)pDstData_asm[92],(unsigned char *)pDstData_asm[93],(unsigned char *)pDstData_asm[94],
		(unsigned char *)pDstData_c[92],(unsigned char *)pDstData_c[93],(unsigned char *)pDstData_c[94]);
	
	for(i=0;i<(1920*1080*4);i++){
		if(pDstData_c[i]!=pDstData_asm[i]){
			printf("MISSMATCH at %d: %2X != %2X\n",i,(unsigned char)pDstData_asm[i],(unsigned char)pDstData_c[i]);
			break;
		}
	}
	
	free(pSrcData[0]);
	free(pSrcData[1]);
	free(pSrcData[2]);
	free(pDstData_c);
	free(pDstData_asm);
	
	return 0;
}
