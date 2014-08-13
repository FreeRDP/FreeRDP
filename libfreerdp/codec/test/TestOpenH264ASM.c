#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "TestOpenH264ASM.h"

#define WIDTH 1920
#define HEIGHT 1080

int main(void){
	int i,j,k;
	int ret;
	unsigned char *pDstData_c,*pDstData_asm,*pSrcData[3];
	int nSrcStep[2];
	
	if(check_ssse3()){
		fprintf(stderr,"ssse3 not supported!\n");
		return EXIT_FAILURE;
	}
	
	struct timeval t1,t2,t3;
	
	pSrcData[0]=malloc(1984*HEIGHT*sizeof(char));
	pSrcData[1]=malloc(1984*HEIGHT/4*sizeof(char));
	pSrcData[2]=malloc(1984*HEIGHT/4*sizeof(char));
	pDstData_asm=malloc(WIDTH*HEIGHT*4*sizeof(char));
	pDstData_c=malloc(WIDTH*HEIGHT*4*sizeof(char));
	
	for(i=0;i<WIDTH*HEIGHT;i++){
		pSrcData[0][i]=i%255;
		pSrcData[1][i/4]=pSrcData[0][i];
		pSrcData[2][i/4]=255-pSrcData[0][i];
	}
	
	nSrcStep[0]=1984;
	nSrcStep[1]=992;
	
	gettimeofday(&t1,NULL);
		ret=freerdp_image_yuv420p_to_xrgb(pDstData_asm,pSrcData,WIDTH,HEIGHT,nSrcStep[0],nSrcStep[1]);
	gettimeofday(&t2,NULL);
		freerdp_image_copy_yuv420p_to_xrgb(pDstData_c,WIDTH*4,0,0,WIDTH,HEIGHT,pSrcData,nSrcStep,0,0);
	gettimeofday(&t3,NULL);
	
	printf("in asm (0x%08X) it took %u sec %u usec,\nin c %u sec %u usec.\n",ret,(int)(t2.tv_sec-t1.tv_sec),(int)(t2.tv_usec-t1.tv_usec),
		(int)(t3.tv_sec-t2.tv_sec),(int)(t3.tv_usec-t2.tv_usec));
	
	printf("in asm the result was %X %X %X\n in c %X %X %X.\n",pDstData_asm[0],pDstData_asm[1],pDstData_asm[2],
		pDstData_c[0],pDstData_c[1],pDstData_c[2]);
	
	/*k=0;
	for(i=0;i<HEIGHT+1;i++){
		for(j=0;j<WIDTH;j++){
			printf("%08X:%08X ",((unsigned int*)pDstData_asm)[k],((unsigned int*)pDstData_c)[k]);
			k++;
		}
		puts("\n");
	}*/
	
	k=1;
	for(i=0;i<(WIDTH*HEIGHT*4);i++){
		if(pDstData_c[i]!=pDstData_asm[i]){
			k=0;
			printf("MISSMATCH at %d: %2X != %2X\n",i,(unsigned char)pDstData_asm[i],(unsigned char)pDstData_c[i]);
			break;
		}
	}
	
	if(k)
		printf("everything OK\n");
	
	free(pSrcData[0]);
	free(pSrcData[1]);
	free(pSrcData[2]);
	free(pDstData_c);
	free(pDstData_asm);
	
	return 0;
}
