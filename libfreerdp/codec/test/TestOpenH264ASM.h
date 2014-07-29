extern int YUV_to_RGB_asm(unsigned char Y,unsigned char U,unsigned char V);
extern int YUV_to_RGB_2asm(unsigned char Y);
extern int YUV_to_RGB(unsigned char Y,unsigned char U,unsigned char V);

extern int freerdp_image_yuv_to_xrgb_asm(unsigned char *pDstData,unsigned char **pSrcData,int nWidth,int nHeight,int istride0,int istride1);
int freerdp_image_copy_yuv420p_to_xrgb(unsigned char* pDstData, int nDstStep, int nXDst, int nYDst,
		int nWidth, int nHeight, unsigned char* pSrcData[3], int nSrcStep[2], int nXSrc, int nYSrc);