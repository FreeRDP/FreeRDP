;R=(256*Y+403*(V-128)+128)/265			=(256*Y+403*V-51456)/256
;G=(256*Y-48*(U-128)-120*(V-128)+128)/256	=(256*Y-48*U-120*V+21632)/256
;B=(256*Y+475*(U-128)+128)/256			=(256*Y+475*U-60672)/256

section .text
	;global YUV_to_RGB_asm
YUV_to_RGB_asm:
	shl edi,8
	
	mov eax,edx
	imul eax,403
	add eax,edi
	sub eax,51456
	
	jae YUV_to_RGB_asm1
	mov eax,0
	jmp YUV_to_RGB_asm11

YUV_to_RGB_asm1:
	cmp eax, 0xFFFF
	jbe YUV_to_RGB_asm11
	mov eax,0xFF00
	
YUV_to_RGB_asm11:
	and eax,0xFF00
	shl eax,8
	
	mov ebx,esi
	imul ebx,475
	add ebx,edi
	sub ebx,60672
	
	jae YUV_to_RGB_asm2
	mov ebx, 0
	jmp YUV_to_RGB_asm21

YUV_to_RGB_asm2:
	cmp ebx,0xFFFF
	jbe YUV_to_RGB_asm21
	mov ebx,0xFF00
	
YUV_to_RGB_asm21:
	and ebx,0xFF00
	shr ebx,8
	
	imul edx,120
	sub edi,edx
	imul esi,48
	sub edi,esi
	add edi,21632
	
	bt edi,31
	jae YUV_to_RGB_asm3
	mov edi, 0
	jmp YUV_to_RGB_asm31
	
YUV_to_RGB_asm3:
	cmp edi,0xFFFF
	jbe YUV_to_RGB_asm31
	mov edi, 0xFF00
	
YUV_to_RGB_asm31:
	and edi,0xFF00
	
	or eax,edi
	or eax,ebx
	
	ret

;extern int freerdp_image_yuv_to_xrgb_asm(unsigned char *pDstData,unsigned char **pSrcData,int nWidth,int nHeight);
	global freerdp_image_yuv_to_xrgb_asm
freerdp_image_yuv_to_xrgb_asm:
	push ebp
	mov ebp, esp
			;cWidth: cx
	sub esp,36	;pDstData,pSrcData[3],nWidth,nHeight,cHeight,scanline,iStride[0] addition
	push ebx
	
	
	mov edi,[ebp+8]
	mov [ebp-4],edi
	
	mov esi,[ebp+12]
	mov eax,[esi]
	mov [ebp-8],eax
	mov eax,[esi+4]
	mov [ebp-12],eax
	mov eax,[esi+8]
	mov [ebp-16],eax
	
	mov edx,[ebp+16]
	mov [ebp-20],edx
	
	
	mov ecx,[ebp+20]
	shr ecx,1	;/2
	mov [ebp-24],ecx
	
	
	shl edx,2
	mov [ebp-32],edx
	
	
	mov eax,[ebp-24]
	mov [ebp-28],eax
	
	
	mov ebx,[ebp+24]
	mov [ebp-36],ebx
	mov eax,[ebp-20]
	shl dword [ebp-36],1
	sub [ebp-36],eax

	shr eax,1
	sub [ebp+28],eax
	
freerdp_image_yuv_to_xrgb_asm_loopH:
	mov ecx,[ebp-20]
	shr ecx,1
	
	
freerdp_image_yuv_to_xrgb_asm_loopW:
	mov eax,[ebp-8]
	mov edi,[eax]
	and edi,0xFF
	
	mov eax,[ebp-12]
	mov esi,[eax]
	and esi,0xFF
	
	mov eax,[ebp-16]
	mov edx,[eax]
	and edx,0xFF
	
	call YUV_to_RGB_asm
	
	mov ebx,[ebp-4]
	mov [ebx],eax
	
	
	mov eax,[ebp-8]
	mov ebx,[ebp+24]
	mov edi,[eax+ebx]
	inc eax
	mov [ebp-8],eax
	and edi,0xFF
	
	mov eax,[ebp-12]
	mov esi,[eax]
	and esi,0xFF
	
	mov eax,[ebp-16]
	mov edx,[eax]
	and edx,0xFF
	
	call YUV_to_RGB_asm
	
	mov ebx,[ebp-4]
	mov edx,[ebp-32]
	mov [ebx+edx],eax
	add ebx,4
	mov [ebp-4],ebx
	
	
	mov eax,[ebp-8]
	mov edi,[eax]
	and edi,0xFF
	
	mov eax,[ebp-12]
	mov esi,[eax]
	and esi,0xFF
	
	mov eax,[ebp-16]
	mov edx,[eax]
	and edx,0xFF
	
	call YUV_to_RGB_asm
	
	mov ebx,[ebp-4]
	mov [ebx],eax
	
	
	mov eax,[ebp-8]
	mov ebx,[ebp+24]
	mov edi,[eax+ebx]
	inc eax
	mov [ebp-8],eax
	and edi,0xFF
	
	mov eax,[ebp-12]
	mov esi,[eax]
	inc eax
	mov [ebp-12],eax
	and esi,0xFF
	
	mov eax,[ebp-16]
	mov edx,[eax]
	inc eax
	mov [ebp-16],eax
	and edx,0xFF
	
	call YUV_to_RGB_asm

	mov ebx,[ebp-4]
	mov edx,[ebp-32]
	mov [ebx+edx],eax
	add ebx,4
	mov [ebp-4],ebx

	dec cx
	jne freerdp_image_yuv_to_xrgb_asm_loopW
	
	
	mov eax,[ebp-4]
	add eax,[ebp-32]
	mov [ebp-4],eax
	
	mov eax,[ebp-8]
	add eax,[ebp-36]
	mov [ebp-8],eax
	
	mov ebx,[ebp+28]
	mov eax,[ebp-12]
	add eax,ebx
	mov [ebp-12],eax
	
	mov eax,[ebp-16]
	add eax,ebx
	mov [ebp-16],eax
	
	dec dword [ebp-28]
	jne freerdp_image_yuv_to_xrgb_asm_loopH
	
;END
	mov eax,0
END:
	pop ebx
	mov esp,ebp
	pop ebp
	ret
